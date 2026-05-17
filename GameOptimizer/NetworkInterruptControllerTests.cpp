// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- NetworkInterruptControllerTests.cpp NetworkInterruptController.cpp PolicyDispatcher.cpp BackgroundController.cpp RollbackManager.cpp ApplyGuard.cpp SchedulerController.cpp ThreadTracker.cpp
// MODULE: NetworkInterruptControllerTests
// PURPOSE: IT-2 style checks for unsupported interrupt-affinity fallback and IRQ_REPIN dispatch routing.

#include "NetworkInterruptController.h"

#include "BackgroundController.h"
#include "PolicyDispatcher.h"
#include "RollbackManager.h"
#include "SchedulerController.h"
#include "ThreadTracker.h"

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

    void testUnsupportedInterruptAffinityIsMonitoringOnly()
    {
        NetworkInterruptController controller(NetworkInterruptControllerConfig{
            .enableDpcMonitoring = false,
            .forceInterruptAffinitySupportedForTests = false});

        REQUIRE(controller.initialize(), "initialization must not fail when interrupt affinity is unsupported");

        const NetworkInterruptStatus status = controller.status();
        REQUIRE(!status.interruptAffinitySupported,
            "unsupported environment must report interrupt affinity as unsupported");
        REQUIRE(status.monitoringOnly,
            "unsupported interrupt affinity must keep the controller in monitoring-only mode");
        REQUIRE(controller.handleIrqRepin().has_value(),
            "unsupported IRQ_REPIN must be ignored as a non-fatal fallback");
    }

    void testPolicyDispatcherRoutesIrqRepinToNetworkController()
    {
        RollbackManager rollbackManager;
        SchedulerController schedulerController(rollbackManager, SchedulerMode::DryRun);
        ThreadTracker threadTracker(1);
        BackgroundController backgroundController(rollbackManager, SchedulerMode::DryRun);
        BackgroundRestrictionPolicy backgroundPolicy{
            .targetProcessId = 1,
            .gameAffinityMask = 0x1,
            .processorGroup = 0};
        NetworkInterruptController networkController(NetworkInterruptControllerConfig{
            .enableDpcMonitoring = false,
            .forceInterruptAffinitySupportedForTests = false});
        (void)networkController.initialize();

        PolicyDispatcher dispatcher(
            schedulerController,
            threadTracker,
            backgroundController,
            backgroundPolicy,
            &networkController);

        REQUIRE(dispatcher.dispatch(PolicyCommand::IrqRepin).has_value(),
            "IT-2: IRQ_REPIN must be routed to the network controller and ignored non-fatally when unsupported");
    }
}

int main()
{
    testUnsupportedInterruptAffinityIsMonitoringOnly();
    testPolicyDispatcherRoutesIrqRepinToNetworkController();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] NetworkInterruptController IT-2 tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] NetworkInterruptController IT-2 tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
