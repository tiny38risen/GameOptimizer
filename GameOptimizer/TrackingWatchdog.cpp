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
}

bool TrackingWatchdog::start(
    TickHandler tickHandler,
    std::chrono::milliseconds interval) noexcept
{
    if (worker_.joinable() || !tickHandler || interval.count() <= 0)
    {
        return false;
    }

    try
    {
        tickHandler_ = std::move(tickHandler);
        worker_ = std::jthread(
            [this, interval](std::stop_token stopToken) noexcept
            {
                run(stopToken, interval);
            });
        return true;
    }
    catch (...)
    {
        tickHandler_ = nullptr;
        return false;
    }
}

void TrackingWatchdog::requestStop() noexcept
{
    if (worker_.joinable())
    {
        worker_.request_stop();
    }
    stopCondition_.notify_all();
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
    return worker_.joinable() && !worker_.get_stop_token().stop_requested();
}

void TrackingWatchdog::run(std::stop_token stopToken, std::chrono::milliseconds interval) noexcept
{
    Logger::info("watchdog started");

    while (!stopToken.stop_requested())
    {
        try
        {
            tickHandler_(stopToken);
        }
        catch (...)
        {
            Logger::error("watchdog tick failed with unexpected exception");
        }

        if (!waitInterruptible(stopToken, interval))
        {
            break;
        }
    }

    Logger::info("watchdog stopped");
}

bool TrackingWatchdog::waitInterruptible(
    std::stop_token stopToken,
    std::chrono::milliseconds interval) noexcept
{
    try
    {
        std::unique_lock lock(waitMutex_);
        stopCondition_.wait_for(
            lock,
            stopToken,
            interval,
            []() noexcept
            {
                return false;
            });
    }
    catch (...)
    {
        Logger::error("watchdog interruptible wait failed with unexpected exception");
        requestStop();
    }

    return !stopToken.stop_requested();
}
