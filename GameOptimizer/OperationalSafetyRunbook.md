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

Apply mode remains conservative. Do not use broad background restriction without an explicit `--background-filter` deny/restrict configuration and clean dry-run plus soft-apply evidence for the same target.

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

## Processor Group / HEDT interpretation

Group 1+ background process-wide restriction is an explicit safety limitation, not a release blocker by itself. `SetProcessAffinityMask` is only considered safe for group 0 process-wide background restriction. When a group 1+ policy is selected, the runtime must record WARN/monitoring-only evidence and skip process-wide background mutation.

Thread-level `SetThreadGroupAffinity` remains the supported group-aware path. Per-thread group-aware background restriction is a Future Phase item and is LOW priority until a dedicated backend exists.

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
5. Real game validation notes for the intended RC target set.

## Real game validation invariant

Before proposing `v3.0-rc1`, validate 2-3 representative game processes with `RealGameValidationRunbook.md`.

Required order:

1. dry-run with no mutation evidence.
2. soft-apply with validated scheduling and rollback baseline evidence.
3. apply only when Access Denied, anti-cheat fallback, DPC/IRQ, Raw Input confidence, and rollback evidence are all documented and no BLOCKER is present.

Access Denied, IRQ unsupported, Raw Input unavailable, and group 1+ process-wide background monitoring-only are WARN findings when fallback evidence is present. They become BLOCKER findings only when the fallback/rollback evidence is missing or the runtime treats the unsupported path as ERROR/FAIL.


## Runtime timeout safe-point invariant

`--max-runtime-seconds` must not stop the process from an arbitrary point in the main polling loop. The main loop first sets the timeout request flag and records `ShutdownReason::MaxRuntimeSoftTimeout`, then gives the watchdog one safe-point grace window. The watchdog callback owns the preferred transition to shutdown and may only call `RuntimeSignalState::requestShutdown(ShutdownReason::MaxRuntimeSoftTimeout)` after:

1. thread tracking update has completed,
2. policy feedback dispatch has completed,
3. latency decision dispatch has completed,
4. the current watchdog cycle has reached its rollback-safe boundary.

If the watchdog safe point does not arrive within the hard-timeout grace window, `RuntimeOrchestrator` must request clean shutdown itself and stop the watchdog so max-runtime cannot hang indefinitely.

Required log sequence:

1. `max runtime limit reached: ... shutdown will be requested at the next watchdog safe point`
2. either `runtime timeout reached at watchdog cycle boundary; requesting clean shutdown` or `max runtime hard-timeout grace exceeded: ... forcing clean shutdown request`
3. `shutdown requested; reason=MaxRuntimeSoftTimeout; stopping policy cycles before rollback` or `shutdown requested; reason=MaxRuntimeHardTimeout; stopping policy cycles before rollback`
4. `runtime validation pre-rollback evidence snapshot begin`
5. `runtime validation post-rollback evidence snapshot begin`
6. `shutdown completed cleanly`

Any timeout path that emits neither the cycle-boundary message nor the hard-timeout grace message is a release blocker.
