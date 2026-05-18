// Build: cl /std:c++latest /O2 /W4 /WX /permissive- ThreadTracker.cpp
// MODULE: ThreadTracker
// ERROR-POLICY: expected
// Reason: thread observation failures are recoverable Win32/runtime conditions.

#include "ThreadTracker.h"

#include <TlHelp32.h>
#include <chrono>
#include <thread>
#include <utility>

#include "Logger.h"
#include "WinApiError.h"

ThreadTracker::ThreadTracker(
    DWORD processId,
    ThreadTrackerConfig config) noexcept
    : processId_(processId),
      config_(config)
{
    normalizeConfig();
    initializeFixedStorage();
}

void ThreadTracker::normalizeConfig() noexcept
{
    if (config_.emaAlpha < 0.2)
    {
        config_.emaAlpha = 0.2;
    }
    else if (config_.emaAlpha > 0.3)
    {
        config_.emaAlpha = 0.3;
    }

    if (config_.sampleCount < 1)
    {
        config_.sampleCount = 1;
    }

    if (config_.sampleInterval < std::chrono::milliseconds(0))
    {
        config_.sampleInterval = std::chrono::milliseconds(0);
    }

    if (config_.stickinessDuration < std::chrono::milliseconds(0))
    {
        config_.stickinessDuration = ThreadTrackerPolicy::kDefaultStickinessDuration;
    }
    else if (config_.stickinessDuration > ThreadTrackerPolicy::kMaxStickinessDuration)
    {
        config_.stickinessDuration = ThreadTrackerPolicy::kMaxStickinessDuration;
    }
}

void ThreadTracker::initializeFixedStorage() noexcept
{
    activeSlots_.fill(kInvalidSlot);
    activePositions_.fill(kInvalidSlot);
    hashThreadIds_.fill(0);
    hashSlots_.fill(kInvalidSlot);
    hashOccupied_.fill(false);

    activeSlotCount_ = 0;
    freeSlotCount_ = kMaxTrackedThreads;
    for (std::size_t index = 0; index < kMaxTrackedThreads; ++index)
    {
        freeSlots_[index] = kMaxTrackedThreads - 1U - index;
    }
}

void ThreadTracker::resetFixedStorageAfterInvariantFailure() noexcept
{
    Logger::error(
        "thread tracker fixed storage invariant failed; resetting all tracked samples");

    for (ThreadSample& sample : samples_)
    {
        sample.reset();
    }

    initializeFixedStorage();
    sampleCapacityWarned_ = false;
    resetCandidateState();
}

std::size_t ThreadTracker::hashThreadId(DWORD threadId) noexcept
{
    return (static_cast<std::size_t>(threadId) * 2654435761ULL) & (kHashCapacity - 1U);
}

std::uint64_t ThreadTracker::fileTimeToUint64(const FILETIME& fileTime) noexcept
{
    ULARGE_INTEGER value{};
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;

    return value.QuadPart;
}

std::expected<std::uint64_t, ErrorCode>
ThreadTracker::queryThreadCpuTime100ns(HANDLE threadHandle) noexcept
{
    FILETIME creationTime{};
    FILETIME exitTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};

    if (!GetThreadTimes(
            threadHandle,
            &creationTime,
            &exitTime,
            &kernelTime,
            &userTime))
    {
        const DWORD lastError = GetLastError();
        if (lastError == ERROR_ACCESS_DENIED)
        {
            return std::unexpected(ErrorCode::AccessDenied);
        }

        return std::unexpected(ErrorCode::ThreadTimeQueryFailed);
    }

    return fileTimeToUint64(kernelTime) + fileTimeToUint64(userTime);
}

std::expected<WinHandle, ErrorCode>
ThreadTracker::openThreadHandle(DWORD threadId) const noexcept
{
    WinHandle threadHandle(OpenThread(
        THREAD_QUERY_LIMITED_INFORMATION,
        FALSE,
        threadId));

    if (!threadHandle)
    {
        return std::unexpected(mapLastErrorToErrorCode(GetLastError()));
    }

    return threadHandle;
}

std::size_t ThreadTracker::findSlotIndex(DWORD threadId) const noexcept
{
    std::size_t probeIndex = hashThreadId(threadId);
    for (std::size_t probeCount = 0; probeCount < kHashCapacity; ++probeCount)
    {
        if (!hashOccupied_[probeIndex])
        {
            return kInvalidSlot;
        }

        if (hashThreadIds_[probeIndex] == threadId)
        {
            return hashSlots_[probeIndex];
        }

        probeIndex = (probeIndex + 1U) & (kHashCapacity - 1U);
    }

    return kInvalidSlot;
}

