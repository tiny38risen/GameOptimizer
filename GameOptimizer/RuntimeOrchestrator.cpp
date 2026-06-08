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
    constexpr auto kMinimumRuntimeTimeoutHardGrace = std::chrono::seconds(3);
    constexpr auto kMaximumRuntimeTimeoutHardGrace = std::chrono::seconds(10);
    std::atomic<RuntimeSignalState*> gActiveSignalState = nullptr;

    [[nodiscard]] std::chrono::milliseconds calculateRuntimeTimeoutHardGrace(
        std::chrono::milliseconds watchdogInterval) noexcept
    {
        const auto intervalBasedGrace = watchdogInterval * 2;
        if (intervalBasedGrace < kMinimumRuntimeTimeoutHardGrace)
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(kMinimumRuntimeTimeoutHardGrace);
        }

        if (intervalBasedGrace > kMaximumRuntimeTimeoutHardGrace)
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(kMaximumRuntimeTimeoutHardGrace);
        }

        return intervalBasedGrace;
    }

    void requestShutdown(ShutdownReason reason) noexcept
    {
        RuntimeSignalState* signalState = gActiveSignalState.load(std::memory_order_acquire);
        if (signalState != nullptr)
        {
            signalState->requestShutdown(reason);
        }
    }

    [[nodiscard]] bool stopSignalFileExists(const std::wstring& path) noexcept
    {
        if (path.empty())
        {
            return false;
        }

        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
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
            requestShutdown(ShutdownReason::ConsoleControl);
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
                if (SetConsoleCtrlHandler(handleConsoleControl, FALSE) == FALSE)
                {
                    Logger::error(
                        "console control handler unregister failed: win32_error={}",
                        GetLastError());
                }
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

    auto context = std::move(*contextResult);
    context_ = std::move(context);

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
        return ShutdownPipeline::shutdownAfterStartupFailure(context_, ShutdownReason::WatchdogFailure);
    }

    const auto runtimeStart = std::chrono::steady_clock::now();
    const auto runtimeTimeoutHardGrace = calculateRuntimeTimeoutHardGrace(context_.startupPlan.watchdogInterval);
    std::optional<std::chrono::steady_clock::time_point> softTimeoutAt;
    while (signalState_.isRunning())
    {
        const auto now = std::chrono::steady_clock::now();
        if (stopSignalFileExists(context_.options.stopSignalFilePath))
        {
            Logger::warn(
                "UI stop signal file detected; requesting clean shutdown through rollback pipeline");
            signalState_.requestShutdown(ShutdownReason::ConsoleControl);
            watchdog.requestStop();
            break;
        }

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
                else if (softTimeoutAt.has_value() && now - softTimeoutAt.value() >= runtimeTimeoutHardGrace)
                {
                    Logger::warn(
                        "max runtime hard-timeout grace exceeded: hard_grace_ms={}, watchdog_interval_ms={}; forcing clean shutdown request",
                        runtimeTimeoutHardGrace.count(),
                        context_.startupPlan.watchdogInterval.count());
                    signalState_.requestShutdown(ShutdownReason::MaxRuntimeHardTimeout);
                    watchdog.requestStop();
                    break;
                }
            }
        }

        (void)signalState_.waitForShutdownFor(std::chrono::milliseconds(100));
    }

    const ShutdownResult shutdownResult = ShutdownPipeline::execute(
        context_,
        watchdog,
        signalState_.currentShutdownReason());

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
