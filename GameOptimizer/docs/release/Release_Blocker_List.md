# GameOptimizer Release Blocker List

## Purpose

This file is the authoritative `v3.0-rc1` release blocker list. If any `BLOCKER` is present, the release candidate must not be tagged.

## BLOCKER

- `run_rc_gate.bat <target.exe>` fails.
- `run_rc_gate.bat` is missing an `[RC-1]` through `[RC-10]` step marker or the markers are out of order.
- The static gate selftest does not prove missing and out-of-order RC step markers are rejected.
- `run_rc_gate.bat` does not run `run_release_gate_static_checks_selftest.py` before `run_release_gate_static_checks.py`.
- Python `py_compile` fails.
- `git diff --check` fails.
- Static release gate fails.
- `docs/architecture/Architecture_Decision_Record.md` is missing or stale.
- `docs/architecture/Contract_Enforcement_Matrix.md` is missing or stale.
- Release x64 MSVC build fails.
- Full regression fails.
- Release smoke fails.
- 30m dry-run soak fails.
- 60m soft-apply soak fails.
- `verify-rc` fails.
- Final evidence bundle generation fails.
- Final evidence bundle generation does not repeat real-game validation before writing the bundle.
- Final evidence bundle manifest is missing `real_game_validation_matrix` or `real_game_validation_matrix_sha256`.
- Final evidence bundle manifest artifact path, SHA-256, or byte-size validation fails.
- Written final JSON/TXT bundle manifest reload or marker validation fails.
- Written final JSON/TXT bundle manifest schema, schema hash, or status validation fails.
- Written final JSON/TXT bundle manifest target, commit, build hash, or binary SHA-256 validation fails.
- Final evidence bundle `source_reports` path validation fails.
- Final evidence bundle `source_reports` source-to-artifact SHA-256 validation fails.
- Static gate selftest does not exercise final bundle validators with real temporary files and expected missing/mismatched-file failures.
- Full regression does not run `run_release_gate_static_checks_selftest.py`.
- Final regression log is missing PASS markers for `run_release_gate_static_checks_selftest.py` or `release_gate_evidence_selftest.py`.
- Final evidence bundle manifest is missing `regression_selftest_summary`.
- Final evidence bundle manifest has a false `regression_selftest_summary` entry.
- Final evidence bundle `regression_selftest_summary` does not match the bundled `final_regression_log` artifact.
- Final text bundle manifest is missing the regression selftest summary or either selftest key.
- Required evidence field is missing.
- Evidence schema mismatch.
- Commit mismatch.
- Binary hash mismatch.
- Missing binary SHA-256.
- Schema hash mismatch.
- Dirty tree flag or status is missing from evidence.
- Runtime validation result is `FAILED`.
- Runtime validation summary reports `critical_failure=true`.
- Runtime validation `FAILED` is not paired with process exit code `1`.
- Rollback failure occurs.
- Rollback preserved state remains after shutdown.
- `ApplyGuard` explicit rollback failure is logged.
- `ApplyGuard` explicit rollback failure is not paired with `rollback_failure_transferred_to_shutdown_count` evidence.
- `ApplyGuard` destructor rollback failure is logged.
- Unsafe rollback state discard is reintroduced.
- `SetThreadGroupAffinity` failure path discards rollback state without post-failure state audit.
- `SetThreadGroupAffinity` failure plus post-failure audit query failure.
- `SetThreadGroupAffinity` failure plus post-failure audit mismatch.
- Timeline monotonicity failure.
- Heartbeat progression failure.
- Hang detection failure.
- Shutdown reason is missing.
- Access Denied or access-boundary event lacks fallback or rollback evidence.
- IRQ unsupported path is treated as ERROR/FAIL instead of WARN + monitoring-only.
- Input thread pinning is attempted without High confidence and `ConcreteTid`.
- Processor Group / HEDT group 1+ mock loses affinity, rollback, or log evidence.
- Group 1+ process-wide background affinity mutation is enabled for `v3.0-rc1`.
- Broad Apply mode is run without explicit background deny/restrict configuration.
- Apply mode is run without prior dry-run PASS and soft-apply PASS.
- Apply mode is run after Access Denied, anti-cheat suspicion, rollback state save failure, insufficient ThreadTracker confidence, ApplyGuard audit failure, unverified group-aware thread path, Low Raw Input TID confidence, group 1+ process-wide background restriction need, or many soft-apply WARN findings.
- A code, test, runbook, or release-gate change conflicts with an accepted ADR without updating the ADR and its enforcement.
- A development change skips the atomic governance change unit: document contract 1 -> static gate 1 -> validator/evidence coupling 1 -> selftest or regression 1 -> document/blocker/runbook sync -> verification -> commit 1.
- A governance contract changes without a matching static gate, validator/evidence coupling, selftest or regression, and release blocker/runbook synchronization.
- Real game validation record is missing before `v3.0-rc1` tagging.
- `run_rc_gate.bat` does not run `verify_real_game_validation.py --matrix docs\release\Game_Verification_Matrix.json` before `verify_rc_candidate.py`.
- Real game validation has fewer than 3 games.
- Real game validation has fewer than 2 successful 60m soft-apply runs.
- Real game validation lacks a limited apply-mode validation.
- Real game validation lacks policy decision telemetry.
- Real game validation matrix still contains copied template notes, example process names, placeholder tokens, or synthetic observations.
- Real game validation run is missing an `evidence_report` field.
- Real game validation run `evidence_report` points to a missing artifact.
- Real game validation `schema_version` is missing or mismatched.

## WARN

- IRQ affinity unsupported, recorded as monitoring-only.
- Raw Input not detected, fallback input policy active.
- Remote Raw Input detection unsupported through public Win32 APIs.
- Access Denied or access boundary encountered with fallback evidence.
- Processor Group 1+ background process-wide restriction blocked as monitoring-only.
- DPC spike observed while IRQ mutation backend is unavailable.
- SoftApply baseline validation records a limitation without mutation.

## INFO

- Dry-run decision output.
- ThreadTracker telemetry.
- Input latency observation without pinning.
- Network/IRQ observation without mutation.
- Processor group provenance.
- Shutdown reason.
- Binary fingerprint.
- Runtime sample count.

## v3.0-rc1 intentional exclusions

These are intentional exclusions, not release blockers when documented as WARN or INFO:

- group 1+ process-wide background affinity application
- low-confidence input thread pinning
- forced IRQ_REPIN mutation
- anti-cheat environment Apply mode without explicit fallback evidence
- self-tuning automatic optimization
- aggressive Apply mode as a default
