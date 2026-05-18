// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: TrackingWatchdog
// ERROR-POLICY: expected
// Reason: watchdog lifetime is explicit and must not detach worker threads.

#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>

class TrackingWatchdog
{
public:
    using TickHandler = std::function<void(std::stop_token)>;

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
    void run(std::stop_token stopToken, std::chrono::milliseconds interval) noexcept;
    [[nodiscard]] bool waitInterruptible(
        std::stop_token stopToken,
        std::chrono::milliseconds interval) noexcept;

private:
    std::mutex waitMutex_;
    std::condition_variable_any stopCondition_;
    std::jthread worker_;
    TickHandler tickHandler_;
};
