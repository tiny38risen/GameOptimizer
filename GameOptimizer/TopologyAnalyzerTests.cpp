// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- TopologyAnalyzerTests.cpp TopologyAnalyzer.cpp
// MODULE: TopologyAnalyzerTests
// PURPOSE: deterministic HEDT/processor-group fallback policy checks.

#include "TopologyAnalyzer.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
    int g_failureCount = 0;

    void recordFailure(const char* file, int line, std::string_view message) noexcept
    {
        std::cerr << "[FAIL] " << file << ':' << line << " - " << message << '\n';
        ++g_failureCount;
    }

#define REQUIRE(condition, message) \
    do \
    { \
        if (!(condition)) \
        { \
            recordFailure(__FILE__, __LINE__, (message)); \
        } \
    } while (false)

    void testHedtFallbackPreservesProcessorGroup()
    {
        const auto result = TopologyAnalyzer::buildProcessAffinityFallbackMask(
            0x00000000000000F0ULL,
            0x00000000000000F0ULL,
            1);

        REQUIRE(result.has_value(), "HEDT group-1 fallback mask should be generated from process affinity");
        if (result)
        {
            const TopologyResult& topology = result.value();
            REQUIRE(topology.processorGroup == 1, "fallback policy must preserve the source processor group");
            REQUIRE(topology.primaryMask == 0x10ULL, "primary mask must use the lowest available bit in the group");
            REQUIRE(topology.fallbackMask == 0x20ULL, "fallback mask must use the next available bit in the same group");
            REQUIRE(topology.validatedMask == 0x30ULL, "validated mask must contain only selected same-group bits");
            REQUIRE(topology.selectedLogicalCoreCount == 2, "fallback should select at most two logical cores");
            REQUIRE(topology.l3ScoreFallback, "process-affinity fallback must be marked as topology-score fallback");
            REQUIRE(topology.maskProvenance == TopologyMaskProvenance::ProcessAffinityFallback,
                "process-affinity fallback must record mask provenance");
        }
    }

    void testFullGroupMaskFallbackIsBoundedToTwoCores()
    {
        const auto result = TopologyAnalyzer::buildProcessAffinityFallbackMask(
            ~static_cast<DWORD_PTR>(0),
            ~static_cast<DWORD_PTR>(0),
            2);

        REQUIRE(result.has_value(), "64-logical-core group fallback must remain representable");
        if (result)
        {
            const TopologyResult& topology = result.value();
            REQUIRE(topology.processorGroup == 2, "64-core fallback must preserve group 2");
            REQUIRE(topology.validatedMask == 0x3ULL, "full group mask fallback must select only the two lowest bits");
            REQUIRE(topology.selectedLogicalCoreCount == 2, "full group fallback must not produce >2 selected cores");
            REQUIRE(topology.maskProvenance == TopologyMaskProvenance::ProcessAffinityFallback,
                "full group fallback must keep process-affinity provenance");
        }
    }

    void testGroupOneSparseFallbackKeepsSameGroupOnly()
    {
        const auto result = TopologyAnalyzer::buildProcessAffinityFallbackMask(
            0x0000000000000A00ULL,
            0x0000000000000E00ULL,
            1);

        REQUIRE(result.has_value(), "sparse group-1 fallback should be generated");
        if (result)
        {
            const TopologyResult& topology = result.value();
            REQUIRE(topology.processorGroup == 1, "sparse fallback must preserve group 1");
            REQUIRE(topology.primaryMask == 0x200ULL, "sparse fallback must keep the lowest same-group bit");
            REQUIRE(topology.fallbackMask == 0x800ULL, "sparse fallback must keep the next same-group bit");
            REQUIRE(topology.validatedMask == 0xA00ULL, "sparse fallback must not invent cross-group bits");
        }
    }

    void testEmptyFallbackMaskFails()
    {
        const auto result = TopologyAnalyzer::buildProcessAffinityFallbackMask(0, 0xFFFF, 1);
        REQUIRE(!result.has_value(), "zero process affinity must fail fallback mask generation");
    }
}

int main()
{
    testHedtFallbackPreservesProcessorGroup();
    testFullGroupMaskFallbackIsBoundedToTwoCores();
    testGroupOneSparseFallbackKeepsSameGroupOnly();
    testEmptyFallbackMaskFails();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] TopologyAnalyzer HEDT fallback tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] TopologyAnalyzer HEDT fallback tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