bool ThreadTracker::insertSlotIndex(DWORD threadId, std::size_t slotIndex) noexcept
{
    std::size_t probeIndex = hashThreadId(threadId);
    for (std::size_t probeCount = 0; probeCount < kHashCapacity; ++probeCount)
    {
        if (!hashOccupied_[probeIndex] || hashThreadIds_[probeIndex] == threadId)
        {
            hashOccupied_[probeIndex] = true;
            hashThreadIds_[probeIndex] = threadId;
            hashSlots_[probeIndex] = slotIndex;
            return true;
        }

        probeIndex = (probeIndex + 1U) & (kHashCapacity - 1U);
    }

    return false;
}

void ThreadTracker::eraseSlotIndex(DWORD threadId) noexcept
{
    std::size_t probeIndex = hashThreadId(threadId);
    for (std::size_t probeCount = 0; probeCount < kHashCapacity; ++probeCount)
    {
        if (!hashOccupied_[probeIndex])
        {
            return;
        }

        if (hashThreadIds_[probeIndex] == threadId)
        {
            hashOccupied_[probeIndex] = false;
            hashThreadIds_[probeIndex] = 0;
            hashSlots_[probeIndex] = kInvalidSlot;

            std::size_t nextIndex = (probeIndex + 1U) & (kHashCapacity - 1U);
            while (hashOccupied_[nextIndex])
            {
                const DWORD rehashThreadId = hashThreadIds_[nextIndex];
                const std::size_t rehashSlot = hashSlots_[nextIndex];
                hashOccupied_[nextIndex] = false;
                hashThreadIds_[nextIndex] = 0;
                hashSlots_[nextIndex] = kInvalidSlot;
                static_cast<void>(insertSlotIndex(rehashThreadId, rehashSlot));
                nextIndex = (nextIndex + 1U) & (kHashCapacity - 1U);
            }

            return;
        }

        probeIndex = (probeIndex + 1U) & (kHashCapacity - 1U);
    }
}

ThreadTracker::ThreadSample* ThreadTracker::findSample(DWORD threadId) noexcept
{
    const std::size_t slotIndex = findSlotIndex(threadId);
    if (slotIndex == kInvalidSlot)
    {
        return nullptr;
    }

    ThreadSample& sample = samples_[slotIndex];
    if (!sample.active || sample.threadId != threadId)
    {
        return nullptr;
    }

    return &sample;
}

const ThreadTracker::ThreadSample* ThreadTracker::findSample(DWORD threadId) const noexcept
{
    const std::size_t slotIndex = findSlotIndex(threadId);
    if (slotIndex == kInvalidSlot)
    {
        return nullptr;
    }

    const ThreadSample& sample = samples_[slotIndex];
    if (!sample.active || sample.threadId != threadId)
    {
        return nullptr;
    }

    return &sample;
}

ThreadTracker::ThreadSample* ThreadTracker::findOrCreateSample(DWORD threadId) noexcept
{
    if (ThreadSample* existingSample = findSample(threadId); existingSample != nullptr)
    {
        return existingSample;
    }

    if (freeSlotCount_ == 0 || activeSlotCount_ >= kMaxTrackedThreads)
    {
        if (!sampleCapacityWarned_)
        {
            Logger::warn(
                "thread tracker capacity reached; additional threads will be ignored (capacity={})",
                kMaxTrackedThreads);
            sampleCapacityWarned_ = true;
        }

        return nullptr;
    }

    const std::size_t slotIndex = freeSlots_[--freeSlotCount_];
    if (!insertSlotIndex(threadId, slotIndex))
    {
        freeSlots_[freeSlotCount_++] = slotIndex;
        return nullptr;
    }

    ThreadSample& sample = samples_[slotIndex];
    sample.reset();
    sample.threadId = threadId;
    sample.active = true;

    activePositions_[slotIndex] = activeSlotCount_;
    activeSlots_[activeSlotCount_++] = slotIndex;

    return &sample;
}

