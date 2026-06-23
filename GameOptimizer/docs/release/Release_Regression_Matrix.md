> Status: NeedsReview  
> 이 문서는 권위, 자동 생성 경로, 또는 현재 기준 여부 확인이 필요합니다. 현재 기준은 `docs/DOCUMENT_REGISTER.md`를 따릅니다.

# Release Regression Matrix

## Purpose
This matrix defines the minimum regression checks before merging a GameOptimizer build after Release Gate hardening.

## Required scenarios

The full RC entry command is:

```bat
run_rc_gate.bat <target.exe>
```

It must pass Python `py_compile`, `git diff --check`, static gate, Release x64 MSVC build, regression, release smoke, the combined 30m+60m soak evidence gate, final RC evidence verification for the current commit, and final RC evidence bundle creation.

RC report severity is split into three levels:

- `BLOCKER`: release is forbidden and the evidence report fails.
- `WARN`: release may continue, but the limitation must remain visible in the report.
- `INFO`: tracking metadata only.

The authoritative governance, architecture, schema, and blocker contracts are `docs/governance/README.md`, `docs/architecture/Architecture_Decision_Record.md`, `docs/architecture/Contract_Enforcement_Matrix.md`, `docs/release/Evidence_Schema.md`, and `docs/release/Release_Blocker_List.md`.

| ID | Mode | Runtime | Required args | Pass criteria |
|---|---|---:|---|---|
| RG-1 | dry-run | 60s | `--dry-run --max-runtime-seconds 60` | no SetThread*/SetProcess* mutation logs, clean shutdown, exit code 0 |
| RG-2 | soft-apply | 120s | `--max-runtime-seconds 120 --thread-detail-log --thread-log-interval 4` | OpenThread validation succeeds or logs recoverable WARN, clean shutdown, exit code 0 |
| RG-3 | apply | 180s | `--apply --latency-ping 8.8.8.8 --background-filter background_filter_example.txt --max-runtime-seconds 180` | rollback audit passes, background stale identities are non-fatal only when identity changed, exit code 0 |
| RG-4 | apply + detail | 60s | `--apply --background-detail-log --thread-detail-log --thread-log-interval 8 --max-runtime-seconds 60` | detail logs are bounded by interval, no log flood, clean shutdown |
| RG-5 | target missing | immediate | `<missing.exe> --dry-run` | process lookup error, no watchdog start, exit code 1 |
| SOAK-30 | dry-run | 30m | `--dry-run --max-runtime-seconds 1800 --thread-detail-log --thread-log-interval 20` | clean shutdown, runtime validation summary, monotonic runtime sample cycles, no runtime validation failure |
| SOAK-60 | soft-apply | 60m | `--max-runtime-seconds 3600 --thread-detail-log --thread-log-interval 40` | clean shutdown, runtime validation summary, monotonic runtime sample cycles, no runtime validation failure, no shutdown rollback failure |

Standalone soak entry points:

- `run_dry_run_soak_30m.bat <target.exe>` runs SOAK-30 and records soak evidence.
- `run_soft_apply_soak_60m.bat <target.exe>` runs SOAK-60 and records soak evidence.

Both standalone runs validate `shutdown_reason`, `runtime_validation_status`, `rollback_preserved_state_count`, BLOCKER count, timeline monotonicity, and heartbeat progression.

## Merge blockers

1. Any rollback failure for the same PID and same creation time while the process is alive.
2. Any expected value access without Assign -> Check -> Bind.
3. Any watchdog hot-path heap allocation introduced for policy command or thread top-N buffers.
4. Any shutdown path that skips watchdog join or latency sensor join.
5. Any log burst that prints per-process background skips unless `--background-detail-log` is explicitly set.
6. Any RC soak evidence report missing either SOAK-30 or SOAK-60.
7. Any RC gate run that does not generate smoke and soak evidence reports.
8. Any current-commit RC evidence verification missing either the smoke or soak PASS report.
9. Any RC evidence schema version, git commit, or exe SHA-256 mismatch.
10. Any RC evidence missing `git_dirty` or `git_status_short`.
11. Any RC candidate missing Runbook, blocker list, evidence bundle, or final regression result.
12. Any Access Denied / access-boundary runtime log without fallback or rollback evidence.
13. Any IRQ unsupported path that becomes ERROR/FAIL instead of WARN + monitoring-only.
14. Any Processor Group / HEDT group 1+ mock missing affinity, rollback, or log evidence.
15. Any Input Latency pinning path enabled without High confidence and `ConcreteTid`.
16. Any missing `ProcessorGroupHedtEvidenceTests` registration or failure.
17. Any missing or stale `docs/architecture/Architecture_Decision_Record.md` architecture contract.


