// Build: cl /std:c++latest /O2 /W4 /WX /permissive- RollbackManager.cpp
// MODULE: RollbackManager
// ERROR-POLICY: expected
// Reason: rollback failures are reported and never hidden.

#include "RollbackManager.h"

#include <cstdint>
#include <new>
#include <vector>

#include "Logger.h"
#include "WinApiError.h"
#include "WinHandle.h"

namespace
{
    constexpr std::uint64_t kFileTimeTicksPerMillisecond = 10'000ULL;

    struct IdentityTimeDelta
    {
        std::uint64_t deltaMs = 0;
        double deltaSeconds = 0.0;
    };

    [[nodiscard]] IdentityTimeDelta calculateIdentityTimeDelta(
        std::uint64_t savedCreationTime100ns,
        std::uint64_t currentCreationTime100ns) noexcept
    {
        const std::uint64_t delta100ns =
            currentCreationTime100ns >= savedCreationTime100ns
                ? currentCreationTime100ns - savedCreationTime100ns
                : savedCreationTime100ns - currentCreationTime100ns;

        const std::uint64_t deltaMs = delta100ns / kFileTimeTicksPerMillisecond;
        return IdentityTimeDelta{
            .deltaMs = deltaMs,
            .deltaSeconds = static_cast<double>(deltaMs) / 1000.0};
    }

    struct RollbackVerificationState
    {
        DWORD_PTR affinityMask = 0;
        WORD processorGroup = 0;
        int priority = THREAD_PRIORITY_ERROR_RETURN;
    };

    [[nodiscard]] bool isProcessStillActive(HANDLE processHandle) noexcept
    {
        DWORD exitCode = 0;
        if (!GetExitCodeProcess(processHandle, &exitCode))
        {
            return false;
        }

        return exitCode == STILL_ACTIVE;
    }

    [[nodiscard]] std::expected<RollbackVerificationState, ErrorCode>
    queryRollbackVerificationState(HANDLE threadHandle) noexcept
    {
        const int currentPriority = GetThreadPriority(threadHandle);
        if (currentPriority == THREAD_PRIORITY_ERROR_RETURN)
        {
            return std::unexpected(ErrorCode::ThreadPriorityQueryFailed);
        }

        GROUP_AFFINITY currentAffinity{};
        if (!GetThreadGroupAffinity(threadHandle, &currentAffinity))
        {
            return std::unexpected(ErrorCode::ThreadAffinityQueryFailed);
        }

        const DWORD_PTR currentAffinityMask = static_cast<DWORD_PTR>(currentAffinity.Mask);
        if (currentAffinityMask == 0)
        {
            return std::unexpected(ErrorCode::ThreadAffinityQueryFailed);
        }

        return RollbackVerificationState{
            .affinityMask = currentAffinityMask,
            .processorGroup = currentAffinity.Group,
            .priority = currentPriority};
    }

}

std::string_view RollbackManager::processRollbackModeToString(
    ProcessRollbackState::RollbackMode rollbackMode) noexcept
{
    switch (rollbackMode)
    {
    case ProcessRollbackState::RollbackMode::LegacyProcessAffinityMask:
        return "LegacyProcessAffinityMask";
    case ProcessRollbackState::RollbackMode::GroupAwareUnsupported:
        return "GroupAwareUnsupported";
    }

    return "UnknownProcessRollbackMode";
}

std::expected<WinHandle, ErrorCode> RollbackManager::openThreadForRollback(DWORD threadId) noexcept
{
    HANDLE rawHandle = OpenThread(
        THREAD_SET_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION,
        FALSE,
        threadId);

    if (rawHandle == nullptr)
    {
        const ErrorCode mappedError = mapLastErrorToErrorCode(GetLastError());
        if (mappedError == ErrorCode::InternalError)
        {
            return std::unexpected(ErrorCode::ThreadOpenFailed);
        }

        return std::unexpected(mappedError);
    }

    return WinHandle(rawHandle);
}


