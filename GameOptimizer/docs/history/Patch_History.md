# GameOptimizer Patch History

## Purpose

This file consolidates the previous `PATCH_SUMMARY*` files into one history document.
It is historical context only; current development policy is governed by `docs/architecture/Architecture_Decision_Record.md`.

## Band: Release Gate And Evidence

### Release Gate Automated Assertions

Added `run_release_gate_static_checks.py` and `run_release_gate_log_assertions.py`.
The static gate enforces Assign -> Check -> Bind expected access, runtime timeout safe-point markers, and later ApplyGuard transaction sequencing.
The log assertion layer validates clean shutdown, rollback safety, dry-run non-mutation markers, apply rollback audit, stale-identity matching, and timeout safe-point sequencing.

Updated smoke and regression documentation:
- `run_release_gate_smoke.bat` captures logs under `release_gate_logs`.
- `docs/operations/ReleaseRegressionMatrix.md` documents automated assertion layers.
- `docs/operations/OperationalSafetyRunbook.md` defines timeout safe-point invariants.
- `docs/operations/ReleaseGateRunbook.md` documents smoke, soak, RC candidate, and bundle flow.

### Release Gate Blocker Fixes

Normalized expected value access in startup/watchdog paths:
- target process ID is bound once after `ProcessFinder` success.
- topology, observed threads, and command buffers are checked and bound before use.
- `--max-runtime-seconds` rejects signed, zero, malformed, and overflow-like inputs, with large values clamped.

Runtime timeout no longer stops the polling loop directly.
It arms a timeout request and lets the watchdog request shutdown at a rollback-safe cycle boundary.

### Release Gate Assertion And RG Sequence Fixes

Strengthened static and log gates:
- direct `std::expected` dereference and `operator->` access are rejected outside explicit bind lines.
- timeout safe-point log ordering is validated.
- apply-mode stale rollback validation matches stale identity evidence by PID.
- ApplyGuard transaction gate was renamed to RG-6 to avoid collision with RG-5 target-missing validation.
- RG-6 now checks function-scope ordering for `SchedulerController::applyMainThreadPolicy` and `BackgroundController::applyRestriction`.

The gate remains a conservative heuristic, not a full control-flow proof.
Branch-local rollback and audited discard paths still require review.

## Band: ApplyGuard And Rollback Safety

### ApplyGuard Introduction

Added `ApplyGuard` as an explicit rollback-safe transaction boundary for live runtime mutation.
Thread-level scheduling and process-level background restriction now follow:

1. save rollback state,
2. construct guard,
3. arm guard,
4. apply Win32 mutation,
5. verify or audit,
6. commit or rollback.

Initial integration covered `SchedulerController`, `BackgroundController`, and `RollbackManager`.

### ApplyGuard Ownership Correction

Removed caller-provided boolean ownership decisions.
`RollbackManager::SaveStateDisposition` became the source of truth:
- `CreatedNewState`
- `ReusedExistingState`

`ApplyGuard` now stores ownership as an enum and only discards rollback state when the current transaction created it.
This removed the `hasProcessState()` TOCTOU ownership pattern from `BackgroundController`.

### ApplyGuard Rollback Failure Transfer

When `rollbackNow()` fails:
- the failure is logged once,
- rollback state remains preserved,
- final retry responsibility transfers to `ShutdownPipeline` / `RollbackManager`,
- destructor duplicate retry is suppressed.

The release gate checks that move assignment remains deleted and that rollback failure transfer markers remain present.

### RollbackManager B2 Cleanup

`RollbackManager.cpp` was normalized to Assign -> Check -> Bind expected access.
Creation-time queries and verified identities are bound to named locals before use.
Rollback optional state reads were converted to value copies before rollback dispatch.

### Background Rollback Lifetime And Identity

Background rollback now distinguishes:
- process exited,
- process is no longer openable,
- PID creation time changed,
- same live process restore failure.

Only same PID + same creation time + live process restore failure is fatal.
Stale or replaced processes release their rollback entries without failing shutdown.

## Band: Runtime Hot Path And Concurrency

### Hot-Path Allocation Hardening

Replaced heap-backed command buffers in hot paths:
- `LatencyDecisionLayer` returns `PolicyCommandBuffer`.
- `AppliedPolicyTracker` returns `PolicyCommandBuffer`.
- `ThreadTracker::getTopThreads()` later moved to fixed `ThreadInfoBuffer`.

Removed transient string allocation from IPv4 formatting and bounded the fallback text path.

### Concurrency And Fixed Storage

Removed hidden hot-path allocations and data races:
- `LatencyMetricsCollector` publishes cached RTT jitter through `std::atomic<double>`.
- `ThreadTracker` moved from `std::unordered_map` to fixed-capacity arrays, active slot lists, and open-addressed lookup.
- per-cycle missing thread detection uses `seenInCycle` flags.
- stop-token aware sampling prevents fixed sleep polling from blocking shutdown.

### Critical Review Fixes

Normalized remaining expected access in `SchedulerController`, `ThreadTracker`, and startup paths.
Removed try/catch from hot sampling paths so failures remain explicit `std::expected` results.
Strengthened `ThreadSample::reset()` ownership order and fixed-storage invariant recovery.

## Band: Observability, Logging, And Metrics

### Thread And Metric Log Cleanup

ThreadTracker multi-sampling now treats the first observation in each watchdog window as a local baseline.
This prevents idle watchdog gaps from inflating sampled CPU deltas.

Log labels were clarified:
- `avg-delta` -> `avg-sample-cpu`
- `EMA` -> `EMA-sample-cpu`

ICMP transient timeout logging was softened:
- first miss is INFO,
- repeated misses are rate-limited,
- handle reopen is logged as a state transition.

Background process detail logs are summary-first by default and expandable with `--background-detail-log`.

### Release Logging Overhead

Thread detail logging became opt-in:
- `--thread-detail-log`
- `--thread-log-interval <cycles>`

Default release runs avoid display-only top-thread work unless diagnostics are requested.

## Band: Policy Feedback And Next Design Stabilization

### Policy Feedback Rearm Cooldown

`AppliedPolicyTracker` gained a rearm cooldown to prevent immediate repeated policy feedback cycles after pass, inconclusive, or rollback-request completion.

### Background Logging And Detail Control

`BackgroundFilterConfig::logSkippedProcessDetails` was added.
Default apply output keeps summaries and real failures; detailed per-process skip/restrict logs require diagnostic flags.

### Cooldown Unit Fix

`LatencyDecisionLayer` cooldown logs now use milliseconds through `duration_cast`.

## Band: Input, Network, And State Lifetime

### State Lifetime And ICMP Rate Limiting

`ThreadTracker` resets accumulated per-window state before every multi-sampling window.
This keeps per-watchdog idle gaps out of sampled CPU metrics.

`LatencyMetricsCollector` ICMP failure logs became state-transition based to avoid polling spam during network loss.

### Input And Timer Safety

Input latency work separated Raw Input detection from pinning eligibility.
Input thread pinning stays disabled unless Raw Input plus High confidence `ConcreteTid` evidence exists.

## Band: Operational Gate And Documentation

### Operational Gate

Added:
- bounded smoke runs through `--max-runtime-seconds`,
- release regression matrix,
- operational safety runbook,
- smoke batch runner for dry-run, soft-apply, apply, and timeout paths.

### RC Stabilization Documentation

Documentation evolved toward:
- ADR-first development,
- evidence-driven release decisions,
- limited explicit apply mode,
- non-invasive fallback-first operation,
- processor-group limitations as WARN plus monitoring-only evidence.

Historical one-off patch summary files were removed after this consolidation.
