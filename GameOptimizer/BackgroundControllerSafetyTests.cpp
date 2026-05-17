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
}

int main()
{
    testEmptyConfigRequiresExplicitTargetsByDefault();
    testFilterFileLoadsDenyAndProtectEntries();

    if (g_failureCount == 0)
    {
        std::cout << "[PASS] BackgroundController safety tests completed\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "[FAIL] BackgroundController safety tests failed: "
              << g_failureCount << " failure(s)\n";
    return EXIT_FAILURE;
}
