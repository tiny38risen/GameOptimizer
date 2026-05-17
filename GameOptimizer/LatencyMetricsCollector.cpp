// Build: cl /std:c++latest /O2 /W4 /WX /permissive- LatencyMetricsCollector.cpp
// MODULE: LatencyMetricsCollector
// ERROR-POLICY: expected
// Reason: this module observes latency indicators and never changes system policy directly.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <Iphlpapi.h>
#include <IcmpAPI.h>

#include "LatencyMetricsCollector.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <string_view>
#include <utility>

#include "Logger.h"

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr std::size_t kMinimumRttSamplesForJitter = 2;
    constexpr DWORD kIcmpReplyBufferExtraBytes = 8;
    constexpr std::size_t kIcmpRequestPayloadBytes = 8;
    constexpr std::uint32_t kMaxConsecutiveIcmpFailuresBeforeReopen = 5;
    constexpr std::chrono::milliseconds kSensorWaitSlice{100};

    [[nodiscard]] std::uint32_t ipv4AddressFromInAddr(const IN_ADDR& address) noexcept
    {
        return address.S_un.S_addr;
    }

    struct Ipv4Text
    {
        std::array<char, INET_ADDRSTRLEN> buffer{};
        std::size_t length = 0;

        [[nodiscard]] std::string_view view() const noexcept
        {
            return std::string_view(buffer.data(), length);
        }
    };

    [[nodiscard]] Ipv4Text formatIpv4Address(std::uint32_t ipv4Address) noexcept
    {
        IN_ADDR address{};
        address.S_un.S_addr = ipv4Address;

        Ipv4Text text{};
        if (InetNtopA(AF_INET, &address, text.buffer.data(), static_cast<DWORD>(text.buffer.size())) == nullptr)
        {
            constexpr std::string_view kInvalidIpv4Text = "<invalid-ipv4>";
            static_assert(kInvalidIpv4Text.size() < INET_ADDRSTRLEN);
            std::memcpy(text.buffer.data(), kInvalidIpv4Text.data(), kInvalidIpv4Text.size());
            text.length = kInvalidIpv4Text.size();
            return text;
        }

        text.length = strnlen_s(text.buffer.data(), text.buffer.size());
        return text;
    }
}

LatencyMetricsCollector::LatencyMetricsCollector(LatencyMetricsCollectorConfig config) noexcept
    : config_(std::move(config))
{
    if (config_.rttWindowSize < kMinimumRttSamplesForJitter)
    {
        config_.rttWindowSize = kMinimumRttSamplesForJitter;
    }

    if (config_.icmpSampleInterval.count() <= 0)
    {
        config_.icmpSampleInterval = std::chrono::milliseconds(1000);
    }

    rttRingBufferMs_.assign(config_.rttWindowSize, 0.0);
}

LatencyMetricsCollector::~LatencyMetricsCollector() noexcept
{
    requestStop();
    join();
}

bool LatencyMetricsCollector::start() noexcept
{
    if (config_.icmpTargetIpv4.empty())
    {
        Logger::info("latency ICMP sensor not started: no target configured");
        return true;
    }

    if (running_.load(std::memory_order_acquire))
    {
        return false;
    }

    if (!prepareIcmpSensor())
    {
        icmpHandle_.close();
        cleanupWinsock();
        Logger::warn("latency ICMP sensor not started: ICMP handle or target preparation failed");
        return true;
    }

    try
    {
        running_.store(true, std::memory_order_release);
        sensorThread_ = std::thread(&LatencyMetricsCollector::runIcmpSensor, this);
        return true;
    }
    catch (...)
    {
        running_.store(false, std::memory_order_release);
        icmpHandle_.close();
        cleanupWinsock();
        Logger::error("latency ICMP sensor start failed");
        return false;
    }
}

void LatencyMetricsCollector::requestStop() noexcept
{
    running_.store(false, std::memory_order_release);
}

void LatencyMetricsCollector::join() noexcept
{
    if (sensorThread_.joinable())
    {
        sensorThread_.join();
    }

    icmpHandle_.close();
    cleanupWinsock();
}

