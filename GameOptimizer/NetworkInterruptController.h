// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: NetworkInterruptController
// ERROR-POLICY: expected
// Reason: interrupt affinity support is environment-dependent and may be monitoring-only.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <Pdh.h>
#include <expected>

#include "ErrorCode.h"

struct NetworkInterruptControllerConfig
{
    bool enableDpcMonitoring = true;
    bool forceInterruptAffinitySupportedForTests = false;
};

struct NetworkInterruptStatus
{
    bool dpcMonitoringAvailable = false;
    bool interruptAffinitySupported = false;
    bool monitoringOnly = true;
    int irqRepinRequestCount = 0;
    int irqRepinSuppressedCount = 0;
};

class NetworkInterruptController
{
public:
    explicit NetworkInterruptController(NetworkInterruptControllerConfig config = {}) noexcept;
    ~NetworkInterruptController() noexcept;

    NetworkInterruptController(const NetworkInterruptController&) = delete;
    NetworkInterruptController& operator=(const NetworkInterruptController&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    [[nodiscard]] int sampleDpcSpikeCount() noexcept;
    [[nodiscard]] bool isInterruptAffinitySupported() const noexcept;
    [[nodiscard]] NetworkInterruptStatus status() const noexcept;

    [[nodiscard]] std::expected<void, ErrorCode> handleIrqRepin() noexcept;

private:
    void closePdhQuery() noexcept;

private:
    NetworkInterruptControllerConfig config_{};
    NetworkInterruptStatus status_{};
    PDH_HQUERY pdhQuery_ = nullptr;
    PDH_HCOUNTER dpcRateCounter_ = nullptr;
    bool dpcCounterPrimed_ = false;
};
