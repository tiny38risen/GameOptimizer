// Build: cl /std:c++latest /EHsc /W4 /WX /permissive- RuntimeValidationMonitorTests.cpp RuntimeValidationMonitor.cpp
// MODULE: RuntimeValidationMonitorTests
// PURPOSE: deterministic validation tests for runtime safety counters and failure gates.

#include "RuntimeValidationMonitor.h"

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

    RuntimeValidationSample makeSample() noexcept
    {
        RuntimeValidationSample sample{};
        sample.mainThreadDetected = true;
        sample.mainThreadPolicyApplied = true;
        return sample;
    }

    void testSummaryCountsNormalCycles()
    {
        RuntimeValidationMonitor monitor(RuntimeValidationMonitorConfig{
            .minimumCyclesForSummary = 2,
            .maxDispatchFailures = 0,
            .maxRollbackRequests = 0,
            .maxConsecutiveNoMainThreadCycles = 3});

        RuntimeValidationSample first = makeSample();
        first.decisionCommandCount = 2;
        first.feedbackCommandCount = 1;
        monitor.observe(first);

        RuntimeValidationSample second = makeSample();
        second.metrics.rttJitterMs = 4.0;
        monitor.observe(second);

        const RuntimeValidationSummary summary = monitor.summary();
        REQUIRE(summary.observedCycles == 2, "observed cycle count must match samples");
        REQUIRE(summary.cyclesWithMainThread == 2, "main-thread cycle count must match detected samples");
        REQUIRE(summary.cyclesWithAppliedMainPolicy == 2, "applied-policy cycle count must match samples");
        REQUIRE(summary.totalDecisionCommands == 2, "decision command count must be accumulated");
        REQUIRE(summary.totalFeedbackCommands == 1, "feedback command count must be accumulated");
        REQUIRE(summary.minimumCycleCountSatisfied, "minimum cycle count must be satisfied");
        REQUIRE(!summary.criticalFailureDetected, "normal cycles must not trigger critical failure");
    }

    void testDispatchFailureTripsCriticalFailure()
    {
        RuntimeValidationMonitor monitor(RuntimeValidationMonitorConfig{
            .minimumCyclesForSummary = 1,
            .maxDispatchFailures = 0,
            .maxRollbackRequests = 1,
            .maxConsecutiveNoMainThreadCycles = 3});

        RuntimeValidationSample sample = makeSample();
        sample.dispatchFailureCount = 1;
        monitor.observe(sample);

        REQUIRE(monitor.hasCriticalFailure(), "dispatch failure above threshold must trip critical failure");
    }

    void testRollbackThresholdTripsCriticalFailure()
    {
        RuntimeValidationMonitor monitor(RuntimeValidationMonitorConfig{
            .minimumCyclesForSummary = 1,
            .maxDispatchFailures = 0,
            .maxRollbackRequests = 0,
            .maxConsecutiveNoMainThreadCycles = 3});

        RuntimeValidationSample sample = makeSample();
        sample.rollbackRequested = true;
        monitor.observe(sample);

        REQUIRE(monitor.hasCriticalFailure(), "rollback request above threshold must trip critical failure");
    }

    void testMissingMainThreadThreshold()
    {
        RuntimeValidationMonitor monitor(RuntimeValidationMonitorConfig{
            .minimumCyclesForSummary = 1,
            .maxDispatchFailures = 0,
            .maxRollbackRequests = 0,
            .maxConsecutiveNoMainThreadCycles = 2});

        RuntimeValidationSample sample{};
        sample.mainThreadDetected = false;
        monitor.observe(sample);
        monitor.observe(sample);
        REQUIRE(!monitor.hasCriticalFailure(), "missing main thread at threshold must not trip before exceeding it");
        monitor.observe(sample);
        REQUIRE(monitor.hasCriticalFailure(), "missing main thread above threshold must trip critical failure");
    }

    void testHighMetricCounters()
    {
        RuntimeValidationMonitor monitor(RuntimeValidationMonitorConfig{
            .minimumCyclesForSummary = 1,
            .maxDispatchFailures = 0,
            .maxRollbackRequests = 1,
            .maxConsecutiveNoMainThreadCycles = 3,
            .highRttJitterWarnMs = 5.0,
            .highDpcSpikeWarnCount = 3,
            .highThreadMigrationWarnCount = 1});

        RuntimeValidationSample sample = makeSample();
        sample.metrics.rttJitterMs = 6.0;
        sample.metrics.dpcSpikeCount = 4;
        sample.metrics.threadMigrationCount = 2;
        monitor.observe(sample);

        const RuntimeValidationSummary summary = monitor.summary();
        REQUIRE(summary.highRttJitterCycles == 1, "high RTT counter must increment");
        REQUIRE(summary.highDpcSpikeCycles == 1, "high DPC counter must increment");
        REQUIRE(summary.highThreadMigrationCycles == 1, "high migration counter must increment");
    }
}

int main()
{
    testSummaryCountsNormalCycles();
    testDispatchFailureTripsCriticalFailure();
    testRollbackThresholdTripsCriticalFailure();
    testMissingMainThreadThreshold();
    testHighMetricCounters();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] RuntimeValidationMonitor deterministic tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] RuntimeValidationMonitor deterministic tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