void ThreadTracker::releaseSampleSlot(std::size_t slotIndex) noexcept
{
    if (slotIndex >= kMaxTrackedThreads)
    {
        Logger::error(
            "thread tracker release requested with invalid slot index: slot={}",
            slotIndex);
        resetFixedStorageAfterInvariantFailure();
        return;
    }

    ThreadSample& sample = samples_[slotIndex];
    if (!sample.active)
    {
        return;
    }

    const std::size_t activePosition = activePositions_[slotIndex];
    if (activePosition == kInvalidSlot || activePosition >= activeSlotCount_)
    {
        Logger::error(
            "thread tracker active position invariant failed: slot={}, activePosition={}, activeCount={}",
            slotIndex,
            activePosition,
            activeSlotCount_);
        resetFixedStorageAfterInvariantFailure();
        return;
    }

    if (activeSlots_[activePosition] != slotIndex)
    {
        Logger::error(
            "thread tracker active slot invariant failed: slot={}, activePosition={}, mappedSlot={}",
            slotIndex,
            activePosition,
            activeSlots_[activePosition]);
        resetFixedStorageAfterInvariantFailure();
        return;
    }

    const DWORD oldThreadId = sample.threadId;
    eraseSlotIndex(oldThreadId);
    sample.reset();

    const std::size_t lastPosition = activeSlotCount_ - 1U;
    const std::size_t movedSlot = activeSlots_[lastPosition];
    if (activePosition != lastPosition)
    {
        activeSlots_[activePosition] = movedSlot;
        activePositions_[movedSlot] = activePosition;
    }

    activeSlots_[lastPosition] = kInvalidSlot;
    --activeSlotCount_;
    activePositions_[slotIndex] = kInvalidSlot;

    if (freeSlotCount_ >= kMaxTrackedThreads)
    {
        Logger::error(
            "thread tracker free-list invariant failed while releasing slot: slot={}, freeCount={}",
            slotIndex,
            freeSlotCount_);
        resetFixedStorageAfterInvariantFailure();
        return;
    }

    freeSlots_[freeSlotCount_++] = slotIndex;
}

void ThreadTracker::releaseAllSamples() noexcept
{
    while (activeSlotCount_ > 0)
    {
        releaseSampleSlot(activeSlots_[activeSlotCount_ - 1U]);
    }
}

std::size_t ThreadTracker::activeSampleCount() const noexcept
{
    return activeSlotCount_;
}

void ThreadTracker::deactivateMissingSamples() noexcept
{
    std::size_t index = 0;
    while (index < activeSlotCount_)
    {
        const std::size_t slotIndex = activeSlots_[index];
        ThreadSample& sample = samples_[slotIndex];

        if (!sample.seenInCycle)
        {
            releaseSampleSlot(slotIndex);
            continue;
        }

        ++index;
    }

    if (activeSlotCount_ == 0)
    {
        resetCandidateState();
    }
}

void ThreadTracker::resetMultiSampleAccumulators() noexcept
{
    for (std::size_t index = 0; index < activeSlotCount_; ++index)
    {
        ThreadSample& sample = samples_[activeSlots_[index]];
        sample.accumulatedDelta100ns = 0;
        sample.accumulatedDeltaCount = 0;
        sample.hasWindowBaseline = false;
    }
}

