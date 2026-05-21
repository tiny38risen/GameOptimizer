// Build: cl /std:c++latest /O2 /W4 /WX /permissive- RuntimeOrchestrator.cpp StartupPipeline.cpp WatchdogCycleRunner.cpp ShutdownPipeline.cpp
// MODULE: RuntimeOrchestrator
// ERROR-POLICY: expected
// Reason: runtime orchestrator owns execution order while pipelines own startup, cycle, and shutdown logic.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "RuntimeOrchestrator.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>

#include "ErrorCode.h"
#include "Logger.h"
#include "ShutdownPipeline.h"
#include "StartupPipeline.h"
#include "TrackingWatchdog.h"
#include "WatchdogCycleRunner.h"

namespace
{
    std::atomic_bool gRunning = true;
    std::atomic_bool gRuntimeTimeoutRequested = false;
    std::mutex gRunStateMutex;
    std::condition_variable gRunStateChanged;

    void requestShutdown() noexcept
    {
        gRunning.store(false, std::memory_order_release);
        gRunStateChanged.notify_all();
    }

    BOOL WINAPI handleConsoleControl(DWORD controlType) noexcept
    {
        switch (controlType)
        {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            requestShutdown();
            return TRUE;
        default:
            return FALSE;
        }
    }
}

RuntimeOrchestrator::RuntimeOrchestrator(int argc, wchar_t* argv[]) noexcept
    : argc_(argc), argv_(argv)
{
}

int RuntimeOrchestrator::run() noexcept
{
    gRunning.store(true, std::memory_order_release);
    gRuntimeTimeoutRequested.store(false, std::memory_order_release);

    if (!SetConsoleCtrlHandler(handleConsoleControl, TRUE))
    {
        Logger::error("startup failed: {}", toString(ErrorCode::ConsoleControlHandlerFailed));
        return 1;
    }

    auto contextResult = StartupPipeline::run(argc_, argv_);
    if (!contextResult)
    {
        return 1;
    }

    context_ = std::move(contextResult.value());

    TrackingWatchdog watchdog;
    WatchdogCycleRunner cycleRunner(context_, gRuntimeTimeoutRequested, requestShutdown);
    const bool watchdogStarted = watchdog.start(
        [&](std::stop_token stopToken) noexcept
        {
            cycleRunner.runCycle(stopToken);
        },
        context_.startupPlan.watchdogInterval);

    if (!watchdogStarted)
    {
        Logger::error("startup failed: watchdog start failed");
        return ShutdownPipeline::shutdownAfterStartupFailure(context_);
    }

    const auto runtimeStart = std::chrono::steady_clock::now();
    while (gRunning.load(std::memory_order_acquire))
    {
        if (context_.options.maxRuntime.has_value())
        {
            const auto elapsed = std::chrono::steady_clock::now() - runtimeStart;
            if (elapsed >= context_.options.maxRuntime.value())
            {
                if (!gRuntimeTimeoutRequested.exchange(true, std::memory_order_acq_rel))
                {
                    Logger::info(
                        "max runtime limit reached: {} seconds; shutdown will be requested at the next watchdog safe point",
                        context_.options.maxRuntime.value().count());
                }
            }
        }

        std::unique_lock lock(gRunStateMutex);
        gRunStateChanged.wait_for(
            lock,
            std::chrono::milliseconds(100),
            []() noexcept
            {
                return !gRunning.load(std::memory_order_acquire);
            });
    }

    const ShutdownResult shutdownResult = ShutdownPipeline::execute(context_, watchdog);

    if (shutdownResult.failed())
    {
        if (shutdownResult.runtimeValidationFailed)
        {
            Logger::error("shutdown completed with runtime validation failure");
        }

        if (shutdownResult.rollbackStatePreserved)
        {
            Logger::error("shutdown completed with preserved rollback state after rollback failure");
        }

        Logger::error("shutdown completed with failures");
        return 1;
    }

    Logger::info("shutdown completed cleanly");
    return 0;
}
