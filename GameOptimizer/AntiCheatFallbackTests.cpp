// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- AntiCheatFallbackTests.cpp SchedulerController.cpp BackgroundController.cpp RollbackManager.cpp ApplyGuard.cpp
// MODULE: AntiCheatFallbackTests
// PURPOSE: deterministic checks for anti-cheat/access-denied fallback classification.

#include "BackgroundController.h"
#include "SchedulerController.h"
#include "WinApiError.h"

#include <Windows.h>
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
        REQUIRE(backgroundText.find("saved state discarded before mutation") != std::string::npos,
            "background affinity access-denied path must prove saved state was discarded before mutation");
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
}

int main()
{
    testWin32AccessDeniedMapsToRecoverableCategory();
    testBackgroundAccessDeniedIsRecoverableButApplyFailureIsNot();
    testAccessDeniedFallbackEvidenceMarkersArePresent();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] Anti-cheat fallback tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] Anti-cheat fallback tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
