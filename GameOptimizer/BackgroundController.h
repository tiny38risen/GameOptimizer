// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: BackgroundController
// ERROR-POLICY: expected
// Reason: background process restriction crosses Win32 process scheduling boundaries.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <chrono>
#include <expected>
#include <string>
#include <unordered_set>
#include <vector>

#include "ErrorCode.h"
#include "RollbackManager.h"
#include "SchedulerController.h"
#include "WinHandle.h"

struct BackgroundRestrictionPolicy
{
    DWORD targetProcessId = 0;
    DWORD_PTR gameAffinityMask = 0;
    WORD processorGroup = 0;
};

struct BackgroundProcessEntry
{
    std::wstring imageName;
    DWORD processId = 0;
};

struct BackgroundFilterConfig
{
    // Apply mode safety gate. When true, real background restriction is blocked unless
    // restrictionTargetProcessNames is non-empty. DryRun and SoftApply still allow broad
    // enumeration so the user can preview what would happen before creating a denylist.
    bool requireExplicitRestrictionTargetsInApply = true;

    // Keep apply-mode console output readable. The summary is always logged;
    // per-process success and skip logs are opt-in for detailed diagnosis.
    bool logRestrictedProcessDetails = false;
    bool logSkippedProcessDetails = false;

    // Names are normalized to lowercase at load time. Runtime lookup must not read disk or
    // perform O(N) case-insensitive scans on every BG_RESTRICT_UP command.
    std::unordered_set<std::wstring> protectedProcessNames;

    // Optional restriction target names. If this set is non-empty, only matching non-protected
    // processes are considered for background restriction. This is intentionally safer than
    // applying restriction to every unknown user process.
    std::unordered_set<std::wstring> restrictionTargetProcessNames;
};

struct BackgroundRestrictionSummary
{
    std::size_t scannedProcessCount = 0;
    std::size_t candidateProcessCount = 0;
    std::size_t restrictedProcessCount = 0;
    std::size_t skippedProcessCount = 0;
    std::size_t protectedProcessSkipCount = 0;
    std::size_t userProtectedProcessSkipCount = 0;
    std::size_t notInDenylistSkipCount = 0;
    bool blockedByMissingDenylist = false;
    bool blockedByUnsupportedProcessorGroup = false;
};

class BackgroundController
{
public:
    BackgroundController(
        RollbackManager& rollbackManager,
        SchedulerMode mode,
        BackgroundFilterConfig filterConfig = {}) noexcept;

    [[nodiscard]] std::expected<BackgroundRestrictionSummary, ErrorCode>
    applyRestriction(const BackgroundRestrictionPolicy& policy) noexcept;

    [[nodiscard]] bool isRestrictionCooldownActive() const noexcept;
    //[[nodiscard]] bool isRealApplyBlockedByMissingDenylist() const noexcept;

    [[nodiscard]] static BackgroundFilterConfig loadFilterConfigFromFile(const std::wstring& path) noexcept;

private:
    [[nodiscard]] static bool isExcludedPid(DWORD processId, DWORD targetProcessId) noexcept;
    [[nodiscard]] static std::wstring normalizeProcessName(std::wstring_view imageName);
    static void normalizeProcessNameInto(std::wstring_view imageName, std::wstring& outputBuffer);
    [[nodiscard]] static bool isBuiltInProtectedProcessName(const std::wstring& normalizedImageName) noexcept;
    [[nodiscard]] bool isUserProtectedProcessName(const std::wstring& normalizedImageName) const noexcept;
    [[nodiscard]] bool isRestrictionTargetAllowedByConfig(const std::wstring& normalizedImageName) const noexcept;

    [[nodiscard]] static std::expected<std::vector<BackgroundProcessEntry>, ErrorCode>
    enumerateProcesses() noexcept;

    [[nodiscard]] static std::expected<HANDLE, ErrorCode> openProcessForRestriction(DWORD processId) noexcept;
    [[nodiscard]] static DWORD_PTR buildBackgroundMask(DWORD_PTR processMask, DWORD_PTR gameMask) noexcept;

private:
    RollbackManager& rollbackManager_;
    SchedulerMode mode_ = SchedulerMode::DryRun;
    BackgroundFilterConfig filterConfig_;
    std::chrono::steady_clock::time_point lastRestrictionTime_{};
};