std::expected<WinHandle, ErrorCode> RollbackManager::openProcessForRollback(DWORD processId) noexcept
{
    HANDLE rawHandle = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_INFORMATION,
        FALSE,
        processId);

    if (rawHandle == nullptr)
    {
        const ErrorCode mappedError = mapLastErrorToErrorCode(GetLastError());
        if (mappedError == ErrorCode::InternalError)
        {
            return std::unexpected(ErrorCode::ProcessOpenFailed);
        }

        return std::unexpected(mappedError);
    }

    return WinHandle(rawHandle);
}

std::expected<std::uint64_t, ErrorCode> RollbackManager::queryThreadCreationTime100ns(
    HANDLE threadHandle) noexcept
{
    FILETIME creationTime{};
    FILETIME exitTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};

    if (!GetThreadTimes(threadHandle, &creationTime, &exitTime, &kernelTime, &userTime))
    {
        return std::unexpected(ErrorCode::ThreadTimeQueryFailed);
    }

    ULARGE_INTEGER converted{};
    converted.LowPart = creationTime.dwLowDateTime;
    converted.HighPart = creationTime.dwHighDateTime;
    return static_cast<std::uint64_t>(converted.QuadPart);
}

std::expected<std::uint64_t, ErrorCode> RollbackManager::queryProcessCreationTime100ns(
    HANDLE processHandle) noexcept
{
    FILETIME creationTime{};
    FILETIME exitTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};

    if (!GetProcessTimes(processHandle, &creationTime, &exitTime, &kernelTime, &userTime))
    {
        return std::unexpected(ErrorCode::ProcessPriorityQueryFailed);
    }

    ULARGE_INTEGER converted{};
    converted.LowPart = creationTime.dwLowDateTime;
    converted.HighPart = creationTime.dwHighDateTime;
    return static_cast<std::uint64_t>(converted.QuadPart);
}

bool RollbackManager::isSameIdentity(
    std::optional<std::uint64_t> expectedCreationTime,
    std::uint64_t currentCreationTime) noexcept
{
    if (!expectedCreationTime.has_value())
    {
        return true;
    }

    const std::uint64_t expectedCreationTime100ns = expectedCreationTime.value();
    return expectedCreationTime100ns == currentCreationTime;
}

