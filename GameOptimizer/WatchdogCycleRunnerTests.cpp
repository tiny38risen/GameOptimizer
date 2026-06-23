// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- WatchdogCycleRunnerTests.cpp WatchdogCycleRunner.cpp
// MODULE: WatchdogCycleRunnerTests
// PURPOSE: deterministic shutdown guard checks before watchdog mutation dispatch.

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <stop_token>
#include <string_view>

#define GAMEOPTIMIZER_ENABLE_WATCHDOG_TEST_HOOKS
#include "WatchdogCycleRunner.h"
#undef GAMEOPTIMIZER_ENABLE_WATCHDOG_TEST_HOOKS

bool watchdogCycleRunnerShutdownGuardTest(
    WatchdogCycleRunner& runner,
    RuntimeContext& context) noexcept
{
    constexpr DWORD kAppliedThreadId = 42;
    constexpr DWORD kNewMainThreadId = 43;

    runner.applyMainThreadPolicyIfNeeded(kNewMainThreadId);
    if (context.lastAppliedThreadId != kAppliedThreadId)
    {
        return false;
    }

    runner.reconcilePolicyIfNeeded(kAppliedThreadId);
    if (context.policyAuditCycleCounter != 0)
    {
        return false;
    }

    RuntimeMetrics runtimeMetrics{};
    RuntimeValidationSample validationSample{};

    const bool feedbackDispatchContinues =
        runner.dispatchFeedbackCommands(validationSample, runtimeMetrics);
    if (!feedbackDispatchContinues)
    {
        return false;
    }

    if (validationSample.feedbackCommandCount != 0 || validationSample.dispatchFailureCount != 0)
    {
        return false;
    }

    runner.dispatchDecisionCommands(validationSample, runtimeMetrics);
    return validationSample.decisionCommandCount == 0 && validationSample.dispatchFailureCount == 0;
}

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

    void noopRequestShutdown(ShutdownReason) noexcept
    {
    }

    void skipsMutationWhenShutdownRequestedBeforeDispatch()
    {
        constexpr DWORD kAppliedThreadId = 42;

        RuntimeContext context;
        RuntimeSignalState signalState;
        signalState.requestShutdown(ShutdownReason::ConsoleControl);

        context.options.schedulerMode = SchedulerMode::Apply;
        context.startupPlan.mainThreadPolicy.affinityMask = 0x1;
        context.startupPlan.mainThreadPolicy.processorGroup = 0;
        context.startupPlan.mainThreadPolicy.threadPriority = THREAD_PRIORITY_ABOVE_NORMAL;
        context.startupPlan.policyAuditIntervalCycles = 1;
        context.lastAppliedThreadId = kAppliedThreadId;

        WatchdogCycleRunner runner(
            context,
            signalState.runtimeTimeoutRequested,
            noopRequestShutdown,
            signalState);

        std::stop_source stopSource;
        runner.runCycle(stopSource.get_token());

        REQUIRE(
            watchdogCycleRunnerShutdownGuardTest(runner, context),
            "shutdown guard must skip apply, reconcile, feedback dispatch, and decision dispatch");
    }
}

int main()
{
    skipsMutationWhenShutdownRequestedBeforeDispatch();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] WatchdogCycleRunner tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] WatchdogCycleRunner tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
