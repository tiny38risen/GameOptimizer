// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: TimerResolutionController
// ERROR-POLICY: expected
// Reason: NtSetTimerResolution is a runtime OS boundary and must roll back cleanly.

#pragma once

#include <Windows.h>
#include <expected>

#include "ErrorCode.h"
#include "SchedulerController.h"

struct TimerResolutionStatus
{
    bool apiAvailable = false;
    bool applied = false;
    ULONG minimum100ns = 0;
    ULONG maximum100ns = 0;
    ULONG currentBefore100ns = 0;
    ULONG currentAfter100ns = 0;
    ULONG requested100ns = 0;
};

class TimerResolutionController
{
public:
    explicit TimerResolutionController(SchedulerMode mode) noexcept;
    ~TimerResolutionController() noexcept;

    TimerResolutionController(const TimerResolutionController&) = delete;
    TimerResolutionController& operator=(const TimerResolutionController&) = delete;

    [[nodiscard]] std::expected<void, ErrorCode> apply() noexcept;
    [[nodiscard]] std::expected<void, ErrorCode> rollback() noexcept;
    [[nodiscard]] TimerResolutionStatus status() const noexcept;

private:
    using NtQueryTimerResolutionFn = LONG (WINAPI*)(PULONG, PULONG, PULONG);
    using NtSetTimerResolutionFn = LONG (WINAPI*)(ULONG, BOOLEAN, PULONG);

    [[nodiscard]] bool resolveApi() noexcept;

private:
    SchedulerMode mode_ = SchedulerMode::DryRun;
    HMODULE ntdll_ = nullptr;
    NtQueryTimerResolutionFn queryTimerResolution_ = nullptr;
    NtSetTimerResolutionFn setTimerResolution_ = nullptr;
    TimerResolutionStatus status_{};
};
