// Build: cl /std:c++latest /O2 /W4 /WX /permissive- TimerResolutionController.cpp
// MODULE: TimerResolutionController
// ERROR-POLICY: expected
// Reason: timer resolution changes are system-wide and must be bounded to apply mode.

#include "TimerResolutionController.h"

#include "Logger.h"

namespace
{
    constexpr ULONG kRequestedTimerResolution100ns = 5000; // 0.5 ms
    constexpr LONG kStatusSuccess = 0;
}

TimerResolutionController::TimerResolutionController(SchedulerMode mode) noexcept
    : mode_(mode)
{
}

TimerResolutionController::~TimerResolutionController() noexcept
{
    (void)rollback();
}

std::expected<void, ErrorCode> TimerResolutionController::apply() noexcept
{
    if (!resolveApi())
    {
        Logger::warn("timer resolution control unavailable: NtQueryTimerResolution/NtSetTimerResolution not found");
        return {};
    }

    status_.requested100ns = kRequestedTimerResolution100ns;

    ULONG minimum = 0;
    ULONG maximum = 0;
    ULONG current = 0;
    if (queryTimerResolution_(&minimum, &maximum, &current) != kStatusSuccess)
    {
        Logger::warn("timer resolution query failed; timer resolution optimization disabled");
        return {};
    }

    status_.minimum100ns = minimum;
    status_.maximum100ns = maximum;
    status_.currentBefore100ns = current;

    if (mode_ != SchedulerMode::Apply)
    {
        Logger::info(
            "{}: would request timer resolution {} x100ns (current={} x100ns, min={} x100ns, max={} x100ns)",
            mode_ == SchedulerMode::DryRun ? "dry-run" : "soft-apply",
            kRequestedTimerResolution100ns,
            current,
            minimum,
            maximum);
        return {};
    }

    ULONG currentAfter = 0;
    const LONG setStatus = setTimerResolution_(kRequestedTimerResolution100ns, TRUE, &currentAfter);
    if (setStatus != kStatusSuccess)
    {
        Logger::warn(
            "timer resolution apply failed: requested={} x100ns, status=0x{:X}",
            kRequestedTimerResolution100ns,
            static_cast<unsigned int>(setStatus));
        return std::unexpected(ErrorCode::InternalError);
    }

    status_.applied = true;
    status_.currentAfter100ns = currentAfter;
    Logger::info(
        "timer resolution applied: requested={} x100ns, previous={} x100ns, current={} x100ns",
        kRequestedTimerResolution100ns,
        current,
        currentAfter);
    return {};
}

std::expected<void, ErrorCode> TimerResolutionController::rollback() noexcept
{
    if (!status_.applied || setTimerResolution_ == nullptr)
    {
        return {};
    }

    ULONG currentAfter = 0;
    const LONG setStatus = setTimerResolution_(status_.requested100ns, FALSE, &currentAfter);
    if (setStatus != kStatusSuccess)
    {
        Logger::error(
            "timer resolution rollback failed: requested={} x100ns, status=0x{:X}",
            status_.requested100ns,
            static_cast<unsigned int>(setStatus));
        return std::unexpected(ErrorCode::InternalError);
    }

    Logger::info(
        "timer resolution rolled back: released={} x100ns, current={} x100ns",
        status_.requested100ns,
        currentAfter);
    status_.applied = false;
    status_.currentAfter100ns = currentAfter;
    return {};
}

TimerResolutionStatus TimerResolutionController::status() const noexcept
{
    return status_;
}

bool TimerResolutionController::resolveApi() noexcept
{
    if (status_.apiAvailable)
    {
        return true;
    }

    ntdll_ = GetModuleHandleW(L"ntdll.dll");
    if (ntdll_ == nullptr)
    {
        return false;
    }

    queryTimerResolution_ = reinterpret_cast<NtQueryTimerResolutionFn>(
        GetProcAddress(ntdll_, "NtQueryTimerResolution"));
    setTimerResolution_ = reinterpret_cast<NtSetTimerResolutionFn>(
        GetProcAddress(ntdll_, "NtSetTimerResolution"));

    status_.apiAvailable = queryTimerResolution_ != nullptr && setTimerResolution_ != nullptr;
    return status_.apiAvailable;
}
