// Build: cl /std:c++latest /O2 /W4 /WX /permissive- WatchdogCycleRunner.cpp
// MODULE: WatchdogCycleRunner
// ERROR-POLICY: expected
// Reason: watchdog cycle errors are observed and logged without collapsing runtime ownership.

#include "WatchdogCycleRunner.h"

#include <chrono>

#include "ErrorCode.h"
#include "Logger.h"
#include "PolicyCommand.h"
#include "ThreadInfo.h"

namespace
{
    void logObservedThreads(const ThreadInfoBuffer& threads) noexcept
    {
        constexpr double kFileTimeUnitsPerMillisecond = 10000.0;

        Logger::info("observed top thread count: {}", threads.size());

        for (const ThreadInfo& thread : threads)
        {
            const double deltaMilliseconds =
                static_cast<double>(thread.deltaCpuTime100ns) / kFileTimeUnitsPerMillisecond;
            const double emaMilliseconds =
                thread.emaCpuTime100ns / kFileTimeUnitsPerMillisecond;

            Logger::info(
                "TID: {} | avg-sample-cpu: {:.3f} ms | EMA-sample-cpu: {:.3f} ms | candidate: {} | main: {}",
                thread.threadId,
                deltaMilliseconds,
                emaMilliseconds,
                thread.isEmaCandidate,
                thread.isMainThread);
        }
    }
}

WatchdogCycleRunner::WatchdogCycleRunner(
    RuntimeContext& context,
    std::atomic_bool& runtimeTimeoutRequested,
    RuntimeShutdownRequest requestShutdown) noexcept
    : context_(context),
      runtimeTimeoutRequested_(runtimeTimeoutRequested),
      requestShutdown_(requestShutdown)
{
}

void WatchdogCycleRunner::runCycle(std::stop_token stopToken) noexcept
{
    if (!updateThreads(stopToken))
    {
        return;
    }

    logThreadDetailsIfNeeded();
    logDecisionState();

    const auto mainThreadId = context_.threadTracker->getMainThreadId();
    applyMainThreadPolicyIfNeeded(mainThreadId);
    reconcilePolicyIfNeeded(mainThreadId);

    RuntimeMetrics runtimeMetrics = collectMetrics();
    RuntimeValidationSample validationSample = makeValidationSample(runtimeMetrics, mainThreadId);

    if (!dispatchFeedbackCommands(validationSample, runtimeMetrics))
    {
        return;
    }

    dispatchDecisionCommands(validationSample, runtimeMetrics);

    context_.runtimeValidationMonitor->observe(validationSample);

    if (runtimeTimeoutRequested_.load(std::memory_order_acquire))
    {
        Logger::info("runtime timeout reached at watchdog cycle boundary; requesting clean shutdown");
        if (requestShutdown_ != nullptr)
        {
            requestShutdown_(ShutdownReason::MaxRuntimeSoftTimeout);
        }
    }
}

bool WatchdogCycleRunner::updateThreads(std::stop_token stopToken) noexcept
{
    threadTrackerResetEventThisCycle_ = false;
    const auto updateResult = context_.threadTracker->update(stopToken);
    if (!updateResult)
    {
        Logger::warn("thread update failed: {}; retrying next cycle", toString(updateResult.error()));
        return false;
    }

    const auto disposition = *updateResult;
    if (disposition == ThreadTrackerUpdateDisposition::ResetAfterInvariantFailure)
    {
        threadTrackerResetEventThisCycle_ = true;
        Logger::error("thread tracker reported a reset event; runtime evidence will retain the reset disposition");
    }
    else if (disposition == ThreadTrackerUpdateDisposition::StopRequested)
    {
        Logger::info("thread tracker update stopped by shutdown request");
        return false;
    }

    return true;
}

void WatchdogCycleRunner::logThreadDetailsIfNeeded() noexcept
{
    bool shouldLogThreadDetails = context_.options.runtimeLogConfig.threadDetailLogEnabled;
    if (shouldLogThreadDetails)
    {
        ++context_.threadDetailLogCycleCounter;
        if (context_.threadDetailLogCycleCounter < context_.options.runtimeLogConfig.threadDetailLogIntervalCycles)
        {
            shouldLogThreadDetails = false;
        }
        else
        {
            context_.threadDetailLogCycleCounter = 0;
        }
    }

    if (!shouldLogThreadDetails)
    {
        return;
    }

    const auto topThreads = context_.threadTracker->getTopThreads(context_.startupPlan.maxDisplayedThreads);
    if (!topThreads)
    {
        Logger::warn("thread data collection failed: {}; retrying next cycle", toString(topThreads.error()));
        return;
    }

    const auto& observedThreads = *topThreads;
    logObservedThreads(observedThreads);
}

