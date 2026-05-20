// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- ProcessorGroupHedtEvidenceTests.cpp TopologyAnalyzer.cpp
// MODULE: ProcessorGroupHedtEvidenceTests
// PURPOSE: deterministic HEDT/group evidence checks for affinity, rollback, and log contracts.

#include "TopologyAnalyzer.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
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

    [[nodiscard]] std::string readTextFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void testGroupOneFallbackCarriesAffinityEvidence()
    {
        const auto result = TopologyAnalyzer::buildProcessAffinityFallbackMask(
            0x00000000000000C0ULL,
            0x00000000000000E0ULL,
            1);

        REQUIRE(result.has_value(), "group 1+ process-affinity fallback must be representable");
        if (!result)
        {
            return;
        }

        const TopologyResult& topology = result.value();
        REQUIRE(topology.processorGroup == 1, "fallback affinity must preserve group 1");
        REQUIRE(topology.primaryMask == 0x40ULL, "fallback primary mask must stay in group-local bits");
        REQUIRE(topology.fallbackMask == 0x80ULL, "fallback secondary mask must stay in group-local bits");
        REQUIRE(topology.validatedMask == 0xC0ULL, "validated fallback mask must keep group-local provenance");
        REQUIRE(topology.maskProvenance == TopologyMaskProvenance::ProcessAffinityFallback,
            "fallback affinity must carry process-affinity provenance");
    }

    void testRollbackSourceContainsGroupEvidence()
    {
        const std::string rollbackText = readTextFile("RollbackManager.cpp");

        REQUIRE(rollbackText.find("originalProcessorGroup") != std::string::npos,
            "rollback state must store original processor group");
        REQUIRE(rollbackText.find("targetAffinity.Group = state.originalProcessorGroup.value_or(0);") != std::string::npos,
            "thread rollback must restore saved processor group");
        REQUIRE(rollbackText.find("rollback state saved for TID {} (group={}") != std::string::npos,
            "thread rollback save log must include group");
        REQUIRE(rollbackText.find("rollback audit passed for TID {} (group={}") != std::string::npos,
            "thread rollback audit log must include group");
        REQUIRE(rollbackText.find("expected(group={}") != std::string::npos,
            "thread rollback mismatch log must include expected group");
        REQUIRE(rollbackText.find("observedProcessorGroup") != std::string::npos,
            "process rollback state must record group as observed evidence, not full group-aware restore capability");
        REQUIRE(rollbackText.find("RollbackMode::LegacyProcessAffinityMask") != std::string::npos,
            "process rollback must distinguish legacy process affinity mode");
        REQUIRE(rollbackText.find("RollbackMode::GroupAwareUnsupported") != std::string::npos,
            "process rollback must distinguish unsupported group-aware process affinity mode");
        REQUIRE(rollbackText.find("background rollback state saved for PID {} (observedGroup={}, rollbackMode={}") != std::string::npos,
            "process rollback save log must include observed group and rollback mode");
        REQUIRE(rollbackText.find("background rollback restored PID {} (observedGroup={}, rollbackMode={}") != std::string::npos,
            "process rollback restore log must include observed group and rollback mode");
        REQUIRE(rollbackText.find("preserved rollback state count: thread={}, process={}") != std::string::npos,
            "rollbackAll must preserve failed rollback states");
    }

    void testProcessorGroupGateSourceContainsEvidenceMarkers()
    {
        const std::string backgroundText = readTextFile("BackgroundController.cpp");
        const std::string staticGateText = readTextFile("run_release_gate_static_checks.py");
        const std::string topologyTestText = readTextFile("TopologyAnalyzerTests.cpp");

        REQUIRE(backgroundText.find("return processorGroup == 0;") != std::string::npos,
            "process-wide background affinity must remain group-0 limited");
        REQUIRE(backgroundText.find("background restriction blocked: processor group") != std::string::npos,
            "group 1+ background restriction must leave WARN evidence");
        REQUIRE(staticGateText.find("background rollback state saved for PID {} (observedGroup={}, rollbackMode={}") != std::string::npos,
            "static gate must guard process rollback observed-group evidence");
        REQUIRE(staticGateText.find("background rollback restored PID {} (observedGroup={}, rollbackMode={}") != std::string::npos,
            "static gate must guard process rollback legacy-mode evidence");
        REQUIRE(staticGateText.find("preserved rollback state count: thread={}, process={}") != std::string::npos,
            "static gate must guard failed rollback state preservation evidence");
        REQUIRE(staticGateText.find("topology fallback policy selected from process affinity: group={}") != std::string::npos,
            "static gate must guard topology fallback group evidence");
        REQUIRE(topologyTestText.find("processorGroup == 1") != std::string::npos,
            "group 1+ mock coverage must remain in topology tests");
    }
}

int main()
{
    testGroupOneFallbackCarriesAffinityEvidence();
    testRollbackSourceContainsGroupEvidence();
    testProcessorGroupGateSourceContainsEvidenceMarkers();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] ProcessorGroup/HEDT evidence tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] ProcessorGroup/HEDT evidence tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
