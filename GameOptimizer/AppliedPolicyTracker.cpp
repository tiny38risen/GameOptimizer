// Build: cl /std:c++latest /O2 /W4 /WX /permissive- AppliedPolicyTracker.cpp
// MODULE: AppliedPolicyTracker
// ERROR-POLICY: expected
// Reason: applied policy feedback must be bounded and side-effect free until Rollback is dispatched.

#include "AppliedPolicyTracker.h"

#include "Logger.h"

AppliedPolicyTracker::AppliedPolicyTracker(AppliedPolicyTrackerConfig config) noexcept
    : config_(config)
{
    if (config_.evaluationWindowCycles == 0)
    {
        config_.evaluationWindowCycles = 1;
    }

    if (config_.regressionRequiredCycles == 0)
    {
        config_.regressionRequiredCycles = 1;
    }

    if (config_.regressionRequiredCycles > config_.evaluationWindowCycles)
    {
        config_.regressionRequiredCycles = config_.evaluationWindowCycles;
    }

    if (config_.minimumRttImprovementMs < 0.0)
    {
        config_.minimumRttImprovementMs = 0.0;
    }

    if (config_.acceptableRttJitterMs < 0.0)
    {
        config_.acceptableRttJitterMs = 0.0;
    }

    if (config_.rttRegressionToleranceMs < 0.0)
    {
        config_.rttRegressionToleranceMs = 0.0;
    }

    if (config_.countRegressionTolerance < 0)
    {
        config_.countRegressionTolerance = 0;
    }

    if (config_.rearmCooldown < std::chrono::milliseconds(0))
    {
        config_.rearmCooldown = std::chrono::milliseconds(0);
    }
}

void AppliedPolicyTracker::recordDispatchResult(
    PolicyCommand command,
    const RuntimeMetrics& baselineMetrics,
    bool dispatchSucceeded,
    std::chrono::steady_clock::time_point now) noexcept
{
    if (command == PolicyCommand::Rollback)
    {
        clear();
        rollbackAlreadyRequested_ = true;
        Logger::warn("policy feedback tracker cleared after ROLLBACK dispatch");
        return;
    }

    if (!dispatchSucceeded)
    {
        Logger::warn(
            "policy feedback tracker ignored failed dispatch result: command={}",
            toString(command));
        return;
    }

    if (!shouldTrack(command, baselineMetrics))
    {
        Logger::info(
            "policy feedback tracker skipped command={} because it has no measurable active feedback path",
            toString(command));
        return;
    }

    if (lastCompletedCommand_ == command &&
        lastCompletedAt_ != std::chrono::steady_clock::time_point{} &&
        now - lastCompletedAt_ < config_.rearmCooldown)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            config_.rearmCooldown - (now - lastCompletedAt_));
        Logger::info(
            "policy feedback tracker suppressed rearm by cooldown: command={}, remaining={} ms",
            toString(command),
            remaining.count() > 0 ? remaining.count() : 0);
        return;
    }

    if (pendingSnapshot_.has_value())
    {
        Logger::info(
            "policy feedback tracker kept active command={} and ignored new command={} until the current feedback window completes",
            toString(pendingSnapshot_->command),
            toString(command));
        return;
    }

    pendingSnapshot_ = AppliedPolicySnapshot{
        .command = command,
        .baselineMetrics = baselineMetrics,
        .appliedAt = now,
        .observedCycles = 0,
        .regressionCycles = 0,
        .dispatchSucceeded = dispatchSucceeded};

    rollbackAlreadyRequested_ = false;

    Logger::info(
        "policy feedback tracker armed: command={}, baseline_rtt={:.3f} ms, acceptable_rtt<={:.3f} ms, baseline_dpc={}, baseline_migrations={}, window_cycles={}, regression_required_cycles={}",
        toString(command),
        baselineMetrics.rttJitterMs,
        config_.acceptableRttJitterMs,
        baselineMetrics.dpcSpikeCount,
        baselineMetrics.threadMigrationCount,
        config_.evaluationWindowCycles,
        config_.regressionRequiredCycles);
}

