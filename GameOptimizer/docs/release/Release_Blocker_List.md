# GameOptimizer Release Blocker List

## Purpose

This file is the authoritative `v3.0-rc1` release blocker list. If any `BLOCKER` is present, the release candidate must not be tagged.

## BLOCKER

- `run_rc_gate.bat <target.exe>` fails.
- Python `py_compile` fails.
- `git diff --check` fails.
- Static release gate fails.
- Release x64 MSVC build fails.
- Full regression fails.
- Release smoke fails.
- 30m dry-run soak fails.
- 60m soft-apply soak fails.
- `verify-rc` fails.
- Final evidence bundle generation fails.
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
- Real game validation record is missing before `v3.0-rc1` tagging.
- Real game validation has fewer than 3 games.
- Real game validation has fewer than 2 successful 60m soft-apply runs.
- Real game validation lacks a limited apply-mode validation.
- Real game validation lacks policy decision telemetry.

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
