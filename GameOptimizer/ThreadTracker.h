// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: ThreadTracker
// ERROR-POLICY: expected
// Reason: thread observation failures are recoverable Win32/runtime conditions.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

#include "ErrorCode.h"
#include "ThreadInfo.h"
#include "WinHandle.h"

namespace ThreadTrackerPolicy
{
    inline constexpr auto kDefaultStickinessDuration = std::chrono::milliseconds(4000);
    inline constexpr auto kMaxStickinessDuration = std::chrono::milliseconds(7000);
    inline constexpr auto kStickyUpStep = std::chrono::milliseconds(1000);
}


class ThreadInfoBuffer
{
public:
    static constexpr std::size_t kCapacity = 8;

    [[nodiscard]] bool pushBack(const ThreadInfo& threadInfo) noexcept
    {
        if (size_ >= entries_.size())
        {
            return false;
        }

        entries_[size_] = threadInfo;
        ++size_;
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return size_ == 0U;
    }

    void clear() noexcept
    {
        size_ = 0;
    }

    [[nodiscard]] const ThreadInfo* begin() const noexcept
    {
        return entries_.data();
    }

    [[nodiscard]] const ThreadInfo* end() const noexcept
    {
        return entries_.data() + size_;
    }

private:
    std::array<ThreadInfo, kCapacity> entries_{};
    std::size_t size_ = 0;
};

struct ThreadTrackerConfig
{
    double emaAlpha = 0.25;
    std::chrono::milliseconds stickinessDuration = ThreadTrackerPolicy::kDefaultStickinessDuration;
    int sampleCount = 3;
    std::chrono::milliseconds sampleInterval = std::chrono::milliseconds(50);
};

enum class ThreadTrackerUpdateDisposition
{
    Updated,
    StopRequested,
    ResetAfterInvariantFailure
};

struct ThreadTrackerTelemetry
{
    std::uint64_t totalOpenThreadFailures = 0;
    std::uint64_t totalOpenThreadAccessDeniedFailures = 0;
    std::uint64_t totalThreadTimeQueryFailures = 0;
    std::uint64_t totalThreadTimeAccessDeniedFailures = 0;
    std::uint64_t totalResetEvents = 0;
    std::uint32_t lastUpdateOpenThreadFailures = 0;
    std::uint32_t lastUpdateOpenThreadAccessDeniedFailures = 0;
    std::uint32_t lastUpdateThreadTimeQueryFailures = 0;
    std::uint32_t lastUpdateThreadTimeAccessDeniedFailures = 0;
};

class ThreadTracker
{
public:
    explicit ThreadTracker(
        DWORD processId,
        ThreadTrackerConfig config = ThreadTrackerConfig{}) noexcept;

    [[nodiscard]] std::expected<ThreadTrackerUpdateDisposition, ErrorCode> update() noexcept;
    [[nodiscard]] std::expected<ThreadTrackerUpdateDisposition, ErrorCode> update(std::stop_token stopToken) noexcept;

    [[nodiscard]] std::expected<ThreadInfoBuffer, ErrorCode>
    getTopThreads(std::size_t maxCount) const noexcept;

    [[nodiscard]] std::optional<DWORD> getEmaCandidateThreadId() const noexcept;
    [[nodiscard]] std::optional<DWORD> getMainThreadId() const noexcept;
    [[nodiscard]] ThreadTrackerTelemetry telemetry() const noexcept;
    [[nodiscard]] int consumeThreadMigrationCount() noexcept;
    void increaseStickinessBy(std::chrono::milliseconds delta) noexcept;

private:
    static constexpr std::size_t kMaxTrackedThreads = 1024;
    static constexpr std::size_t kHashCapacity = 2048;
    static constexpr std::size_t kInvalidSlot = std::numeric_limits<std::size_t>::max();

    struct ThreadSample
    {
        ThreadSample() noexcept
            : threadHandle(nullptr)
        {
        }

        ThreadSample(const ThreadSample&) = delete;
        ThreadSample& operator=(const ThreadSample&) = delete;
        ThreadSample(ThreadSample&&) noexcept = delete;
        ThreadSample& operator=(ThreadSample&&) noexcept = delete;

