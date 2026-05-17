// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: RuntimeValidationMonitor
// ERROR-POLICY: no-throw
// Reason: validation must observe runtime health without changing optimization behavior.

#pragma once

#include <cstdint>

#include "LatencyDecisionLayer.h"

struct RuntimeValidationMonitorConfig
{
    std::uint32_t minimumCyclesForSummary = 3;
    std::uint32_t maxDispatchFailures = 0;
    std::uint32_t maxRollbackRequests = 0;
    std::uint32_t maxConsecutiveNoMainThreadCycles = 20;
    double highRttJitterWarnMs = 5.0;
    int highDpcSpikeWarnCount = 3;
    int highThreadMigrationWarnCount = 1;
};

struct RuntimeValidationSample
{
    RuntimeMetrics metrics{};
    bool mainThreadDetected = false;
    bool mainThreadPolicyApplied = false;
    std::uint32_t decisionCommandCount = 0;
    std::uint32_t feedbackCommandCount = 0;
    std::uint32_t dispatchFailureCount = 0;
    bool rollbackRequested = false;
};

struct RuntimeValidationSummary
{
    std::uint32_t observedCycles = 0;
    std::uint32_t cyclesWithMainThread = 0;
    std::uint32_t cyclesWithAppliedMainPolicy = 0;
    std::uint32_t totalDecisionCommands = 0;
    std::uint32_t totalFeedbackCommands = 0;
    std::uint32_t totalDispatchFailures = 0;
    std::uint32_t totalRollbackRequests = 0;
    std::uint32_t highRttJitterCycles = 0;
    std::uint32_t highDpcSpikeCycles = 0;
    std::uint32_t highThreadMigrationCycles = 0;
    std::uint32_t consecutiveNoMainThreadCycles = 0;
    bool minimumCycleCountSatisfied = false;
    bool criticalFailureDetected = false;
};

class RuntimeValidationMonitor
{
public:
    explicit RuntimeValidationMonitor(RuntimeValidationMonitorConfig config = {}) noexcept;

    void observe(const RuntimeValidationSample& sample) noexcept;
    void logSummary() const noexcept;

    [[nodiscard]] RuntimeValidationSummary summary() const noexcept;
    [[nodiscard]] bool hasCriticalFailure() const noexcept;

private:
    void updateCriticalFailureState() noexcept;

private:
    RuntimeValidationMonitorConfig config_{};
    RuntimeValidationSummary summary_{};
};
