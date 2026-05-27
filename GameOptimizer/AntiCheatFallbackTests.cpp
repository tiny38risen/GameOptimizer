// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- AntiCheatFallbackTests.cpp SchedulerController.cpp BackgroundController.cpp RollbackManager.cpp ApplyGuard.cpp
// MODULE: AntiCheatFallbackTests
// PURPOSE: deterministic checks for anti-cheat/access-denied fallback classification.

#include "BackgroundController.h"
#include "SchedulerController.h"
#include "WinApiError.h"

#include <Windows.h>
#include <cstddef>
#include <cstdlib>
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

    [[nodiscard]] std::string readTextFile(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    [[nodiscard]] bool contains(std::string_view text, std::string_view marker) noexcept
    {
        return text.find(marker) != std::string_view::npos;
    }

    [[nodiscard]] bool appearsBefore(
        std::string_view text,
        std::string_view first,
        std::string_view second) noexcept
    {
        const std::size_t firstIndex = text.find(first);
        const std::size_t secondIndex = text.find(second);
        return firstIndex != std::string_view::npos &&
            secondIndex != std::string_view::npos &&
            firstIndex < secondIndex;
    }

    void testWin32AccessDeniedMapsToRecoverableCategory()
    {
        REQUIRE(mapLastErrorToErrorCode(ERROR_ACCESS_DENIED) == ErrorCode::AccessDenied,
            "ERROR_ACCESS_DENIED must map to AccessDenied");
        REQUIRE(SchedulerController::isRecoverableAccessLimitation(ErrorCode::AccessDenied),
            "scheduler must treat AccessDenied as a recoverable access limitation");
        REQUIRE(SchedulerController::isRecoverableAccessLimitation(ErrorCode::ThreadOpenFailed),
            "scheduler must treat OpenThread failure as a recoverable access limitation");
        REQUIRE(!SchedulerController::isRecoverableAccessLimitation(ErrorCode::ThreadAffinityApplyFailed),
            "scheduler must not hide generic affinity apply failures");
    }

    void testBackgroundAccessDeniedIsRecoverableButApplyFailureIsNot()
    {
        REQUIRE(BackgroundController::isRecoverableAccessLimitation(ErrorCode::AccessDenied),
            "background controller must treat AccessDenied as a recoverable access limitation");
        REQUIRE(BackgroundController::isRecoverableAccessLimitation(ErrorCode::ProcessOpenFailed),
            "background controller must treat OpenProcess failure as a recoverable access limitation");
        REQUIRE(!BackgroundController::isRecoverableAccessLimitation(ErrorCode::ProcessAffinityApplyFailed),
            "background controller must not hide generic process affinity failures");
        REQUIRE(!BackgroundController::isRecoverableAccessLimitation(ErrorCode::ProcessPriorityApplyFailed),
            "background controller must not hide generic process priority failures");
    }

    void testAccessDeniedFallbackEvidenceMarkersArePresent()
    {
        const std::string schedulerText = readTextFile("SchedulerController.cpp");
        const std::string backgroundText = readTextFile("BackgroundController.cpp");
        const std::string rollbackText = readTextFile("RollbackManager.cpp");
        const std::string runtimeText = readTextFile("WatchdogCycleRunner.cpp");
        const std::string assertionsText = readTextFile("run_release_gate_log_assertions.py");

        REQUIRE(schedulerText.find("monitoring-only fallback remains active") != std::string::npos,
            "scheduler access-denied path must leave monitoring-only fallback evidence");
        REQUIRE(schedulerText.find("rollback path was invoked when needed") != std::string::npos,
            "scheduler access-denied priority path must leave rollback-path evidence");
        REQUIRE(backgroundText.find("background process skipped by recoverable access limitation") != std::string::npos,
            "background OpenProcess access-denied path must leave skip evidence");
        REQUIRE(backgroundText.find("post-failure audit completed before state cleanup") != std::string::npos,
            "background affinity access-denied path must prove post-failure audit before state cleanup");
        REQUIRE(backgroundText.find("rollback path was invoked when needed") != std::string::npos,
            "background priority access-denied path must leave rollback-path evidence");
        REQUIRE(rollbackText.find("blocked by an access boundary") != std::string::npos,
            "rollback access-denied path must leave access-boundary evidence");
        REQUIRE(rollbackText.find("no longer openable") != std::string::npos,
            "rollback open failure must leave non-fatal skip evidence");
        REQUIRE(runtimeText.find("policy drift audit limited by access boundary") != std::string::npos,
            "runtime drift audit access boundary must leave monitoring-only evidence");
        REQUIRE(assertionsText.find("validate_access_denied_fallback_evidence") != std::string::npos,
            "release log assertions must validate access-denied fallback evidence");
    }

    void testSchedulerAffinityFailureAuditContractIsPinned()
    {
        const std::string schedulerText = readTextFile("SchedulerController.cpp");
        const std::string applyGuardHeader = readTextFile("ApplyGuard.h");
        const std::string applyGuardImpl = readTextFile("ApplyGuard.cpp");
        const std::string staticGateText = readTextFile("run_release_gate_static_checks.py");

        REQUIRE(!contains(schedulerText, "stateAfterFailedAffinityApply.value()"),
            "scheduler affinity failure audit must not use expected.value()");
        REQUIRE(contains(schedulerText, "const auto stateAfterFailedAffinityApply = queryCurrentSchedulingState"),
            "affinity failure must query current state before deciding rollback cleanup");
        REQUIRE(contains(schedulerText, "if (!stateAfterFailedAffinityApply)"),
            "affinity failure must handle audit query failure before binding");
        REQUIRE(contains(schedulerText, "const auto& auditedState = *stateAfterFailedAffinityApply;"),
            "affinity failure audit must bind expected state explicitly");
        REQUIRE(contains(schedulerText, "matchesOriginalState(auditedState, original)"),
            "affinity failure audit must compare bound state with the original state");
        REQUIRE(contains(schedulerText, "enum class FailedAffinityApplyDisposition"),
            "affinity failure handling must keep an explicit disposition enum");
        REQUIRE(contains(schedulerText, "auditFailedAffinityApply("),
            "affinity failure audit must be isolated in a helper");
        REQUIRE(contains(schedulerText, "makeAffinityApplyFailureResult("),
            "affinity failure return mapping must be isolated in a helper");
        REQUIRE(contains(schedulerText, "logAffinityRollbackFailure("),
            "affinity rollback failure context logging must be isolated in a helper");
        REQUIRE(contains(schedulerText, "applyGuard.discardSavedState();"),
            "affinity failure may discard rollback state only after original-state audit match");
        REQUIRE(contains(schedulerText, "post-failure audit could not query current state"),
            "affinity failure audit query failure must be visible in logs before rollback");
        REQUIRE(contains(schedulerText, "post-failure audit found state drift"),
            "affinity failure audit mismatch must be visible in logs before rollback");
        REQUIRE(contains(schedulerText, "auto rollbackResult = applyGuard.rollbackNow();"),
            "affinity failure audit query failure or mismatch must invoke ApplyGuard rollback");
        REQUIRE(appearsBefore(
                    schedulerText,
                    "const auto stateAfterFailedAffinityApply = queryCurrentSchedulingState",
                    "const auto& auditedState = *stateAfterFailedAffinityApply;"),
            "affinity failure audit must assign, check, then bind");
        REQUIRE(appearsBefore(
                    schedulerText,
                    "matchesOriginalState(auditedState, original)",
                    "applyGuard.discardSavedState();"),
            "rollback state discard must remain after original-state audit comparison");
        REQUIRE(appearsBefore(
                    schedulerText,
                    "post-failure audit found state drift",
                    "auto rollbackResult = applyGuard.rollbackNow();"),
            "drift evidence must be emitted before rollback on affinity failure");

        REQUIRE(contains(applyGuardHeader, "ApplyGuard& operator=(ApplyGuard&& other) = delete;"),
            "ApplyGuard move assignment must remain deleted");
        REQUIRE(!contains(applyGuardImpl, "ApplyGuard& ApplyGuard::operator=(ApplyGuard&& other)"),
            "ApplyGuard move assignment implementation must not be reintroduced");
        REQUIRE(contains(staticGateText, "stateAfterFailedAffinityApply\\.value"),
            "static gate must reject expected.value() on the SchedulerController affinity failure audit path");
    }

    void testBackgroundAffinityFailureAuditContractIsPinned()
    {
        const std::string backgroundText = readTextFile("BackgroundController.cpp");
        const std::string staticGateText = readTextFile("run_release_gate_static_checks.py");

        REQUIRE(!contains(backgroundText, "stateAfterFailedAffinityApply.value()"),
            "background affinity failure audit must not use expected.value()");
        REQUIRE(contains(backgroundText, "const auto stateAfterFailedAffinityApply = queryProcessAffinityState"),
            "background affinity failure must query current affinity before deciding rollback cleanup");
        REQUIRE(contains(backgroundText, "if (!stateAfterFailedAffinityApply)"),
            "background affinity failure must handle audit query failure before binding");
        REQUIRE(contains(backgroundText, "const auto& auditedAffinityState = *stateAfterFailedAffinityApply;"),
            "background affinity failure audit must bind expected state explicitly");
        REQUIRE(contains(backgroundText, "matchesOriginalAffinity(auditedAffinityState, processMask)"),
            "background affinity failure audit must compare bound state with the original process mask");
        REQUIRE(contains(backgroundText, "applyGuard.discardSavedState();"),
            "background affinity failure may discard rollback state only after original-affinity audit match");
        REQUIRE(contains(backgroundText, "post-failure audit could not query affinity"),
            "background affinity audit query failure must be visible in logs before rollback");
        REQUIRE(contains(backgroundText, "post-failure audit found affinity drift"),
            "background affinity audit mismatch must be visible in logs before rollback");
        REQUIRE(contains(backgroundText, "auto rollbackResult = applyGuard.rollbackNow();"),
            "background affinity audit query failure or mismatch must invoke ApplyGuard rollback");
        REQUIRE(appearsBefore(
                    backgroundText,
                    "const auto stateAfterFailedAffinityApply = queryProcessAffinityState",
                    "const auto& auditedAffinityState = *stateAfterFailedAffinityApply;"),
            "background affinity audit must assign, check, then bind");
        REQUIRE(appearsBefore(
                    backgroundText,
                    "matchesOriginalAffinity(auditedAffinityState, processMask)",
                    "applyGuard.discardSavedState();"),
            "background rollback state discard must remain after original-affinity audit comparison");
        REQUIRE(appearsBefore(
                    backgroundText,
                    "post-failure audit found affinity drift",
                    "auto rollbackResult = applyGuard.rollbackNow();"),
            "background drift evidence must be emitted before rollback on affinity failure");
        REQUIRE(contains(staticGateText, "BackgroundController must not call expected.value()"),
            "static gate must reject expected.value() on the BackgroundController affinity failure audit path");
    }
}

int main()
{
    testWin32AccessDeniedMapsToRecoverableCategory();
    testBackgroundAccessDeniedIsRecoverableButApplyFailureIsNot();
    testAccessDeniedFallbackEvidenceMarkersArePresent();
    testSchedulerAffinityFailureAuditContractIsPinned();
    testBackgroundAffinityFailureAuditContractIsPinned();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] Anti-cheat fallback tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] Anti-cheat fallback tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