RuntimeMetrics LatencyMetricsCollector::collect(int threadMigrationCount) noexcept
{
    const double cachedJitter = cachedRttJitterMs_.load(std::memory_order_relaxed);

    const RuntimeMetrics metrics{
        .rttJitterMs = cachedJitter,
        .dpcSpikeCount = estimateDpcSpikeCount(),
        .threadMigrationCount = threadMigrationCount,
        .interruptAffinitySupported = config_.interruptAffinitySupported};

    Logger::info(
        "latency metrics: rtt_jitter={:.3f} ms, dpc_spikes={}, thread_migrations={}, irq_supported={}",
        metrics.rttJitterMs,
        metrics.dpcSpikeCount,
        metrics.threadMigrationCount,
        metrics.interruptAffinitySupported);

    return metrics;
}

void LatencyMetricsCollector::runIcmpSensor() noexcept
{
    Logger::info(
        "latency ICMP sensor started: interval={} ms, timeout={} ms",
        config_.icmpSampleInterval.count(),
        config_.icmpTimeout.count());

    while (running_.load(std::memory_order_acquire))
    {
        const auto rttSample = sampleIcmpRttMs();
        if (rttSample.has_value())
        {
            pushRttSample(*rttSample);
        }

        if (!waitInterruptible(config_.icmpSampleInterval))
        {
            break;
        }
    }

    Logger::info("latency ICMP sensor stopped");
}


bool LatencyMetricsCollector::prepareIcmpSensor() noexcept
{
    if (!ensureWinsockStarted())
    {
        Logger::warn("ICMP sensor preparation failed: WSAStartup failed");
        return false;
    }

    if (!resolveTargetAddress(false))
    {
        Logger::warn("ICMP sensor preparation failed: target address resolution failed");
        return false;
    }

    if (!icmpHandle_.open())
    {
        Logger::warn("ICMP sensor preparation failed: IcmpCreateFile failed");
        return false;
    }

    Logger::info(
        "ICMP handle opened once and will be reused by the latency sensor: target_ip={}",
        formatIpv4Address(targetIpv4Address_).view());
    return true;
}

bool LatencyMetricsCollector::ensureWinsockStarted() noexcept
{
    if (winsockStarted_)
    {
        return true;
    }

    WSADATA data{};
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &data);
    if (startupResult != 0)
    {
        Logger::warn("WSAStartup failed for latency DNS resolution: code={}", startupResult);
        return false;
    }

    winsockStarted_ = true;
    return true;
}

void LatencyMetricsCollector::cleanupWinsock() noexcept
{
    if (!winsockStarted_)
    {
        return;
    }

    WSACleanup();
    winsockStarted_ = false;
}

