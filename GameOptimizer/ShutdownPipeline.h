// Build: included by RuntimeOrchestrator.cpp and ShutdownPipeline.cpp
// MODULE: ShutdownPipeline
// ERROR-POLICY: expected
// Reason: shutdown owns stop ordering, rollback classification, and failure evidence.

#pragma once

#include "RuntimeContext.h"
#include "TrackingWatchdog.h"

struct ShutdownResult
{
    bool timerRollbackFailed = false;
    bool schedulerRollbackFailed = false;
    bool runtimeValidationFailed = false;
    bool rollbackStatePreserved = false;

    [[nodiscard]] bool failed() const noexcept
    {
        return timerRollbackFailed || schedulerRollbackFailed || runtimeValidationFailed;
    }
};

class ShutdownPipeline
{
public:
    [[nodiscard]] static ShutdownResult execute(RuntimeContext& context, TrackingWatchdog& watchdog) noexcept;
    [[nodiscard]] static int shutdownAfterStartupFailure(RuntimeContext& context) noexcept;

private:
    static void logRuntimeValidationSnapshot(
        const RuntimeContext& context,
        const char* evidencePhase,
        ShutdownResult& shutdownResult) noexcept;
};