        void reset() noexcept
        {
            threadHandle.reset();
            threadId = 0;
            previousCpuTime100ns = 0;
            currentCpuTime100ns = 0;
            deltaCpuTime100ns = 0;
            accumulatedDelta100ns = 0;
            accumulatedDeltaCount = 0;
            emaCpuTime100ns = 0.0;
            hasEma = false;
            eligibleForCandidate = false;
            hasWindowBaseline = false;
            active = false;
            seenInCycle = false;
        }

        DWORD threadId = 0;
        WinHandle threadHandle{nullptr};
        std::uint64_t previousCpuTime100ns = 0;
        std::uint64_t currentCpuTime100ns = 0;
        std::uint64_t deltaCpuTime100ns = 0;
        std::uint64_t accumulatedDelta100ns = 0;
        int accumulatedDeltaCount = 0;
        double emaCpuTime100ns = 0.0;
        bool hasEma = false;
        bool eligibleForCandidate = false;
        bool hasWindowBaseline = false;
        bool active = false;
        bool seenInCycle = false;
    };

    [[nodiscard]] static std::expected<std::uint64_t, ErrorCode>
    queryThreadCpuTime100ns(HANDLE threadHandle) noexcept;

    [[nodiscard]] static std::uint64_t fileTimeToUint64(const FILETIME& fileTime) noexcept;

    [[nodiscard]] std::expected<WinHandle, ErrorCode> openThreadHandle(DWORD threadId) const noexcept;
    void normalizeConfig() noexcept;
    void initializeFixedStorage() noexcept;
    void resetFixedStorageAfterInvariantFailure() noexcept;
    [[nodiscard]] static std::size_t hashThreadId(DWORD threadId) noexcept;
    [[nodiscard]] std::size_t findSlotIndex(DWORD threadId) const noexcept;
    [[nodiscard]] bool insertSlotIndex(DWORD threadId, std::size_t slotIndex) noexcept;
    void eraseSlotIndex(DWORD threadId) noexcept;
    [[nodiscard]] ThreadSample* findSample(DWORD threadId) noexcept;
    [[nodiscard]] const ThreadSample* findSample(DWORD threadId) const noexcept;
    [[nodiscard]] ThreadSample* findOrCreateSample(DWORD threadId) noexcept;
    void releaseSampleSlot(std::size_t slotIndex) noexcept;
    void releaseAllSamples() noexcept;
    void deactivateMissingSamples() noexcept;
    [[nodiscard]] std::size_t activeSampleCount() const noexcept;
    [[nodiscard]] std::expected<void, ErrorCode> observeOnce() noexcept;
    [[nodiscard]] bool waitForNextSample(
        std::stop_token stopToken,
        std::chrono::milliseconds interval) noexcept;
    void finalizeMultiSample() noexcept;
    void resetMultiSampleAccumulators() noexcept;

    void updateCandidateState(std::chrono::steady_clock::time_point now) noexcept;
    void resetCandidateState() noexcept;
    void resetLastUpdateTelemetry() noexcept;
    void recordOpenThreadFailure(ErrorCode errorCode) noexcept;
    void recordThreadTimeQueryFailure(ErrorCode errorCode) noexcept;
    void logLastUpdateTelemetry() const noexcept;

private:
    DWORD processId_ = 0;
    ThreadTrackerConfig config_;
    std::array<ThreadSample, kMaxTrackedThreads> samples_{};
    std::array<std::size_t, kMaxTrackedThreads> activeSlots_{};
    std::array<std::size_t, kMaxTrackedThreads> activePositions_{};
    std::size_t activeSlotCount_ = 0;
    std::array<std::size_t, kMaxTrackedThreads> freeSlots_{};
    std::size_t freeSlotCount_ = 0;
    std::array<DWORD, kHashCapacity> hashThreadIds_{};
    std::array<std::size_t, kHashCapacity> hashSlots_{};
    std::array<bool, kHashCapacity> hashOccupied_{};
    bool sampleCapacityWarned_ = false;
    std::optional<DWORD> emaCandidateThreadId_;
    std::optional<DWORD> mainThreadId_;
    std::optional<DWORD> lastConfirmedMainThreadId_;
    std::optional<std::chrono::steady_clock::time_point> candidateStartedAt_;
    int threadMigrationCount_ = 0;
    ThreadTrackerTelemetry telemetry_{};
    bool resetOccurredDuringUpdate_ = false;
    std::mutex sampleWaitMutex_;
    std::condition_variable_any sampleWaitCondition_;
};
