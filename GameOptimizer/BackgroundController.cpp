// Build: cl /std:c++latest /O2 /W4 /WX /permissive- BackgroundController.cpp
// MODULE: BackgroundController
// ERROR-POLICY: expected
// Reason: per-process restriction failures must be isolated and logged, not fatal to the engine.

#include "BackgroundController.h"

#include "ApplyGuard.h"

#include <TlHelp32.h>
#include <array>
#include <chrono>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <new>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "Logger.h"
#include "LogString.h"
#include "WinApiError.h"
#include "WinHandle.h"

namespace
{
    constexpr DWORD kSystemProcessId = 4;
    constexpr DWORD kIdleProcessId = 0;
    constexpr DWORD kRequiredProcessAccess =
        PROCESS_QUERY_LIMITED_INFORMATION |
        PROCESS_SET_INFORMATION;
    constexpr auto kBackgroundRestrictionCooldown = std::chrono::seconds(10);

    struct ProcessAffinityState
    {
        DWORD_PTR processMask = 0;
        DWORD_PTR systemMask = 0;
    };

    constexpr std::array<const wchar_t*, 23> kProtectedProcessNames = {
        L"system",
        L"registry",
        L"system idle process",
        L"csrss.exe",
        L"wininit.exe",
        L"winlogon.exe",
        L"services.exe",
        L"lsass.exe",
        L"smss.exe",
        L"dwm.exe",
        L"explorer.exe",
        L"audiodg.exe",
        L"fontdrvhost.exe",
        L"wudfhost.exe",
        L"spoolsv.exe",
        L"securityhealthservice.exe",
        L"msmpeng.exe",
        L"vgk.exe",
        L"vgc.exe",
        L"beservice.exe",
        L"battleeye.exe",
        L"easyanticheat.exe",
        L"easyanticheat_eos.exe",
    };

    [[nodiscard]] std::wstring trim(std::wstring_view value)
    {
        const auto isSpace = [](wchar_t ch) noexcept
        {
            return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
        };

        std::size_t begin = 0;
        while (begin < value.size() && isSpace(value[begin]))
        {
            ++begin;
        }

        std::size_t end = value.size();
        while (end > begin && isSpace(value[end - 1]))
        {
            --end;
        }

        return std::wstring(value.substr(begin, end - begin));
    }

    [[nodiscard]] std::expected<ProcessAffinityState, ErrorCode> queryProcessAffinityState(HANDLE processHandle) noexcept
    {
        DWORD_PTR processMask = 0;
        DWORD_PTR systemMask = 0;
        if (!GetProcessAffinityMask(processHandle, &processMask, &systemMask) || processMask == 0)
        {
            return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
        }

        return ProcessAffinityState{
            .processMask = processMask,
            .systemMask = systemMask};
    }

    [[nodiscard]] bool matchesOriginalAffinity(
        const ProcessAffinityState& currentState,
        DWORD_PTR originalProcessMask) noexcept
    {
        return currentState.processMask == originalProcessMask;
    }

}

BackgroundController::BackgroundController(
    RollbackManager& rollbackManager,
    SchedulerMode mode,
    BackgroundFilterConfig filterConfig) noexcept
    : rollbackManager_(rollbackManager),
      mode_(mode),
      filterConfig_(std::move(filterConfig))
{
}

