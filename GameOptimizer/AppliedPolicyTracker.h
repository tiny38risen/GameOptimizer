// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: AppliedPolicyTracker
// ERROR-POLICY: expected
// Reason: policy feedback failures must be converted into bounded rollback commands.

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "LatencyDecisionLayer.h"
#include "PolicyCommand.h"

struct AppliedPolicyTrackerConfig
{
    std::uint32_t evaluationWindowCycles = 3;
    std::uint32_t regressionRequiredCycles = 2;
    double minimumRttImprovementMs = 0.5;
    double acceptableRttJitterMs = 5.0;
    double rttRegressionToleranceMs = 0.5;
    int minimumDpcImprovementCount = 1;
    int minimumMigrationImprovementCount = 1;
    int countRegressionTolerance = 1;
    bool rollbackOnPolicyRegression = true;
    std::chrono::milliseconds rearmCooldown = std::chrono::seconds(30);
};

struct AppliedPolicySnapshot
{
    PolicyCommand command = PolicyCommand::None;
    RuntimeMetrics baselineMetrics{};
    std::chrono::steady_clock::time_point appliedAt{};
    std::uint32_t observedCycles = 0;
    std::uint32_t regressionCycles = 0;
    bool dispatchSucceeded = false;
};

class AppliedPolicyTracker
{
public:
    explicit AppliedPolicyTracker(AppliedPolicyTrackerConfig config = {}) noexcept;

    void recordDispatchResult(
        PolicyCommand command,
        const RuntimeMetrics& baselineMetrics,
        bool dispatchSucceeded,
        std::chrono::steady_clock::time_point now) noexcept;

    [[nodiscard]] PolicyCommandBuffer evaluate(
        const RuntimeMetrics& currentMetrics,
        std::chrono::steady_clock::time_point now) noexcept;

    void clear() noexcept;

private:
    [[nodiscard]] bool shouldTrack(PolicyCommand command, const RuntimeMetrics& metrics) const noexcept;
    [[nodiscard]] bool hasImproved(const AppliedPolicySnapshot& snapshot, const RuntimeMetrics& currentMetrics) const noexcept;
    [[nodiscard]] bool hasRegressed(const AppliedPolicySnapshot& snapshot, const RuntimeMetrics& currentMetrics) const noexcept;

private:
    AppliedPolicyTrackerConfig config_{};
    std::optional<AppliedPolicySnapshot> pendingSnapshot_{};
    bool rollbackAlreadyRequested_ = false;
    PolicyCommand lastCompletedCommand_ = PolicyCommand::None;
    std::chrono::steady_clock::time_point lastCompletedAt_{};
};