std::expected<void, ErrorCode> RollbackManager::saveDryRunState(DWORD threadId) noexcept
{
    if (threadId == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    try
    {
        std::scoped_lock lock(mutex_);
        if (states_.contains(threadId))
        {
            return {};
        }

        states_.insert_or_assign(
            threadId,
            ThreadRollbackState{
                .creationTime100ns = std::nullopt,
                .originalAffinityMask = std::nullopt,
                .originalProcessorGroup = std::nullopt,
                .originalPriority = std::nullopt,
                .threadId = threadId,
                .kind = RollbackStateKind::DryRunMarker});

        Logger::info(
            "dry-run rollback marker saved for TID {} (no WinAPI state query)",
            threadId);
        return {};
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

std::expected<void, ErrorCode> RollbackManager::saveValidatedReadState(
    DWORD threadId,
    DWORD_PTR currentAffinityMask,
    WORD processorGroup,
    int currentPriority) noexcept
{
    if (threadId == 0 || currentAffinityMask == 0 || currentPriority == THREAD_PRIORITY_ERROR_RETURN)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    try
    {
        std::optional<std::uint64_t> creationTime;
        auto openedThread = openThreadForRollback(threadId);
        if (openedThread)
        {
            WinHandle& threadHandle = *openedThread;
            const auto queriedCreationTime = queryThreadCreationTime100ns(threadHandle.get());
            if (queriedCreationTime)
            {
                const auto queriedCreationTime100ns = *queriedCreationTime;
                creationTime = queriedCreationTime100ns;
            }
            else
            {
                Logger::warn(
                    "soft-apply identity timestamp capture failed for TID {}: {}; rollback will use best-effort identity",
                    threadId,
                    toString(queriedCreationTime.error()));
            }
        }
        else
        {
            Logger::warn(
                "soft-apply identity timestamp capture skipped for TID {}: {}; rollback will use best-effort identity",
                threadId,
                toString(openedThread.error()));
        }

        std::scoped_lock lock(mutex_);
        states_.insert_or_assign(
            threadId,
            ThreadRollbackState{
                .creationTime100ns = creationTime,
                .originalAffinityMask = currentAffinityMask,
                .originalProcessorGroup = processorGroup,
                .originalPriority = currentPriority,
                .threadId = threadId,
                .kind = RollbackStateKind::SoftApplyValidatedRead});

        Logger::info(
            "soft-apply validated scheduling baseline captured for TID {} (affinity=0x{:X}, group={}, priority={}, creationTime100ns={})",
            threadId,
            static_cast<unsigned long long>(currentAffinityMask),
            static_cast<unsigned int>(processorGroup),
            currentPriority,
            creationTime.value_or(0));
        return {};
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

std::expected<RollbackManager::SaveStateDisposition, ErrorCode> RollbackManager::saveAppliedState(
    DWORD threadId,
    DWORD_PTR previousAffinityMask,
    WORD processorGroup,
    int originalPriority) noexcept
{
    if (threadId == 0 || previousAffinityMask == 0 || originalPriority == THREAD_PRIORITY_ERROR_RETURN)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    try
    {
        std::optional<std::uint64_t> creationTime;
        auto openedThread = openThreadForRollback(threadId);
        if (openedThread)
        {
            WinHandle& threadHandle = *openedThread;
            const auto queriedCreationTime = queryThreadCreationTime100ns(threadHandle.get());
            if (queriedCreationTime)
            {
                const auto queriedCreationTime100ns = *queriedCreationTime;
                creationTime = queriedCreationTime100ns;
            }
            else
            {
                Logger::warn(
                    "rollback identity timestamp capture failed for TID {}: {}; rollback will use best-effort identity",
                    threadId,
                    toString(queriedCreationTime.error()));
            }
        }
        else
        {
            return std::unexpected(openedThread.error());
        }

        std::scoped_lock lock(mutex_);
        if (states_.contains(threadId))
        {
            return SaveStateDisposition::ReusedExistingState;
        }

        states_.insert_or_assign(
            threadId,
            ThreadRollbackState{
                .creationTime100ns = creationTime,
                .originalAffinityMask = previousAffinityMask,
                .originalProcessorGroup = processorGroup,
                .originalPriority = originalPriority,
                .threadId = threadId,
                .kind = RollbackStateKind::Applied});

        Logger::info(
            "rollback state saved for TID {} (group={}, affinity=0x{:X}, priority={}, creationTime100ns={})",
            threadId,
            static_cast<unsigned int>(processorGroup),
            static_cast<unsigned long long>(previousAffinityMask),
            originalPriority,
            creationTime.value_or(0));
        return SaveStateDisposition::CreatedNewState;
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

std::expected<void, ErrorCode> RollbackManager::rollbackThread(DWORD threadId) noexcept
{
    try
    {
        std::optional<ThreadRollbackState> state;
        {
            std::scoped_lock lock(mutex_);
            const auto found = states_.find(threadId);
            if (found == states_.end())
            {
                return std::unexpected(ErrorCode::RollbackStateNotFound);
            }

            state = found->second;
        }

        const ThreadRollbackState rollbackStateValue = state.value();
        const auto rollbackResult = rollbackState(rollbackStateValue);
        if (!rollbackResult)
        {
            Logger::error("thread rollback failed for TID {}: {}", threadId, toString(rollbackResult.error()));
            return std::unexpected(rollbackResult.error());
        }

        removeThreadState(threadId);
        Logger::info("rollback completed for TID {}", threadId);
        return {};
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

std::expected<void, ErrorCode> RollbackManager::rollbackAll() noexcept
{
    try
    {
        std::vector<ThreadRollbackState> threadStates;
        std::vector<ProcessRollbackState> processStates;
        {
            std::scoped_lock lock(mutex_);
            threadStates.reserve(states_.size());
            for (const auto& [threadId, state] : states_)
            {
                threadStates.push_back(state);
            }

            processStates.reserve(processStates_.size());
            for (const auto& [processId, state] : processStates_)
            {
                processStates.push_back(state);
            }
        }

        bool hasFailure = false;
        std::vector<DWORD> completedThreadIds;
        std::vector<DWORD> completedProcessIds;
        completedThreadIds.reserve(threadStates.size());
        completedProcessIds.reserve(processStates.size());

        for (const ThreadRollbackState& state : threadStates)
        {
            const auto result = rollbackState(state);
            if (!result)
            {
                hasFailure = true;
                Logger::error(
                    "rollback failed for TID {}: {}",
                    state.threadId,
                    toString(result.error()));
                continue;
            }

            completedThreadIds.push_back(state.threadId);
        }

        for (const ProcessRollbackState& state : processStates)
        {
            const auto result = rollbackProcessState(state);
            if (!result)
            {
                hasFailure = true;
                Logger::error(
                    "background rollback failed for PID {}: {}",
                    state.processId,
                    toString(result.error()));
                continue;
            }

            completedProcessIds.push_back(state.processId);
        }

        {
            std::scoped_lock lock(mutex_);
            for (const DWORD threadId : completedThreadIds)
            {
                states_.erase(threadId);
            }

            for (const DWORD processId : completedProcessIds)
            {
                processStates_.erase(processId);
            }
        }

        if (hasFailure)
        {
            Logger::error(
                "rollback completed with failures; preserved rollback state count: thread={}, process={}",
                threadStates.size() - completedThreadIds.size(),
                processStates.size() - completedProcessIds.size());
            return std::unexpected(ErrorCode::RollbackFailed);
        }

        Logger::info("rollback completed for all tracked threads/processes");
        return {};
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

std::expected<RollbackManager::SaveStateDisposition, ErrorCode> RollbackManager::saveProcessState(
    DWORD processId,
    DWORD_PTR originalAffinityMask,
    WORD processorGroup,
    DWORD originalPriorityClass) noexcept
{
    if (processId == 0 || originalAffinityMask == 0 || originalPriorityClass == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    const ProcessRollbackState::RollbackMode rollbackMode =
        processorGroup == 0
            ? ProcessRollbackState::RollbackMode::LegacyProcessAffinityMask
            : ProcessRollbackState::RollbackMode::GroupAwareUnsupported;

    if (rollbackMode == ProcessRollbackState::RollbackMode::GroupAwareUnsupported)
    {
        Logger::warn(
            "background rollback state save blocked for PID {} observedGroup={}: SetProcessAffinityMask cannot restore processor-group-specific process affinity",
            processId,
            static_cast<unsigned int>(processorGroup));
        return std::unexpected(ErrorCode::UnsupportedProcessorGroupRollback);
    }

    try
    {
        std::uint64_t creationTime = 0;
        auto openedProcess = openProcessForRollback(processId);
        if (openedProcess)
        {
            WinHandle& processHandle = *openedProcess;
            const auto queriedCreationTime = queryProcessCreationTime100ns(processHandle.get());
            if (!queriedCreationTime)
            {
                return std::unexpected(queriedCreationTime.error());
            }

            const auto queriedCreationTime100ns = *queriedCreationTime;
            creationTime = queriedCreationTime100ns;
        }
        else
        {
            return std::unexpected(openedProcess.error());
        }

        std::scoped_lock lock(mutex_);
        if (processStates_.contains(processId))
        {
            return SaveStateDisposition::ReusedExistingState;
        }

        processStates_.insert_or_assign(
            processId,
            ProcessRollbackState{
                .creationTime100ns = creationTime,
                .originalAffinityMask = originalAffinityMask,
                .observedProcessorGroup = processorGroup,
                .processId = processId,
                .originalPriorityClass = originalPriorityClass,
                .rollbackMode = rollbackMode});

        Logger::info(
            "background rollback state saved for PID {} (observedGroup={}, rollbackMode={}, affinity=0x{:X}, priorityClass=0x{:X}, creationTime100ns={})",
            processId,
            static_cast<unsigned int>(processorGroup),
            processRollbackModeToString(rollbackMode),
            static_cast<unsigned long long>(originalAffinityMask),
            static_cast<unsigned int>(originalPriorityClass),
            creationTime);
        return SaveStateDisposition::CreatedNewState;
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

std::expected<void, ErrorCode> RollbackManager::rollbackProcess(DWORD processId) noexcept
{
    try
    {
        std::optional<ProcessRollbackState> state;
        {
            std::scoped_lock lock(mutex_);
            const auto found = processStates_.find(processId);
            if (found == processStates_.end())
            {
                return std::unexpected(ErrorCode::RollbackStateNotFound);
            }

            state = found->second;
        }

        const ProcessRollbackState rollbackStateValue = state.value();
        const auto rollbackResult = rollbackProcessState(rollbackStateValue);
        if (!rollbackResult)
        {
            Logger::error("background rollback failed for PID {}: {}", processId, toString(rollbackResult.error()));
            return std::unexpected(rollbackResult.error());
        }

        removeProcessState(processId);
        Logger::info("background rollback completed for PID {}", processId);
        return {};
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

void RollbackManager::removeThreadState(DWORD threadId) noexcept
{
    std::scoped_lock lock(mutex_);
    states_.erase(threadId);
}

void RollbackManager::removeProcessState(DWORD processId) noexcept
{
    std::scoped_lock lock(mutex_);
    processStates_.erase(processId);
}

bool RollbackManager::hasThreadState(DWORD threadId) const noexcept
{
    std::scoped_lock lock(mutex_);
    return states_.contains(threadId);
}

bool RollbackManager::hasProcessState(DWORD processId) const noexcept
{
    std::scoped_lock lock(mutex_);
    return processStates_.contains(processId);
}

std::expected<void, ErrorCode> RollbackManager::rollbackState(
    const ThreadRollbackState& state) noexcept
{
    if (state.kind == RollbackStateKind::DryRunMarker)
    {
        Logger::info(
            "dry-run rollback completed for TID {} without touching target thread",
            state.threadId);
        return {};
    }

    if (state.kind == RollbackStateKind::SoftApplyValidatedRead)
    {
        Logger::info(
            "soft-apply validated scheduling baseline verified for TID {} (no scheduling state was changed)",
            state.threadId);
        return {};
    }

    auto openedThread = openThreadForRollback(state.threadId);
    if (!openedThread)
    {
        if (openedThread.error() == ErrorCode::ThreadOpenFailed || openedThread.error() == ErrorCode::AccessDenied)
        {
            Logger::warn(
                "rollback skipped for TID {} because the original thread is no longer openable ({})",
                state.threadId,
                toString(openedThread.error()));
            return {};
        }

        return std::unexpected(openedThread.error());
    }

    WinHandle& threadHandle = *openedThread;

    const auto currentCreationTime = queryThreadCreationTime100ns(threadHandle.get());
    if (!currentCreationTime)
    {
        return std::unexpected(currentCreationTime.error());
    }

    const auto currentCreationTime100ns = *currentCreationTime;
    if (!isSameIdentity(state.creationTime100ns, currentCreationTime100ns))
    {
        const std::uint64_t savedCreationTime = state.creationTime100ns.value_or(0);
        const IdentityTimeDelta delta = calculateIdentityTimeDelta(
            savedCreationTime,
            currentCreationTime100ns);

        Logger::warn(
            "rollback skipped for TID {} because the thread identity changed "
            "(savedCreationTime100ns={}, currentCreationTime100ns={}, delta={} ms / {:.3f} s)",
            state.threadId,
            savedCreationTime,
            currentCreationTime100ns,
            delta.deltaMs,
            delta.deltaSeconds);
        return {};
    }

    if (state.originalAffinityMask.has_value())
    {
        const DWORD_PTR originalAffinityMask = state.originalAffinityMask.value();
        GROUP_AFFINITY targetAffinity{};
        targetAffinity.Mask = static_cast<KAFFINITY>(originalAffinityMask);
        targetAffinity.Group = state.originalProcessorGroup.value_or(0);

        if (!SetThreadGroupAffinity(threadHandle.get(), &targetAffinity, nullptr))
        {
            return std::unexpected(ErrorCode::ThreadAffinityApplyFailed);
        }
    }

    if (state.originalPriority.has_value() &&
        !SetThreadPriority(threadHandle.get(), state.originalPriority.value()))
    {
        return std::unexpected(ErrorCode::ThreadPriorityApplyFailed);
    }

    if (state.kind == RollbackStateKind::Applied)
    {
        const auto verifiedState = queryRollbackVerificationState(threadHandle.get());
        if (!verifiedState)
        {
            Logger::error(
                "rollback audit failed for TID {}: current state query failed ({})",
                state.threadId,
                toString(verifiedState.error()));
            return std::unexpected(ErrorCode::RollbackVerificationFailed);
        }

        const auto& currentState = *verifiedState;

        const bool affinityMatches =
            !state.originalAffinityMask.has_value() ||
            (currentState.affinityMask == state.originalAffinityMask.value() &&
             currentState.processorGroup == state.originalProcessorGroup.value_or(currentState.processorGroup));

        const bool priorityMatches =
            !state.originalPriority.has_value() ||
            currentState.priority == state.originalPriority.value();

        if (!affinityMatches || !priorityMatches)
        {
            Logger::error(
                "rollback audit mismatch for TID {}: current(group={}, affinity=0x{:X}, priority={}) expected(group={}, affinity=0x{:X}, priority={})",
                state.threadId,
                static_cast<unsigned int>(currentState.processorGroup),
                static_cast<unsigned long long>(currentState.affinityMask),
                currentState.priority,
                static_cast<unsigned int>(state.originalProcessorGroup.value_or(0)),
                static_cast<unsigned long long>(state.originalAffinityMask.value_or(0)),
                state.originalPriority.value_or(THREAD_PRIORITY_ERROR_RETURN));
            return std::unexpected(ErrorCode::RollbackVerificationFailed);
        }

        Logger::info(
            "rollback audit passed for TID {} (group={}, affinity=0x{:X}, priority={})",
            state.threadId,
            static_cast<unsigned int>(currentState.processorGroup),
            static_cast<unsigned long long>(currentState.affinityMask),
            currentState.priority);
    }

    return {};
}

std::expected<void, ErrorCode> RollbackManager::rollbackProcessState(
    const ProcessRollbackState& state) noexcept
{
    enum class IdentityCheckResult
    {
        SameIdentityAlive,
        StaleOrNoLongerTrusted,
        VerificationFailed
    };

    auto openedProcess = openProcessForRollback(state.processId);
    if (!openedProcess)
    {
        if (openedProcess.error() == ErrorCode::ProcessOpenFailed ||
            openedProcess.error() == ErrorCode::AccessDenied)
        {
            Logger::warn(
                "background rollback skipped for PID {} because the original process is no longer openable or is blocked by an access boundary ({})",
                state.processId,
                toString(openedProcess.error()));
            return {};
        }

        return std::unexpected(openedProcess.error());
    }

    WinHandle& processHandle = *openedProcess;

    const auto currentCreationTime = queryProcessCreationTime100ns(processHandle.get());
    if (!currentCreationTime)
    {
        return std::unexpected(currentCreationTime.error());
    }

    const auto currentCreationTime100ns = *currentCreationTime;
    if (state.creationTime100ns != currentCreationTime100ns)
    {
        const IdentityTimeDelta delta = calculateIdentityTimeDelta(
            state.creationTime100ns,
            currentCreationTime100ns);

        Logger::warn(
            "background rollback skipped for PID {} because the process identity changed "
            "(savedCreationTime100ns={}, currentCreationTime100ns={}, delta={} ms / {:.3f} s)",
            state.processId,
            state.creationTime100ns,
            currentCreationTime100ns,
            delta.deltaMs,
            delta.deltaSeconds);
        return {};
    }

    if (state.rollbackMode == ProcessRollbackState::RollbackMode::GroupAwareUnsupported)
    {
        Logger::error(
            "background process affinity rollback unsupported for PID {} observedGroup={}; SetProcessAffinityMask cannot restore processor-group-specific process affinity, rollback state preserved",
            state.processId,
            static_cast<unsigned int>(state.observedProcessorGroup));
        return std::unexpected(ErrorCode::UnsupportedProcessorGroupRollback);
    }

    const auto verifyIdentityAfterRestoreFailure = [&]() noexcept -> IdentityCheckResult
    {
        auto reopenedProcess = openProcessForRollback(state.processId);
        if (reopenedProcess)
        {
            WinHandle& verificationHandle = *reopenedProcess;
            if (!isProcessStillActive(verificationHandle.get()))
            {
                return IdentityCheckResult::StaleOrNoLongerTrusted;
            }

            const auto verifiedCreationTime = queryProcessCreationTime100ns(verificationHandle.get());
            if (!verifiedCreationTime)
            {
                return IdentityCheckResult::VerificationFailed;
            }

            const auto verifiedCreationTime100ns = *verifiedCreationTime;
            if (state.creationTime100ns != verifiedCreationTime100ns)
            {
                return IdentityCheckResult::StaleOrNoLongerTrusted;
            }

            return IdentityCheckResult::SameIdentityAlive;
        }

        if (!isProcessStillActive(processHandle.get()))
        {
            return IdentityCheckResult::StaleOrNoLongerTrusted;
        }

        const auto verifiedCreationTime = queryProcessCreationTime100ns(processHandle.get());
        if (!verifiedCreationTime)
        {
            return IdentityCheckResult::VerificationFailed;
        }

        const auto verifiedCreationTime100ns = *verifiedCreationTime;
        if (state.creationTime100ns != verifiedCreationTime100ns)
        {
            return IdentityCheckResult::StaleOrNoLongerTrusted;
        }

        return IdentityCheckResult::SameIdentityAlive;
    };

    if (!SetProcessAffinityMask(processHandle.get(), state.originalAffinityMask))
    {
        const DWORD affinityError = GetLastError();
        const IdentityCheckResult identityCheck = verifyIdentityAfterRestoreFailure();
        if (identityCheck == IdentityCheckResult::StaleOrNoLongerTrusted)
        {
            Logger::warn(
                "background rollback skipped for PID {} because process identity became stale after affinity restore failure (lastError={})",
                state.processId,
                affinityError);
            return {};
        }

        if (identityCheck == IdentityCheckResult::VerificationFailed)
        {
            Logger::error(
                "background rollback identity verification failed for live PID {} after affinity restore failure (lastError={})",
                state.processId,
                affinityError);
        }

        return std::unexpected(ErrorCode::ProcessAffinityApplyFailed);
    }

    if (!SetPriorityClass(processHandle.get(), state.originalPriorityClass))
    {
        const DWORD priorityError = GetLastError();
        const IdentityCheckResult identityCheck = verifyIdentityAfterRestoreFailure();
        if (identityCheck == IdentityCheckResult::StaleOrNoLongerTrusted)
        {
            Logger::warn(
                "background rollback skipped for PID {} because process identity became stale after priority restore failure (lastError={})",
                state.processId,
                priorityError);
            return {};
        }

        if (identityCheck == IdentityCheckResult::VerificationFailed)
        {
            Logger::error(
                "background rollback identity verification failed for live PID {} after priority restore failure (lastError={})",
                state.processId,
                priorityError);
        }

        return std::unexpected(ErrorCode::ProcessPriorityApplyFailed);
    }

    Logger::info(
        "background rollback restored PID {} (observedGroup={}, rollbackMode={}, affinity=0x{:X}, priorityClass=0x{:X})",
        state.processId,
        static_cast<unsigned int>(state.observedProcessorGroup),
        processRollbackModeToString(state.rollbackMode),
        static_cast<unsigned long long>(state.originalAffinityMask),
        static_cast<unsigned int>(state.originalPriorityClass));

    return {};
}
