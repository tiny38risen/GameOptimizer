// Build example (MSVC):
// cl /std:c++latest /W4 /WX /permissive- LatencyDecisionLayerTests.cpp LatencyDecisionLayer.cpp
// MODULE: LatencyDecisionLayerTests
// ERROR-POLICY: exceptions
// Reason: test harness reports accumulated failures and uses process exit codes.
// PURPOSE: deterministic tests for hysteresis, debounce, and cooldown using injected time.

#include "LatencyDecisionLayer.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <print>
#include <string_view>

namespace
{
    using Clock = std::chrono::steady_clock;

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

    [[nodiscard]] PolicyCommandBuffer commandsOrEmpty(
        const std::expected<PolicyCommandBuffer, ErrorCode>& result) noexcept
    {
        return result ? *result : PolicyCommandBuffer{};
    }

    int g_failureCount = 0;

    void recordFailure(const char* file, int line, std::string_view message) noexcept
    {
        std::print(stderr, "[FAIL] {}:{} - {}\n", file, line, message);
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

    RuntimeMetrics makeMetrics(Clock::time_point now) noexcept
    {
        RuntimeMetrics metrics{};
        metrics.timestamp = now;
        return metrics;
    }

    void testBgRestrictDebounceAndCooldown()
    {
        LatencyDecisionLayer layer;
        const auto t0 = Clock::time_point{} + std::chrono::seconds(100);

        auto metrics = makeMetrics(t0);
        metrics.rttJitterMs = 6.0;

        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0)), PolicyCommand::BgRestrictUp),
            "BG_RESTRICT_UP must not trigger on first high RTT sample");
        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(250))), PolicyCommand::BgRestrictUp),
            "BG_RESTRICT_UP must not trigger on second high RTT sample");
        REQUIRE(containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(500))), PolicyCommand::BgRestrictUp),
            "BG_RESTRICT_UP must trigger on third high RTT sample");

        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::seconds(1))), PolicyCommand::BgRestrictUp),
            "BG_RESTRICT_UP must be suppressed during cooldown");
        REQUIRE(containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::seconds(11))), PolicyCommand::BgRestrictUp),
            "BG_RESTRICT_UP must be allowed after cooldown elapses");
    }

    void testRttReleaseResetsDebounce()
    {
        LatencyDecisionLayer layer;
        const auto t0 = Clock::time_point{} + std::chrono::seconds(200);

        auto metrics = makeMetrics(t0);
        metrics.rttJitterMs = 6.0;
        (void)layer.evaluate(metrics, t0);
        (void)layer.evaluate(metrics, t0 + std::chrono::milliseconds(250));

        metrics.rttJitterMs = 2.0;
        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(500))), PolicyCommand::BgRestrictUp),
            "RTT release zone must not emit BG_RESTRICT_UP");

        metrics.rttJitterMs = 6.0;
        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(750))), PolicyCommand::BgRestrictUp),
            "debounce must restart after release");
    }

    void testRttHysteresisHoldDoesNotEmit()
    {
        LatencyDecisionLayer layer;
        const auto t0 = Clock::time_point{} + std::chrono::seconds(300);

        auto metrics = makeMetrics(t0);
        metrics.rttJitterMs = 4.0;
        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0)), PolicyCommand::BgRestrictUp),
            "RTT hysteresis hold zone must not emit command");
    }

    void testIrqRepinSuppressedWhenUnsupported()
    {
        LatencyDecisionLayer layer;
        const auto t0 = Clock::time_point{} + std::chrono::seconds(400);

        auto metrics = makeMetrics(t0);
        metrics.dpcSpikeCount = 4;
        metrics.interruptAffinitySupported = false;

        (void)layer.evaluate(metrics, t0);
        (void)layer.evaluate(metrics, t0 + std::chrono::milliseconds(250));
        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(500))), PolicyCommand::IrqRepin),
            "IRQ_REPIN must be suppressed when interrupt affinity is unsupported");
    }

    void testIrqRepinDebounceAndCooldown()
    {
        LatencyDecisionLayer layer;
        const auto t0 = Clock::time_point{} + std::chrono::seconds(500);

        auto metrics = makeMetrics(t0);
        metrics.dpcSpikeCount = 4;
        metrics.interruptAffinitySupported = true;

        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0)), PolicyCommand::IrqRepin),
            "IRQ_REPIN must not trigger on first DPC spike sample");
        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(250))), PolicyCommand::IrqRepin),
            "IRQ_REPIN must not trigger on second DPC spike sample");
        REQUIRE(containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(500))), PolicyCommand::IrqRepin),
            "IRQ_REPIN must trigger on third DPC spike sample");

        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::seconds(1))), PolicyCommand::IrqRepin),
            "IRQ_REPIN must be suppressed during cooldown");
        REQUIRE(containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::seconds(4))), PolicyCommand::IrqRepin),
            "IRQ_REPIN must be allowed after cooldown elapses");
    }

    void testStickyUpDebounceAndCooldown()
    {
        LatencyDecisionLayer layer;
        const auto t0 = Clock::time_point{} + std::chrono::seconds(600);

        auto metrics = makeMetrics(t0);
        metrics.threadMigrationCount = 2;

        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0)), PolicyCommand::StickyUp),
            "STICKY_UP must not trigger on first migration sample");
        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(250))), PolicyCommand::StickyUp),
            "STICKY_UP must not trigger on second migration sample");
        REQUIRE(containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::milliseconds(500))), PolicyCommand::StickyUp),
            "STICKY_UP must trigger on third migration sample");

        REQUIRE(!containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::seconds(2))), PolicyCommand::StickyUp),
            "STICKY_UP must be suppressed during cooldown");
        REQUIRE(containsCommand(commandsOrEmpty(layer.evaluate(metrics, t0 + std::chrono::seconds(6))), PolicyCommand::StickyUp),
            "STICKY_UP must be allowed after cooldown elapses");
    }
}

int main()
{
    testBgRestrictDebounceAndCooldown();
    testRttReleaseResetsDebounce();
    testRttHysteresisHoldDoesNotEmit();
    testIrqRepinSuppressedWhenUnsupported();
    testIrqRepinDebounceAndCooldown();
    testStickyUpDebounceAndCooldown();

    if (g_failureCount == 0)
    {
        std::print(stdout, "[PASS] LatencyDecisionLayer deterministic tests completed\n");
        return EXIT_SUCCESS;
    }

    std::print(stderr, "[FAIL] LatencyDecisionLayer deterministic tests failed: {} failure(s)\n", g_failureCount);
    return EXIT_FAILURE;
}
