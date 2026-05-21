// Build: included by RuntimeOrchestrator.cpp and WatchdogCycleRunner.cpp
// MODULE: WatchdogCycleRunner
// ERROR-POLICY: expected
// Reason: owns one watchdog policy cycle without owning runtime startup or shutdown.

#pragma once

#include <atomic>
#include <stop_token>

#include "RuntimeContext.h"

using RuntimeShutdownRequest = void (*)() noexcept;

class WatchdogCycleRunner
{
public:
    WatchdogCycleRunner(
        RuntimeContext& context,
        std::atomic_bool& runtimeTimeoutRequested,
        RuntimeShutdownRequest requestShutdown) noexcept;

    void runCycle(std::stop_token stopToken) noexcept;

private:
    [[nodiscard]] bool updateThreads(std::stop_token stopToken) noexcept;
    void logThreadDetailsIfNeeded() noexcept;
    void logDecisionState() const noexcept;
    void applyMainThreadPolicyIfNeeded(std::optional<DWORD> mainThreadId) noexcept;
    void reconcilePolicyIfNeeded(std::optional<DWORD> mainThreadId) noexcept;
    [[nodiscard]] RuntimeMetrics collectMetrics() noexcept;
    [[nodiscard]] RuntimeValidationSample makeValidationSample(
        const RuntimeMetrics& runtimeMetrics,
        std::optional<DWORD> mainThreadId) const noexcept;
    [[nodiscard]] bool dispatchFeedbackCommands(
        RuntimeValidationSample& validationSample,
        const RuntimeMetrics& runtimeMetrics) noexcept;
    void dispatchDecisionCommands(
        RuntimeValidationSample& validationSample,
        const RuntimeMetrics& runtimeMetrics) noexcept;

    RuntimeContext& context_;
    std::atomic_bool& runtimeTimeoutRequested_;
    RuntimeShutdownRequest requestShutdown_ = nullptr;
    bool threadTrackerResetEventThisCycle_ = false;
};
