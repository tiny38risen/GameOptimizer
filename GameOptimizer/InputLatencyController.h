// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: InputLatencyController
// ERROR-POLICY: expected
// Reason: Raw Input detection for a remote process is best-effort and must fall back safely.

#pragma once

#include <Windows.h>
#include <expected>

#include "ErrorCode.h"
#include "SchedulerController.h"

struct InputLatencyStatus
{
    bool rawInputDetected = false;
    bool inputThreadPinned = false;
    DWORD inputThreadId = 0;
};

class InputLatencyController
{
public:
    explicit InputLatencyController(SchedulerMode mode) noexcept;

    [[nodiscard]] std::expected<InputLatencyStatus, ErrorCode>
    detectAndApply(DWORD processId, const SchedulerPolicy& policy) noexcept;

private:
    [[nodiscard]] bool detectRawInputUsage(DWORD processId) noexcept;

private:
    SchedulerMode mode_ = SchedulerMode::DryRun;
    InputLatencyStatus status_{};
};
