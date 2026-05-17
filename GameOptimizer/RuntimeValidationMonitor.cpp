// Build: cl /std:c++latest /O2 /W4 /WX /permissive- RuntimeValidationMonitor.cpp
// MODULE: RuntimeValidationMonitor
// ERROR-POLICY: no-throw
// Reason: validation telemetry must never interrupt watchdog execution.

#include "RuntimeValidationMonitor.h"

#include "Logger.h"

RuntimeValidationMonitor::RuntimeValidationMonitor(RuntimeValidationMonitorConfig config) noexcept
    : config_(config)
{
    if (config_.minimumCyclesForSummary == 0)
    {
        config_.minimumCyclesForSummary = 1;
    }
}

void RuntimeValidationMonitor::observe(const RuntimeValidationSample& sample) noexcept
{
    ++summary_.observedCycles;
    summary_.totalDecisionCommands += sample.decisionCommandCount;
    summary_.totalFeedbackCommands += sample.feedbackCommandCount;
    summary_.totalDispatchFailures += sample.dispatchFailureCount;

    if (sample.rollbackRequested)
    {
        ++summary_.totalRollbackRequests;
    }

    if (sample.mainThreadDetected)
    {
        ++summary_.cyclesWithMainThread;
        summary_.consecutiveNoMainThreadCycles = 0;
    }
    else
    {
        ++summary_.consecutiveNoMainThreadCycles;
    }

    if (sample.mainThreadPolicyApplied)
    {
        ++summary_.cyclesWithAppliedMainPolicy;
    }

    if (sample.metrics.rttJitterMs > config_.highRttJitterWarnMs)
    {
        ++summary_.highRttJitterCycles;
        Logger::warn(
            "runtime validation: high RTT jitter observed (jitter={:.3f} ms, threshold>{:.3f} ms, cycle={})",
            sample.metrics.rttJitterMs,
            config_.highRttJitterWarnMs,
            summary_.observedCycles);
    }

    if (sample.metrics.dpcSpikeCount > config_.highDpcSpikeWarnCount)
    {
        ++summary_.highDpcSpikeCycles;
        Logger::warn(
            "runtime validation: high DPC spike count observed (dpc_spikes={}, threshold>{}, cycle={})",
            sample.metrics.dpcSpikeCount,
            config_.highDpcSpikeWarnCount,
            summary_.observedCycles);
    }

    if (sample.metrics.threadMigrationCount > config_.highThreadMigrationWarnCount)
    {
        ++summary_.highThreadMigrationCycles;
        Logger::warn(
            "runtime validation: high thread migration count observed (migrations={}, threshold>{}, cycle={})",
            sample.metrics.threadMigrationCount,
            config_.highThreadMigrationWarnCount,
            summary_.observedCycles);
    }

    if (sample.dispatchFailureCount > 0)
    {
        Logger::error(
            "runtime validation: dispatch failure observed (failures_this_cycle={}, total_failures={})",
            sample.dispatchFailureCount,
            summary_.totalDispatchFailures);
    }

    if (sample.rollbackRequested)
    {
        Logger::warn(
            "runtime validation: rollback request observed (total_rollback_requests={})",
            summary_.totalRollbackRequests);
    }

    updateCriticalFailureState();

    Logger::info(
        "runtime validation sample: cycle={}, main_detected={}, main_policy_applied={}, decision_commands={}, feedback_commands={}, dispatch_failures={}, rollback_requested={}, rtt_jitter={:.3f} ms, dpc_spikes={}, migrations={}",
        summary_.observedCycles,
        sample.mainThreadDetected,
        sample.mainThreadPolicyApplied,
        sample.decisionCommandCount,
        sample.feedbackCommandCount,
        sample.dispatchFailureCount,
        sample.rollbackRequested,
        sample.metrics.rttJitterMs,
        sample.metrics.dpcSpikeCount,
        sample.metrics.threadMigrationCount);
}

void RuntimeValidationMonitor::logSummary() const noexcept
{
    const bool passedMinimumCycles = summary_.observedCycles >= config_.minimumCyclesForSummary;

    Logger::info(
        "runtime validation summary: cycles={}, minimum_required={}, minimum_satisfied={}, main_detected_cycles={}, main_policy_applied_cycles={}, decision_commands={}, feedback_commands={}, dispatch_failures={}, rollback_requests={}, high_rtt_cycles={}, high_dpc_cycles={}, high_migration_cycles={}, consecutive_no_main_cycles={}, critical_failure={}",
        summary_.observedCycles,
        config_.minimumCyclesForSummary,
        passedMinimumCycles,
        summary_.cyclesWithMainThread,
        summary_.cyclesWithAppliedMainPolicy,
        summary_.totalDecisionCommands,
        summary_.totalFeedbackCommands,
        summary_.totalDispatchFailures,
        summary_.totalRollbackRequests,
        summary_.highRttJitterCycles,
        summary_.highDpcSpikeCycles,
        summary_.highThreadMigrationCycles,
        summary_.consecutiveNoMainThreadCycles,
        summary_.criticalFailureDetected);

    if (!passedMinimumCycles)
    {
        Logger::warn(
            "runtime validation summary is preliminary because observed cycles {} < required cycles {}",
            summary_.observedCycles,
            config_.minimumCyclesForSummary);
    }

    if (summary_.criticalFailureDetected)
    {
        Logger::error("runtime validation result: FAILED");
        return;
    }

    Logger::info("runtime validation result: PASSED_OR_INCONCLUSIVE");
}

RuntimeValidationSummary RuntimeValidationMonitor::summary() const noexcept
{
    RuntimeValidationSummary result = summary_;
    result.minimumCycleCountSatisfied = result.observedCycles >= config_.minimumCyclesForSummary;
    return result;
}

bool RuntimeValidationMonitor::hasCriticalFailure() const noexcept
{
    return summary_.criticalFailureDetected;
}

void RuntimeValidationMonitor::updateCriticalFailureState() noexcept
{
    summary_.minimumCycleCountSatisfied = summary_.observedCycles >= config_.minimumCyclesForSummary;

    if (summary_.totalDispatchFailures > config_.maxDispatchFailures)
    {
        summary_.criticalFailureDetected = true;
    }

    if (summary_.totalRollbackRequests > config_.maxRollbackRequests)
    {
        summary_.criticalFailureDetected = true;
    }

    if (summary_.consecutiveNoMainThreadCycles > config_.maxConsecutiveNoMainThreadCycles)
    {
        summary_.criticalFailureDetected = true;
    }
}
