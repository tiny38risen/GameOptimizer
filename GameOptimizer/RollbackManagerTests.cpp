// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- RollbackManagerTests.cpp RollbackManager.cpp
// MODULE: RollbackManagerTests
// PURPOSE: deterministic rollback safety checks for incomplete processor-group evidence.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "ErrorCode.h"
#include "WinHandle.h"

#define private public
#include "RollbackManager.h"
#undef private

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

    void rollbackThreadFailsWhenOriginalProcessorGroupIsMissing()
    {
        RollbackManager rollbackManager;
        constexpr DWORD kSyntheticThreadId = 0x7FFF0001;

        {
            std::scoped_lock lock(rollbackManager.mutex_);
            rollbackManager.states_.insert_or_assign(
                kSyntheticThreadId,
                RollbackManager::ThreadRollbackState{
                    .creationTime100ns = std::nullopt,
                    .originalAffinityMask = static_cast<DWORD_PTR>(0x1),
                    .originalProcessorGroup = std::nullopt,
                    .originalPriority = THREAD_PRIORITY_NORMAL,
                    .threadId = kSyntheticThreadId,
                    .kind = RollbackManager::RollbackStateKind::Applied});
        }

        const auto rollbackResult = rollbackManager.rollbackThread(kSyntheticThreadId);

        REQUIRE(!rollbackResult, "rollback must fail when processor group evidence is missing");
        if (!rollbackResult)
        {
            REQUIRE(
                rollbackResult.error() == ErrorCode::RollbackVerificationFailed,
                "missing processor group evidence must be classified as rollback verification failure");
        }

        REQUIRE(
            rollbackManager.hasThreadState(kSyntheticThreadId),
            "failed rollback state must remain preserved for shutdown/blocker classification");
    }
}

int main()
{
    rollbackThreadFailsWhenOriginalProcessorGroupIsMissing();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] RollbackManager tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] RollbackManager tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