bool LatencyMetricsCollector::resolveTargetAddress(bool allowFallbackToPrevious) noexcept
{
    if (config_.icmpTargetIpv4.empty())
    {
        return false;
    }

    const std::uint32_t previousAddress = targetIpv4Address_;

    IN_ADDR literalAddress{};
    const int parseResult = InetPtonW(AF_INET, config_.icmpTargetIpv4.c_str(), &literalAddress);
    if (parseResult == 1)
    {
        targetIpv4Address_ = ipv4AddressFromInAddr(literalAddress);
        targetIsDomain_ = false;
        Logger::info(
            "ICMP target resolved as IPv4 literal: ip={}",
            formatIpv4Address(targetIpv4Address_).view());
        return true;
    }

    targetIsDomain_ = true;

    ADDRINFOW hints{};
    hints.ai_family = AF_INET;

    ADDRINFOW* result = nullptr;
    const int resolveResult = GetAddrInfoW(config_.icmpTargetIpv4.c_str(), nullptr, &hints, &result);
    if (resolveResult != 0 || result == nullptr)
    {
        if (allowFallbackToPrevious && previousAddress != 0U)
        {
            Logger::warn(
                "ICMP DNS refresh failed; keeping previous target IP: error={}, previous_ip={}",
                resolveResult,
                formatIpv4Address(previousAddress).view());
            targetIpv4Address_ = previousAddress;
            return true;
        }

        Logger::warn("ICMP DNS resolution failed: error={}", resolveResult);
        return false;
    }

    if (result->ai_addr == nullptr || result->ai_addrlen < sizeof(SOCKADDR_IN))
    {
        FreeAddrInfoW(result);

        if (allowFallbackToPrevious && previousAddress != 0U)
        {
            Logger::warn(
                "ICMP DNS refresh returned an unusable address record; keeping previous target IP: previous_ip={}",
                formatIpv4Address(previousAddress).view());
            targetIpv4Address_ = previousAddress;
            return true;
        }

        Logger::warn("ICMP DNS resolution failed: unusable address record");
        return false;
    }

    const auto* socketAddress = reinterpret_cast<const SOCKADDR_IN*>(result->ai_addr);
    const std::uint32_t resolvedAddress = ipv4AddressFromInAddr(socketAddress->sin_addr);
    FreeAddrInfoW(result);

    if (resolvedAddress == 0U)
    {
        if (allowFallbackToPrevious && previousAddress != 0U)
        {
            Logger::warn(
                "ICMP DNS refresh returned an invalid IPv4 address; keeping previous target IP: previous_ip={}",
                formatIpv4Address(previousAddress).view());
            targetIpv4Address_ = previousAddress;
            return true;
        }

        Logger::warn("ICMP DNS resolution failed: resolved IPv4 address is zero");
        return false;
    }

    targetIpv4Address_ = resolvedAddress;

    if (previousAddress != 0U && previousAddress != resolvedAddress)
    {
        Logger::info(
            "ICMP DNS target refreshed: old_ip={}, new_ip={}",
            formatIpv4Address(previousAddress).view(),
            formatIpv4Address(resolvedAddress).view());
    }
    else
    {
        Logger::info("ICMP DNS target resolved: ip={}", formatIpv4Address(resolvedAddress).view());
    }

    return true;
}


bool LatencyMetricsCollector::waitInterruptible(std::chrono::milliseconds interval) noexcept
{
    auto remaining = interval;
    while (remaining.count() > 0 && running_.load(std::memory_order_acquire))
    {
        const auto sleepDuration = remaining < kSensorWaitSlice ? remaining : kSensorWaitSlice;
        std::this_thread::sleep_for(sleepDuration);
        remaining -= sleepDuration;
    }

    return running_.load(std::memory_order_acquire);
}

std::optional<double> LatencyMetricsCollector::sampleIcmpRttMs() noexcept
{
    if (!icmpHandle_.isValid() || targetIpv4Address_ == 0U)
    {
        Logger::warn("ICMP RTT sampling skipped: sensor handle is not prepared");
        return std::nullopt;
    }

    std::array<char, kIcmpRequestPayloadBytes> requestPayload{'G', 'O', 'P', 'T', 'I', 'M', 'Z', 'R'};
    std::array<std::byte, sizeof(ICMP_ECHO_REPLY) + kIcmpRequestPayloadBytes + kIcmpReplyBufferExtraBytes> replyBuffer{};
    const DWORD replyBufferSize = static_cast<DWORD>(replyBuffer.size());

    const DWORD replyCount = IcmpSendEcho(
        icmpHandle_.get(),
        targetIpv4Address_,
        requestPayload.data(),
        static_cast<WORD>(requestPayload.size()),
        nullptr,
        replyBuffer.data(),
        replyBufferSize,
        static_cast<DWORD>(config_.icmpTimeout.count()));

    if (replyCount == 0)
    {
        recordIcmpSampleFailure("timeout_or_send_failed");
        return std::nullopt;
    }

    const auto* reply = reinterpret_cast<const ICMP_ECHO_REPLY*>(replyBuffer.data());
    if (reply->Status != IP_SUCCESS)
    {
        recordIcmpSampleFailure("reply_status_failed");
        return std::nullopt;
    }

    recordIcmpSampleSuccess();
    return static_cast<double>(reply->RoundTripTime);
}

void LatencyMetricsCollector::recordIcmpSampleSuccess() noexcept
{
    if (consecutiveIcmpFailureCount_ != 0U)
    {
        Logger::info(
            "ICMP sensor recovered after {} consecutive failure(s)",
            consecutiveIcmpFailureCount_);
    }

    consecutiveIcmpFailureCount_ = 0U;
}