## Automated assertions

The Release Gate smoke script now runs two assertion layers before accepting a build:

1. `run_release_gate_static_checks.py`
   - rejects direct `std::expected` dereference and `operator->` access across project source files using the Release Gate B2 scanner. Explicit bind lines are the only allowed value-access form.
   - verifies that runtime timeout shutdown is routed through the watchdog cycle boundary safe-point log.
   - verifies the timeout safe-point log sequence order, not just marker presence.
   - verifies that the ADR contract index exists and contains the accepted runtime mutation, observation, processor-group, soft-apply, background restriction, access-boundary fallback, input pinning, limited apply mode, and release evidence decisions.

2. `run_release_gate_log_assertions.py`
   - rejects missing clean shutdown logs.
   - rejects shutdown rollback failures.
   - rejects rollback error logs.
   - rejects dry-run mutation markers.
   - requires apply-mode rollback audit success.
   - verifies background rollback stale evidence by PID, preventing unrelated stale logs from masking real rollback failures.
   - requires timeout shutdown to pass through the watchdog safe point in the documented order.

3. `release_gate_evidence.py`
   - creates a run-id directory under `release_gate_logs`.
   - records per-step command exit codes, assertion exit codes, log paths, and log SHA-256 values.
   - records git commit, build hash, and `GameOptimizer.exe` SHA-256.
   - writes `rc_evidence_report.txt` and `rc_evidence_report.json`.
   - fails the report when `runtime validation result: FAILED` appears, and verifies that this condition pairs with process exit code 1.
   - verifies the final RC evidence set with `verify-rc`, requiring current-commit smoke and soak PASS reports for the target.
   - blocks schema version, git commit, exe SHA-256, smoke/soak exe hash, or smoke/soak build hash mismatch.
   - records `severity_summary` as `BLOCKER`, `WARN`, and `INFO`; WARN-only reports produce `PASS_WITH_WARNINGS`, not release failure.

4. `verify_rc_candidate.py`
   - verifies the RC runbook, evidence schema, blocker list, evidence bundle, and final regression result.
   - blocks `v3.0-rc1` preparation when any required RC candidate artifact is missing.

5. `create_rc_evidence_bundle.py`
   - runs the same RC candidate checks before writing any bundle manifest.
   - creates `rc_evidence_bundle_manifest.json` and `rc_evidence_bundle_manifest.txt`.
   - copies smoke evidence, soak evidence, step logs, and the final regression log into one run-id directory.
   - records ThreadTracker telemetry, Input Latency fallback state, Network/IRQ monitoring-only state, Access Denied fallback evidence, runtime validation summaries, processor group mode summaries, rollback preserved-state summaries, and the fixed BLOCKER/WARN/INFO severity policy.
   - records `RC_CANDIDATE_PASS` only when there are no `BLOCKER` findings.

6. `docs/release/Real_Game_Validation_Runbook.md`
   - defines the required dry-run -> soft-apply evidence table for 2-3 real game processes.
   - keeps Apply mode blocked until dry-run/soft-apply evidence is clean and the deny/restrict configuration is explicit.

RG-3 identity rule remains strict: background rollback failures are non-fatal only when stale identity evidence is logged. Same PID + same creation time + live process rollback failure is a release blocker.

## RG-6 ApplyGuard Transaction Gate

ApplyGuard is a merge gate for all apply-mode mutations. RG-6 is intentionally separate from RG-5 target-missing validation.

Required checks:

1. SchedulerController uses `saveAppliedState()` -> `ApplyGuard::forThread()` -> `arm()` before thread mutation.
2. SchedulerController uses `rollbackNow()` on partial apply/audit failure.
3. SchedulerController uses `commit()` only after apply verification.
4. BackgroundController uses `saveProcessState()` -> `ApplyGuard::forProcess()` -> `arm()` before process mutation.
5. BackgroundController does not use `hasProcessState()` for rollback ownership decisions.
6. Direct expected dereference outside an explicit bind line is a merge blocker.

Automated check:

```bat
python run_release_gate_static_checks.py
```

Scope note:

The ApplyGuard static gate now verifies the minimum function-scope lexical order for the concrete SchedulerController and BackgroundController apply paths: save -> guard construction -> arm -> mutation -> verification/apply completion -> commit. It also rejects known forbidden rollback-ownership patterns. This is still not a full branch-level control-flow proof; every rollbackNow/discardSavedState failure path must still be reviewed during merge.
