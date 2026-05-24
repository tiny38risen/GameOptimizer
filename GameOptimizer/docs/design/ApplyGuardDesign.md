# ApplyGuard Design

## Purpose

ApplyGuard defines a rollback-safe transaction boundary for partial policy application.
It does not replace Win32 calls. It wraps the period after rollback state has been saved and before apply/audit is committed.

## Scope

Current integration targets:

1. SchedulerController main-thread affinity/priority apply
2. BackgroundController background-process affinity/priority restriction

Timer/network system settings are intentionally deferred until their original-state capture interfaces are stable.

## State Machine

1. Construct: inactive guard, no side effect
2. Save rollback state through RollbackManager
3. arm(): guard becomes responsible for cleanup
4. apply Win32 changes
5. audit/verify
6. commit(): keep rollback state for shutdown recovery and disable guard

Failure paths:

- apply fails before any state mutation is possible: discardSavedState() only for newly created rollback state
- apply fails after partial mutation: rollbackNow()
- audit fails: rollbackNow()
- unexpected scope exit while armed: destructor performs best-effort rollback and logs failure
- rollbackNow() failure: log one BLOCKER, preserve RollbackManager state, transfer final retry responsibility to ShutdownPipeline/RollbackManager, and suppress destructor duplicate retry

## Invariants

- ApplyGuard never creates rollback state by itself.
- ApplyGuard never hides explicit rollbackNow() failure.
- ApplyGuard move assignment is deleted. An armed guard cannot be overwritten by another guard.
- commit() means the applied state is valid and rollback data must remain available for shutdown.
- discardSavedState() is only legal when the saved rollback state was created in the same transaction.
- Existing rollback state must not be discarded on pre-mutation apply failure.
- After rollbackNow() failure, ApplyGuard no longer owns retry. RollbackManager state remains preserved for shutdown rollbackAll().

## Non-goals

- No hot-path allocation changes.
- No global rollback policy rewrite.
- No timer/network state integration in this patch.

## Rev Ownership Fix

ApplyGuard must not infer rollback-state ownership from caller-provided boolean combinations.
RollbackManager is the source of truth for whether a rollback state was newly created or reused.

`saveAppliedState()` and `saveProcessState()` return `SaveStateDisposition`:

- `CreatedNewState`: the current transaction owns the saved rollback entry until commit.
- `ReusedExistingState`: a prior rollback entry already existed and must not be discarded by the current transaction.

Pre-apply failure handling:

- `CreatedNewState` -> `ApplyGuard::discardSavedState()` removes the rollback state.
- `ReusedExistingState` -> `ApplyGuard::discardSavedState()` releases the guard without removing the previous rollback state.

This removes the `hasProcessState()` / `saveProcessState()` TOCTOU pattern from the background apply path.

## Rollback Failure Responsibility Transfer

Explicit rollback failure is release-critical, but the guard must not spam duplicate rollback attempts.

Contract:

1. `rollbackNow()` logs the ApplyGuard rollback failure once.
2. RollbackManager keeps the failed rollback state.
3. ApplyGuard marks rollback responsibility as transferred to ShutdownPipeline/RollbackManager.
4. ApplyGuard destructor skips duplicate retry for that transferred failure.
5. ShutdownPipeline owns the final `rollbackAll()` attempt and evidence classification.
