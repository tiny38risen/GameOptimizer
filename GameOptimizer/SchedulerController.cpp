// Build: cl /std:c++latest /O2 /W4 /WX /permissive- SchedulerController.cpp
// MODULE: SchedulerController
// ERROR-POLICY: expected
// Reason: partial Win32 failures trigger rollback or state cleanup.

#include "SchedulerController.h"

#include "ApplyGuard.h"
#include "Logger.h"
#include "WinApiError.h"
#include "WinHandle.h"

#include <bit>
#include <utility>

namespace
{
    constexpr int kMaxPolicyDriftReapplyCountPerThread = 3;

    struct CurrentSchedulingState
    {
        DWORD_PTR affinityMask = 0;
        WORD processorGroup = 0;
        int priority = THREAD_PRIORITY_ERROR_RETURN;
    };

    [[nodiscard]] std::expected<CurrentSchedulingState, ErrorCode> queryCurrentSchedulingState(HANDLE threadHandle) noexcept
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

        return CurrentSchedulingState{
            .affinityMask = currentAffinityMask,
            .processorGroup = currentAffinity.Group,
            .priority = currentPriority};
    }

    [[nodiscard]] bool matchesPolicy(
        const CurrentSchedulingState& currentState,
        const SchedulerPolicy& policy) noexcept
    {
        return currentState.affinityMask == policy.affinityMask &&
               currentState.processorGroup == policy.processorGroup &&
               currentState.priority == policy.threadPriority;
    }
}

SchedulerController::SchedulerController(
    RollbackManager& rollbackManager,
    SchedulerMode mode) noexcept
    : rollbackManager_(rollbackManager),
      mode_(mode)
{
}


