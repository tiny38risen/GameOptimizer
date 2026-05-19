// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- TimerInputControllerTests.cpp TimerResolutionController.cpp InputLatencyController.cpp
// MODULE: TimerInputControllerTests
// PURPOSE: deterministic safety checks for timer dry-run/soft-apply and Raw Input fallback behavior.

#include "InputLatencyController.h"
#include "TimerResolutionController.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
    int g_failureCount = 0;

    void recordFailure(const char* file, int line, std::string_view message) noexcept
    {
        std::cerr << "[FAIL] " << file << ':' << line << " - " << message << '\n';
        ++g_failureCount;
    }

#define REQUIRE(condition, message) \
    do \
    { \
        if (!(condition)) \
        { \
            recordFailure(__FILE__, __LINE__, (message)); \
        } \
    } while (false)

    void testTimerDryRunDoesNotApply()
    {
        TimerResolutionController controller(SchedulerMode::DryRun);
        REQUIRE(controller.apply().has_value(), "dry-run timer apply should be non-fatal");
        const TimerResolutionStatus status = controller.status();
        REQUIRE(!status.applied, "dry-run must not apply system timer resolution");
        if (status.apiAvailable)
        {
            REQUIRE(status.requested100ns != 0, "discoverable timer API should record the requested resolution");
        }
        REQUIRE(controller.rollback().has_value(), "dry-run timer rollback should be a no-op success");
    }

    void testLocalRawInputDetectionPathIsNonFatal()
    {
        InputLatencyController controller(SchedulerMode::DryRun);
        const auto result = controller.detectAndApply(GetCurrentProcessId(), SchedulerPolicy{
            .affinityMask = 0x1,
            .processorGroup = 0,
            .threadPriority = THREAD_PRIORITY_ABOVE_NORMAL});

        REQUIRE(result.has_value(), "local Raw Input detection path must be non-fatal");
        if (result)
        {
            REQUIRE(result->detectionAttempted, "local Raw Input detection must be marked as attempted");
            REQUIRE(result->remoteDetectionSupported, "same-process Raw Input registration query should be supported");
            REQUIRE(result->detectionPath == RawInputDetectionPath::LocalProcessRegisteredDevices,
                "same-process Raw Input detection must use registered-device query path");
            REQUIRE(!result->inputThreadPinned, "input thread pinning must not run from detection alone");
            REQUIRE(!result->pinningEligible,
                "Raw Input detection success must not imply input-thread pinning eligibility");
            if (result->rawInputDetected)
            {
                REQUIRE(result->tidConfidence == InputThreadTidConfidence::Low,
                    "local Raw Input detection without concrete TID must record low TID confidence");
                REQUIRE(result->tidSource == InputThreadTidSource::EtwInvestigationPending,
                    "local Raw Input detection must leave concrete TID acquisition to ETW investigation");
                REQUIRE(result->fallbackMonitoringOnly,
                    "low TID confidence must stay in monitoring-only fallback");
            }
        }
    }

    void testRemoteRawInputDetectionFallsBack()
    {
        InputLatencyController controller(SchedulerMode::DryRun);
        const DWORD remoteLikePid = GetCurrentProcessId() + 1;
        const auto result = controller.detectAndApply(remoteLikePid, SchedulerPolicy{
            .affinityMask = 0x1,
            .processorGroup = 0,
            .threadPriority = THREAD_PRIORITY_ABOVE_NORMAL});

        REQUIRE(result.has_value(), "unsupported remote Raw Input detection must be a non-fatal fallback");
        if (result)
        {
            REQUIRE(result->detectionAttempted, "remote Raw Input detection must be marked as attempted");
            REQUIRE(!result->rawInputDetected, "remote Raw Input detector must not claim unsupported detection");
            REQUIRE(!result->remoteDetectionSupported, "remote Raw Input detection must be explicitly unsupported");
            REQUIRE(result->fallbackMonitoringOnly, "remote Raw Input miss must switch to monitoring-only fallback");
            REQUIRE(result->detectionPath == RawInputDetectionPath::RemoteProcessUnsupported,
                "remote Raw Input detection must record the unsupported path");
            REQUIRE(!result->inputThreadPinned, "input thread pinning must not run without detection");
            REQUIRE(result->inputThreadId == 0, "input thread id must remain empty without concrete detection");
            REQUIRE(!result->pinningEligible, "remote unsupported detection must not be pinning eligible");
            REQUIRE(result->tidConfidence == InputThreadTidConfidence::None,
                "remote unsupported detection must not assign TID confidence");
            REQUIRE(result->tidSource == InputThreadTidSource::None,
                "remote unsupported detection must not claim a TID source");
        }
    }

    void testInputPinningRequiresConcreteThreadId()
    {
        REQUIRE(!InputLatencyController::isInputThreadPinningAllowed(InputLatencyStatus{
            .rawInputDetected = false,
            .inputThreadId = 99}),
            "pinning must require Raw Input detection");
        REQUIRE(!InputLatencyController::isInputThreadPinningAllowed(InputLatencyStatus{
            .rawInputDetected = true,
            .inputThreadId = 0,
            .tidConfidence = InputThreadTidConfidence::High}),
            "pinning must require a concrete input TID");
        REQUIRE(!InputLatencyController::isInputThreadPinningAllowed(InputLatencyStatus{
            .rawInputDetected = true,
            .inputThreadId = 99,
            .tidConfidence = InputThreadTidConfidence::Low}),
            "pinning must reject low-confidence TID candidates");
        REQUIRE(InputLatencyController::isInputThreadPinningAllowed(InputLatencyStatus{
            .rawInputDetected = true,
            .inputThreadId = 99,
            .tidConfidence = InputThreadTidConfidence::High}),
            "pinning should become eligible only after Raw Input and high-confidence concrete TID are both available");
    }
}

int main()
{
    testTimerDryRunDoesNotApply();
    testLocalRawInputDetectionPathIsNonFatal();
    testRemoteRawInputDetectionFallsBack();
    testInputPinningRequiresConcreteThreadId();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] Timer/Input controller tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] Timer/Input controller tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
