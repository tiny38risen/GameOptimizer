// Build: cl /std:c++latest /O2 /W4 /WX /permissive- ApplyGuard.cpp
// MODULE: ApplyGuard
// ERROR-POLICY: expected
// Reason: transaction cleanup failure must be visible to the caller when explicit rollback is requested.

#include "ApplyGuard.h"

#include "Logger.h"

#include <utility>

ApplyGuard::ApplyGuard(
    RollbackManager& rollbackManager,
    TargetKind targetKind,
    DWORD targetId,
    RollbackStateOwnership rollbackStateOwnership) noexcept
    : rollbackManager_(&rollbackManager),
      targetId_(targetId),
      targetKind_(targetKind),
      rollbackStateOwnership_(rollbackStateOwnership)
{
}

ApplyGuard::RollbackStateOwnership ApplyGuard::toOwnership(
    RollbackManager::SaveStateDisposition saveDisposition) noexcept
{
    switch (saveDisposition)
    {
    case RollbackManager::SaveStateDisposition::CreatedNewState:
        return RollbackStateOwnership::CreatedByTransaction;
    case RollbackManager::SaveStateDisposition::ReusedExistingState:
        return RollbackStateOwnership::ReusedExistingState;
    }

    return RollbackStateOwnership::None;
}

ApplyGuard ApplyGuard::forThread(
    RollbackManager& rollbackManager,
    DWORD threadId,
    RollbackManager::SaveStateDisposition saveDisposition) noexcept
{
    return ApplyGuard(
        rollbackManager,
        TargetKind::Thread,
        threadId,
        toOwnership(saveDisposition));
}

ApplyGuard ApplyGuard::forProcess(
    RollbackManager& rollbackManager,
    DWORD processId,
    RollbackManager::SaveStateDisposition saveDisposition) noexcept
{
    return ApplyGuard(
        rollbackManager,
        TargetKind::Process,
        processId,
        toOwnership(saveDisposition));
}

ApplyGuard::ApplyGuard(ApplyGuard&& other) noexcept
{
    resetFrom(std::move(other));
}

ApplyGuard::~ApplyGuard() noexcept
{
    if (rollbackFailureTransferredToShutdown_)
    {
        return;
    }

    if (!armed_)
    {
        return;
    }

    const auto rollbackResult = rollbackTarget();
    if (!rollbackResult)
    {
        const ErrorCode error = rollbackResult.error();
        const bool missingState = error == ErrorCode::RollbackStateNotFound;
        const bool verificationFailure = error == ErrorCode::RollbackVerificationFailed;
        Logger::error(
            "apply guard destructor rollback failed for target {}: {} (category={}); rollback state is preserved for shutdown recovery",
            targetId_,
            toString(error),
            missingState ? "stale_or_missing_identity" : (verificationFailure ? "rollback_audit_failure" : "rollback_apply_failure"));
    }
}

void ApplyGuard::arm() noexcept
{
    armed_ = rollbackManager_ != nullptr && targetId_ != 0 && rollbackStateOwnership_ != RollbackStateOwnership::None;
}

void ApplyGuard::commit() noexcept
{
    releaseWithoutAction();
}

void ApplyGuard::discardSavedState() noexcept
{
    if (!armed_)
    {
        return;
    }

    if (rollbackStateOwnership_ == RollbackStateOwnership::CreatedByTransaction)
    {
        removeSavedState();
    }

    releaseWithoutAction();
}

std::expected<void, ErrorCode> ApplyGuard::rollbackNow() noexcept
{
    if (!armed_)
    {
        return {};
    }

    auto rollbackResult = rollbackTarget();
    if (!rollbackResult)
    {
        const DWORD failedTargetId = targetId_;
        Logger::error(
            "apply guard explicit rollback failed for target {}; rollback responsibility transferred to ShutdownPipeline/RollbackManager; rollback state remains preserved: {}",
            failedTargetId,
            toString(rollbackResult.error()));
        transferRollbackFailureToShutdown();
        return std::unexpected(rollbackResult.error());
    }

    releaseWithoutAction();
    return {};
}

void ApplyGuard::resetFrom(ApplyGuard&& other) noexcept
{
    rollbackManager_ = other.rollbackManager_;
    targetId_ = other.targetId_;
    targetKind_ = other.targetKind_;
    armed_ = other.armed_;
    rollbackFailureTransferredToShutdown_ = other.rollbackFailureTransferredToShutdown_;
    rollbackStateOwnership_ = other.rollbackStateOwnership_;

    other.releaseWithoutAction();
}

void ApplyGuard::transferRollbackFailureToShutdown() noexcept
{
    rollbackManager_ = nullptr;
    targetId_ = 0;
    armed_ = false;
    rollbackFailureTransferredToShutdown_ = true;
    rollbackStateOwnership_ = RollbackStateOwnership::None;
}

void ApplyGuard::releaseWithoutAction() noexcept
{
    rollbackManager_ = nullptr;
    targetId_ = 0;
    armed_ = false;
    rollbackFailureTransferredToShutdown_ = false;
    rollbackStateOwnership_ = RollbackStateOwnership::None;
}

void ApplyGuard::removeSavedState() noexcept
{
    if (rollbackManager_ == nullptr || targetId_ == 0)
    {
        return;
    }

    if (targetKind_ == TargetKind::Thread)
    {
        rollbackManager_->removeThreadState(targetId_);
        return;
    }

    rollbackManager_->removeProcessState(targetId_);
}

std::expected<void, ErrorCode> ApplyGuard::rollbackTarget() noexcept
{
    if (rollbackManager_ == nullptr || targetId_ == 0)
    {
        return {};
    }

    if (targetKind_ == TargetKind::Thread)
    {
        auto rollbackResult = rollbackManager_->rollbackThread(targetId_);
        if (!rollbackResult)
        {
            Logger::error(
                "thread apply guard rollback failed for TID {}: {}",
                targetId_,
                toString(rollbackResult.error()));
            return std::unexpected(rollbackResult.error());
        }

        return {};
    }

    auto rollbackResult = rollbackManager_->rollbackProcess(targetId_);
    if (!rollbackResult)
    {
        Logger::error(
            "process apply guard rollback failed for PID {}: {}",
            targetId_,
            toString(rollbackResult.error()));
        return std::unexpected(rollbackResult.error());
    }

    return {};
}