void LatencyMetricsCollector::recordIcmpSampleFailure(const char* reason) noexcept
{
    ++consecutiveIcmpFailureCount_;

    if (consecutiveIcmpFailureCount_ == 1U)
    {
        Logger::info(
            "ICMP RTT transient sample miss: reason={}, consecutive_failures={}/{}",
            reason,
            consecutiveIcmpFailureCount_,
            kMaxConsecutiveIcmpFailuresBeforeReopen);
        return;
    }

    // Polling sensors must not emit repeated I/O while the network path is down.
    // Failures 2..threshold-1 are intentionally silent; the next log is the
    // self-healing state transition at the threshold.
    if (consecutiveIcmpFailureCount_ < kMaxConsecutiveIcmpFailuresBeforeReopen)
    {
        return;
    }

    if (reopenIcmpHandleAfterFailures())
    {
        consecutiveIcmpFailureCount_ = 0U;
        return;
    }

    // Keep the counter one step below the threshold to avoid reopening and
    // logging on every single failed sample while the network stack is down.
    consecutiveIcmpFailureCount_ = kMaxConsecutiveIcmpFailuresBeforeReopen - 1U;
}

bool LatencyMetricsCollector::reopenIcmpHandleAfterFailures() noexcept
{
    Logger::warn(
        "ICMP sensor self-healing: reopening stale handle after {} consecutive failure(s)",
        consecutiveIcmpFailureCount_);

    icmpHandle_.close();

    if (targetIsDomain_)
    {
        // A stale ICMP handle often follows adapter reset or network migration.
        // Domain targets may also point to a different IP after that event, so
        // refresh DNS before reopening the handle. DNS failure keeps the last
        // known IP as a bounded fallback and never blocks the watchdog thread.
        if (!resolveTargetAddress(true))
        {
            Logger::warn("ICMP sensor self-healing failed: DNS refresh had no usable fallback");
            return false;
        }
    }

    if (!icmpHandle_.open())
    {
        Logger::warn("ICMP sensor self-healing failed: IcmpCreateFile failed");
        return false;
    }

    Logger::info(
        "ICMP sensor self-healing completed: ICMP handle reopened, target_ip={}",
        formatIpv4Address(targetIpv4Address_).view());
    return true;
}

void LatencyMetricsCollector::pushRttSample(double rttMs) noexcept
{
    if (!std::isfinite(rttMs) || rttMs < 0.0)
    {
        return;
    }

    const std::lock_guard lock(rttMutex_);
    rttRingBufferMs_[rttWriteIndex_] = rttMs;
    rttWriteIndex_ = (rttWriteIndex_ + 1U) % rttRingBufferMs_.size();
    rttSampleCount_ = std::min(rttSampleCount_ + 1U, rttRingBufferMs_.size());
    cachedRttJitterMs_.store(calculateRttJitterMsLocked(), std::memory_order_relaxed);
}

double LatencyMetricsCollector::calculateRttJitterMsLocked() const noexcept
{
    if (rttSampleCount_ < kMinimumRttSamplesForJitter)
    {
        return 0.0;
    }

    double sum = 0.0;
    for (std::size_t index = 0; index < rttSampleCount_; ++index)
    {
        sum += rttRingBufferMs_[index];
    }

    const double mean = sum / static_cast<double>(rttSampleCount_);

    double squaredErrorSum = 0.0;
    for (std::size_t index = 0; index < rttSampleCount_; ++index)
    {
        const double error = rttRingBufferMs_[index] - mean;
        squaredErrorSum += error * error;
    }

    return std::sqrt(squaredErrorSum / static_cast<double>(rttSampleCount_));
}

int LatencyMetricsCollector::estimateDpcSpikeCount() noexcept
{
    // Phase-2 placeholder.
    // Real DPC monitoring requires an ETW/PDH-backed module. Returning zero keeps
    // IRQ_REPIN disabled until the network/interrupt module owns this metric.
    return 0;
}
