// Build: included by RuntimeOrchestrator.cpp and WatchdogCycleRunner.cpp
// MODULE: RuntimeSignalState
// ERROR-POLICY: no-throw
// Reason: runtime signal ownership must be explicit even though the Win32 console handler is static.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

enum class ShutdownReason
{
    None,
    ConsoleControl,
    MaxRuntimeSoftTimeout,
    MaxRuntimeHardTimeout,
    WatchdogFailure,
    PolicyRollbackRequest,
};

[[nodiscard]] inline const char* toString(ShutdownReason reason) noexcept
{
    switch (reason)
    {
    case ShutdownReason::None:
        return "None";
    case ShutdownReason::ConsoleControl:
        return "ConsoleControl";
    case ShutdownReason::MaxRuntimeSoftTimeout:
        return "MaxRuntimeSoftTimeout";
    case ShutdownReason::MaxRuntimeHardTimeout:
        return "MaxRuntimeHardTimeout";
    case ShutdownReason::WatchdogFailure:
        return "WatchdogFailure";
    case ShutdownReason::PolicyRollbackRequest:
        return "PolicyRollbackRequest";
    default:
        return "Unknown";
    }
}

struct RuntimeSignalState
{
    std::atomic_bool running{true};
    std::atomic_bool runtimeTimeoutRequested{false};
    std::atomic<ShutdownReason> shutdownReason{ShutdownReason::None};
    std::mutex mutex;
    std::condition_variable changed;

    void reset() noexcept
    {
        running.store(true, std::memory_order_release);
        runtimeTimeoutRequested.store(false, std::memory_order_release);
        shutdownReason.store(ShutdownReason::None, std::memory_order_release);
    }

    void requestShutdown(ShutdownReason reason) noexcept
    {
        markShutdownReason(reason);
        running.store(false, std::memory_order_release);
        changed.notify_all();
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return running.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool requestRuntimeTimeout() noexcept
    {
        const bool newlyRequested = !runtimeTimeoutRequested.exchange(true, std::memory_order_acq_rel);
        if (newlyRequested)
        {
            markShutdownReason(ShutdownReason::MaxRuntimeSoftTimeout);
        }
        return newlyRequested;
    }

    [[nodiscard]] ShutdownReason currentShutdownReason() const noexcept
    {
        return shutdownReason.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool waitForShutdownFor(std::chrono::milliseconds interval) noexcept
    {
        std::unique_lock lock(mutex);
        return changed.wait_for(
            lock,
            interval,
            [this]() noexcept
            {
                return !running.load(std::memory_order_acquire);
            });
    }

private:
    void markShutdownReason(ShutdownReason reason) noexcept
    {
        if (reason == ShutdownReason::None)
        {
            return;
        }

        if (reason == ShutdownReason::MaxRuntimeHardTimeout)
        {
            shutdownReason.store(reason, std::memory_order_release);
            return;
        }

        ShutdownReason expected = ShutdownReason::None;
        (void)shutdownReason.compare_exchange_strong(
            expected,
            reason,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }
};
