// Build: included by RuntimeOrchestrator.cpp and WatchdogCycleRunner.cpp
// MODULE: WatchdogCycleRunner
// ERROR-POLICY: expected
// Reason: owns one watchdog policy cycle without owning runtime startup or shutdown.

#pragma once

#include <atomic>
#include <stop_token>

#include "RuntimeContext.h"
#include "RuntimeSignalState.h"

using RuntimeShutdownRequest = void (*)(ShutdownReason reason) noexcept;

class WatchdogCycleRunner
{
public:
    WatchdogCycleRunner(
        RuntimeContext& context,
        std::atomic_bool& runtimeTimeoutRequested,
        RuntimeShutdownRequest requestShutdown,
        const RuntimeSignalState& signalState) noexcept;

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
    [[nodiscard]] bool isShutdownRequested() const noexcept;
    [[nodiscard]] bool skipMutationWhenShutdownRequested(const char* phase) const noexcept;

    RuntimeContext& context_;
    std::atomic_bool& runtimeTimeoutRequested_;
    RuntimeShutdownRequest requestShutdown_ = nullptr;
    const RuntimeSignalState& signalState_;
    bool threadTrackerResetEventThisCycle_ = false;

#ifdef GAMEOPTIMIZER_ENABLE_WATCHDOG_TEST_HOOKS
    friend bool watchdogCycleRunnerShutdownGuardTest(
        WatchdogCycleRunner& runner,
        RuntimeContext& context) noexcept;
#endif
};