void WatchdogCycleRunner::logDecisionState() const noexcept
{
    const auto candidateThreadId = context_.threadTracker->getEmaCandidateThreadId();
    const auto mainThreadId = context_.threadTracker->getMainThreadId();

    if (candidateThreadId.has_value())
    {
        Logger::info("EMA candidate TID: {}", candidateThreadId.value());
    }
    else
    {
        Logger::info("EMA candidate TID: none");
    }

    if (mainThreadId.has_value())
    {
        Logger::info("confirmed main TID: {}", mainThreadId.value());
    }
    else
    {
        Logger::info("confirmed main TID: none");
    }
}

void WatchdogCycleRunner::applyMainThreadPolicyIfNeeded(std::optional<DWORD> mainThreadId) noexcept
{
    if (!mainThreadId.has_value())
    {
        return;
    }

    const DWORD currentMainThreadId = mainThreadId.value();
    const auto policyShapeResult =
        SchedulerController::validatePolicyShape(context_.startupPlan.mainThreadPolicy);
    if (!policyShapeResult)
    {
        Logger::warn(
            "pre-apply gate blocked scheduler validation for main TID {}: {}",
            currentMainThreadId,
            toString(policyShapeResult.error()));
        return;
    }

    if (context_.lastAppliedThreadId.has_value() && context_.lastAppliedThreadId.value() == currentMainThreadId)
    {
        return;
    }

    const std::optional<DWORD> previousMainThreadId = context_.lastAppliedThreadId;
    if (previousMainThreadId.has_value())
    {
        Logger::info(
            "main thread handoff started: previous TID {} remains active until new TID {} passes apply verification",
            previousMainThreadId.value(),
            currentMainThreadId);
    }

    const auto applyResult =
        context_.schedulerController->applyMainThreadPolicy(currentMainThreadId, context_.startupPlan.mainThreadPolicy);
    if (!applyResult)
    {
        if (SchedulerController::isRecoverableAccessLimitation(applyResult.error()))
        {
            Logger::warn(
                "main-thread policy limited by access boundary for TID {}; monitoring-only fallback remains active: {}",
                currentMainThreadId,
                toString(applyResult.error()));
            return;
        }

        Logger::error(
            "failed to validate/apply policy for new main TID {}; previous main TID {} remains the active optimized thread: {}",
            currentMainThreadId,
            previousMainThreadId.value_or(0),
            toString(applyResult.error()));
        return;
    }

    if (previousMainThreadId.has_value())
    {
        Logger::info(
            "new main TID {} passed apply verification; rolling back previous main TID {}",
            currentMainThreadId,
            previousMainThreadId.value());

        const auto rollbackResult = context_.schedulerController->rollbackThread(previousMainThreadId.value());
        if (!rollbackResult)
        {
            if (rollbackResult.error() == ErrorCode::ThreadOpenFailed)
            {
                Logger::warn(
                    "handoff rollback skipped for previous main TID {} after new TID {} was applied: previous thread is no longer openable, so dual optimization risk is gone ({})",
                    previousMainThreadId.value(),
                    currentMainThreadId,
                    toString(rollbackResult.error()));
            }
            else
            {
                Logger::error(
                    "handoff rollback failed for previous main TID {} after new TID {} was applied: {}; rolling back new TID to avoid dual optimized threads",
                    previousMainThreadId.value(),
                    currentMainThreadId,
                    toString(rollbackResult.error()));

                const auto newRollbackResult = context_.schedulerController->rollbackThread(currentMainThreadId);
                if (!newRollbackResult)
                {
                    Logger::error(
                        "failed to rollback newly applied main TID {} after handoff failure: {}",
                        currentMainThreadId,
                        toString(newRollbackResult.error()));
                }
                return;
            }
        }
    }

    context_.lastAppliedThreadId = currentMainThreadId;
}