std::expected<void, ErrorCode> ThreadTracker::observeOnce() noexcept
{
    for (std::size_t index = 0; index < activeSlotCount_; ++index)
    {
        samples_[activeSlots_[index]].seenInCycle = false;
    }

    WinHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
    if (!snapshot)
    {
        return std::unexpected(ErrorCode::SnapshotCreationFailed);
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    if (!Thread32First(snapshot.get(), &entry))
    {
        const DWORD lastError = GetLastError();
        if (lastError == ERROR_NO_MORE_FILES)
        {
            releaseAllSamples();
            resetCandidateState();
            return {};
        }

        return std::unexpected(ErrorCode::EnumerationFailed);
    }

    do
    {
        if (entry.th32OwnerProcessID != processId_)
        {
            continue;
        }

        ThreadSample* sample = findOrCreateSample(entry.th32ThreadID);
        if (sample == nullptr)
        {
            continue;
        }

        sample->seenInCycle = true;

        if (!sample->threadHandle)
        {
            auto openedHandle = openThreadHandle(entry.th32ThreadID);
            if (!openedHandle)
            {
                continue;
            }

            WinHandle& threadHandle = *openedHandle;
            sample->threadHandle = std::move(threadHandle);
        }

        auto currentCpuTime = queryThreadCpuTime100ns(sample->threadHandle.get());
        if (!currentCpuTime)
        {
            sample->threadHandle.reset();
            continue;
        }

        const std::uint64_t& cpuTime100ns = *currentCpuTime;
        sample->currentCpuTime100ns = cpuTime100ns;

        if (sample->hasWindowBaseline &&
            sample->previousCpuTime100ns != 0 &&
            sample->currentCpuTime100ns >= sample->previousCpuTime100ns)
        {
            const std::uint64_t delta =
                sample->currentCpuTime100ns - sample->previousCpuTime100ns;
            sample->accumulatedDelta100ns += delta;
            ++sample->accumulatedDeltaCount;
        }

        sample->previousCpuTime100ns = sample->currentCpuTime100ns;
        sample->hasWindowBaseline = true;
    }
    while (Thread32Next(snapshot.get(), &entry));

    const DWORD lastError = GetLastError();
    if (lastError != ERROR_NO_MORE_FILES)
    {
        return std::unexpected(ErrorCode::EnumerationFailed);
    }

    deactivateMissingSamples();
    return {};
}

void ThreadTracker::finalizeMultiSample() noexcept
{
    for (std::size_t index = 0; index < activeSlotCount_; ++index)
    {
        ThreadSample& sample = samples_[activeSlots_[index]];
        if (sample.accumulatedDeltaCount <= 0)
        {
            sample.deltaCpuTime100ns = 0;
            continue;
        }

        sample.deltaCpuTime100ns =
            sample.accumulatedDelta100ns / static_cast<std::uint64_t>(sample.accumulatedDeltaCount);

        const double currentDelta = static_cast<double>(sample.deltaCpuTime100ns);
        if (sample.hasEma)
        {
            sample.emaCpuTime100ns =
                (config_.emaAlpha * currentDelta) +
                ((1.0 - config_.emaAlpha) * sample.emaCpuTime100ns);
        }
        else
        {
            sample.emaCpuTime100ns = currentDelta;
            sample.hasEma = true;
        }
    }
}

std::expected<void, ErrorCode> ThreadTracker::update() noexcept
{
    return update(std::stop_token{});
}

std::expected<void, ErrorCode> ThreadTracker::update(std::stop_token stopToken) noexcept
{
    resetMultiSampleAccumulators();

    const int sampleCount = config_.sampleCount;
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        if (stopToken.stop_requested())
        {
            return {};
        }

        auto observeResult = observeOnce();
        if (!observeResult)
        {
            Logger::error(
                "thread tracker observation failed during sample {}/{}: {}",
                sampleIndex + 1,
                sampleCount,
                toString(observeResult.error()));
            return std::unexpected(observeResult.error());
        }

        if (sampleIndex + 1 < sampleCount)
        {
            if (!waitForNextSample(stopToken, config_.sampleInterval))
            {
                return {};
            }
        }
    }

    finalizeMultiSample();
    updateCandidateState(std::chrono::steady_clock::now());
    return {};
}

