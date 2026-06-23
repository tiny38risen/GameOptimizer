> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# Module Ownership Matrix

## Purpose

This document is part of `GameOptimizer Runtime Safety & Release Governance`.
It defines which module owns each runtime responsibility, which modules may call into that owner, and which modules must not take that responsibility.

The review question is:

```text
Does this change move responsibility outside the owning module?
```

If yes, the change is a `BLOCKER` unless the ADR, Contract Enforcement Matrix, static gate, runtime validation, and evidence contract are updated first.

## Ownership Rules

| Responsibility | Owner module | Access allowed from | Forbidden modules / paths | Severity | Enforcement |
|---|---|---|---|---|---|
| thread observation | `ThreadTracker` | `WatchdogCycleRunner`, `PolicyDispatcher`, runtime validation telemetry | mutation APIs, `RollbackManager`, `SchedulerController` mutation paths | BLOCKER | `check_thread_tracker_runtime_contracts` |
| thread mutation | `SchedulerController` | `PolicyDispatcher`, `WatchdogCycleRunner`, `StartupPipeline` policy construction | `ThreadTracker`, `InputLatencyController`, release scripts | BLOCKER | `check_apply_guard_transaction_patterns`, B2 expected scanner |
| process background restriction | `BackgroundController` | `PolicyDispatcher`, `StartupPipeline` policy construction | `ThreadTracker`, `SchedulerController`, input or topology modules | BLOCKER | `check_apply_mode_policy_contract`, `check_background_processor_group_policy_is_explicit` |
| rollback state ownership | `RollbackManager` | `SchedulerController`, `BackgroundController`, `ShutdownPipeline`, `ApplyGuard` cleanup | `ThreadTracker`, telemetry-only modules, evidence scripts | BLOCKER | `check_apply_guard_transaction_patterns`, `check_apply_guard_rollback_failure_ownership` |
| transaction cleanup | `ApplyGuard` | `SchedulerController`, `BackgroundController` | `ThreadTracker`, release scripts, telemetry modules | BLOCKER | `check_apply_guard_rollback_failure_ownership` |
| shutdown rollback | `ShutdownPipeline` | `RuntimeOrchestrator` | `ThreadTracker`, `SchedulerController` destructor paths, `BackgroundController` destructor paths | BLOCKER | shutdown result and preserved-state evidence gates |
| runtime validation | `RuntimeValidationMonitor` | `WatchdogCycleRunner`, `RuntimeOrchestrator` | mutation modules directly changing validation state without observation samples | BLOCKER | runtime validation summary and evidence gates |
| release evidence | `release_gate_evidence.py` | `run_rc_gate.bat`, `verify_rc_candidate.py`, `create_rc_evidence_bundle.py` | production runtime modules writing final RC decisions directly | BLOCKER | `check_release_evidence_contract`, `check_rc_candidate_contract` |
| processor topology policy | `TopologyAnalyzer` | `StartupPipeline`, `SchedulerController`, `BackgroundController` | group 0 coercion, process-wide group 1+ background mutation bypass | BLOCKER / WARN | `check_background_processor_group_policy_is_explicit` |
| input pinning eligibility | `InputLatencyController` | `StartupPipeline`, `TimerInputControllerTests` | `SchedulerController` input pinning before High confidence `ConcreteTid` evidence | BLOCKER | `check_timer_input_module_registration` |

## Cross-Module Rules

- `ThreadTracker` is observation-only. It must not call `SetThreadAffinityMask`, `SetThreadGroupAffinity`, `SetThreadPriority`, `SetPriorityClass`, `SetProcessAffinityMask`, or `RollbackManager`.
- `SchedulerController` is the only owner of thread-level scheduling mutation.
- `BackgroundController` is the only owner of process-level background restriction.
- `RollbackManager` owns rollback state. Callers may request save, rollback, discard, or validate operations, but they must not infer ownership from ad hoc booleans.
- `ApplyGuard` owns transaction cleanup after rollback state has been saved.
- `ShutdownPipeline` owns the final shutdown `rollbackAll()` attempt and evidence classification.
- Release scripts consume evidence. They must not mask a production `BLOCKER` as a `WARN`.

## Static Gate Consumption

`run_release_gate_static_checks.py` must verify that this file exists and contains the owner rows for:

- `ThreadTracker`
- `SchedulerController`
- `BackgroundController`
- `RollbackManager`
- `ApplyGuard`
- `ShutdownPipeline`
- `RuntimeValidationMonitor`
- `release_gate_evidence.py`

Any missing owner row is a `BLOCKER`.

The same gate must scan production source files for ownership-sensitive APIs:

- thread mutation APIs are allowed only in `SchedulerController.cpp` and `RollbackManager.cpp`;
- process mutation APIs are allowed only in `BackgroundController.cpp` and `RollbackManager.cpp`;
- `ApplyGuard::forThread()` and `ApplyGuard::forProcess()` are allowed only in mutation owner modules;
- `ThreadTracker.cpp` must contain no mutation API, `RollbackManager`, or `ApplyGuard` marker after comments and strings are removed.
