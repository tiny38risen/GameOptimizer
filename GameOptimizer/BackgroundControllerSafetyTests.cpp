// Build example (MSVC):
// cl /std:c++latest /EHsc /W4 /WX /permissive- BackgroundControllerSafetyTests.cpp BackgroundController.cpp RollbackManager.cpp SchedulerController.cpp
// MODULE: BackgroundControllerSafetyTests
// PURPOSE: deterministic checks for denylist-based apply-mode safety configuration.

#include "BackgroundController.h"

#include <cstdlib>
#include <fstream>
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

    void testEmptyConfigRequiresExplicitTargetsByDefault()
    {
        BackgroundFilterConfig config{};
        REQUIRE(config.requireExplicitRestrictionTargetsInApply,
            "apply-mode safety must require explicit restriction targets by default");
        REQUIRE(config.restrictionTargetProcessNames.empty(),
            "empty config must not contain implicit restriction targets");
    }

    void testFilterFileLoadsDenyAndProtectEntries()
    {
        const wchar_t* path = L"background_controller_safety_test_filter.txt";
        {
            std::wofstream file(path);
            file << L"deny=Chrome.exe\n";
            file << L"restrict=SteamWebHelper.exe\n";
            file << L"protect=devenv.exe\n";
            file << L"allow=Explorer.exe\n";
        }

        const BackgroundFilterConfig config = BackgroundController::loadFilterConfigFromFile(path);

        REQUIRE(config.restrictionTargetProcessNames.contains(L"chrome.exe"),
            "deny entry must be normalized and loaded");
        REQUIRE(config.restrictionTargetProcessNames.contains(L"steamwebhelper.exe"),
            "restrict entry must be normalized and loaded");
        REQUIRE(config.protectedProcessNames.contains(L"devenv.exe"),
            "protect entry must be normalized and loaded");
        REQUIRE(config.protectedProcessNames.contains(L"explorer.exe"),
            "allow entry must be normalized and loaded");

        std::remove("background_controller_safety_test_filter.txt");
    }

    void testProcessorGroupPolicyBlocksProcessWideRestriction()
    {
        REQUIRE(BackgroundController::supportsProcessWideRestrictionForGroup(0),
            "group 0 must remain supported for process-wide restriction");
        REQUIRE(!BackgroundController::supportsProcessWideRestrictionForGroup(1),
            "group 1+ must be blocked until a group-aware background policy exists");

        RollbackManager directRollbackManager;
        const auto unsupportedSave = directRollbackManager.saveProcessState(
            GetCurrentProcessId(),
            0x1,
            1,
            NORMAL_PRIORITY_CLASS);
        REQUIRE(!unsupportedSave.has_value(),
            "process rollback state save must reject unsupported group-aware rollback before any mutation");
        if (!unsupportedSave)
        {
            REQUIRE(unsupportedSave.error() == ErrorCode::UnsupportedProcessorGroupRollback,
                "unsupported process rollback save must report UnsupportedProcessorGroupRollback");
        }

        RollbackManager rollbackManager;
        BackgroundController controller(rollbackManager, SchedulerMode::Apply);

        const auto result = controller.applyRestriction(BackgroundRestrictionPolicy{
            .targetProcessId = 1,
            .gameAffinityMask = 0x1,
            .processorGroup = 1});

        REQUIRE(result.has_value(),
            "unsupported processor group policy should return a non-fatal summary");

        if (result)
        {
            REQUIRE(result->blockedByUnsupportedProcessorGroup,
                "unsupported processor group policy must be visible in the summary");
            REQUIRE(result->blockedProcessorGroup == 1,
                "unsupported processor group summary must record the blocked group");
            REQUIRE(result->restrictedProcessCount == 0,
                "unsupported processor group policy must not restrict processes");
        }
    }

    void testInvalidMainThreadPolicyDisablesBackgroundRestriction()
    {
        RollbackManager rollbackManager;
        BackgroundController controller(rollbackManager, SchedulerMode::Apply);

        const auto result = controller.applyRestriction(BackgroundRestrictionPolicy{
            .targetProcessId = 1,
            .gameAffinityMask = 0,
            .processorGroup = 0});

        REQUIRE(result.has_value(),
            "invalid main-thread policy mask should disable background restriction without a dispatch failure");

        if (result)
        {
            REQUIRE(result->blockedByInvalidMainThreadPolicy,
                "invalid main-thread policy summary must record the disabled background restriction state");
            REQUIRE(result->restrictedProcessCount == 0,
                "invalid main-thread policy must not restrict background processes");
        }
    }
}

int main()
{
    testEmptyConfigRequiresExplicitTargetsByDefault();
    testFilterFileLoadsDenyAndProtectEntries();
    testProcessorGroupPolicyBlocksProcessWideRestriction();
    testInvalidMainThreadPolicyDisablesBackgroundRestriction();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] BackgroundController safety tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] BackgroundController safety tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
