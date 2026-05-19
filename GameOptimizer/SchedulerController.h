// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: SchedulerController
// ERROR-POLICY: expected
// Reason: scheduler policy application is a Win32 boundary and can fail per-thread.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <expected>
#include <unordered_map>

#include "ErrorCode.h"
#include "RollbackManager.h"
#include "WinHandle.h"

enum class SchedulerMode
{
    DryRun,
    SoftApply,
    Apply
};

struct SchedulerPolicy
{
    DWORD_PTR affinityMask = 0;
    WORD processorGroup = 0;
    int threadPriority = THREAD_PRIORITY_ABOVE_NORMAL;
};

class SchedulerController
{
public:
    explicit SchedulerController(RollbackManager& rollbackManager, SchedulerMode mode) noexcept;

    [[nodiscard]] static std::expected<void, ErrorCode>
    validatePolicyShape(const SchedulerPolicy& policy) noexcept;

    [[nodiscard]] static bool isRecoverableAccessLimitation(ErrorCode errorCode) noexcept;

    [[nodiscard]] std::expected<void, ErrorCode>
    applyMainThreadPolicy(DWORD threadId, const SchedulerPolicy& policy) noexcept;

    [[nodiscard]] std::expected<void, ErrorCode>
    reconcileMainThreadPolicy(DWORD threadId, const SchedulerPolicy& policy) noexcept;

    [[nodiscard]] std::expected<void, ErrorCode> rollbackThread(DWORD threadId) noexcept;
    [[nodiscard]] std::expected<void, ErrorCode> rollbackAll() noexcept;

private:
    void clearDriftCounter(DWORD threadId) noexcept;

    [[nodiscard]] static std::expected<WinHandle, ErrorCode> openThreadForSchedulingProbe(DWORD threadId) noexcept;

    [[nodiscard]] std::expected<void, ErrorCode>
    validateSchedulingRightsAndState(DWORD threadId, const SchedulerPolicy& policy) noexcept;

private:
    RollbackManager& rollbackManager_;
    SchedulerMode mode_ = SchedulerMode::DryRun;
    std::unordered_map<DWORD, int> driftReapplyCounts_;
};
