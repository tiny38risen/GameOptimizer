// Build: cl /std:c++latest /O2 /W4 /WX /permissive- ShutdownPipeline.cpp
// MODULE: ShutdownPipeline
// ERROR-POLICY: expected
// Reason: shutdown failure classification feeds release evidence.

#include "ShutdownPipeline.h"

#include "ErrorCode.h"
#include "Logger.h"

ShutdownResult ShutdownPipeline::execute(RuntimeContext& context, TrackingWatchdog& watchdog) noexcept
{
    Logger::info("shutdown requested; stopping watchdog and latency sensor before rollback");
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
    if (context.runtimeValidationMonitor)
    {
        context.runtimeValidationMonitor->logSummary();
        shutdownResult.runtimeValidationFailed = context.runtimeValidationMonitor->hasCriticalFailure();
    }

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
    return shutdownResult.failed() ? 1 : 1;
}
