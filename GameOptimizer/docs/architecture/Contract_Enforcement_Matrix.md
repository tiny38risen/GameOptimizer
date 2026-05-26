# Contract Enforcement Matrix

## Purpose

This matrix turns accepted architecture contracts into enforceable release gates.
It is not explanatory background. It defines what code must be rejected, which module owns the contract, what severity applies, which static gate must exist, which runtime validation is required, and which evidence field must prove compliance.

Current development rule:

```text
Architecture_Decision_Record.md
-> Contract_Enforcement_Matrix.md
-> static gate
-> runtime validation
-> evidence
-> run_rc_gate
-> release decision
```

## Severity Model

- `BLOCKER`: merge or release candidate must fail.
- `WARN`: allowed only with explicit fallback or limitation evidence.
- `INFO`: tracking evidence only.

## Enforcement Matrix

| Contract | Owner | Owned files | Forbidden pattern | Severity | Static gate | Runtime validation | Evidence fields |
|---|---|---|---|---|---|---|---|
| Runtime mutation is transactional | ApplyGuard / RollbackManager | `ApplyGuard.*`, `RollbackManager.*`, `SchedulerController.cpp`, `BackgroundController.cpp` | Win32 mutation before rollback state save; commit before verification; rollback state discard without audit | BLOCKER | `check_apply_guard_transaction_patterns`, `check_apply_guard_rollback_failure_ownership` | rollback failure preserves state and shutdown retries | `apply_guard_rollback_failure`, `apply_guard_rollback_failure_count`, `rollback_failure_transferred_to_shutdown_count`, `rollback_preserved_state_count`, `blocker_count` |
| ThreadTracker is observation-only | ThreadTracker | `ThreadTracker.*` | `SetThreadAffinityMask`, `SetThreadGroupAffinity`, `SetThreadPriority`, `SetPriorityClass`, `SetProcessAffinityMask`, `RollbackManager` calls | BLOCKER | `check_thread_tracker_runtime_contracts` plus forbidden API scan | ThreadTracker telemetry records access/query failures without mutation | `thread_tracker_telemetry`, `runtime_validation_status` |
| Thread scheduling mutation owner | SchedulerController | `SchedulerController.*` | thread-level mutation outside `SchedulerController`; direct expected dereference | BLOCKER | `check_apply_guard_transaction_patterns`, B2 expected scanner | post-apply group, affinity, priority audit | `test_results`, `blocker_count` |
| Processor group policy | TopologyAnalyzer / SchedulerController / BackgroundController | `TopologyAnalyzer.*`, `SchedulerController.*`, `BackgroundController.*`, `RollbackManager.*` | group 0 coercion; group 1+ process-wide background affinity mutation | BLOCKER for mutation, WARN for documented monitoring-only limitation | `check_background_processor_group_policy_is_explicit` | group 1+ restriction is blocked and logged as monitoring-only | `processor_group_mode`, `background_restriction_mode`, `warn_count` |
| SoftApply is evidence, not rollback state | SchedulerController / BackgroundController / RollbackManager | `SchedulerController.cpp`, `BackgroundController.cpp`, `RollbackManager.cpp` | storing soft-apply baseline as applied rollback state; restoring soft-apply baseline on shutdown | BLOCKER | `check_release_evidence_contract`, `check_apply_mode_policy_contract` | soft-apply baseline count does not increase preserved applied state | `soft_apply_baseline_summary`, `rollback_preserved_state_count` |
| Release decisions are evidence-driven | Release gate | `release_gate_evidence.py`, `verify_rc_candidate.py`, `create_rc_evidence_bundle.py`, `run_rc_gate.bat` | missing schema/hash/commit/binary/evidence field; WARN-only treated as failure; BLOCKER treated as pass | BLOCKER | `check_release_evidence_contract`, `check_rc_candidate_contract` | final smoke/soak reports match commit and binary | `schema_hash`, `commit_sha`, `binary_sha256`, `blocker_count`, `test_results` |
| Background restriction owner | BackgroundController | `BackgroundController.*` | broad apply without deny/restrict config; protected process mutation; process rollback discard without audit | BLOCKER | `check_apply_mode_policy_contract`, `check_apply_guard_transaction_patterns` | process apply failures audit current state before discard | `background_restriction_mode`, `access_denied_fallback_summary`, `blocker_count` |
| Access boundary fallback | SchedulerController / BackgroundController / RollbackManager | `SchedulerController.cpp`, `BackgroundController.cpp`, `RollbackManager.cpp` | treating Access Denied as success without fallback evidence; escalation beyond public user-mode API | BLOCKER when evidence missing, WARN with fallback evidence | `check_anti_cheat_fallback_contract` | denied access remains monitoring-only or rollback evidence exists | `access_denied_fallback_summary`, `warn_count`, `blocker_count` |
| Input pinning eligibility | InputLatencyController | `InputLatencyController.*`, `TimerInputControllerTests.cpp` | input thread pinning without Raw Input + High confidence `ConcreteTid` | BLOCKER | `check_timer_input_module_registration` | low/missing TID stays monitoring-only | `input_latency_summary`, `warn_count` |
| Apply mode is limited and explicit | CliOptions / StartupPipeline / real-game validation | `CliOptions.cpp`, `StartupPipeline.cpp`, `verify_real_game_validation.py` | apply default; broad background apply without explicit filter; apply after anti-cheat suspicion or low confidence | BLOCKER | `check_apply_mode_policy_contract`, `check_rc_candidate_contract` | real-game matrix proves dry-run and soft-apply before limited apply | `test_results`, `policy_decision_telemetry`, `blocker_count` |
| Runtime timeout safe point | RuntimeOrchestrator / WatchdogCycleRunner / ShutdownPipeline | `RuntimeOrchestrator.*`, `WatchdogCycleRunner.*`, `ShutdownPipeline.*` | max-runtime stops from arbitrary polling point; missing cycle-boundary or hard-timeout evidence | BLOCKER | `REQUIRED_MAIN_PATTERNS`, log assertions | timeout reaches watchdog rollback-safe boundary or hard-timeout clean shutdown | `shutdown_reason`, `runtime_validation_status`, `test_results` |
| Atomic governance change unit | Engineering governance | `Engineering_Handbook.md`, `Contract_Enforcement_Matrix.md`, `Release_Gate_Spec.md`, `Release_Blocker_List.md`, `RC_Runbook.md`, `run_release_gate_static_checks.py` | unrelated contract chains in one commit; contract change without static gate; validator/evidence omission; missing selftest/regression evidence; blocker/runbook drift | BLOCKER | `check_atomic_governance_change_unit_contract` | verification output records static gate plus validator/selftest or regression evidence before commit | `test_results`, `blocker_count`, `git_commit` |

