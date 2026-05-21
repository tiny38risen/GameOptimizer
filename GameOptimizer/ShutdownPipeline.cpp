// Build: cl /std:c++latest /O2 /W4 /WX /permissive- ShutdownPipeline.cpp
// MODULE: ShutdownPipeline
// ERROR-POLICY: expected
// Reason: shutdown failure classification feeds release evidence.

#include "ShutdownPipeline.h"

#include "ErrorCode.h"
#include "Logger.h"

ShutdownResult ShutdownPipeline::execute(RuntimeContext& context, TrackingWatchdog& watchdog) noexcept
{
    Logger::info("shutdown requested; stopping policy cycles before rollback");
    watchdog.requestStop();
    if (context.latencyMetricsCollector)
    {
        context.latencyMetricsCollector->requestStop();
    }

    watchdog.join();
    if (context.latencyMetricsCollector)
    {
        context.latencyMetricsCollector->join();
    }

    ShutdownResult shutdownResult{};
    logRuntimeValidationSnapshot(context, "pre-rollback", shutdownResult);

    if (context.timerResolutionController)
    {
        const auto timerRollbackResult = context.timerResolutionController->rollback();
        if (!timerRollbackResult)
        {
            shutdownResult.timerRollbackFailed = true;
            Logger::error("shutdown timer rollback failed: {}", toString(timerRollbackResult.error()));
        }
    }

    if (context.schedulerController)
    {
        const auto rollbackResult = context.schedulerController->rollbackAll();
        if (!rollbackResult)
        {
            shutdownResult.schedulerRollbackFailed = true;
            shutdownResult.rollbackStatePreserved = true;
            Logger::error("shutdown rollback failed: {}", toString(rollbackResult.error()));
        }
    }

    logRuntimeValidationSnapshot(context, "post-rollback", shutdownResult);

    Logger::info(
        "shutdown result: timerRollbackFailed={}, schedulerRollbackFailed={}, runtimeValidationFailed={}, rollbackStatePreserved={}",
        shutdownResult.timerRollbackFailed,
        shutdownResult.schedulerRollbackFailed,
        shutdownResult.runtimeValidationFailed,
        shutdownResult.rollbackStatePreserved);

    return shutdownResult;
}

int ShutdownPipeline::shutdownAfterStartupFailure(RuntimeContext& context) noexcept
{
    TrackingWatchdog watchdog;
    const ShutdownResult shutdownResult = execute(context, watchdog);
    if (shutdownResult.failed())
    {
        Logger::error("startup failure cleanup also completed with shutdown failures");
    }

    return 1;
}

void ShutdownPipeline::logRuntimeValidationSnapshot(
    const RuntimeContext& context,
    const char* evidencePhase,
    ShutdownResult& shutdownResult) noexcept
{
    if (!context.runtimeValidationMonitor)
    {
        Logger::info("runtime validation {} evidence snapshot: monitor unavailable", evidencePhase);
        return;
    }

    Logger::info("runtime validation {} evidence snapshot begin", evidencePhase);
    context.runtimeValidationMonitor->logSummary();
    shutdownResult.runtimeValidationFailed = context.runtimeValidationMonitor->hasCriticalFailure();
    Logger::info(
        "runtime validation {} evidence snapshot end: critical_failure={}",
        evidencePhase,
        context.runtimeValidationMonitor->hasCriticalFailure());
}
