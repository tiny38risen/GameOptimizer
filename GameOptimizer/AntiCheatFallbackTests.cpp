// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- AntiCheatFallbackTests.cpp SchedulerController.cpp BackgroundController.cpp RollbackManager.cpp ApplyGuard.cpp
// MODULE: AntiCheatFallbackTests
// PURPOSE: deterministic checks for anti-cheat/access-denied fallback classification.

#include "BackgroundController.h"
#include "SchedulerController.h"
#include "WinApiError.h"

#include <Windows.h>
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
}

int main()
{
    testWin32AccessDeniedMapsToRecoverableCategory();
    testBackgroundAccessDeniedIsRecoverableButApplyFailureIsNot();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] Anti-cheat fallback tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] Anti-cheat fallback tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
