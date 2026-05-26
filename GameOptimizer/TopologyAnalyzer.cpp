// Build: cl /std:c++latest /O2 /W4 /WX /permissive- TopologyAnalyzer.cpp
// MODULE: TopologyAnalyzer
// ERROR-POLICY: expected
// Reason: topology fallback must be explicit and non-throwing.

#include "TopologyAnalyzer.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Logger.h"
#include "WinApiError.h"
#include "WinHandle.h"

namespace
{
    constexpr int kTopologyInfoMaxRetryCount = 3;

    constexpr long long kKnownL3SameCacheBonus = 1'000'000LL;
    constexpr long long kKnownL3SizeKilobyteWeight = 1LL;
    constexpr long long kKnownL3SelectedCoreWeight = 100LL;

    constexpr long long kFallbackSmtCleanBonus = 500'000LL;
    constexpr long long kFallbackSelectedCoreWeight = 1'000LL;

    constexpr long long kLowerGroupTieBreakerWeight = 1LL;

    struct L3Candidate
    {
        DWORD_PTR mask = 0;
        DWORD sizeBytes = 0;
    };

    struct GroupCandidate
    {
        DWORD_PTR physicalRepresentativeMask = 0;
        std::vector<L3Candidate> l3Candidates;
    };

    struct SelectedCandidate
    {
        WORD group = 0;
        DWORD_PTR mask = 0;
        DWORD l3SizeBytes = 0;
        bool sameL3 = false;
        bool smtClean = false;
        bool l3SizeKnown = false;
        bool l3ScoreFallback = false;
        int selectedCoreCount = 0;
        long long score = -1;
    };

    [[nodiscard]] int countSetBitsLocal(DWORD_PTR mask) noexcept
    {
        return std::popcount(mask);
    }

    [[nodiscard]] DWORD_PTR lowestSetBitLocal(DWORD_PTR mask) noexcept
    {
        return mask == 0 ? 0 : (static_cast<DWORD_PTR>(1) << std::countr_zero(mask));
    }

    [[nodiscard]] bool containsGroup(const std::vector<WORD>& groups, WORD group) noexcept
    {
        return std::find(groups.begin(), groups.end(), group) != groups.end();
    }

    [[nodiscard]] DWORD_PTR activeProcessorMaskForGroup(WORD group) noexcept
    {
        const DWORD activeCount = GetActiveProcessorCount(group);
        if (activeCount == 0 || activeCount > (sizeof(DWORD_PTR) * 8U))
        {
            return 0;
        }

        if (activeCount == (sizeof(DWORD_PTR) * 8U))
        {
            return ~static_cast<DWORD_PTR>(0);
        }

        return (static_cast<DWORD_PTR>(1) << activeCount) - 1;
    }

    [[nodiscard]] std::expected<std::vector<WORD>, ErrorCode>
    queryProcessGroups(DWORD processId)
    {
        WinHandle processHandle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
        if (!processHandle)
        {
            return std::unexpected(ErrorCode::ProcessOpenFailed);
        }

        USHORT groupCount = GetActiveProcessorGroupCount();
        if (groupCount == 0)
        {
            return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
        }

        std::vector<USHORT> rawGroups(groupCount);
        USHORT writableGroupCount = groupCount;
        if (!GetProcessGroupAffinity(processHandle.get(), &writableGroupCount, rawGroups.data()))
        {
            const DWORD lastError = GetLastError();
            if (lastError == ERROR_INSUFFICIENT_BUFFER && writableGroupCount > rawGroups.size())
            {
                rawGroups.resize(writableGroupCount);
                if (!GetProcessGroupAffinity(processHandle.get(), &writableGroupCount, rawGroups.data()))
                {
                    return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
                }
            }
            else
            {
                return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
            }
        }

        rawGroups.resize(writableGroupCount);
        std::vector<WORD> groups;
        groups.reserve(rawGroups.size());
        for (const USHORT rawGroup : rawGroups)
        {
            const WORD group = static_cast<WORD>(rawGroup);
            if (!containsGroup(groups, group))
            {
                groups.push_back(group);
            }
        }

        if (groups.empty())
        {
            return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
        }

        std::sort(groups.begin(), groups.end());
        return groups;
    }

    [[nodiscard]] std::string formatGroups(const std::vector<WORD>& groups)
    {
        std::string text;
        for (size_t i = 0; i < groups.size(); ++i)
        {
            if (i != 0)
            {
                text += ",";
            }
            text += std::to_string(static_cast<unsigned int>(groups[i]));
        }
        return text;
    }

