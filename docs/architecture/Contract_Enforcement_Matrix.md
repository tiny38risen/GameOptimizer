# Contract Enforcement Matrix

This document is not descriptive documentation. It is the enforcement map that controls code review, static gates, runtime validation, and release approval.

## Status vocabulary

- REQUIRED: must be implemented or verified before v3.0-rc1.
- BLOCKER: release must fail when violated.
- WARN: release may continue only when the limitation is recorded in evidence.
- INFO: telemetry only.

## CEM-001 Runtime mutation transaction

Contract:
Every live Windows runtime mutation must follow Query -> Save -> Arm -> Apply -> Verify -> Commit.

Owned by:
- SchedulerController.cpp
- BackgroundController.cpp
- TimerResolutionController.cpp
- ApplyGuard.cpp
- RollbackManager.cpp

Forbidden patterns:
- SetThreadGroupAffinity before saveAppliedState
- SetThreadPriority before saveAppliedState
- SetPriorityClass before saveProcessState
- SetProcessAffinityMask before saveProcessState
- rollback state discard without audit

Violation severity:
BLOCKER

Static gate requirements:
- Reject direct scheduling mutation outside SchedulerController, BackgroundController, TimerResolutionController, or approved rollback code.
- Reject discardSavedState call unless nearby post-failure audit exists.

Runtime validation:
- Apply failure must produce rollbackNow or audited no-side-effect discard.
- Verify failure must produce rollbackNow.
- rollback failure must preserve state.

Evidence requirements:
- mutation_attempt_count
- rollback_failure_count
- rollback_preserved_state_count
- unsafe_discard_count

## CEM-002 ThreadTracker observation-only

Contract:
ThreadTracker may observe only. It must never mutate scheduling or rollback state.

Owned by:
- ThreadTracker.cpp
- ThreadTracker.h

Forbidden patterns:
- SetThreadAffinityMask
- SetThreadGroupAffinity
- SetThreadPriority
- SetPriorityClass
- RollbackManager
- ApplyGuard

Violation severity:
BLOCKER

Static gate requirements:
- Reject the forbidden patterns in ThreadTracker.*.

Evidence requirements:
- thread_tracker_telemetry_summary
- migration_count
- main_thread_confidence

## CEM-003 SchedulerController expected contract

Contract:
All std::expected results in production code must use Assign -> Check -> Bind.

Owned by:
- SchedulerController.cpp
- RollbackManager.cpp
- ApplyGuard.cpp
- StartupPipeline.cpp
- ShutdownPipeline.cpp
- WatchdogCycleRunner.cpp

Forbidden patterns:
- return func(...); for expected-returning calls
- func().value()
- result.value() in runtime code
- passing expected without checking

Violation severity:
BLOCKER

Static gate requirements:
- Reject .value() in GameOptimizer/*.cpp except explicit test files.
- Reject tail-return propagation of expected-returning calls.

## CEM-004 ApplyGuard ownership

Contract:
ApplyGuard owns transaction-scope rollback responsibility until commit, discard after audited no-side-effect, or explicit responsibility transfer to RollbackManager/ShutdownPipeline.

Owned by:
- ApplyGuard.h
- ApplyGuard.cpp
- SchedulerController.cpp
- BackgroundController.cpp

Forbidden patterns:
- Copy construction
- Copy assignment
- Armed move assignment
- rollback failure followed by state deletion
- rollback failure classified lower than BLOCKER

Violation severity:
BLOCKER

Static gate requirements:
- ApplyGuard copy operations must be deleted.
- ApplyGuard move assignment must be deleted or must reject armed replacement.
- rollbackNow failure must not call releaseWithoutAction.

Runtime validation:
- rollbackNow failure must keep rollback state preserved.
- duplicate destructor retry must not create duplicate BLOCKER counts unless explicitly classified as retry.

Evidence requirements:
- apply_guard_rollback_failure_count
- apply_guard_retry_count
- rollback_state_preserved_after_apply_guard_failure

## CEM-005 SoftApply baseline separation

Contract:
SoftApply is validation. It is not mutation. It must not be counted as applied rollback state.

Owned by:
- SchedulerController.cpp
- RollbackManager.cpp
- evidence generation scripts

Forbidden patterns:
- SoftApply baseline counted as rollback preserved state
- SoftApply baseline restored by rollbackAll as mutation
- SoftApply baseline classified as BLOCKER when no mutation occurred

Violation severity:
BLOCKER when it corrupts rollback evidence; otherwise WARN.

Static gate requirements:
- saveValidatedReadState must be distinguishable from saveAppliedState.
- evidence must separate soft_apply_baseline_count from rollback_preserved_state_count.

Evidence requirements:
- soft_apply_baseline_count
- rollback_preserved_state_count

## CEM-006 Processor Group policy

Contract:
Thread-level affinity is group-aware. Process-wide group 1+ background affinity restriction is monitoring-only in v3.0-rc1.

Owned by:
- TopologyAnalyzer.cpp
- SchedulerController.cpp
- BackgroundController.cpp
- RollbackManager.cpp

Forbidden patterns:
- group 0 hardcoding
- DWORD_PTR-only CoreMask model
- SetProcessAffinityMask for group 1+ background restriction

Violation severity:
BLOCKER for unsafe apply; WARN for monitoring-only limitation.

Static gate requirements:
- CoreMask must include processorGroup and affinity mask.
- group 1+ process-wide restriction must not call SetProcessAffinityMask.

Evidence requirements:
- processor_group_mode
- group_1_plus_monitoring_only_count
- blocked_group_id

## CEM-007 Runtime shutdown contract

Contract:
Shutdown must be reason-coded, safe-point aware, and evidence-visible.

Owned by:
- RuntimeSignalState.h
- RuntimeOrchestrator.cpp
- WatchdogCycleRunner.cpp
- ShutdownPipeline.cpp

Forbidden patterns:
- unstructured shutdown reason
- hidden watchdog stop failure
- rollback failure hidden during shutdown

Violation severity:
BLOCKER

Runtime validation:
- heartbeat progression must be monotonic.
- timeline must be monotonic.
- shutdown reason must be recorded.

Evidence requirements:
- shutdown_reason
- heartbeat_progression_status
- timeline_monotonicity_status
- runtime_validation_status

## CEM-008 Evidence-driven release

Contract:
No release candidate is valid without a complete evidence bundle.

Owned by:
- release scripts
- evidence scripts
- docs/release

Forbidden patterns:
- missing binary SHA-256
- missing commit SHA
- missing schema version
- schema mismatch
- commit mismatch
- hash mismatch

Violation severity:
BLOCKER

Evidence requirements:
- schema_version
- schema_hash
- commit_sha
- branch
- dirty_tree
- build_configuration
- compiler_version
- binary_path
- binary_sha256
- blocker_count
- warn_count
- info_count
