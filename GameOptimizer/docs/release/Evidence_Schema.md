# GameOptimizer RC Evidence Schema

## Purpose

This document freezes the `v3.0-rc1` release evidence contract. A release is approved only when the evidence schema matches this document, required fields exist, identity hashes match, and `BLOCKER` count is zero.

## Required report files

Each smoke or soak run must create:

- `evidence_state.json`
- `rc_evidence_report.json`
- `rc_evidence_report.txt`
- `logs/<step>.log`

The final bundle must create:

- `rc_evidence_bundle_manifest.json`
- `rc_evidence_bundle_manifest.txt`
- copied smoke evidence
- copied soak evidence
- copied step logs
- copied final regression log
- copied real-game validation matrix

## Required JSON fields

Each `rc_evidence_report.json` and final bundle manifest must expose these release-facing fields:

- `schema_version`
- `schema_hash`
- `commit_sha`
- `branch`
- `dirty_tree`
- `build_configuration`
- `compiler_version`
- `binary_path`
- `binary_sha256`
- `target_process`
- `scheduler_mode`
- `shutdown_reason`
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

Compatibility aliases such as `schema`, `git_commit`, `exe_sha256`, and `severity_summary` may remain, but the release-facing fields above are mandatory.

## Field meanings

- `schema_version`: evidence schema identifier, currently `gameoptimizer.rc_evidence.v1` for run reports and `gameoptimizer.rc_evidence_bundle.v1` for bundles.
- `schema_hash`: SHA-256 of this file.
- `commit_sha`: current Git commit SHA.
- `branch`: current Git branch name.
- `dirty_tree`: whether `git status --short` had entries when the evidence was initialized.
- `build_configuration`: expected release configuration, `Release|x64`.
- `compiler_version`: detected MSVC toolset version or `unknown`.
- `binary_path`: executable path used for the run.
- `binary_sha256`: SHA-256 of the executable.
- `target_process`: target process name passed to the gate.
- `scheduler_mode`: run mode or `mixed` for multi-mode smoke/soak reports.
- `shutdown_reason`: latest structured shutdown reason observed in step evidence.
- `runtime_validation_status`: `PASSED_OR_INCONCLUSIVE`, `FAILED`, or `UNKNOWN`.
- `rollback_preserved_state_count`: total preserved rollback states after shutdown.
- `apply_guard_rollback_failure_count`: total explicit or destructor `ApplyGuard` rollback failures observed across steps.
- `rollback_failure_transferred_to_shutdown_count`: total explicit `ApplyGuard` rollback failures that transferred final recovery responsibility to `ShutdownPipeline` / `RollbackManager`.
- `blocker_count`, `warn_count`, `info_count`: severity counts.
- `processor_group_mode`: processor-group summary collected from logs.
- `background_restriction_mode`: background restriction summary collected from logs.
- `thread_tracker_telemetry`: ThreadTracker summary collected from logs.
- `test_results`: per-step command/assertion/runtime validation results.
- `real_game_validation_matrix`: bundled copy path for `docs/release/Game_Verification_Matrix.json`.
- `real_game_validation_matrix_sha256`: SHA-256 of the bundled real-game validation matrix.
- `regression_selftest_summary`: final regression PASS booleans for `run_release_gate_static_checks_selftest.py` and `release_gate_evidence_selftest.py`.
- `regression_selftest_summary` is valid only when both selftest values are `true`; false or missing values are `BLOCKER`.
- `regression_selftest_summary` must match the copied `final_regression_log` artifact in the final bundle; mismatches are `BLOCKER`.
- final bundle artifact entries: each `artifacts[]` item must point to an existing copied file and match its recorded `sha256` and `bytes` before PASS is printed.
- final bundle manifests: `rc_evidence_bundle_manifest.json` must reload with matching release fields, including `schema`, `schema_version`, `schema_hash`, and `status`; `rc_evidence_bundle_manifest.txt` must contain the candidate decision, schema hash, status, commit SHA, real-game matrix SHA-256, and BLOCKER none before PASS is printed.
- final bundle text manifest must include `Regression selftest summary:` plus both selftest keys before PASS is printed.
- final bundle `source_reports`: `smoke`, `soak`, `regression_log`, and `real_game_validation_matrix` must point to existing source files before PASS is printed.
- final bundle `source_reports` must SHA-256 match their copied artifacts: `smoke_json`, `soak_json`, `final_regression_log`, and `real_game_validation_matrix`.

Step-level evidence still records compatibility and audit fields used by the gate:

- `shutdown_failure_classification.shutdown_reason`
- `soft_apply_baseline_summary`
- `apply_guard_rollback_failure`
- `apply_guard_rollback_failure_count`
- `rollback_failure_transferred_to_shutdown_count`

## Identity mismatch messages

The RC verifier must treat these identity failures as `BLOCKER` conditions:

- schema version mismatch
- schema_hash mismatch
- git commit mismatch
- build hash mismatch
- missing dirty tree flag
- missing dirty tree status
- copied or placeholder real-game validation matrix
- missing real-game validation run evidence artifact
- exe SHA-256 mismatch

## BLOCKER conditions

- required field missing
- schema mismatch
- commit mismatch
- hash mismatch
- rollback failure
- `ApplyGuard` rollback failure
- `ApplyGuard` explicit rollback failure without matching shutdown responsibility transfer evidence
- `SetThreadGroupAffinity` failure plus audit query failure
- `SetThreadGroupAffinity` failure plus audit mismatch
- runtime validation `FAILED`
- timeline monotonicity failure
- heartbeat progression failure
- unsafe rollback state discard

## Failure injection matrix

The evidence self-test must inject these release failures or limitations:

- missing exe: `BLOCKER`
- missing binary SHA-256: `BLOCKER`
- schema mismatch: `BLOCKER`
- commit mismatch: `BLOCKER`
- hash mismatch: `BLOCKER`
- runtime validation `FAILED`: `BLOCKER`, finalizer exit code `1`
- rollback failure: `BLOCKER`, finalizer exit code `1`
- `ApplyGuard` rollback failure: `BLOCKER`, finalizer exit code `1`
- `SetThreadGroupAffinity` failure plus audit query failure: `BLOCKER`
- `SetThreadGroupAffinity` failure plus audit mismatch: `BLOCKER`
- Access Denied fallback: `WARN`
- group 1+ mock background restriction: `WARN`
- timeline monotonicity failure: `BLOCKER`
- heartbeat progression failure: `BLOCKER`
- telemetry-only observation: `INFO`