    [[nodiscard]] std::expected<std::vector<std::byte>, ErrorCode>
    queryLogicalProcessorInfo()
    {
        DWORD bufferSize = 0;

        for (int attempt = 0; attempt < kTopologyInfoMaxRetryCount; ++attempt)
        {
            if (bufferSize == 0)
            {
                if (GetLogicalProcessorInformationEx(RelationAll, nullptr, &bufferSize) ||
                    GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
                    bufferSize == 0)
                {
                    return std::unexpected(ErrorCode::TopologyScanFailed);
                }
            }

            std::vector<std::byte> buffer(bufferSize);
            auto* records = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
            DWORD writableSize = bufferSize;

            if (GetLogicalProcessorInformationEx(RelationAll, records, &writableSize))
            {
                buffer.resize(writableSize);
                return buffer;
            }

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || writableSize == 0)
            {
                return std::unexpected(ErrorCode::TopologyScanFailed);
            }

            bufferSize = writableSize;
            Logger::warn(
                "topology scan buffer resized during GetLogicalProcessorInformationEx retry attempt {}",
                attempt + 1);
        }

        return std::unexpected(ErrorCode::TopologyScanFailed);
    }

    [[nodiscard]] SelectedCandidate chooseBestCandidate(
        const std::unordered_map<WORD, GroupCandidate>& candidates,
        const std::vector<WORD>& processGroups) noexcept
    {
        SelectedCandidate best{};
        if (!processGroups.empty())
        {
            best.group = processGroups.front();
        }

        bool hasAnyKnownL3Size = false;
        for (const WORD group : processGroups)
        {
            const auto it = candidates.find(group);
            if (it == candidates.end())
            {
                continue;
            }

            for (const L3Candidate& l3Candidate : it->second.l3Candidates)
            {
                if (l3Candidate.sizeBytes > 0)
                {
                    hasAnyKnownL3Size = true;
                    break;
                }
            }

            if (hasAnyKnownL3Size)
            {
                break;
            }
        }

        for (const WORD group : processGroups)
        {
            const auto it = candidates.find(group);
            if (it == candidates.end())
            {
                continue;
            }

            const GroupCandidate& groupCandidate = it->second;
            SelectedCandidate local{};
            local.group = group;
            local.mask = groupCandidate.physicalRepresentativeMask;
            local.smtClean = groupCandidate.physicalRepresentativeMask != 0;
            local.selectedCoreCount = countSetBitsLocal(local.mask);

            if (hasAnyKnownL3Size)
            {
                for (const L3Candidate& l3Candidate : groupCandidate.l3Candidates)
                {
                    const DWORD_PTR candidateMask = groupCandidate.physicalRepresentativeMask & l3Candidate.mask;
                    const int candidateCount = countSetBitsLocal(candidateMask);
                    if (candidateCount == 0)
                    {
                        continue;
                    }

                    const bool candidateHasKnownL3Size = l3Candidate.sizeBytes > 0;
                    const bool candidateIsBetter =
                        !local.sameL3 ||
                        (candidateHasKnownL3Size && !local.l3SizeKnown) ||
                        (candidateHasKnownL3Size && l3Candidate.sizeBytes > local.l3SizeBytes) ||
                        (l3Candidate.sizeBytes == local.l3SizeBytes && candidateCount > local.selectedCoreCount);

                    if (candidateIsBetter)
                    {
                        local.mask = candidateMask;
                        local.l3SizeBytes = l3Candidate.sizeBytes;
                        local.sameL3 = true;
                        local.l3SizeKnown = candidateHasKnownL3Size;
                        local.selectedCoreCount = candidateCount;
                    }
                }
            }

            if (hasAnyKnownL3Size)
            {
                const long long sameCacheScore = local.sameL3 ? kKnownL3SameCacheBonus : 0LL;
                const long long l3SizeScore = local.l3SizeKnown
                    ? ((static_cast<long long>(local.l3SizeBytes) / 1024LL) * kKnownL3SizeKilobyteWeight)
                    : 0LL;
                const long long selectedCoreScore =
                    static_cast<long long>(local.selectedCoreCount) * kKnownL3SelectedCoreWeight;
                const long long groupTieBreaker =
                    static_cast<long long>(group) * kLowerGroupTieBreakerWeight;

                local.score = sameCacheScore + l3SizeScore + selectedCoreScore - groupTieBreaker;
            }
            else
            {
                local.l3ScoreFallback = true;
                const long long smtCleanScore = local.smtClean ? kFallbackSmtCleanBonus : 0LL;
                const long long selectedCoreScore =
                    static_cast<long long>(local.selectedCoreCount) * kFallbackSelectedCoreWeight;
                const long long groupTieBreaker =
                    static_cast<long long>(group) * kLowerGroupTieBreakerWeight;

                local.score = smtCleanScore + selectedCoreScore - groupTieBreaker;
            }

            if (local.score > best.score)
            {
                best = local;
            }
        }

        return best;
    }

    [[nodiscard]] std::expected<void, ErrorCode> collectTopologyCandidates(
        std::span<const std::byte> topologyBuffer,
        std::unordered_map<WORD, GroupCandidate>& candidates) noexcept
    {
        const std::byte* cursor = topologyBuffer.data();
        const std::byte* const end = topologyBuffer.data() + topologyBuffer.size();
        while (cursor < end)
        {
            const auto remainingBytes = static_cast<std::size_t>(end - cursor);
            if (remainingBytes < sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX))
            {
                return std::unexpected(ErrorCode::TopologyScanFailed);
            }

            const auto* record = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(cursor);
            if (record->Size == 0 || record->Size > remainingBytes)
            {
                return std::unexpected(ErrorCode::TopologyScanFailed);
            }

            if (record->Relationship == RelationProcessorCore)
            {
                for (WORD i = 0; i < record->Processor.GroupCount; ++i)
                {
                    const GROUP_AFFINITY& groupMask = record->Processor.GroupMask[i];
                    const WORD group = groupMask.Group;
                    const auto candidateIt = candidates.find(group);
                    if (candidateIt == candidates.end())
                    {
                        continue;
                    }

                    const DWORD_PTR activeMask = activeProcessorMaskForGroup(group);
                    const DWORD_PTR coreMask = static_cast<DWORD_PTR>(groupMask.Mask) & activeMask;
                    candidateIt->second.physicalRepresentativeMask |= lowestSetBitLocal(coreMask);
                }
            }
            else if (record->Relationship == RelationCache && record->Cache.Level == 3)
            {
                const GROUP_AFFINITY& groupMask = record->Cache.GroupMask;
                const WORD group = groupMask.Group;
                const auto candidateIt = candidates.find(group);
                if (candidateIt != candidates.end())
                {
                    const DWORD_PTR activeMask = activeProcessorMaskForGroup(group);
                    const DWORD_PTR l3Mask = static_cast<DWORD_PTR>(groupMask.Mask) & activeMask;
                    if (l3Mask != 0)
                    {
                        candidateIt->second.l3Candidates.push_back(L3Candidate{
                            .mask = l3Mask,
                            .sizeBytes = record->Cache.CacheSize});
                    }
                }
            }

            cursor += record->Size;
        }

        return {};
    }
}

