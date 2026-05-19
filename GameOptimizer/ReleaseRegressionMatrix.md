# Release Regression Matrix

## Purpose
This matrix defines the minimum regression checks before merging a GameOptimizer build after Release Gate hardening.

## Required scenarios

| ID | Mode | Runtime | Required args | Pass criteria |
|---|---|---:|---|---|
| RG-1 | dry-run | 60s | `--dry-run --max-runtime-seconds 60` | no SetThread*/SetProcess* mutation logs, clean shutdown, exit code 0 |
| RG-2 | soft-apply | 120s | `--max-runtime-seconds 120 --thread-detail-log --thread-log-interval 4` | OpenThread validation succeeds or logs recoverable WARN, clean shutdown, exit code 0 |
| RG-3 | apply | 180s | `--apply --latency-ping 8.8.8.8 --background-filter background_filter_example.txt --max-runtime-seconds 180` | rollback audit passes, background stale identities are non-fatal only when identity changed, exit code 0 |
| RG-4 | apply + detail | 60s | `--apply --background-detail-log --thread-detail-log --thread-log-interval 8 --max-runtime-seconds 60` | detail logs are bounded by interval, no log flood, clean shutdown |
| RG-5 | target missing | immediate | `<missing.exe> --dry-run` | process lookup error, no watchdog start, exit code 1 |
| SOAK-30 | dry-run | 30m | `--dry-run --max-runtime-seconds 1800 --thread-detail-log --thread-log-interval 20` | clean shutdown, runtime validation summary, monotonic runtime sample cycles, no runtime validation failure |
| SOAK-60 | soft-apply | 60m | `--max-runtime-seconds 3600 --thread-detail-log --thread-log-interval 40` | clean shutdown, runtime validation summary, monotonic runtime sample cycles, no runtime validation failure, no shutdown rollback failure |

## Merge blockers

1. Any rollback failure for the same PID and same creation time while the process is alive.
2. Any expected value access without Assign -> Check -> Bind.
3. Any watchdog hot-path heap allocation introduced for policy command or thread top-N buffers.
4. Any shutdown path that skips watchdog join or latency sensor join.
5. Any log burst that prints per-process background skips unless `--background-detail-log` is explicitly set.
6. Any RC soak evidence report missing either SOAK-30 or SOAK-60.


## Automated assertions

The Release Gate smoke script now runs two assertion layers before accepting a build:

1. `run_release_gate_static_checks.py`
   - rejects direct `std::expected` dereference and `operator->` access across project source files using the Release Gate B2 scanner. Explicit bind lines are the only allowed value-access form.
   - verifies that runtime timeout shutdown is routed through the watchdog cycle boundary safe-point log.
   - verifies the timeout safe-point log sequence order, not just marker presence.

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
