// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: TopologyAnalyzer
// ERROR-POLICY: expected
// Reason: CPU topology and process affinity queries can fail on unsupported environments.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <expected>

#include "ErrorCode.h"

enum class TopologyMaskProvenance
{
    WeightedTopology,
    ActiveGroupFallback,
    ProcessAffinityFallback
};

struct TopologyResult
{
    DWORD_PTR primaryMask = 0;
    DWORD_PTR fallbackMask = 0;
    DWORD_PTR validatedMask = 0;
    WORD processorGroup = 0;
    bool sameL3Cache = false;
    bool smtClean = false;
    bool l3SizeKnown = false;
    bool l3ScoreFallback = false;
    TopologyMaskProvenance maskProvenance = TopologyMaskProvenance::WeightedTopology;
    int selectedLogicalCoreCount = 0;
};

class TopologyAnalyzer
{
public:
    [[nodiscard]] static std::expected<TopologyResult, ErrorCode>
    buildMainThreadMask(DWORD processId) noexcept;

    [[nodiscard]] static std::expected<TopologyResult, ErrorCode>
    buildProcessAffinityFallbackMask(
        DWORD_PTR processMask,
        DWORD_PTR systemMask,
        WORD processorGroup) noexcept;

private:
    [[nodiscard]] static DWORD_PTR lowestSetBit(DWORD_PTR mask) noexcept;
    [[nodiscard]] static DWORD_PTR takeLowestBits(DWORD_PTR mask, int count) noexcept;
    [[nodiscard]] static int countSetBits(DWORD_PTR mask) noexcept;
};
