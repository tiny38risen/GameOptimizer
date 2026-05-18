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

    void testInputFallbackWhenRawInputNotDetected()
    {
        InputLatencyController controller(SchedulerMode::DryRun);
        const auto result = controller.detectAndApply(GetCurrentProcessId(), SchedulerPolicy{
            .affinityMask = 0x1,
            .processorGroup = 0,
            .threadPriority = THREAD_PRIORITY_ABOVE_NORMAL});

        REQUIRE(result.has_value(), "Raw Input miss must be a non-fatal fallback");
        if (result)
        {
            REQUIRE(!result->rawInputDetected, "current remote Raw Input detector should report not detected");
            REQUIRE(!result->inputThreadPinned, "input thread pinning must not run without Raw Input detection");
            REQUIRE(result->inputThreadId == 0, "input thread id must remain empty without detection");
        }
    }
}

int main()
{
    testTimerDryRunDoesNotApply();
    testInputFallbackWhenRawInputNotDetected();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] Timer/Input controller tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] Timer/Input controller tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