DWORD_PTR TopologyAnalyzer::lowestSetBit(DWORD_PTR mask) noexcept
{
    return mask == 0 ? 0 : (static_cast<DWORD_PTR>(1) << std::countr_zero(mask));
}

DWORD_PTR TopologyAnalyzer::takeLowestBits(DWORD_PTR mask, int count) noexcept
{
    DWORD_PTR result = 0;
    for (int i = 0; i < count && mask != 0; ++i)
    {
        const DWORD_PTR bit = lowestSetBit(mask);
        result |= bit;
        mask &= ~bit;
    }

    return result;
}

int TopologyAnalyzer::countSetBits(DWORD_PTR mask) noexcept
{
    return std::popcount(mask);
}

std::expected<TopologyResult, ErrorCode> TopologyAnalyzer::buildProcessAffinityFallbackMask(
    DWORD_PTR processMask,
    DWORD_PTR systemMask,
    WORD processorGroup) noexcept
{
    const DWORD_PTR availableMask = processMask & systemMask;
    const DWORD_PTR selectedMask = takeLowestBits(availableMask, 2);
    if (selectedMask == 0)
    {
        return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
    }

    const DWORD_PTR primaryMask = lowestSetBit(selectedMask);
    DWORD_PTR fallbackMask = selectedMask & ~primaryMask;
    const int selectedCount = countSetBits(selectedMask);
    if (selectedCount == 1)
    {
        fallbackMask = 0;
    }

    return TopologyResult{
        .primaryMask = primaryMask,
        .fallbackMask = fallbackMask,
        .validatedMask = selectedMask,
        .processorGroup = processorGroup,
        .sameL3Cache = false,
        .smtClean = false,
        .l3SizeKnown = false,
        .l3ScoreFallback = true,
        .maskProvenance = TopologyMaskProvenance::ProcessAffinityFallback,
        .selectedLogicalCoreCount = selectedCount};
}