PolicyCommandBuffer AppliedPolicyTracker::evaluate(
    const RuntimeMetrics& currentMetrics,
    std::chrono::steady_clock::time_point now) noexcept
{
    PolicyCommandBuffer commands;

    if (!pendingSnapshot_.has_value() || rollbackAlreadyRequested_)
    {
        return commands;
    }

    AppliedPolicySnapshot& snapshot = *pendingSnapshot_;
    ++snapshot.observedCycles;

    if (hasImproved(snapshot, currentMetrics))
    {
        Logger::info(
            "policy feedback passed: command={}, observed_cycles={}, current_rtt={:.3f} ms, current_dpc={}, current_migrations={}",
            toString(snapshot.command),
            snapshot.observedCycles,
            currentMetrics.rttJitterMs,
            currentMetrics.dpcSpikeCount,
            currentMetrics.threadMigrationCount);
        lastCompletedCommand_ = snapshot.command;
        lastCompletedAt_ = now;
        pendingSnapshot_.reset();
        return commands;
    }

    if (hasRegressed(snapshot, currentMetrics))
    {
        ++snapshot.regressionCycles;
        Logger::warn(
            "policy feedback regression sample: command={}, regression_cycles={}/{}, observed_cycles={}/{}, baseline_rtt={:.3f} ms, current_rtt={:.3f} ms, baseline_dpc={}, current_dpc={}, baseline_migrations={}, current_migrations={}",
            toString(snapshot.command),
            snapshot.regressionCycles,
            config_.regressionRequiredCycles,
            snapshot.observedCycles,
            config_.evaluationWindowCycles,
            snapshot.baselineMetrics.rttJitterMs,
            currentMetrics.rttJitterMs,
            snapshot.baselineMetrics.dpcSpikeCount,
            currentMetrics.dpcSpikeCount,
            snapshot.baselineMetrics.threadMigrationCount,
            currentMetrics.threadMigrationCount);
    }
    else
    {
        snapshot.regressionCycles = 0;
    }

    if (snapshot.observedCycles < config_.evaluationWindowCycles)
    {
        Logger::info(
            "policy feedback pending: command={}, observed_cycles={}/{}, current_rtt={:.3f} ms, current_dpc={}, current_migrations={}",
            toString(snapshot.command),
            snapshot.observedCycles,
            config_.evaluationWindowCycles,
            currentMetrics.rttJitterMs,
            currentMetrics.dpcSpikeCount,
            currentMetrics.threadMigrationCount);
        return commands;
    }

    if (config_.rollbackOnPolicyRegression && snapshot.regressionCycles >= config_.regressionRequiredCycles)
    {
        Logger::error(
            "policy feedback persistent regression detected: command={}, regression_cycles={}, baseline_rtt={:.3f} ms, current_rtt={:.3f} ms, baseline_dpc={}, current_dpc={}, baseline_migrations={}, current_migrations={}; requesting ROLLBACK",
            toString(snapshot.command),
            snapshot.regressionCycles,
            snapshot.baselineMetrics.rttJitterMs,
            currentMetrics.rttJitterMs,
            snapshot.baselineMetrics.dpcSpikeCount,
            currentMetrics.dpcSpikeCount,
            snapshot.baselineMetrics.threadMigrationCount,
            currentMetrics.threadMigrationCount);
        (void)commands.pushBack(PolicyCommand::Rollback);
        rollbackAlreadyRequested_ = true;
        lastCompletedCommand_ = snapshot.command;
        lastCompletedAt_ = now;
        pendingSnapshot_.reset();
        return commands;
    }

    Logger::warn(
        "policy feedback inconclusive: command={} did not meet improvement threshold within {} cycle(s); tracker disarmed without rollback",
        toString(snapshot.command),
        config_.evaluationWindowCycles);
    lastCompletedCommand_ = snapshot.command;
    lastCompletedAt_ = now;
    pendingSnapshot_.reset();
    return commands;
}

void AppliedPolicyTracker::clear() noexcept
{
    pendingSnapshot_.reset();
    rollbackAlreadyRequested_ = false;
    lastCompletedCommand_ = PolicyCommand::None;
    lastCompletedAt_ = std::chrono::steady_clock::time_point{};
}

bool AppliedPolicyTracker::shouldTrack(PolicyCommand command, const RuntimeMetrics& metrics) const noexcept
{
    switch (command)
    {
    case PolicyCommand::BgRestrictUp:
        return metrics.rttJitterMs > 0.0;
    case PolicyCommand::IrqRepin:
        return metrics.interruptAffinitySupported && metrics.dpcSpikeCount > 0;
    case PolicyCommand::StickyUp:
        return metrics.threadMigrationCount > 0;
    case PolicyCommand::None:
    case PolicyCommand::Rollback:
        return false;
    }

    return false;
}

bool AppliedPolicyTracker::hasImproved(
    const AppliedPolicySnapshot& snapshot,
    const RuntimeMetrics& currentMetrics) const noexcept
{
    switch (snapshot.command)
    {
    case PolicyCommand::BgRestrictUp:
    {
        const bool improvedEnough =
            snapshot.baselineMetrics.rttJitterMs - currentMetrics.rttJitterMs >= config_.minimumRttImprovementMs;
        const bool reachedAcceptableRange =
            currentMetrics.rttJitterMs <= config_.acceptableRttJitterMs;
        return improvedEnough && reachedAcceptableRange;
    }
    case PolicyCommand::IrqRepin:
        return snapshot.baselineMetrics.dpcSpikeCount - currentMetrics.dpcSpikeCount >= config_.minimumDpcImprovementCount;
    case PolicyCommand::StickyUp:
        return snapshot.baselineMetrics.threadMigrationCount - currentMetrics.threadMigrationCount >= config_.minimumMigrationImprovementCount;
    case PolicyCommand::None:
    case PolicyCommand::Rollback:
        return false;
    }

    return false;
}

bool AppliedPolicyTracker::hasRegressed(
    const AppliedPolicySnapshot& snapshot,
    const RuntimeMetrics& currentMetrics) const noexcept
{
    switch (snapshot.command)
    {
    case PolicyCommand::BgRestrictUp:
        return currentMetrics.rttJitterMs - snapshot.baselineMetrics.rttJitterMs > config_.rttRegressionToleranceMs;
    case PolicyCommand::IrqRepin:
        return currentMetrics.dpcSpikeCount - snapshot.baselineMetrics.dpcSpikeCount > config_.countRegressionTolerance;
    case PolicyCommand::StickyUp:
        return currentMetrics.threadMigrationCount - snapshot.baselineMetrics.threadMigrationCount > config_.countRegressionTolerance;
    case PolicyCommand::None:
    case PolicyCommand::Rollback:
        return false;
    }

    return false;
}
