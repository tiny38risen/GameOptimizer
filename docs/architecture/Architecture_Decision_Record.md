# Architecture Decision Record

## ADR-001: Runtime mutation must be transactional

Status: Accepted

Decision:
All affinity, priority, timer, and background restriction mutations must be guarded by an explicit save/apply/verify/rollback contract.

Rationale:
GameOptimizer modifies live Windows runtime state owned by external processes and threads. Any partial mutation can leave the user system in a degraded state.

Contract:
- Query original state before mutation.
- Save rollback state before mutation.
- Arm an ApplyGuard before mutation.
- Verify the post-apply state.
- Commit only after verification succeeds.
- Roll back on apply or verify failure.

Forbidden:
- Applying Win32 mutation APIs before rollback state is saved.
- Discarding rollback state without audit.
- Treating rollback failure as WARN or INFO.

---

## ADR-002: ThreadTracker is observation-only

Status: Accepted

Decision:
ThreadTracker may only observe thread CPU deltas, EMA, stickiness, migration, and main-thread candidates.

Forbidden:
- SetThreadAffinityMask
- SetThreadGroupAffinity
- SetThreadPriority
- SetPriorityClass
- RollbackManager calls

Rationale:
Observation and mutation in the same module creates hidden state mutation and rollback ambiguity.

---

## ADR-003: SchedulerController is the only thread scheduling mutator

Status: Accepted

Decision:
Thread-level affinity and priority mutation is centralized in SchedulerController.

Contract:
- SchedulerController must use RollbackManager and ApplyGuard.
- SchedulerController must use std::expected Assign -> Check -> Bind.
- SchedulerController must perform post-apply verification.
- SetThreadGroupAffinity failure must run post-failure audit.

---

## ADR-004: Processor Group policy

Status: Accepted

Decision:
Thread-level affinity is processor-group-aware. Process-wide group 1+ background affinity restriction is not supported in v3.0-rc1.

Rationale:
SetProcessAffinityMask cannot safely express and restore processor-group-specific process affinity.

Contract:
- CoreMask must include processorGroup and KAFFINITY.
- group 0 hardcoding is forbidden.
- group 1+ process-wide background restriction is WARN + monitoring-only.

---

## ADR-005: SoftApply is not rollback state

Status: Accepted

Decision:
SoftApply validates rights and state, but must not be treated as an applied rollback state.

Contract:
- SoftApply baseline is evidence INFO.
- SoftApply baseline must not increase rollback preserved state count.
- SoftApply baseline must not be restored by rollbackAll as if mutation happened.

---

## ADR-006: Release decisions are evidence-driven

Status: Accepted

Decision:
A release candidate is valid only when the release gate produces a complete evidence bundle.

Contract:
- Missing evidence field is BLOCKER.
- binary hash mismatch is BLOCKER.
- schema mismatch is BLOCKER.
- runtime validation FAILED is BLOCKER.
- rollback failure is BLOCKER.