std::expected<TopologyResult, ErrorCode>
TopologyAnalyzer::buildMainThreadMask(DWORD processId) noexcept
{
    if (processId == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    try
    {
        const auto processGroupsResult = queryProcessGroups(processId);
        if (!processGroupsResult)
        {
            return std::unexpected(processGroupsResult.error());
        }

        const auto& processGroups = *processGroupsResult;
        const auto bufferResult = queryLogicalProcessorInfo();
        if (!bufferResult)
        {
            return std::unexpected(bufferResult.error());
        }

        std::unordered_map<WORD, GroupCandidate> candidates;
        for (const WORD group : processGroups)
        {
            candidates.emplace(group, GroupCandidate{});
        }

        const auto& topologyBuffer = *bufferResult;
        const auto collectResult = collectTopologyCandidates(topologyBuffer, candidates);
        if (!collectResult)
        {
            return std::unexpected(collectResult.error());
        }

        SelectedCandidate selected = chooseBestCandidate(candidates, processGroups);
        const WORD selectedGroup = selected.group;

        DWORD_PTR candidateMask = selected.mask;
        bool sameL3Cache = selected.sameL3;
        bool smtClean = selected.smtClean;
        const DWORD selectedL3SizeBytes = selected.l3SizeBytes;
        const bool l3SizeKnown = selected.l3SizeKnown;
        const bool l3ScoreFallback = selected.l3ScoreFallback;

        if (candidateMask == 0)
        {
            candidateMask = activeProcessorMaskForGroup(selectedGroup);
            smtClean = false;
            sameL3Cache = false;
            Logger::warn(
                "topology fallback: using active processor mask for group {} because weighted topology scan produced no candidates",
                static_cast<unsigned int>(selectedGroup));
        }
        const TopologyMaskProvenance maskProvenance = selected.mask == 0
            ? TopologyMaskProvenance::ActiveGroupFallback
            : TopologyMaskProvenance::WeightedTopology;

        DWORD_PTR selectedMask = takeLowestBits(candidateMask, 2);
        if (selectedMask == 0)
        {
            return std::unexpected(ErrorCode::TopologyScanFailed);
        }

        const DWORD_PTR primaryMask = lowestSetBit(selectedMask);
        DWORD_PTR fallbackMask = selectedMask & ~primaryMask;
        const DWORD_PTR validatedMask = selectedMask & activeProcessorMaskForGroup(selectedGroup);
        if (validatedMask == 0)
        {
            return std::unexpected(ErrorCode::TopologyScanFailed);
        }

        const int selectedCount = countSetBits(validatedMask);
        if (selectedCount == 1)
        {
            fallbackMask = 0;
            Logger::warn("topology validation: only one logical core is available in selected group {}; fallback core disabled",
                         static_cast<unsigned int>(selectedGroup));
        }
        else if (selectedCount > 2)
        {
            return std::unexpected(ErrorCode::TopologyScanFailed);
        }

        if (l3ScoreFallback)
        {
            Logger::warn(
                "topology scoring fallback: L3 cache size is unavailable or masked for all process groups; scoring uses smt_clean and selected core count only");
        }

        Logger::info(
            "topology validation: allowed_groups=[{}], selected_group={}, mask_provenance={}, primary=0x{:X}, fallback=0x{:X}, validated=0x{:X}, selectedCount={}, same_l3={}, l3_kb={}, l3_known={}, l3_score_fallback={}, smt_clean={}, score={}",
            formatGroups(processGroups),
            static_cast<unsigned int>(selectedGroup),
            maskProvenance == TopologyMaskProvenance::ActiveGroupFallback ? "active_group_fallback" : "weighted_topology",
            static_cast<unsigned long long>(primaryMask),
            static_cast<unsigned long long>(fallbackMask),
            static_cast<unsigned long long>(validatedMask),
            selectedCount,
            sameL3Cache,
            static_cast<unsigned int>(selectedL3SizeBytes / 1024U),
            l3SizeKnown,
            l3ScoreFallback,
            smtClean,
            selected.score);

        return TopologyResult{
            .primaryMask = primaryMask,
            .fallbackMask = fallbackMask,
            .validatedMask = validatedMask,
            .processorGroup = selectedGroup,
            .sameL3Cache = sameL3Cache,
            .smtClean = smtClean,
            .l3SizeKnown = l3SizeKnown,
            .l3ScoreFallback = l3ScoreFallback,
            .maskProvenance = maskProvenance,
            .selectedLogicalCoreCount = selectedCount};
    }
    catch (const std::bad_alloc&)
    {
        return std::unexpected(ErrorCode::AllocationFailed);
    }
    catch (...)
    {
        return std::unexpected(ErrorCode::InternalError);
    }
}