## Required Static Gate Behavior

The release static gate must fail when:

- required contract documents are missing,
- an accepted ADR is missing from `Architecture_Decision_Record.md`,
- this matrix is missing a contract row for an accepted ADR,
- `Module_Ownership_Matrix.md` is missing an owner row for an accepted runtime responsibility,
- forbidden mutation APIs appear in observation-only modules,
- expected values are consumed outside Assign -> Check -> Bind,
- group 1+ background process-wide mutation can bypass the processor-group guard,
- ApplyGuard transaction ordering markers are missing,
- SoftApply baseline storage writes into rollback state,
- release evidence schema/blocker/runbook contracts are stale.
- the atomic governance change unit is missing from engineering guidance, CEM, release blockers, runbook, or static gate.

## Required Runtime Validation Behavior

Runtime validation must surface these as `BLOCKER` evidence:

- rollback failure,
- preserved rollback state after shutdown,
- runtime validation `FAILED`,
- timeline monotonicity failure,
- heartbeat progression failure,
- unsafe rollback state discard,
- missing fallback evidence after access-boundary events,
- input pinning without High confidence `ConcreteTid`,
- group 1+ process-wide background affinity mutation.

## Required Evidence Coupling

The evidence schema must expose release-facing fields sufficient to reject each `BLOCKER` contract violation:

- `schema_hash`
- `commit_sha`
- `binary_sha256`
- `runtime_validation_status`
- `rollback_preserved_state_count`
- `apply_guard_rollback_failure_count`
- `rollback_failure_transferred_to_shutdown_count`
- `blocker_count`
- `warn_count`
- `info_count`
- `processor_group_mode`
- `background_restriction_mode`
- `thread_tracker_telemetry`
- `test_results`

Compatibility summaries used by validators must remain available:

- `shutdown_failure_classification.shutdown_reason`
- `soft_apply_baseline_summary`
- `apply_guard_rollback_failure`
- `apply_guard_rollback_failure_count`
- `rollback_failure_transferred_to_shutdown_count`
- `input_latency_summary`
- `network_irq_summary`
- `access_denied_fallback_summary`
- `runtime_validation_summary`

## Release Gate Consumption

`run_rc_gate.bat` must execute the static contract gate before build, regression, smoke, soak, verify-rc, candidate verification, and final bundle creation. The static gate must reject RC step drift by checking the ordered `[RC-1]` through `[RC-10]` markers in `run_rc_gate.bat`, and `run_release_gate_static_checks_selftest.py` must prove missing or out-of-order markers are rejected.

The expected chain is:

```text
Contract_Enforcement_Matrix.md
-> Module_Ownership_Matrix.md
-> run_release_gate_static_checks.py
-> run_release_gate_log_assertions.py
-> release_gate_evidence.py
-> verify_rc_candidate.py
-> create_rc_evidence_bundle.py
-> run_rc_gate.bat
```

Any break in this chain is a `BLOCKER`.
