// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: LatencyMetricsCollector
// ERROR-POLICY: expected
// Reason: runtime metric collection must be bounded and non-mutating.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <WinSock2.h>
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ErrorCode.h"
#include "IcmpHandle.h"
#include "LatencyDecisionLayer.h"

class NetworkInterruptController;

struct LatencyMetricsCollectorConfig
{
    // Accepts either an IPv4 literal such as L"8.8.8.8" or a DNS host name.
    // The old field name is kept to avoid touching main.cpp argument wiring.
    std::wstring icmpTargetIpv4;
    std::chrono::milliseconds icmpTimeout = std::chrono::milliseconds(50);
    std::chrono::milliseconds icmpSampleInterval = std::chrono::milliseconds(1000);
    std::size_t rttWindowSize = 10;
    bool interruptAffinitySupported = false;
    NetworkInterruptController* networkInterruptController = nullptr;
};

class LatencyMetricsCollector
{
public:
    explicit LatencyMetricsCollector(LatencyMetricsCollectorConfig config) noexcept;
    ~LatencyMetricsCollector() noexcept;

    LatencyMetricsCollector(const LatencyMetricsCollector&) = delete;
    LatencyMetricsCollector& operator=(const LatencyMetricsCollector&) = delete;

    [[nodiscard]] bool start() noexcept;
    void requestStop() noexcept;
    void join() noexcept;

    // Non-blocking from the watchdog point of view. This function only reads the
    // latest cached sensor state and never calls IcmpSendEcho.
    [[nodiscard]] RuntimeMetrics collect(int threadMigrationCount) noexcept;

private:
    void runIcmpSensor() noexcept;
    [[nodiscard]] bool waitInterruptible(std::chrono::milliseconds interval) noexcept;
    [[nodiscard]] bool prepareIcmpSensor() noexcept;
    [[nodiscard]] bool ensureWinsockStarted() noexcept;
    void cleanupWinsock() noexcept;
    [[nodiscard]] bool resolveTargetAddress(bool allowFallbackToPrevious) noexcept;
    [[nodiscard]] std::optional<double> sampleIcmpRttMs() noexcept;
    void recordIcmpSampleSuccess() noexcept;
    void recordIcmpSampleFailure(const char* reason) noexcept;
    [[nodiscard]] bool reopenIcmpHandleAfterFailures() noexcept;
    void pushRttSample(double rttMs) noexcept;
    [[nodiscard]] double calculateRttJitterMsLocked() const noexcept;
    [[nodiscard]] int estimateDpcSpikeCount() noexcept;

private:
    LatencyMetricsCollectorConfig config_;
    IcmpHandle icmpHandle_;
    std::uint32_t targetIpv4Address_ = 0;
    bool targetIsDomain_ = false;
    bool winsockStarted_ = false;
    std::uint32_t consecutiveIcmpFailureCount_ = 0;

    mutable std::mutex rttMutex_;
    std::vector<double> rttRingBufferMs_;
    std::size_t rttWriteIndex_ = 0;
    std::size_t rttSampleCount_ = 0;
    std::atomic<double> cachedRttJitterMs_ = 0.0;

    std::atomic_bool running_ = false;
    std::thread sensorThread_;
};
