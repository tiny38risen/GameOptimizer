// Build: cl /std:c++latest /O2 /W4 /WX /permissive- NetworkInterruptController.cpp
// MODULE: NetworkInterruptController
// ERROR-POLICY: expected
// Reason: DPC counters and interrupt-affinity support are runtime environment capabilities.

#include "NetworkInterruptController.h"

#include <algorithm>
#include <cmath>

#include "Logger.h"

#pragma comment(lib, "Pdh.lib")

namespace
{
    constexpr const wchar_t* kDpcRateCounterPath = L"\\Processor(_Total)\\DPC Rate";
    constexpr int kMaxReportedDpcSpikeCount = 100000;
}

NetworkInterruptController::NetworkInterruptController(NetworkInterruptControllerConfig config) noexcept
    : config_(config)
{
}

NetworkInterruptController::~NetworkInterruptController() noexcept
{
    closePdhQuery();
}

bool NetworkInterruptController::initialize() noexcept
{
    status_.interruptAffinitySupported = config_.forceInterruptAffinitySupportedForTests;
    status_.monitoringOnly = !status_.interruptAffinitySupported;

    if (status_.interruptAffinitySupported)
    {
        Logger::warn(
            "interrupt affinity support forced by test/configuration; IRQ_REPIN dispatch path is enabled");
    }
    else
    {
        Logger::warn(
            "interrupt affinity control is unsupported by the current Win32 runtime path; DPC monitoring remains active when available");
    }

    if (!config_.enableDpcMonitoring)
    {
        Logger::warn("DPC monitoring disabled by configuration");
        return true;
    }

    PDH_HQUERY query = nullptr;
    PDH_STATUS status = PdhOpenQueryW(nullptr, 0, &query);
    if (status != ERROR_SUCCESS)
    {
        Logger::warn("DPC monitoring unavailable: PdhOpenQueryW failed (status=0x{:X})", status);
        return true;
    }

    PDH_HCOUNTER counter = nullptr;
    status = PdhAddEnglishCounterW(query, kDpcRateCounterPath, 0, &counter);
    if (status != ERROR_SUCCESS)
    {
        Logger::warn("DPC monitoring unavailable: PdhAddEnglishCounterW failed (status=0x{:X})", status);
        PdhCloseQuery(query);
        return true;
    }

    status = PdhCollectQueryData(query);
    if (status != ERROR_SUCCESS)
    {
        Logger::warn("DPC monitoring unavailable: initial PdhCollectQueryData failed (status=0x{:X})", status);
        PdhCloseQuery(query);
        return true;
    }

    pdhQuery_ = query;
    dpcRateCounter_ = counter;
    dpcCounterPrimed_ = true;
    status_.dpcMonitoringAvailable = true;
    Logger::info("DPC monitoring enabled via PDH counter: {}", "\\Processor(_Total)\\DPC Rate");
    return true;
}

int NetworkInterruptController::sampleDpcSpikeCount() noexcept
{
    if (!status_.dpcMonitoringAvailable || pdhQuery_ == nullptr || dpcRateCounter_ == nullptr)
    {
        constexpr int kNoDpcSpikes = 0;
        return kNoDpcSpikes;
    }

    const PDH_STATUS collectStatus = PdhCollectQueryData(pdhQuery_);
    if (collectStatus != ERROR_SUCCESS)
    {
        Logger::warn("DPC monitoring sample skipped: PdhCollectQueryData failed (status=0x{:X})", collectStatus);
        constexpr int kNoDpcSpikes = 0;
        return kNoDpcSpikes;
    }

    PDH_FMT_COUNTERVALUE value{};
    const PDH_STATUS valueStatus = PdhGetFormattedCounterValue(
        dpcRateCounter_,
        PDH_FMT_DOUBLE,
        nullptr,
        &value);
    if (valueStatus != ERROR_SUCCESS || value.CStatus != ERROR_SUCCESS || !std::isfinite(value.doubleValue))
    {
        Logger::warn(
            "DPC monitoring sample skipped: PdhGetFormattedCounterValue failed (status=0x{:X}, counter_status=0x{:X})",
            valueStatus,
            value.CStatus);
        constexpr int kNoDpcSpikes = 0;
        return kNoDpcSpikes;
    }

    const double boundedRate = std::clamp(value.doubleValue, 0.0, static_cast<double>(kMaxReportedDpcSpikeCount));
    return static_cast<int>(std::ceil(boundedRate));
}

bool NetworkInterruptController::isInterruptAffinitySupported() const noexcept
{
    return status_.interruptAffinitySupported;
}

NetworkInterruptStatus NetworkInterruptController::status() const noexcept
{
    return status_;
}

std::expected<void, ErrorCode> NetworkInterruptController::handleIrqRepin() noexcept
{
    if (!status_.interruptAffinitySupported)
    {
        Logger::warn(
            "IRQ_REPIN ignored: interrupt affinity control is unsupported; continuing DPC monitoring only");
        return {};
    }

    Logger::warn(
        "IRQ_REPIN dispatch path reached, but no public Win32 interrupt-affinity mutation backend is enabled; continuing monitoring-only");
    return {};
}

void NetworkInterruptController::closePdhQuery() noexcept
{
    if (pdhQuery_ != nullptr)
    {
        PdhCloseQuery(pdhQuery_);
        pdhQuery_ = nullptr;
        dpcRateCounter_ = nullptr;
        dpcCounterPrimed_ = false;
        status_.dpcMonitoringAvailable = false;
    }
}
