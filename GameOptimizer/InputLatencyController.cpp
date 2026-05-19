// Build: cl /std:c++latest /O2 /W4 /WX /permissive- InputLatencyController.cpp
// MODULE: InputLatencyController
// ERROR-POLICY: expected
// Reason: remote Raw Input usage is not reliably inspectable through public Win32 APIs.

#include "InputLatencyController.h"

#include "Logger.h"

#include <vector>

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
    status_ = detectRawInputUsage(processId);
    status_.pinningEligible = isInputThreadPinningAllowed(status_);

    if (!status_.rawInputDetected)
    {
        status_.fallbackMonitoringOnly = true;
        Logger::warn(
            "Raw Input usage was not detected for PID {}; input thread pinning disabled, timer/scheduler fallback remains active",
            processId);
        return status_;
    }

    if (!status_.pinningEligible)
    {
        status_.pinningBlockedUntilConcreteTid = true;
        status_.fallbackMonitoringOnly = true;
        Logger::warn(
            "Raw Input detected for PID {}, but input TID confidence is below High or no concrete TID is available; input thread pinning skipped",
            processId);
        return status_;
    }

    // Pinning remains intentionally gated. The only allowed path is Raw Input
    // detected plus a concrete input TID; detection currently never fabricates
    // that TID from window ownership or GUI-thread heuristics.
    Logger::warn(
        "Raw Input detected for PID {} with input TID {}, but input pinning is not wired to SchedulerController yet",
        processId,
        status_.inputThreadId);
    return status_;
}

bool InputLatencyController::isInputThreadPinningAllowed(const InputLatencyStatus& status) noexcept
{
    return status.rawInputDetected
        && status.inputThreadId != 0
        && status.tidConfidence == InputThreadTidConfidence::High;
}

InputLatencyStatus InputLatencyController::detectRawInputUsage(DWORD processId) noexcept
{
    InputLatencyStatus status{};
    status.detectionAttempted = true;

    if (processId == GetCurrentProcessId())
    {
        status.detectionPath = RawInputDetectionPath::LocalProcessRegisteredDevices;
        status.remoteDetectionSupported = true;
        status.rawInputDetected = detectLocalRawInputRegistration();
        if (status.rawInputDetected)
        {
            status.tidConfidence = InputThreadTidConfidence::Low;
            status.tidSource = InputThreadTidSource::EtwInvestigationPending;
            Logger::info(
                "Raw Input local-process registration detected for PID {}; input TID confidence=Low, source=ETW investigation pending",
                processId);
            status.pinningBlockedUntilConcreteTid = true;
            status.fallbackMonitoringOnly = true;
            Logger::warn(
                "input thread pinning remains monitoring-only for PID {} because TID confidence is below High",
                processId);
        }
        else
        {
            status.tidConfidence = InputThreadTidConfidence::None;
            status.tidSource = InputThreadTidSource::ForegroundMessageQueueInvestigationPending;
            Logger::warn(
                "Raw Input local-process registration not found for PID {}; using fallback input policy",
                processId);
        }

        if (mode_ == SchedulerMode::DryRun)
        {
            Logger::info("dry-run: would pin an input thread only after Raw Input and a concrete input TID are detected");
        }
        else if (mode_ == SchedulerMode::SoftApply)
        {
            Logger::info("soft-apply: input thread pinning validation skipped until a concrete input TID is detected");
        }

        return status;
    }

    status.detectionPath = RawInputDetectionPath::RemoteProcessUnsupported;
    status.remoteDetectionSupported = false;
    status.fallbackMonitoringOnly = true;
    status.tidConfidence = InputThreadTidConfidence::None;
    status.tidSource = InputThreadTidSource::None;
    Logger::warn(
        "Raw Input remote-process detection is unavailable through the current public Win32 path for PID {}; using fallback input policy",
        processId);

    if (mode_ == SchedulerMode::DryRun)
    {
        Logger::info("dry-run: would pin an input thread only after Raw Input and a concrete input TID are detected");
    }
    else if (mode_ == SchedulerMode::SoftApply)
    {
        Logger::info("soft-apply: input thread pinning validation skipped because remote Raw Input detection is unsupported");
    }

    return status;
}

bool InputLatencyController::detectLocalRawInputRegistration() noexcept
{
    UINT deviceCount = 0;
    const UINT querySize = sizeof(RAWINPUTDEVICE);
    const UINT initialResult = GetRegisteredRawInputDevices(nullptr, &deviceCount, querySize);
    if (initialResult == static_cast<UINT>(-1))
    {
        Logger::warn("Raw Input local registration query failed while reading device count");
        return false;
    }

    if (deviceCount == 0)
    {
        return false;
    }

    try
    {
        std::vector<RAWINPUTDEVICE> devices(deviceCount);
        UINT writableDeviceCount = deviceCount;
        const UINT readCount = GetRegisteredRawInputDevices(
            devices.data(),
            &writableDeviceCount,
            querySize);
        if (readCount == static_cast<UINT>(-1))
        {
            Logger::warn("Raw Input local registration query failed while reading registered devices");
            return false;
        }

        return readCount > 0;
    }
    catch (...)
    {
        Logger::warn("Raw Input local registration query failed due to allocation/runtime error");
        return false;
    }
}