std::expected<void, ErrorCode> SchedulerController::validatePolicyShape(
    const SchedulerPolicy& policy) noexcept
{
    if (policy.affinityMask == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    const WORD activeGroupCount = GetActiveProcessorGroupCount();
    if (activeGroupCount == 0 || policy.processorGroup >= activeGroupCount)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    const int selectedBitCount = std::popcount(static_cast<unsigned long long>(policy.affinityMask));
    if (selectedBitCount <= 0 || selectedBitCount > 2)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    return {};
}

bool SchedulerController::isRecoverableAccessLimitation(ErrorCode errorCode) noexcept
{
    return errorCode == ErrorCode::AccessDenied ||
           errorCode == ErrorCode::ThreadOpenFailed;
}

std::expected<WinHandle, ErrorCode> SchedulerController::openThreadForSchedulingProbe(
    DWORD threadId) noexcept
{
    HANDLE rawHandle = OpenThread(
        THREAD_QUERY_LIMITED_INFORMATION | THREAD_SET_INFORMATION,
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

std::expected<void, ErrorCode> SchedulerController::validateSchedulingRightsAndState(
    DWORD threadId,
    const SchedulerPolicy& policy) noexcept
{
    const auto policyShapeResult = validatePolicyShape(policy);
    if (!policyShapeResult)
    {
        return std::unexpected(policyShapeResult.error());
    }

    auto openedThread = openThreadForSchedulingProbe(threadId);
    if (!openedThread)
    {
        return std::unexpected(openedThread.error());
    }

    auto& openedThreadHandle = *openedThread;
    WinHandle threadHandle(std::move(openedThreadHandle));

    const auto currentState = queryCurrentSchedulingState(threadHandle.get());
    if (!currentState)
    {
        return std::unexpected(currentState.error());
    }

    const auto& state = *currentState;

    const auto saveResult = rollbackManager_.saveValidatedReadState(
        threadId,
        state.affinityMask,
        state.processorGroup,
        state.priority);
    if (!saveResult)
    {
        return std::unexpected(saveResult.error());
    }


    if (state.processorGroup != policy.processorGroup)
    {
        Logger::warn(
            "soft-apply: target processor group differs from current group for TID {} (currentGroup={}, targetGroup={})",
            threadId,
            static_cast<unsigned int>(state.processorGroup),
            static_cast<unsigned int>(policy.processorGroup));
    }

    return {};
}

std::expected<void, ErrorCode> SchedulerController::applyMainThreadPolicy(
    DWORD threadId,
    const SchedulerPolicy& policy) noexcept
{
    if (threadId == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    if (mode_ == SchedulerMode::DryRun)
    {
        const auto saveResult = rollbackManager_.saveDryRunState(threadId);
        if (!saveResult)
        {
            return std::unexpected(saveResult.error());
        }

        Logger::info(
            "dry-run: would apply main-thread policy to TID {} (group={}, affinity=0x{:X}, priority={})",
            threadId,
            static_cast<unsigned int>(policy.processorGroup),
            static_cast<unsigned long long>(policy.affinityMask),
            policy.threadPriority);
        return {};
    }

    if (mode_ == SchedulerMode::SoftApply)
    {
        auto validationResult = validateSchedulingRightsAndState(threadId, policy);
        if (!validationResult)
        {
            return std::unexpected(validationResult.error());
        }

        return {};
    }

    const auto policyShapeResult = validatePolicyShape(policy);
    if (!policyShapeResult)
    {
        return std::unexpected(policyShapeResult.error());
    }

    auto openedThread = openThreadForSchedulingProbe(threadId);
    if (!openedThread)
    {
        return std::unexpected(openedThread.error());
    }

    auto& openedThreadHandle = *openedThread;
    WinHandle threadHandle(std::move(openedThreadHandle));

    const auto originalState = queryCurrentSchedulingState(threadHandle.get());
    if (!originalState)
    {
        return std::unexpected(originalState.error());
    }

    const auto& original = *originalState;

    if (matchesPolicy(original, policy))
    {
        return {};
    }

    const auto saveResult = rollbackManager_.saveAppliedState(
        threadId,
        original.affinityMask,
        original.processorGroup,
        original.priority);
    if (!saveResult)
    {
        return std::unexpected(saveResult.error());
    }

    const auto saveDisposition = *saveResult;
    auto applyGuard = ApplyGuard::forThread(rollbackManager_, threadId, saveDisposition);
    applyGuard.arm();

    GROUP_AFFINITY targetAffinity{};
    targetAffinity.Mask = static_cast<KAFFINITY>(policy.affinityMask);
    targetAffinity.Group = policy.processorGroup;

    GROUP_AFFINITY previousAffinity{};
    if (!SetThreadGroupAffinity(threadHandle.get(), &targetAffinity, &previousAffinity))
    {
        const ErrorCode mappedError = mapLastErrorToErrorCode(GetLastError());
        applyGuard.discardSavedState();
        if (isRecoverableAccessLimitation(mappedError))
        {
            Logger::warn(
                "main-thread affinity apply blocked by recoverable access limitation for TID {}; monitoring-only fallback remains active ({})",
                threadId,
                toString(mappedError));
            return std::unexpected(mappedError);
        }

        return std::unexpected(ErrorCode::ThreadAffinityApplyFailed);
    }

    if (!SetThreadPriority(threadHandle.get(), policy.threadPriority))
    {
        const ErrorCode mappedError = mapLastErrorToErrorCode(GetLastError());
        auto rollbackResult = applyGuard.rollbackNow();
        if (!rollbackResult)
        {
            Logger::error(
                "priority apply failed and rollback also failed for TID {}: {}; rollback state is preserved for shutdown recovery",
                threadId,
                toString(rollbackResult.error()));
        }

        if (isRecoverableAccessLimitation(mappedError))
        {
            Logger::warn(
                "main-thread priority apply blocked by recoverable access limitation for TID {}; rollback path was invoked when needed ({})",
                threadId,
                toString(mappedError));
            return std::unexpected(mappedError);
        }

        return std::unexpected(ErrorCode::ThreadPriorityApplyFailed);
    }

    const auto verifiedState = queryCurrentSchedulingState(threadHandle.get());
    if (!verifiedState)
    {
        Logger::error(
            "apply audit failed for TID {}: could not query current scheduling state after apply ({})",
            threadId,
            toString(verifiedState.error()));

        auto rollbackResult = applyGuard.rollbackNow();
        if (!rollbackResult)
        {
            Logger::error(
                "apply audit failed and rollback also failed for TID {}: {}",
                threadId,
                toString(rollbackResult.error()));
        }

        return std::unexpected(ErrorCode::ApplyVerificationFailed);
    }

    const auto& verified = *verifiedState;
    if (!matchesPolicy(verified, policy))
    {
        Logger::error(
            "apply audit mismatch for TID {}: current(group={}, affinity=0x{:X}, priority={}) expected(group={}, affinity=0x{:X}, priority={})",
            threadId,
            static_cast<unsigned int>(verified.processorGroup),
            static_cast<unsigned long long>(verified.affinityMask),
            verified.priority,
            static_cast<unsigned int>(policy.processorGroup),
            static_cast<unsigned long long>(policy.affinityMask),
            policy.threadPriority);

        auto rollbackResult = applyGuard.rollbackNow();
        if (!rollbackResult)
        {
            Logger::error(
                "apply audit failed and rollback also failed for TID {}: {}",
                threadId,
                toString(rollbackResult.error()));
        }

        return std::unexpected(ErrorCode::ApplyVerificationFailed);
    }

    applyGuard.commit();
    driftReapplyCounts_.try_emplace(threadId, 0);
    return {};
}


std::expected<void, ErrorCode> SchedulerController::reconcileMainThreadPolicy(
    DWORD threadId,
    const SchedulerPolicy& policy) noexcept
{
    if (threadId == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    const auto policyShapeResult = validatePolicyShape(policy);
    if (!policyShapeResult)
    {
        return std::unexpected(policyShapeResult.error());
    }

    if (mode_ != SchedulerMode::Apply)
    {
        return {};
    }

    auto openedThread = openThreadForSchedulingProbe(threadId);
    if (!openedThread)
    {
        return std::unexpected(openedThread.error());
    }

    auto& openedThreadHandle = *openedThread;
    WinHandle threadHandle(std::move(openedThreadHandle));
    const auto currentState = queryCurrentSchedulingState(threadHandle.get());
    if (!currentState)
    {
        return std::unexpected(currentState.error());
    }

    const auto& state = *currentState;

    if (matchesPolicy(state, policy))
    {
        return {};
    }

    const auto driftCounterIt = driftReapplyCounts_.find(threadId);
    if (driftCounterIt == driftReapplyCounts_.end())
    {
        Logger::error(
            "policy drift detected for unregistered TID {}; cold-path apply state was not initialized",
            threadId);
        return std::unexpected(ErrorCode::InternalError);
    }

    int& driftReapplyCount = driftCounterIt->second;
    if (driftReapplyCount >= kMaxPolicyDriftReapplyCountPerThread)
    {
        Logger::error(
            "policy drift re-apply limit reached for TID {} (limit={}); rollback state is preserved",
            threadId,
            kMaxPolicyDriftReapplyCountPerThread);
        return std::unexpected(ErrorCode::PolicyDriftReapplyLimitExceeded);
    }

    ++driftReapplyCount;

    Logger::warn(
        "policy drift detected for TID {}: current(group={}, affinity=0x{:X}, priority={}) expected(group={}, affinity=0x{:X}, priority={}); re-applying policy (attempt {}/{})",
        threadId,
        static_cast<unsigned int>(state.processorGroup),
        static_cast<unsigned long long>(state.affinityMask),
        state.priority,
        static_cast<unsigned int>(policy.processorGroup),
        static_cast<unsigned long long>(policy.affinityMask),
        policy.threadPriority,
        driftReapplyCount,
        kMaxPolicyDriftReapplyCountPerThread);

    auto applyResult = applyMainThreadPolicy(threadId, policy);
    if (!applyResult)
    {
        return std::unexpected(applyResult.error());
    }

    return {};
}


void SchedulerController::clearDriftCounter(DWORD threadId) noexcept
{
    driftReapplyCounts_.erase(threadId);
}

std::expected<void, ErrorCode> SchedulerController::rollbackThread(DWORD threadId) noexcept
{
    clearDriftCounter(threadId);

    if (!rollbackManager_.hasThreadState(threadId))
    {
        return {};
    }

    auto rollbackResult = rollbackManager_.rollbackThread(threadId);
    if (!rollbackResult)
    {
        return std::unexpected(rollbackResult.error());
    }

    return {};
}


std::expected<void, ErrorCode> SchedulerController::rollbackAll() noexcept
{
    driftReapplyCounts_.clear();
    auto rollbackResult = rollbackManager_.rollbackAll();
    if (!rollbackResult)
    {
        return std::unexpected(rollbackResult.error());
    }

    return {};
}
