// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: LatencyDecisionLayer
// ERROR-POLICY: expected
// Reason: decision layer must not directly mutate scheduler, network, or background state.

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>

#include "ErrorCode.h"
#include "PolicyCommand.h"

struct RuntimeMetrics
{
    std::chrono::steady_clock::time_point timestamp{};
    double rttJitterMs = 0.0;
    int dpcSpikeCount = 0;
    int threadMigrationCount = 0;
    bool interruptAffinitySupported = false;
};

class PolicyCommandBuffer
{
public:
    static constexpr std::size_t kCapacity = 3;

    [[nodiscard]] bool pushBack(PolicyCommand command) noexcept
    {
        if (size_ >= commands_.size())
        {
            return false;
        }

        commands_[size_] = command;
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

    [[nodiscard]] const PolicyCommand* begin() const noexcept
    {
        return commands_.data();
    }

    [[nodiscard]] const PolicyCommand* end() const noexcept
    {
        return commands_.data() + size_;
    }

private:
    std::array<PolicyCommand, kCapacity> commands_{};
    std::size_t size_ = 0;
};

class LatencyDecisionLayer
{
public:
    [[nodiscard]] std::expected<PolicyCommandBuffer, ErrorCode> evaluate(const RuntimeMetrics& metrics) noexcept;
    [[nodiscard]] std::expected<PolicyCommandBuffer, ErrorCode> evaluate(
        const RuntimeMetrics& metrics,
        std::chrono::steady_clock::time_point now) noexcept;

private:
    [[nodiscard]] bool isCooldownElapsed(
        std::chrono::steady_clock::time_point now,
        std::chrono::steady_clock::time_point lastEmission,
        std::chrono::milliseconds cooldown) const noexcept;

private:
    bool bgRestrictionActive_ = false;
    bool irqRepinActive_ = false;
    bool stickyBoostActive_ = false;

    std::uint32_t rttTriggerStreak_ = 0;
    std::uint32_t dpcTriggerStreak_ = 0;
    std::uint32_t migrationTriggerStreak_ = 0;

    std::chrono::steady_clock::time_point lastBgRestrictEmission_{};
    std::chrono::steady_clock::time_point lastIrqRepinEmission_{};
    std::chrono::steady_clock::time_point lastStickyUpEmission_{};
};
