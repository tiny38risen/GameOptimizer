# Operational Safety Runbook

## Default validation order

1. Run dry-run first.
2. Run soft-apply next.
3. Run apply only after rollback and latency sensor shutdown are clean in the previous modes.

## Recommended commands

Dry-run:

```bat
GameOptimizer.exe target.exe --dry-run --max-runtime-seconds 60
```

Soft-apply:

```bat
GameOptimizer.exe target.exe --max-runtime-seconds 120 --thread-detail-log --thread-log-interval 4
```

Apply:

```bat
GameOptimizer.exe target.exe --apply --latency-ping 8.8.8.8 --background-filter background_filter_example.txt --max-runtime-seconds 180
```

## Rollback interpretation

Non-fatal background rollback cases:

- process already exited
- PID creation time differs from saved rollback identity
- rollback entry is stale

Fatal background rollback cases:

- same PID
- same creation time
- process is still alive
- affinity or priority restoration fails

## Stop condition

A release candidate fails if exit code is non-zero, unless the scenario explicitly expects startup failure, such as missing target process validation.

## RC candidate artifact stop condition

`verify_rc_candidate.py --target <target.exe> --regression-log <log>` is mandatory before `v3.0-rc1`.

`create_rc_evidence_bundle.py --target <target.exe> --regression-log <log>` is mandatory after candidate verification. It creates the final evidence bundle and writes `rc_evidence_bundle_manifest.json` plus `rc_evidence_bundle_manifest.txt`.

The candidate is blocked when any of the following is missing:

1. Runbook coverage.
2. Release blocker list.
3. Current-commit smoke/soak evidence bundle.
4. Final regression result with `failed=0`.


## Runtime timeout safe-point invariant

`--max-runtime-seconds` must not stop the process from an arbitrary point in the main polling loop. The main loop may only set the timeout request flag. The watchdog callback owns the final transition to shutdown and may only call `gRunning.store(false, std::memory_order_release)` after:

1. thread tracking update has completed,
2. policy feedback dispatch has completed,
3. latency decision dispatch has completed,
4. the current watchdog cycle has reached its rollback-safe boundary.

Required log sequence:

1. `max runtime limit reached: ... shutdown will be requested at the next watchdog safe point`
2. `runtime timeout reached at watchdog cycle boundary; requesting clean shutdown`
3. `shutdown requested; stopping watchdog and latency sensor before rollback`
4. `shutdown completed cleanly`

Any timeout path that skips the cycle-boundary message or emits the required timeout logs out of order is a release blocker.
