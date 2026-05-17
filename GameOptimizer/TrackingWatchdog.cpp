// Build: cl /std:c++latest /O2 /W4 /WX /permissive- TrackingWatchdog.cpp
// MODULE: TrackingWatchdog
// ERROR-POLICY: expected
// Reason: watchdog must stop cleanly on Ctrl+C and never throw across thread boundary.

#include "TrackingWatchdog.h"

#include "Logger.h"

#include <utility>

TrackingWatchdog::~TrackingWatchdog() noexcept
{
    requestStop();
    join();
}

bool TrackingWatchdog::start(
    TickHandler tickHandler,
    std::chrono::milliseconds interval) noexcept
{
    if (running_.load(std::memory_order_acquire) || !tickHandler || interval.count() <= 0)
    {
        return false;
    }

    try
    {
        tickHandler_ = std::move(tickHandler);
        running_.store(true, std::memory_order_release);
        worker_ = std::thread(&TrackingWatchdog::run, this, interval);
        return true;
    }
    catch (...)
    {
        running_.store(false, std::memory_order_release);
        tickHandler_ = nullptr;
        return false;
    }
}

void TrackingWatchdog::requestStop() noexcept
{
    running_.store(false, std::memory_order_release);
}

void TrackingWatchdog::join() noexcept
{
    if (worker_.joinable())
    {
        worker_.join();
    }
}

bool TrackingWatchdog::isRunning() const noexcept
{
    return running_.load(std::memory_order_acquire);
}

void TrackingWatchdog::run(std::chrono::milliseconds interval) noexcept
{
    Logger::info("watchdog started");

    while (running_.load(std::memory_order_acquire))
    {
        try
        {
            tickHandler_();
        }
        catch (...)
        {
            Logger::error("watchdog tick failed with unexpected exception");
        }

        if (!waitInterruptible(interval))
        {
            break;
        }
    }

    Logger::info("watchdog stopped");
}

bool TrackingWatchdog::waitInterruptible(std::chrono::milliseconds interval) noexcept
{
    constexpr auto kSlice = std::chrono::milliseconds(100);
    auto remaining = interval;

    while (remaining.count() > 0 && running_.load(std::memory_order_acquire))
    {
        const auto sleepDuration = remaining < kSlice ? remaining : kSlice;
        std::this_thread::sleep_for(sleepDuration);
        remaining -= sleepDuration;
    }

    return running_.load(std::memory_order_acquire);
}
