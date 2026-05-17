// Build: cl /std:c++latest /O2 /W4 /WX /permissive- LatencyDecisionLayer.cpp
// MODULE: LatencyDecisionLayer
// ERROR-POLICY: expected
// Reason: metric thresholds map to SRS PolicyCommand values only.

#include "LatencyDecisionLayer.h"

#include "Logger.h"


namespace
{
    constexpr double kRttJitterTriggerMs = 5.0;
    constexpr double kRttJitterReleaseMs = 3.0;
    constexpr int kDpcSpikeTriggerPerCycle = 3;
    constexpr int kDpcSpikeReleasePerCycle = 1;
    constexpr int kThreadMigrationTriggerPerCycle = 1;
    constexpr int kThreadMigrationReleasePerCycle = 0;

    constexpr std::uint32_t kRttDebounceCycles = 3;
    constexpr std::uint32_t kDpcDebounceCycles = 3;
    constexpr std::uint32_t kMigrationDebounceCycles = 3;

    constexpr auto kBgRestrictCooldown = std::chrono::seconds(10);
    constexpr auto kIrqRepinCooldown = std::chrono::seconds(3);
    constexpr auto kStickyUpCooldown = std::chrono::seconds(5);
}

bool LatencyDecisionLayer::isCooldownElapsed(
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point lastEmission,
    std::chrono::milliseconds cooldown) const noexcept
{
    if (lastEmission == std::chrono::steady_clock::time_point{})
    {
        return true;
    }

    return now - lastEmission >= cooldown;
}

std::expected<PolicyCommandBuffer, ErrorCode> LatencyDecisionLayer::evaluate(const RuntimeMetrics& metrics) noexcept
{
    const auto now = metrics.timestamp == std::chrono::steady_clock::time_point{}
        ? std::chrono::steady_clock::now()
        : metrics.timestamp;

    return evaluate(metrics, now);
}

std::expected<PolicyCommandBuffer, ErrorCode> LatencyDecisionLayer::evaluate(
    const RuntimeMetrics& metrics,
    std::chrono::steady_clock::time_point now) noexcept
{
    PolicyCommandBuffer commands;

    if (metrics.rttJitterMs > kRttJitterTriggerMs)
    {
        ++rttTriggerStreak_;
        if (rttTriggerStreak_ >= kRttDebounceCycles &&
            isCooldownElapsed(now, lastBgRestrictEmission_, kBgRestrictCooldown))
        {
            bgRestrictionActive_ = true;
            lastBgRestrictEmission_ = now;
            Logger::warn(
                "decision: BG_RESTRICT_UP triggered (jitter={:.3f} ms, trigger>{:.3f} ms, streak={}, cooldown={} ms)",
                metrics.rttJitterMs,
                kRttJitterTriggerMs,
                rttTriggerStreak_,
                std::chrono::duration_cast<std::chrono::milliseconds>(kBgRestrictCooldown).count());
            if (!commands.pushBack(PolicyCommand::BgRestrictUp))
            {
                return std::unexpected(ErrorCode::InternalError);
            }
        }
    }
    else if (metrics.rttJitterMs < kRttJitterReleaseMs)
    {
        if (bgRestrictionActive_)
        {
            Logger::info(
                "decision: BG_RESTRICT state released (jitter={:.3f} ms, release<{:.3f} ms)",
                metrics.rttJitterMs,
                kRttJitterReleaseMs);
        }
        bgRestrictionActive_ = false;
        rttTriggerStreak_ = 0;
    }
    else
    {
        // Hysteresis hold zone: keep previous state and avoid resetting the streak.
        Logger::info(
            "decision: RTT jitter {:.3f} ms is inside hysteresis hold zone [{:.3f}, {:.3f}] ms; no BG command emitted",
            metrics.rttJitterMs,
            kRttJitterReleaseMs,
            kRttJitterTriggerMs);
    }

    if (metrics.dpcSpikeCount > kDpcSpikeTriggerPerCycle)
    {
        ++dpcTriggerStreak_;
        if (!metrics.interruptAffinitySupported)
        {
            if (dpcTriggerStreak_ >= kDpcDebounceCycles)
            {
                Logger::warn(
                    "decision: DPC spike count {} exceeded trigger {}, but interrupt affinity is unsupported; IRQ_REPIN suppressed",
                    metrics.dpcSpikeCount,
                    kDpcSpikeTriggerPerCycle);
            }
        }
        else if (dpcTriggerStreak_ >= kDpcDebounceCycles &&
                 isCooldownElapsed(now, lastIrqRepinEmission_, kIrqRepinCooldown))
        {
            irqRepinActive_ = true;
            lastIrqRepinEmission_ = now;
            Logger::warn(
                "decision: IRQ_REPIN triggered (dpc_spikes={}, trigger>{}, streak={}, cooldown={} ms)",
                metrics.dpcSpikeCount,
                kDpcSpikeTriggerPerCycle,
                dpcTriggerStreak_,
                std::chrono::duration_cast<std::chrono::milliseconds>(kIrqRepinCooldown).count());
            if (!commands.pushBack(PolicyCommand::IrqRepin))
            {
                return std::unexpected(ErrorCode::InternalError);
            }
        }
    }
    else if (metrics.dpcSpikeCount <= kDpcSpikeReleasePerCycle)
    {
        if (irqRepinActive_)
        {
            Logger::info(
                "decision: IRQ_REPIN state released (dpc_spikes={}, release<={})",
                metrics.dpcSpikeCount,
                kDpcSpikeReleasePerCycle);
        }
        irqRepinActive_ = false;
        dpcTriggerStreak_ = 0;
    }

    if (metrics.threadMigrationCount > kThreadMigrationTriggerPerCycle)
    {
        ++migrationTriggerStreak_;
        if (migrationTriggerStreak_ >= kMigrationDebounceCycles &&
            isCooldownElapsed(now, lastStickyUpEmission_, kStickyUpCooldown))
        {
            stickyBoostActive_ = true;
            lastStickyUpEmission_ = now;
            Logger::warn(
                "decision: STICKY_UP triggered (migrations={}, trigger>{}, streak={}, cooldown={} ms)",
                metrics.threadMigrationCount,
                kThreadMigrationTriggerPerCycle,
                migrationTriggerStreak_,
                std::chrono::duration_cast<std::chrono::milliseconds>(kStickyUpCooldown).count());
            if (!commands.pushBack(PolicyCommand::StickyUp))
            {
                return std::unexpected(ErrorCode::InternalError);
            }
        }
    }
    else if (metrics.threadMigrationCount <= kThreadMigrationReleasePerCycle)
    {
        if (stickyBoostActive_)
        {
            Logger::info(
                "decision: STICKY_UP state released (migrations={}, release<={})",
                metrics.threadMigrationCount,
                kThreadMigrationReleasePerCycle);
        }
        stickyBoostActive_ = false;
        migrationTriggerStreak_ = 0;
    }

    return commands;
}
