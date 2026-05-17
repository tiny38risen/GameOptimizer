// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: TrackingWatchdog
// ERROR-POLICY: expected
// Reason: watchdog lifetime is explicit and must not detach worker threads.

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

class TrackingWatchdog
{
public:
    using TickHandler = std::function<void()>;

    TrackingWatchdog() noexcept = default;
    ~TrackingWatchdog() noexcept;

    TrackingWatchdog(const TrackingWatchdog&) = delete;
    TrackingWatchdog& operator=(const TrackingWatchdog&) = delete;

    [[nodiscard]] bool start(
        TickHandler tickHandler,
        std::chrono::milliseconds interval) noexcept;

    void requestStop() noexcept;
    void join() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

private:
    void run(std::chrono::milliseconds interval) noexcept;
    [[nodiscard]] bool waitInterruptible(std::chrono::milliseconds interval) noexcept;

private:
    std::atomic_bool running_ = false;
    std::thread worker_;
    TickHandler tickHandler_;
};
