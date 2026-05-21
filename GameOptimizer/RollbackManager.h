// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: RollbackManager
// ERROR-POLICY: expected
// Reason: rollback state can fail when a target thread exits or access is denied.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <expected>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "ErrorCode.h"
#include "WinHandle.h"

class RollbackManager
{
public:
    enum class SaveStateDisposition
    {
        CreatedNewState,
        ReusedExistingState
    };

    enum class RollbackDisposition
    {
        RolledBack,
        NoState,
        StaleOrMissingIdentity
    };

    [[nodiscard]] std::expected<void, ErrorCode>
    saveDryRunState(DWORD threadId) noexcept;

    [[nodiscard]] std::expected<void, ErrorCode>
    saveValidatedReadState(
        DWORD threadId,
        DWORD_PTR currentAffinityMask,
        WORD processorGroup,
        int currentPriority) noexcept;

    [[nodiscard]] std::expected<SaveStateDisposition, ErrorCode>
    saveAppliedState(
        DWORD threadId,
        DWORD_PTR previousAffinityMask,
        WORD processorGroup,
        int originalPriority) noexcept;

    [[nodiscard]] std::expected<RollbackDisposition, ErrorCode> rollbackThread(DWORD threadId) noexcept;

    [[nodiscard]] std::expected<SaveStateDisposition, ErrorCode>
    saveProcessState(
        DWORD processId,
        DWORD_PTR originalAffinityMask,
        WORD processorGroup,
        DWORD originalPriorityClass) noexcept;

    [[nodiscard]] std::expected<void, ErrorCode>
    validateProcessRollbackState(
        DWORD processId,
        DWORD_PTR originalAffinityMask,
        WORD processorGroup,
        DWORD originalPriorityClass) noexcept;

    [[nodiscard]] std::expected<RollbackDisposition, ErrorCode> rollbackProcess(DWORD processId) noexcept;
    [[nodiscard]] std::expected<void, ErrorCode> rollbackAll() noexcept;

    void removeThreadState(DWORD threadId) noexcept;
    void removeProcessState(DWORD processId) noexcept;
    [[nodiscard]] bool hasThreadState(DWORD threadId) const noexcept;
    [[nodiscard]] bool hasProcessState(DWORD processId) const noexcept;

private:
    enum class RollbackStateKind
    {
        DryRunMarker,
        Applied
    };

    struct ThreadRollbackState
    {
        std::optional<std::uint64_t> creationTime100ns;
        std::optional<DWORD_PTR> originalAffinityMask;
        std::optional<WORD> originalProcessorGroup;
        std::optional<int> originalPriority;
        DWORD threadId = 0;
        RollbackStateKind kind = RollbackStateKind::DryRunMarker;
    };

    struct ProcessRollbackState
    {
        enum class RollbackMode
        {
            LegacyProcessAffinityMask,
            GroupAwareUnsupported
        };

        std::uint64_t creationTime100ns = 0;
        DWORD_PTR originalAffinityMask = 0;
        WORD observedProcessorGroup = 0;
        DWORD processId = 0;
        DWORD originalPriorityClass = 0;
        RollbackMode rollbackMode = RollbackMode::LegacyProcessAffinityMask;
    };

    [[nodiscard]] static std::expected<WinHandle, ErrorCode> openThreadForRollback(DWORD threadId) noexcept;
    [[nodiscard]] static std::expected<WinHandle, ErrorCode> openProcessForRollback(DWORD processId) noexcept;
    [[nodiscard]] static std::expected<std::uint64_t, ErrorCode> queryThreadCreationTime100ns(HANDLE threadHandle) noexcept;
    [[nodiscard]] static std::expected<std::uint64_t, ErrorCode> queryProcessCreationTime100ns(HANDLE processHandle) noexcept;
    [[nodiscard]] static bool isSameIdentity(std::optional<std::uint64_t> expectedCreationTime, std::uint64_t currentCreationTime) noexcept;
    [[nodiscard]] static std::string_view processRollbackModeToString(ProcessRollbackState::RollbackMode rollbackMode) noexcept;
    [[nodiscard]] std::expected<RollbackDisposition, ErrorCode> rollbackState(const ThreadRollbackState& state) noexcept;
    [[nodiscard]] std::expected<RollbackDisposition, ErrorCode> rollbackProcessState(const ProcessRollbackState& state) noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<DWORD, ThreadRollbackState> states_;
    std::unordered_map<DWORD, ProcessRollbackState> processStates_;
};
