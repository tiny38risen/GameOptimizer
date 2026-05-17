// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: ApplyGuard
// ERROR-POLICY: expected
// Reason: failed partial apply paths must either rollback or explicitly discard pending rollback state.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <expected>

#include "ErrorCode.h"
#include "RollbackManager.h"

class ApplyGuard final
{
public:
    enum class TargetKind
    {
        Thread,
        Process
    };

    enum class RollbackStateOwnership
    {
        None,
        CreatedByTransaction,
        ReusedExistingState
    };

    [[nodiscard]] static ApplyGuard forThread(
        RollbackManager& rollbackManager,
        DWORD threadId,
        RollbackManager::SaveStateDisposition saveDisposition) noexcept;

    [[nodiscard]] static ApplyGuard forProcess(
        RollbackManager& rollbackManager,
        DWORD processId,
        RollbackManager::SaveStateDisposition saveDisposition) noexcept;

    ApplyGuard(const ApplyGuard&) = delete;
    ApplyGuard& operator=(const ApplyGuard&) = delete;

    ApplyGuard(ApplyGuard&& other) noexcept;
    ApplyGuard& operator=(ApplyGuard&& other) noexcept;

    ~ApplyGuard() noexcept;

    void arm() noexcept;
    void commit() noexcept;
    void discardSavedState() noexcept;

    [[nodiscard]] std::expected<void, ErrorCode> rollbackNow() noexcept;

private:
    ApplyGuard(
        RollbackManager& rollbackManager,
        TargetKind targetKind,
        DWORD targetId,
        RollbackStateOwnership rollbackStateOwnership) noexcept;

    [[nodiscard]] static RollbackStateOwnership toOwnership(
        RollbackManager::SaveStateDisposition saveDisposition) noexcept;

    void resetFrom(ApplyGuard&& other) noexcept;
    void releaseWithoutAction() noexcept;
    void removeSavedState() noexcept;
    [[nodiscard]] std::expected<void, ErrorCode> rollbackTarget() noexcept;

private:
    RollbackManager* rollbackManager_ = nullptr;
    DWORD targetId_ = 0;
    TargetKind targetKind_ = TargetKind::Thread;
    bool armed_ = false;
    RollbackStateOwnership rollbackStateOwnership_ = RollbackStateOwnership::None;
};