BackgroundFilterConfig BackgroundController::loadFilterConfigFromFile(const std::wstring& path) noexcept
{
    BackgroundFilterConfig config{};
    if (path.empty())
    {
        Logger::info("background filter config disabled: no config path provided");
        return config;
    }

    std::wifstream file(path);
    if (!file.is_open())
    {
        Logger::warn(
            "background filter config could not be opened during startup preload: {}",
            narrowForLog(path));
        return config;
    }

    std::wstring line;
    std::size_t lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        const std::wstring cleaned = trim(line);
        if (cleaned.empty() || cleaned[0] == L'#')
        {
            continue;
        }

        const std::size_t equalsPos = cleaned.find(L'=');
        if (equalsPos == std::wstring::npos)
        {
            Logger::warn(
                "background filter config line ignored: line={}, reason=missing_equals",
                lineNumber);
            continue;
        }

        const std::wstring key = trim(std::wstring_view(cleaned).substr(0, equalsPos));
        const std::wstring value = trim(std::wstring_view(cleaned).substr(equalsPos + 1));
        if (value.empty())
        {
            Logger::warn(
                "background filter config line ignored: line={}, reason=empty_value",
                lineNumber);
            continue;
        }

        const std::wstring normalizedKey = normalizeProcessName(key);
        const std::wstring normalizedValue = normalizeProcessName(value);
        if (normalizedValue.empty())
        {
            Logger::warn(
                "background filter config line ignored: line={}, reason=empty_normalized_value",
                lineNumber);
            continue;
        }

        if (normalizedKey == L"allow" || normalizedKey == L"protect")
        {
            config.protectedProcessNames.insert(normalizedValue);
            continue;
        }

        if (normalizedKey == L"deny" || normalizedKey == L"restrict")
        {
            config.restrictionTargetProcessNames.insert(normalizedValue);
            continue;
        }

        Logger::warn(
            "background filter config line ignored: line={}, reason=unknown_key",
            lineNumber);
    }

    Logger::info(
        "background filter config preloaded into memory: user_protected={}, restriction_targets={}, lookup=lowercase_unordered_set",
        config.protectedProcessNames.size(),
        config.restrictionTargetProcessNames.size());

    return config;
}

std::wstring BackgroundController::normalizeProcessName(std::wstring_view imageName)
{
    std::wstring normalized;
    normalized.reserve(imageName.size());
    normalizeProcessNameInto(imageName, normalized);
    return normalized;
}

void BackgroundController::normalizeProcessNameInto(std::wstring_view imageName, std::wstring& outputBuffer)
{
    outputBuffer.clear();
    outputBuffer.reserve(imageName.size());
    for (const wchar_t ch : imageName)
    {
        outputBuffer.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
}

bool BackgroundController::isBuiltInProtectedProcessName(const std::wstring& normalizedImageName) noexcept
{
    if (normalizedImageName.empty())
    {
        return false;
    }

    for (const wchar_t* protectedName : kProtectedProcessNames)
    {
        // kProtectedProcessNames entries are intentionally stored in lowercase.
        // The runtime process name is normalized once per enumerated process.
        if (normalizedImageName == protectedName)
        {
            return true;
        }
    }

    return false;
}

bool BackgroundController::isUserProtectedProcessName(const std::wstring& normalizedImageName) const noexcept
{
    if (normalizedImageName.empty())
    {
        return false;
    }

    return filterConfig_.protectedProcessNames.contains(normalizedImageName);
}

bool BackgroundController::isRestrictionTargetAllowedByConfig(const std::wstring& normalizedImageName) const noexcept
{
    if (filterConfig_.restrictionTargetProcessNames.empty())
    {
        return true;
    }

    if (normalizedImageName.empty())
    {
        return false;
    }

    return filterConfig_.restrictionTargetProcessNames.contains(normalizedImageName);
}

std::expected<std::vector<BackgroundProcessEntry>, ErrorCode> BackgroundController::enumerateProcesses() noexcept
{
    HANDLE rawSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (rawSnapshot == INVALID_HANDLE_VALUE)
    {
        return std::unexpected(ErrorCode::SnapshotCreationFailed);
    }

    WinHandle snapshot(rawSnapshot);
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot.get(), &entry))
    {
        return std::unexpected(ErrorCode::EnumerationFailed);
    }

    std::vector<BackgroundProcessEntry> processes;
    do
    {
        processes.push_back(BackgroundProcessEntry{
            .imageName = std::wstring(entry.szExeFile),
            .processId = entry.th32ProcessID,
        });
    } while (Process32NextW(snapshot.get(), &entry));

    return processes;
}

