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
#include <optional>
#include <utility>

#include "ErrorCode.h"
#include "Logger.h"
#include "RuntimeSignalState.h"
#include "ShutdownPipeline.h"
#include "StartupPipeline.h"
#include "TrackingWatchdog.h"
#include "WatchdogCycleRunner.h"

namespace
{
    constexpr auto kRuntimeTimeoutHardGrace = std::chrono::seconds(3);
    std::atomic<RuntimeSignalState*> gActiveSignalState = nullptr;

    void requestShutdown() noexcept
    {
        RuntimeSignalState* signalState = gActiveSignalState.load(std::memory_order_acquire);
        if (signalState != nullptr)
        {
            signalState->requestShutdown();
        }
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

    class ConsoleControlRegistration
    {
    public:
        explicit ConsoleControlRegistration(RuntimeSignalState& signalState) noexcept
            : signalState_(&signalState)
        {
            gActiveSignalState.store(signalState_, std::memory_order_release);
            registered_ = SetConsoleCtrlHandler(handleConsoleControl, TRUE) != FALSE;
            if (!registered_)
            {
                gActiveSignalState.store(nullptr, std::memory_order_release);
            }
        }

        ~ConsoleControlRegistration() noexcept
        {
            if (registered_)
            {
                SetConsoleCtrlHandler(handleConsoleControl, FALSE);
            }

            gActiveSignalState.compare_exchange_strong(
                signalState_,
                nullptr,
                std::memory_order_acq_rel,
                std::memory_order_acquire);
        }

        ConsoleControlRegistration(const ConsoleControlRegistration&) = delete;
        ConsoleControlRegistration& operator=(const ConsoleControlRegistration&) = delete;

        [[nodiscard]] bool registered() const noexcept
        {
            return registered_;
        }

    private:
        RuntimeSignalState* signalState_ = nullptr;
        bool registered_ = false;
    };
}

RuntimeOrchestrator::RuntimeOrchestrator(int argc, wchar_t* argv[]) noexcept
    : argc_(argc), argv_(argv)
{
}

int RuntimeOrchestrator::run() noexcept
{
    signalState_.reset();

    ConsoleControlRegistration consoleControl(signalState_);
    if (!consoleControl.registered())
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
    WatchdogCycleRunner cycleRunner(context_, signalState_.runtimeTimeoutRequested, requestShutdown);
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
    std::optional<std::chrono::steady_clock::time_point> softTimeoutAt;
    while (signalState_.isRunning())
    {
        const auto now = std::chrono::steady_clock::now();
        if (context_.options.maxRuntime.has_value())
        {
            const auto elapsed = now - runtimeStart;
            if (elapsed >= context_.options.maxRuntime.value())
            {
                if (signalState_.requestRuntimeTimeout())
                {
                    softTimeoutAt = now;
                    Logger::info(
                        "max runtime limit reached: {} seconds; shutdown will be requested at the next watchdog safe point",
                        context_.options.maxRuntime.value().count());
                }
                else if (softTimeoutAt.has_value() && now - softTimeoutAt.value() >= kRuntimeTimeoutHardGrace)
                {
                    Logger::warn(
                        "max runtime hard-timeout grace exceeded: {} ms after soft timeout; forcing clean shutdown request",
                        kRuntimeTimeoutHardGrace.count() * 1000);
                    signalState_.requestShutdown();
                    watchdog.requestStop();
                    break;
                }
            }
        }

        (void)signalState_.waitForShutdownFor(std::chrono::milliseconds(100));
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
