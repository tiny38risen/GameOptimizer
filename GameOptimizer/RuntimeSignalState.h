// Build: included by RuntimeOrchestrator.cpp and WatchdogCycleRunner.cpp
// MODULE: RuntimeSignalState
// ERROR-POLICY: no-throw
// Reason: runtime signal ownership must be explicit even though the Win32 console handler is static.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

struct RuntimeSignalState
{
    std::atomic_bool running{true};
    std::atomic_bool runtimeTimeoutRequested{false};
    std::mutex mutex;
    std::condition_variable changed;

    void reset() noexcept
    {
        running.store(true, std::memory_order_release);
        runtimeTimeoutRequested.store(false, std::memory_order_release);
    }

    void requestShutdown() noexcept
    {
        running.store(false, std::memory_order_release);
        changed.notify_all();
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return running.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool requestRuntimeTimeout() noexcept
    {
        return !runtimeTimeoutRequested.exchange(true, std::memory_order_acq_rel);
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
};
