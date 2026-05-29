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
- SchedulerController affinity failure helper markers are missing,
- StartupPipeline prepare helper markers are missing,
- SoftApply baseline storage writes into rollback state,
- release evidence schema/blocker/runbook contracts are stale.
- the atomic governance change unit is missing from engineering guidance, CEM, release blockers, runbook, or static gate.
- CEM loses the recent contract markers required by `check_contract_enforcement_matrix`.

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

`SchedulerController` affinity failure handling must remain helper-split:

- `FailedAffinityApplyDisposition`
- `auditFailedAffinityApply`
- `makeAffinityApplyFailureResult`
- `logAffinityRollbackFailure`

Inlining these responsibilities back into `applyMainThreadPolicy()` is a `BLOCKER` because it obscures the audit -> rollback/discard -> evidence contract.

`StartupPipeline::prepare()` must keep startup responsibilities helper-split:

- `logStartupPolicySummary`
- `buildMainThreadPolicy`
- `loadAndPrepareBackgroundFilterConfig`
- `buildBackgroundRestrictionPolicy`

Inlining these responsibilities back into `prepare()` is a `BLOCKER` because it hides topology fallback, background filter preparation, and policy assembly boundaries.

`release_gate_evidence_selftest.py` must include ApplyGuard rollback failure fixtures for all three cases:

- explicit rollback failure without shutdown-transfer evidence, which must add the transfer-missing BLOCKER,
- explicit rollback failure with shutdown-transfer evidence, which must not duplicate the transfer-missing BLOCKER,
- destructor rollback failure must add one rollback failure BLOCKER and must not add the explicit transfer-missing BLOCKER.

`release_gate_evidence_selftest.py` must prove SoftApply baseline evidence stays separate from rollback preserved-state evidence:

- `soft_apply_baseline_summary` may record thread and process baseline counts,
- `rollback_preserved_state_count` must remain `0`,
- the step `rollback_preserved_state_summary.has_preserved_state` must remain `false`,
- the report must remain `PASS` when only SoftApply baseline audit evidence is present.

`Release_Blocker_List.md` must classify SoftApply baseline evidence that increases `rollback_preserved_state_count` or creates BLOCKER/WARN findings by itself under `BLOCKER`, not `WARN`.

ApplyGuard rollback evidence markers must stay in `BLOCKER`, not `WARN`: explicit rollback failure, missing shutdown-transfer evidence, and destructor rollback failure.

Processor Group 1+ monitoring-only marker must stay in `WARN`, not `BLOCKER`: group 1+ process-wide background restriction is a documented limitation only when mutation is skipped and monitoring-only evidence exists.

Access Denied fallback marker must stay in `WARN`, not `BLOCKER`: access-boundary failures are release-blocking only when fallback or rollback evidence is missing.

IRQ unsupported monitoring-only marker must stay in `WARN`, not `BLOCKER`: unsupported IRQ affinity remains a documented limitation when mutation is suppressed and monitoring-only evidence exists.

Raw Input fallback marker must stay in `WARN`, not `BLOCKER`: missing local Raw Input detection is release-blocking only if code attempts pinning without high-confidence `ConcreteTid` evidence.

Remote Raw Input unsupported marker must stay in `WARN`, not `BLOCKER`: remote-process Raw Input detection is an intentional public Win32 limitation unless the runtime attempts pinning without concrete local evidence.

DPC spike monitoring-only marker must stay in `WARN`, not `BLOCKER`: DPC spike observation is release-blocking only if the runtime treats unsupported IRQ mutation as ERROR/FAIL or applies an unsupported backend.

SoftApply limitation marker must stay in `WARN`, not `BLOCKER`: SoftApply baseline limitation evidence is release-blocking only if it increases rollback preserved-state evidence or creates severity findings by itself.

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
- `real_game_validation_matrix`
- `real_game_validation_matrix_sha256`
- `regression_selftest_summary`

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

`run_rc_gate.bat` must execute the static gate selftest and static contract gate before build, regression, smoke, soak, verify-rc, candidate verification, and final bundle creation. The static gate must reject RC step drift by checking the ordered `[RC-1]` through `[RC-10]` markers in `run_rc_gate.bat`, and `run_release_gate_static_checks_selftest.py` must run before `run_release_gate_static_checks.py` to prove missing or out-of-order markers are rejected.

Within `[RC-9]`, `run_rc_gate.bat` must run `verify_real_game_validation.py --matrix docs\release\Game_Verification_Matrix.json` before `verify_rc_candidate.py`, so real-game validation failures are visible as their own `BLOCKER`.

`create_rc_evidence_bundle.py` must also call the real-game matrix validator before creating the bundle directory. Direct bundle creation is a release decision path and cannot bypass real-game validation.

The final RC evidence bundle must copy `docs/release/Game_Verification_Matrix.json` and expose both `real_game_validation_matrix` and `real_game_validation_matrix_sha256` in the bundle manifest.

Before printing PASS, `create_rc_evidence_bundle.py` must validate every manifest artifact path, SHA-256, and byte size against the copied files.

After writing `rc_evidence_bundle_manifest.json` and `rc_evidence_bundle_manifest.txt`, `create_rc_evidence_bundle.py` must reload and validate both manifests before printing PASS.

Written final bundle manifests must preserve `schema`, `schema_version`, `schema_hash`, and `status` exactly; any mismatch is a `BLOCKER`.

Written final bundle manifests must preserve release identity fields exactly: `target`, `target_process`, `git_commit`, `commit_sha`, `build_hash`, and `binary_sha256`; any mismatch is a `BLOCKER`.

The final bundle manifest `source_reports` entries must point to existing smoke, soak, final regression, and real-game validation matrix source files before PASS.

The final bundle manifest `source_reports` entries must SHA-256 match their copied bundle artifacts (`smoke_json`, `soak_json`, `final_regression_log`, and `real_game_validation_matrix`) before PASS.

`run_release_gate_static_checks_selftest.py` must exercise the final bundle validators against real temporary files, including PASS cases and missing/mismatched file failures.

`run_regression_tests.bat` must run `run_release_gate_static_checks_selftest.py` so local/full regression catches static-gate selftest failures before RC gate execution.

`verify_rc_candidate.py` must reject final regression logs that do not include PASS markers for both `run_release_gate_static_checks_selftest.py` and `release_gate_evidence_selftest.py`.

`create_rc_evidence_bundle.py` must expose those final regression selftest PASS markers as `regression_selftest_summary` in the final bundle manifest.

`validate_written_manifests` must treat any false or missing `regression_selftest_summary` entry as a `BLOCKER`.

The text bundle manifest must include `Regression selftest summary:` plus both selftest keys before PASS.

`create_rc_evidence_bundle.py` must compare `regression_selftest_summary` with the bundled `final_regression_log` artifact before PASS; any mismatch is a `BLOCKER`.

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