bool BackgroundController::isExcludedPid(DWORD processId, DWORD targetProcessId) noexcept
{
    if (processId == kIdleProcessId || processId == kSystemProcessId)
    {
        return true;
    }

    if (processId == targetProcessId || processId == GetCurrentProcessId())
    {
        return true;
    }

    return false;
}

std::expected<HANDLE, ErrorCode> BackgroundController::openProcessForRestriction(DWORD processId) noexcept
{
    HANDLE rawHandle = OpenProcess(kRequiredProcessAccess, FALSE, processId);
    if (rawHandle == nullptr)
    {
        const ErrorCode mappedError = mapLastErrorToErrorCode(GetLastError());
        if (mappedError == ErrorCode::InternalError)
        {
            return std::unexpected(ErrorCode::ProcessOpenFailed);
        }

        return std::unexpected(mappedError);
    }

    return rawHandle;
}

DWORD_PTR BackgroundController::buildBackgroundMask(DWORD_PTR processMask, DWORD_PTR gameMask) noexcept
{
    const DWORD_PTR restrictedMask = processMask & ~gameMask;
    if (restrictedMask != 0)
    {
        return restrictedMask;
    }

    return processMask;
}

bool BackgroundController::isRestrictionCooldownActive() const noexcept
{
    if (lastRestrictionTime_ == std::chrono::steady_clock::time_point{})
    {
        return false;
    }

    return std::chrono::steady_clock::now() - lastRestrictionTime_ < kBackgroundRestrictionCooldown;
}

bool BackgroundController::supportsProcessWideRestrictionForGroup(WORD processorGroup) noexcept
{
    // SetProcessAffinityMask operates on the process default group. For multi-group
    // HEDT systems, non-zero group policies must stay monitoring-only until a
    // group-aware per-thread/process strategy is implemented.
    return processorGroup == 0;
}

bool BackgroundController::isRecoverableAccessLimitation(ErrorCode errorCode) noexcept
{
    return errorCode == ErrorCode::AccessDenied ||
           errorCode == ErrorCode::ProcessOpenFailed;
}

