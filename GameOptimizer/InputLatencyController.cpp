// Build: cl /std:c++latest /O2 /W4 /WX /permissive- InputLatencyController.cpp
// MODULE: InputLatencyController
// ERROR-POLICY: expected
// Reason: remote Raw Input usage is not reliably inspectable through public Win32 APIs.

#include "InputLatencyController.h"

#include "Logger.h"

InputLatencyController::InputLatencyController(SchedulerMode mode) noexcept
    : mode_(mode)
{
}

std::expected<InputLatencyStatus, ErrorCode> InputLatencyController::detectAndApply(
    DWORD processId,
    const SchedulerPolicy& policy) noexcept
{
    if (processId == 0 || policy.affinityMask == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    status_ = {};
    status_.rawInputDetected = detectRawInputUsage(processId);

    if (!status_.rawInputDetected)
    {
        Logger::warn(
            "Raw Input usage was not detected for PID {}; input thread pinning disabled, timer/scheduler fallback remains active",
            processId);
        return status_;
    }

    // Remote Raw Input registration does not expose a stable public mapping to
    // an input handling thread. Keep this path gated until detection can provide
    // a concrete thread identity.
    Logger::warn(
        "Raw Input detected for PID {}, but no input thread identity is available; input thread pinning skipped",
        processId);
    return status_;
}

bool InputLatencyController::detectRawInputUsage(DWORD processId) noexcept
{
    (void)processId;
    Logger::warn(
        "Raw Input remote-process detection is unavailable through the current public Win32 path; using fallback input policy");

    if (mode_ == SchedulerMode::DryRun)
    {
        Logger::info("dry-run: would pin an input thread only after Raw Input and a concrete input TID are detected");
    }
    else if (mode_ == SchedulerMode::SoftApply)
    {
        Logger::info("soft-apply: input thread pinning validation skipped because Raw Input was not detected");
    }

    return false;
}