bool ThreadTracker::waitForNextSample(
    std::stop_token stopToken,
    std::chrono::milliseconds interval) noexcept
{
    if (interval.count() <= 0)
    {
        return !stopToken.stop_requested();
    }

    try
    {
        std::mutex waitMutex;
        std::condition_variable_any condition;
        std::unique_lock lock(waitMutex);
        condition.wait_for(
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
        return false;
    }

    return !stopToken.stop_requested();
}

std::expected<ThreadInfoBuffer, ErrorCode>
ThreadTracker::getTopThreads(std::size_t maxCount) const noexcept
{
    ThreadInfoBuffer topThreads;
    const std::size_t effectiveMaxCount = maxCount < ThreadInfoBuffer::kCapacity
        ? maxCount
        : ThreadInfoBuffer::kCapacity;

    if (effectiveMaxCount == 0U)
    {
        return topThreads;
    }

    for (std::size_t index = 0; index < activeSlotCount_; ++index)
    {
        const ThreadSample& sample = samples_[activeSlots_[index]];
        const ThreadInfo candidate{
            .deltaCpuTime100ns = sample.deltaCpuTime100ns,
            .emaCpuTime100ns = sample.emaCpuTime100ns,
            .threadId = sample.threadId,
            .isEmaCandidate = emaCandidateThreadId_.has_value() &&
                              *emaCandidateThreadId_ == sample.threadId,
            .isMainThread = mainThreadId_.has_value() &&
                            *mainThreadId_ == sample.threadId
        };

        std::array<ThreadInfo, ThreadInfoBuffer::kCapacity> ordered{};
        std::size_t orderedSize = 0;
        for (const ThreadInfo& existing : topThreads)
        {
            ordered[orderedSize] = existing;
            ++orderedSize;
        }

        std::size_t insertPosition = 0;
        while (insertPosition < orderedSize &&
               ordered[insertPosition].emaCpuTime100ns >= candidate.emaCpuTime100ns)
        {
            ++insertPosition;
        }

        if (insertPosition >= effectiveMaxCount)
        {
            continue;
        }

        if (orderedSize < effectiveMaxCount)
        {
            ++orderedSize;
        }

        std::size_t shiftIndex = orderedSize - 1U;
        while (shiftIndex > insertPosition)
        {
            ordered[shiftIndex] = ordered[shiftIndex - 1U];
            --shiftIndex;
        }
        ordered[insertPosition] = candidate;

        topThreads.clear();
        for (std::size_t copyIndex = 0; copyIndex < orderedSize; ++copyIndex)
        {
            if (!topThreads.pushBack(ordered[copyIndex]))
            {
                return std::unexpected(ErrorCode::InternalError);
            }
        }
    }

    return topThreads;
}

std::optional<DWORD> ThreadTracker::getEmaCandidateThreadId() const noexcept
{
    return emaCandidateThreadId_;
}

std::optional<DWORD> ThreadTracker::getMainThreadId() const noexcept
{
    return mainThreadId_;
}

int ThreadTracker::consumeThreadMigrationCount() noexcept
{
    const int value = threadMigrationCount_;
    threadMigrationCount_ = 0;
    return value;
}

void ThreadTracker::increaseStickinessBy(std::chrono::milliseconds delta) noexcept
{
    if (delta <= std::chrono::milliseconds(0))
    {
        return;
    }

    const auto oldDuration = config_.stickinessDuration;
    config_.stickinessDuration += delta;
    if (config_.stickinessDuration > ThreadTrackerPolicy::kMaxStickinessDuration)
    {
        config_.stickinessDuration = ThreadTrackerPolicy::kMaxStickinessDuration;
    }

    Logger::info(
        "stickiness updated: {} ms -> {} ms (cap={} ms)",
        oldDuration.count(),
        config_.stickinessDuration.count(),
        ThreadTrackerPolicy::kMaxStickinessDuration.count());

    if (oldDuration == ThreadTrackerPolicy::kMaxStickinessDuration &&
        config_.stickinessDuration == ThreadTrackerPolicy::kMaxStickinessDuration)
    {
        Logger::warn(
            "STICKY_UP ignored because stickiness is already capped at {} ms",
            ThreadTrackerPolicy::kMaxStickinessDuration.count());
    }
}

void ThreadTracker::updateCandidateState(
    std::chrono::steady_clock::time_point now) noexcept
{
    std::optional<DWORD> bestThreadId;
    double bestEma = 0.0;

    for (std::size_t index = 0; index < activeSlotCount_; ++index)
    {
        const ThreadSample& sample = samples_[activeSlots_[index]];
        if (!sample.hasEma)
        {
            continue;
        }

        if (!bestThreadId.has_value() || sample.emaCpuTime100ns > bestEma)
        {
            bestThreadId = sample.threadId;
            bestEma = sample.emaCpuTime100ns;
        }
    }

    if (!bestThreadId.has_value())
    {
        resetCandidateState();
        return;
    }

    if (!emaCandidateThreadId_.has_value() || *emaCandidateThreadId_ != *bestThreadId)
    {
        emaCandidateThreadId_ = *bestThreadId;
        mainThreadId_.reset();
        candidateStartedAt_ = now;
        return;
    }

    if (!candidateStartedAt_.has_value())
    {
        candidateStartedAt_ = now;
        return;
    }

    const auto stableDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - *candidateStartedAt_);

    if (stableDuration >= config_.stickinessDuration)
    {
        if (!mainThreadId_.has_value() || *mainThreadId_ != *bestThreadId)
        {
            if (lastConfirmedMainThreadId_.has_value() && *lastConfirmedMainThreadId_ != *bestThreadId)
            {
                ++threadMigrationCount_;
                Logger::warn(
                    "main thread migration observed: previous confirmed TID {} -> new TID {} (cycleCount={})",
                    *lastConfirmedMainThreadId_,
                    *bestThreadId,
                    threadMigrationCount_);
            }

            mainThreadId_ = *bestThreadId;
            lastConfirmedMainThreadId_ = *bestThreadId;
        }
    }
}

void ThreadTracker::resetCandidateState() noexcept
{
    emaCandidateThreadId_.reset();
    mainThreadId_.reset();
    lastConfirmedMainThreadId_.reset();
    candidateStartedAt_.reset();
}
