// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- AppliedPolicyTrackerTests.cpp AppliedPolicyTracker.cpp
// MODULE: AppliedPolicyTrackerTests
// PURPOSE: deterministic tests for applied-policy feedback, improvement, regression filtering, duplicate arm suppression, and rollback request.

#include "AppliedPolicyTracker.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
    using Clock = std::chrono::steady_clock;

    int gFailureCount = 0;

    void recordFailure(const char* file, int line, std::string_view message) noexcept
    {
        std::cerr << "[FAIL] " << file << ':' << line << " - " << message << '\n';
        ++gFailureCount;
    }

#define REQUIRE(condition, message) \
    do \
    { \
        if (!(condition)) \
        { \
            recordFailure(__FILE__, __LINE__, (message)); \
        } \
    } while (false)

    [[nodiscard]] bool containsCommand(const PolicyCommandBuffer& commands, PolicyCommand expected) noexcept
    {
        for (const PolicyCommand command : commands)
        {
            if (command == expected)
            {
                return true;
            }
        }
        return false;
    }

    RuntimeMetrics makeMetrics(Clock::time_point now) noexcept
    {
        RuntimeMetrics metrics{};
        metrics.timestamp = now;
        return metrics;
    }

    void testBgRestrictImprovementDisarmsTracker()
    {
        AppliedPolicyTracker tracker(AppliedPolicyTrackerConfig{
            .evaluationWindowCycles = 3,
            .regressionRequiredCycles = 2,
            .minimumRttImprovementMs = 0.5,
            .acceptableRttJitterMs = 5.0});

        const auto t0 = Clock::time_point{} + std::chrono::seconds(100);
        auto baseline = makeMetrics(t0);
        baseline.rttJitterMs = 6.0;

        tracker.recordDispatchResult(PolicyCommand::BgRestrictUp, baseline, true, t0);

        auto current = makeMetrics(t0 + std::chrono::seconds(3));
        current.rttJitterMs = 4.8;

        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "BG_RESTRICT_UP feedback must pass only when it improves and reaches acceptable RTT range");

        current.rttJitterMs = 8.0;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "tracker must be disarmed after successful improvement");
    }

    void testBgRestrictImprovedButStillHighDoesNotPass()
    {
        AppliedPolicyTracker tracker(AppliedPolicyTrackerConfig{
            .evaluationWindowCycles = 2,
            .regressionRequiredCycles = 2,
            .minimumRttImprovementMs = 0.5,
            .acceptableRttJitterMs = 5.0,
            .rttRegressionToleranceMs = 0.5,
            .rollbackOnPolicyRegression = true});

        const auto t0 = Clock::time_point{} + std::chrono::seconds(150);
        auto baseline = makeMetrics(t0);
        baseline.rttJitterMs = 29.8;

        tracker.recordDispatchResult(PolicyCommand::BgRestrictUp, baseline, true, t0);

        auto current = makeMetrics(t0 + std::chrono::seconds(3));
        current.rttJitterMs = 28.6;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "RTT that improves but remains above acceptable range must not be treated as feedback pass");

        current.timestamp = t0 + std::chrono::seconds(6);
        current.rttJitterMs = 28.5;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "improved but still-high RTT should end as inconclusive without rollback when it is not regression");

        current.timestamp = t0 + std::chrono::seconds(9);
        current.rttJitterMs = 40.0;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "tracker must be disarmed after improved-but-still-high inconclusive result");
    }

    void testBgRestrictSingleSpikeDoesNotRollback()
    {
        AppliedPolicyTracker tracker(AppliedPolicyTrackerConfig{
            .evaluationWindowCycles = 3,
            .regressionRequiredCycles = 2,
            .minimumRttImprovementMs = 0.5,
            .rttRegressionToleranceMs = 0.5,
            .rollbackOnPolicyRegression = true});

        const auto t0 = Clock::time_point{} + std::chrono::seconds(200);
        auto baseline = makeMetrics(t0);
        baseline.rttJitterMs = 6.0;

        tracker.recordDispatchResult(PolicyCommand::BgRestrictUp, baseline, true, t0);

        auto current = makeMetrics(t0 + std::chrono::seconds(3));
        current.rttJitterMs = 6.8;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "single RTT regression sample must not request rollback before persistence is proven");

        current.timestamp = t0 + std::chrono::seconds(6);
        current.rttJitterMs = 6.1;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "neutral RTT sample must reset regression persistence");

        current.timestamp = t0 + std::chrono::seconds(9);
        current.rttJitterMs = 6.2;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "inconclusive feedback must disarm without rollback when persistent regression is absent");
    }

    void testBgRestrictPersistentRegressionRequestsRollbackAfterWindow()
    {
        AppliedPolicyTracker tracker(AppliedPolicyTrackerConfig{
            .evaluationWindowCycles = 3,
            .regressionRequiredCycles = 2,
            .minimumRttImprovementMs = 0.5,
            .rttRegressionToleranceMs = 0.5,
            .rollbackOnPolicyRegression = true});

        const auto t0 = Clock::time_point{} + std::chrono::seconds(300);
        auto baseline = makeMetrics(t0);
        baseline.rttJitterMs = 6.0;

        tracker.recordDispatchResult(PolicyCommand::BgRestrictUp, baseline, true, t0);

        auto current = makeMetrics(t0 + std::chrono::seconds(3));
        current.rttJitterMs = 6.7;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "rollback must not trigger before evaluation window completes");

        current.timestamp = t0 + std::chrono::seconds(6);
        current.rttJitterMs = 6.8;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "persistent regression must wait until evaluation window completes");

        current.timestamp = t0 + std::chrono::seconds(9);
        current.rttJitterMs = 6.9;
        REQUIRE(containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "persistent BG_RESTRICT_UP regression must request rollback after evaluation window");
    }

    void testDuplicateArmIsIgnoredWhilePending()
    {
        AppliedPolicyTracker tracker(AppliedPolicyTrackerConfig{
            .evaluationWindowCycles = 2,
            .regressionRequiredCycles = 2,
            .minimumRttImprovementMs = 0.5,
            .rttRegressionToleranceMs = 0.5});

        const auto t0 = Clock::time_point{} + std::chrono::seconds(400);
        auto firstBaseline = makeMetrics(t0);
        firstBaseline.rttJitterMs = 6.0;
        tracker.recordDispatchResult(PolicyCommand::BgRestrictUp, firstBaseline, true, t0);

        auto secondBaseline = makeMetrics(t0 + std::chrono::seconds(1));
        secondBaseline.rttJitterMs = 20.0;
        tracker.recordDispatchResult(PolicyCommand::BgRestrictUp, secondBaseline, true, secondBaseline.timestamp);

        auto current = makeMetrics(t0 + std::chrono::seconds(3));
        current.rttJitterMs = 5.4;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "duplicate arm must not replace the original baseline while feedback is pending");
    }

    void testFailedDispatchIsNotTracked()
    {
        AppliedPolicyTracker tracker(AppliedPolicyTrackerConfig{
            .evaluationWindowCycles = 1,
            .regressionRequiredCycles = 1,
            .minimumRttImprovementMs = 0.5});

        const auto t0 = Clock::time_point{} + std::chrono::seconds(500);
        auto baseline = makeMetrics(t0);
        baseline.rttJitterMs = 6.0;

        tracker.recordDispatchResult(PolicyCommand::BgRestrictUp, baseline, false, t0);

        auto current = makeMetrics(t0 + std::chrono::seconds(3));
        current.rttJitterMs = 9.0;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "failed dispatch must not arm feedback rollback");
    }

    void testStickyUpImprovementDisarmsTracker()
    {
        AppliedPolicyTracker tracker(AppliedPolicyTrackerConfig{
            .evaluationWindowCycles = 2,
            .regressionRequiredCycles = 2,
            .minimumMigrationImprovementCount = 1});

        const auto t0 = Clock::time_point{} + std::chrono::seconds(600);
        auto baseline = makeMetrics(t0);
        baseline.threadMigrationCount = 2;

        tracker.recordDispatchResult(PolicyCommand::StickyUp, baseline, true, t0);

        auto current = makeMetrics(t0 + std::chrono::seconds(3));
        current.threadMigrationCount = 1;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "improved STICKY_UP feedback must not request rollback");
    }

    void testIrqRepinUnsupportedIsNotTracked()
    {
        AppliedPolicyTracker tracker(AppliedPolicyTrackerConfig{.evaluationWindowCycles = 1});

        const auto t0 = Clock::time_point{} + std::chrono::seconds(700);
        auto baseline = makeMetrics(t0);
        baseline.dpcSpikeCount = 4;
        baseline.interruptAffinitySupported = false;

        tracker.recordDispatchResult(PolicyCommand::IrqRepin, baseline, true, t0);

        auto current = makeMetrics(t0 + std::chrono::seconds(3));
        current.dpcSpikeCount = 8;
        current.interruptAffinitySupported = false;
        REQUIRE(!containsCommand(tracker.evaluate(current, current.timestamp), PolicyCommand::Rollback),
            "unsupported IRQ_REPIN must not be tracked because no actual feedback path exists");
    }
}

int main()
{
    testBgRestrictImprovementDisarmsTracker();
    testBgRestrictImprovedButStillHighDoesNotPass();
    testBgRestrictSingleSpikeDoesNotRollback();
    testBgRestrictPersistentRegressionRequestsRollbackAfterWindow();
    testDuplicateArmIsIgnoredWhilePending();
    testFailedDispatchIsNotTracked();
    testStickyUpImprovementDisarmsTracker();
    testIrqRepinUnsupportedIsNotTracked();

    if (gFailureCount == 0)
    {
        std::cout << "[PASS] AppliedPolicyTracker deterministic tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] AppliedPolicyTracker deterministic tests failed: "
              << gFailureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