std::expected<BackgroundRestrictionSummary, ErrorCode> BackgroundController::applyRestriction(
    const BackgroundRestrictionPolicy& policy) noexcept
{
    if (policy.targetProcessId == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    if (policy.gameAffinityMask == 0)
    {
        BackgroundRestrictionSummary summary{};
        summary.blockedByInvalidMainThreadPolicy = true;
        Logger::warn(
            "background restriction disabled: main-thread policy has no valid affinity mask; monitoring-only fallback remains active until topology or process-affinity fallback succeeds");
        Logger::warn(
            "background restriction evidence: background_restriction_mode=monitoring_only_due_to_invalid_main_thread_policy, process_wide_affinity_supported=false");
        return summary;
    }

    if (!supportsProcessWideRestrictionForGroup(policy.processorGroup))
    {
        BackgroundRestrictionSummary summary{};
        summary.blockedByUnsupportedProcessorGroup = true;
        summary.blockedProcessorGroup = policy.processorGroup;
        Logger::warn(
            "background restriction blocked: processor group {} is selected, but process-wide SetProcessAffinityMask is only safe for group 0 policies; priority-class-only background restriction is also blocked until affinity and priority rollback state are split; thread-level SetThreadGroupAffinity remains supported",
            static_cast<unsigned int>(policy.processorGroup));
        Logger::warn(
            "background restriction evidence: background_restriction_mode=monitoring_only_due_to_processor_group, blocked_processor_group={}, process_wide_affinity_supported=false",
            static_cast<unsigned int>(policy.processorGroup));
        return summary;
    }

    if (mode_ == SchedulerMode::Apply &&
        filterConfig_.requireExplicitRestrictionTargetsInApply &&
        filterConfig_.restrictionTargetProcessNames.empty())
    {
        BackgroundRestrictionSummary summary{};
        summary.blockedByMissingDenylist = true;
        Logger::warn(
            "background restriction blocked: apply mode requires explicit deny/restrict targets; unknown background processes remain monitoring-only");
        return summary;
    }

    if (isRestrictionCooldownActive())
    {
        const auto elapsed = std::chrono::steady_clock::now() - lastRestrictionTime_;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            kBackgroundRestrictionCooldown - elapsed);
        Logger::info(
            "background restriction skipped by cooldown: remaining={} ms",
            remaining.count() > 0 ? remaining.count() : 0);
        return BackgroundRestrictionSummary{};
    }

    const auto processEntries = enumerateProcesses();
    if (!processEntries)
    {
        return std::unexpected(processEntries.error());
    }

    lastRestrictionTime_ = std::chrono::steady_clock::now();

    const auto& entries = *processEntries;

    BackgroundRestrictionSummary summary{};
    summary.scannedProcessCount = entries.size();

    // Hot-loop buffer reuse:
    // BG_RESTRICT_UP may enumerate hundreds of processes. Reusing one reserved buffer avoids
    // per-process std::wstring heap allocation when normalizing long process names.
    std::wstring normalizedProcessNameBuffer;
    normalizedProcessNameBuffer.reserve(MAX_PATH);

    std::string processNameLogBuffer;
    processNameLogBuffer.reserve(MAX_PATH);

    for (const BackgroundProcessEntry& processEntry : entries)
    {
        const DWORD processId = processEntry.processId;
        narrowForLogInto(processEntry.imageName, processNameLogBuffer);
        const std::string& processNameForLog = processNameLogBuffer;
        normalizeProcessNameInto(processEntry.imageName, normalizedProcessNameBuffer);
        const std::wstring& normalizedProcessName = normalizedProcessNameBuffer;

        if (isExcludedPid(processId, policy.targetProcessId))
        {
            ++summary.skippedProcessCount;
            continue;
        }

        if (isBuiltInProtectedProcessName(normalizedProcessName))
        {
            ++summary.skippedProcessCount;
            ++summary.protectedProcessSkipCount;
            if (filterConfig_.logSkippedProcessDetails)
            {
                Logger::info(
                    "protected process skipped: {} (pid={}, reason=built-in protected match)",
                    processNameForLog,
                    processId);
            }
            continue;
        }

        if (isUserProtectedProcessName(normalizedProcessName))
        {
            ++summary.skippedProcessCount;
            ++summary.userProtectedProcessSkipCount;
            if (filterConfig_.logSkippedProcessDetails)
            {
                Logger::info(
                    "user-protected process skipped: {} (pid={}, reason=config allow/protect match)",
                    processNameForLog,
                    processId);
            }
            continue;
        }

        if (!isRestrictionTargetAllowedByConfig(normalizedProcessName))
        {
            ++summary.skippedProcessCount;
            ++summary.notInDenylistSkipCount;
            continue;
        }

        const auto openedProcess = openProcessForRestriction(processId);
        if (!openedProcess)
        {
            ++summary.skippedProcessCount;
            if (filterConfig_.logSkippedProcessDetails)
            {
                if (isRecoverableAccessLimitation(openedProcess.error()))
                {
                    Logger::warn(
                        "background process skipped by recoverable access limitation: {} (pid={}, error={})",
                        processNameForLog,
                        processId,
                        toString(openedProcess.error()));
                }
                else
                {
                    Logger::warn(
                        "background process skipped: {} (pid={}, reason=open_process_failed, error={})",
                        processNameForLog,
                        processId,
                        toString(openedProcess.error()));
                }
            }
            continue;
        }

        auto& openedProcessHandle = *openedProcess;
        WinHandle processHandle(std::move(openedProcessHandle));
        const auto affinityState = queryProcessAffinityState(processHandle.get());
        if (!affinityState)
        {
            ++summary.skippedProcessCount;
            if (filterConfig_.logSkippedProcessDetails)
            {
                Logger::warn(
                    "background process skipped: {} (pid={}, reason=affinity_query_failed)",
                    processNameForLog,
                    processId);
            }
            continue;
        }

        const auto& processAffinity = *affinityState;
        const DWORD_PTR processMask = processAffinity.processMask;
        const DWORD_PTR targetMask = buildBackgroundMask(processMask, policy.gameAffinityMask);
        ++summary.candidateProcessCount;

        if (mode_ == SchedulerMode::DryRun)
        {
            Logger::info(
                "dry-run: would restrict background process {} (pid={}, currentAffinity=0x{:X}, targetAffinity=0x{:X}, priority=IDLE)",
                processNameForLog,
                processId,
                static_cast<unsigned long long>(processMask),
                static_cast<unsigned long long>(targetMask));
            continue;
        }

        const DWORD currentPriority = GetPriorityClass(processHandle.get());
        if (currentPriority == 0)
        {
            ++summary.skippedProcessCount;
            if (filterConfig_.logSkippedProcessDetails)
            {
                Logger::warn(
                    "background process skipped: {} (pid={}, reason=priority_query_failed)",
                    processNameForLog,
                    processId);
            }
            continue;
        }

        if (mode_ == SchedulerMode::SoftApply)
        {
            const auto baselineValidationResult = rollbackManager_.validateProcessRollbackState(
                processId,
                processMask,
                policy.processorGroup,
                currentPriority);
            if (!baselineValidationResult)
            {
                ++summary.skippedProcessCount;
                Logger::warn(
                    "soft-apply: background process rollback baseline validation failed: {} (pid={}, error={})",
                    processNameForLog,
                    processId,
                    toString(baselineValidationResult.error()));
                continue;
            }

            Logger::info(
                "soft-apply: validated background restriction path for {} (pid={}, currentAffinity=0x{:X}, targetAffinity=0x{:X}, currentPriority=0x{:X}, targetPriority=IDLE)",
                processNameForLog,
                processId,
                static_cast<unsigned long long>(processMask),
                static_cast<unsigned long long>(targetMask),
                static_cast<unsigned int>(currentPriority));
            continue;
        }

        const auto saveResult = rollbackManager_.saveProcessState(
            processId,
            processMask,
            policy.processorGroup,
            currentPriority);
        if (!saveResult)
        {
            ++summary.skippedProcessCount;
            if (filterConfig_.logSkippedProcessDetails)
            {
                Logger::warn(
                    "background process skipped: {} (pid={}, reason=rollback_state_save_failed, error={})",
                    processNameForLog,
                    processId,
                    toString(saveResult.error()));
            }
            continue;
        }

        const auto saveDisposition = *saveResult;
        auto applyGuard = ApplyGuard::forProcess(rollbackManager_, processId, saveDisposition);
        applyGuard.arm();

        if (targetMask != processMask && !SetProcessAffinityMask(processHandle.get(), targetMask))
        {
            const ErrorCode mappedError = mapLastErrorToErrorCode(GetLastError());
            const auto stateAfterFailedAffinityApply = queryProcessAffinityState(processHandle.get());
            if (!stateAfterFailedAffinityApply)
            {
                Logger::error(
                    "background affinity restriction failed for {} (pid={}), and post-failure audit could not query affinity: {}; invoking rollback",
                    processNameForLog,
                    processId,
                    toString(stateAfterFailedAffinityApply.error()));
            }
            else
            {
                const auto& auditedAffinityState = *stateAfterFailedAffinityApply;
                const bool auditMatchedOriginal = matchesOriginalAffinity(auditedAffinityState, processMask);
                if (auditMatchedOriginal)
                {
                    Logger::info(
                        "background affinity restriction failed for {} (pid={}), but post-failure audit matched the original affinity; saved rollback state discarded",
                        processNameForLog,
                        processId);
                    applyGuard.discardSavedState();
                }
                else
                {
                    Logger::error(
                        "background affinity restriction failed for {} (pid={}), and post-failure audit found affinity drift: current=0x{:X}, original=0x{:X}; invoking rollback",
                        processNameForLog,
                        processId,
                        static_cast<unsigned long long>(auditedAffinityState.processMask),
                        static_cast<unsigned long long>(processMask));
                }

                if (auditMatchedOriginal)
                {
                    ++summary.skippedProcessCount;
                    if (isRecoverableAccessLimitation(mappedError))
                    {
                        Logger::warn(
                            "background affinity restriction blocked by recoverable access limitation: {} (pid={}, error={}); post-failure audit completed before state cleanup",
                            processNameForLog,
                            processId,
                            toString(mappedError));
                    }
                    else
                    {
                        Logger::warn(
                            "background restrict failed: {} (pid={}, reason=SetProcessAffinityMask_failed)",
                            processNameForLog,
                            processId);
                    }
                    continue;
                }
            }

            {
                auto rollbackResult = applyGuard.rollbackNow();
                if (!rollbackResult)
                {
                    Logger::error(
                        "background affinity restriction failed for {} (pid={}); rollback also failed: {}; rollback state is preserved for shutdown recovery",
                        processNameForLog,
                        processId,
                        toString(rollbackResult.error()));
                }
            }

            ++summary.skippedProcessCount;
            if (isRecoverableAccessLimitation(mappedError))
            {
                Logger::warn(
                    "background affinity restriction blocked by recoverable access limitation: {} (pid={}, error={}); post-failure audit completed before state cleanup",
                    processNameForLog,
                    processId,
                    toString(mappedError));
            }
            else
            {
                Logger::warn(
                    "background restrict failed: {} (pid={}, reason=SetProcessAffinityMask_failed)",
                    processNameForLog,
                    processId);
            }
            continue;
        }

        if (!SetPriorityClass(processHandle.get(), IDLE_PRIORITY_CLASS))
        {
            const ErrorCode mappedError = mapLastErrorToErrorCode(GetLastError());
            auto rollbackResult = applyGuard.rollbackNow();
            if (!rollbackResult)
            {
                Logger::error(
                    "background priority apply failed for {} (pid={}); rollback also failed: {}",
                    processNameForLog,
                    processId,
                    toString(rollbackResult.error()));
            }

            ++summary.skippedProcessCount;
            if (isRecoverableAccessLimitation(mappedError))
            {
                Logger::warn(
                    "background priority restriction blocked by recoverable access limitation: {} (pid={}, error={}); rollback path was invoked when needed",
                    processNameForLog,
                    processId,
                    toString(mappedError));
            }
            else
            {
                Logger::warn(
                    "background restrict failed: {} (pid={}, reason=SetPriorityClass_failed)",
                    processNameForLog,
                    processId);
            }
            continue;
        }

        applyGuard.commit();
        ++summary.restrictedProcessCount;
        if (filterConfig_.logRestrictedProcessDetails)
        {
            Logger::info(
                "background restricted: {} (pid={}, affinity 0x{:X} -> 0x{:X}, priority -> IDLE)",
                processNameForLog,
                processId,
                static_cast<unsigned long long>(processMask),
                static_cast<unsigned long long>(targetMask));
        }
    }

    Logger::info(
        "background restriction summary: scanned={}, candidates={}, restricted={}, skipped={}, protected_skipped={}, user_protected_skipped={}, not_in_denylist_skipped={}, blocked_group_policy={}",
        summary.scannedProcessCount,
        summary.candidateProcessCount,
        summary.restrictedProcessCount,
        summary.skippedProcessCount,
        summary.protectedProcessSkipCount,
        summary.userProtectedProcessSkipCount,
        summary.notInDenylistSkipCount,
        summary.blockedByUnsupportedProcessorGroup);

    return summary;
}