void WatchdogCycleRunner::reconcilePolicyIfNeeded(std::optional<DWORD> mainThreadId) noexcept
{
    if (!mainThreadId.has_value() || context_.options.schedulerMode != SchedulerMode::Apply)
    {
        return;
    }

    const DWORD currentMainThreadId = mainThreadId.value();
    if (!context_.lastAppliedThreadId.has_value() ||
        context_.lastAppliedThreadId.value() != currentMainThreadId)
    {
        context_.policyAuditCycleCounter = 0;
        return;
    }

    ++context_.policyAuditCycleCounter;
    if (context_.policyAuditCycleCounter < context_.startupPlan.policyAuditIntervalCycles)
    {
        return;
    }

    context_.policyAuditCycleCounter = 0;

    const auto reconcileResult =
        context_.schedulerController->reconcileMainThreadPolicy(currentMainThreadId, context_.startupPlan.mainThreadPolicy);
    if (!reconcileResult)
    {
        if (SchedulerController::isRecoverableAccessLimitation(reconcileResult.error()))
        {
            Logger::warn(
                "policy drift audit limited by access boundary for main TID {}; monitoring-only fallback remains active: {}",
                currentMainThreadId,
                toString(reconcileResult.error()));
            return;
        }

        Logger::error(
            "policy drift audit failed for main TID {}: {}",
            currentMainThreadId,
            toString(reconcileResult.error()));
    }
}

RuntimeMetrics WatchdogCycleRunner::collectMetrics() noexcept
{
    RuntimeMetrics runtimeMetrics = context_.latencyMetricsCollector->collect(
        context_.threadTracker->consumeThreadMigrationCount());
    runtimeMetrics.timestamp = std::chrono::steady_clock::now();
    return runtimeMetrics;
}

RuntimeValidationSample WatchdogCycleRunner::makeValidationSample(
    const RuntimeMetrics& runtimeMetrics,
    std::optional<DWORD> mainThreadId) const noexcept
{
    RuntimeValidationSample validationSample{};
    validationSample.metrics = runtimeMetrics;
    validationSample.mainThreadDetected = mainThreadId.has_value();
    validationSample.mainThreadPolicyApplied =
        mainThreadId.has_value() &&
        context_.lastAppliedThreadId.has_value() &&
        context_.lastAppliedThreadId.value() == mainThreadId.value();
    validationSample.threadTrackerResetEvent = threadTrackerResetEventThisCycle_;
    return validationSample;
}

bool WatchdogCycleRunner::dispatchFeedbackCommands(
    RuntimeValidationSample& validationSample,
    const RuntimeMetrics& runtimeMetrics) noexcept
{
    const auto feedbackCommands = context_.appliedPolicyTracker->evaluate(runtimeMetrics, runtimeMetrics.timestamp);
    for (PolicyCommand command : feedbackCommands)
    {
        ++validationSample.feedbackCommandCount;
        if (command == PolicyCommand::Rollback)
        {
            validationSample.rollbackRequested = true;
        }

        const auto dispatchResult = context_.policyDispatcher->dispatch(command);
        const bool dispatchSucceeded = dispatchResult.has_value();
        context_.appliedPolicyTracker->recordDispatchResult(
            command,
            runtimeMetrics,
            dispatchSucceeded,
            runtimeMetrics.timestamp);

        if (!dispatchResult)
        {
            ++validationSample.dispatchFailureCount;
            Logger::error(
                "policy feedback command dispatch failed (command={}): {}",
                toString(command),
                toString(dispatchResult.error()));
            continue;
        }

        if (command == PolicyCommand::Rollback)
        {
            Logger::error("policy feedback requested rollback; shutting down watchdog loop");
            context_.runtimeValidationMonitor->observe(validationSample);
            if (requestShutdown_ != nullptr)
            {
                requestShutdown_(ShutdownReason::PolicyRollbackRequest);
            }
            return false;
        }
    }

    return true;
}

void WatchdogCycleRunner::dispatchDecisionCommands(
    RuntimeValidationSample& validationSample,
    const RuntimeMetrics& runtimeMetrics) noexcept
{
    const auto commandsResult = context_.latencyDecisionLayer->evaluate(runtimeMetrics, runtimeMetrics.timestamp);
    if (!commandsResult)
    {
        Logger::error(
            "latency decision evaluation failed: {}",
            toString(commandsResult.error()));
        return;
    }

    const auto& commands = *commandsResult;
    for (PolicyCommand command : commands)
    {
        ++validationSample.decisionCommandCount;
        if (command == PolicyCommand::Rollback)
        {
            validationSample.rollbackRequested = true;
        }

        const auto dispatchResult = context_.policyDispatcher->dispatch(command);
        const bool dispatchSucceeded = dispatchResult.has_value();
        context_.appliedPolicyTracker->recordDispatchResult(
            command,
            runtimeMetrics,
            dispatchSucceeded,
            runtimeMetrics.timestamp);

        if (!dispatchResult)
        {
            ++validationSample.dispatchFailureCount;
            Logger::error(
                "policy command dispatch failed (command={}): {}",
                toString(command),
                toString(dispatchResult.error()));
        }
    }
}
