# GameOptimizer Development Roadmap

## Direction

GameOptimizer is no longer focused on adding more features first. The current direction is release-grade stabilization: structure, rollback trust, evidence integrity, and operational safety.

## Phase 1 - Release Candidate Stabilization

Goal: reach an RC that can be applied safely.

| Work item | Priority | Status |
| --- | --- | --- |
| Split `main.cpp` | HIGH | Done |
| Structure startup/shutdown pipeline | HIGH | In progress |
| Introduce `RuntimeContext` ownership | HIGH | Done |
| Strengthen RC evidence automation | HIGH | In progress |
| Timestamp and sequence logging | MEDIUM | Done |
| Normalize process-name comparison | MEDIUM | Done |

Exit criteria:

1. Apply mode has clean shutdown and rollback evidence.
2. Runtime validation failure is recorded as BLOCKER and exits non-zero.
3. RC evidence includes binary fingerprint, shutdown classification, rollback preserved-state summary, and processor-group mode summary.
4. Release gate static checks and regression pass on the current commit.

## Phase 2 - Release Hardening

Goal: reach the level of an always-on system utility.

| Work item | Priority |
| --- | --- |
| Strengthen watchdog telemetry | HIGH |
| Runtime anomaly classification | HIGH |
| Crash-safe evidence flush | HIGH |
| Soak test automation | HIGH |
| Policy replay/debug mode | MEDIUM |

Exit criteria:

1. Watchdog cycles can be correlated by timestamp, sequence, cycle id, and runtime state.
2. Runtime anomalies are classified as BLOCKER, WARN, or INFO.
3. Evidence can still be recovered after abnormal termination where possible.
4. Soak presets produce complete evidence without manual report assembly.

## Phase 3 - Future Architecture

Goal: extend safely toward HEDT, hybrid CPU, and NUMA-aware operation.

| Work item | Priority |
| --- | --- |
| Per-thread background restriction | LOW |
| Dynamic topology scoring | LOW |
| P-core / E-core awareness | LOW |
| NUMA-aware policy | LOW |

Exit criteria:

1. Group 1+ process-wide background restriction remains blocked until a safe backend exists.
2. Future HEDT work is per-thread or group-aware by construction.
3. Do not apply priority-class-only background restriction for group 1+ until process affinity rollback state and priority rollback state are split.
4. Topology policy changes remain replayable and auditable through evidence.

## Target Architecture

```text
GameOptimizer
 ├─ Runtime Core
 │   ├─ Watchdog
 │   ├─ Policy Dispatcher
 │   ├─ Runtime Validation
 │   └─ Latency Decision Layer
 │
 ├─ Scheduling Layer
 │   ├─ ThreadTracker
 │   ├─ SchedulerController
 │   ├─ ApplyGuard
 │   └─ RollbackManager
 │
 ├─ Hardware Layer
 │   ├─ TopologyAnalyzer
 │   ├─ ProcessorGroupManager
 │   └─ IRQ / Timer Layer
 │
 ├─ Evidence Layer
 │   ├─ RuntimeSnapshotRecorder
 │   ├─ RC Evidence Report
 │   ├─ Timeline Recorder
 │   └─ Validator
 │
 └─ Application Layer
     ├─ CLI
     ├─ StartupPipeline
     ├─ ShutdownPipeline
     └─ RuntimeOrchestrator
```

## Release Philosophy

The project has passed the simple feature-prototype stage. The next correct work is not broad feature expansion. The priority is:

1. Stabilize structure.
2. Preserve rollback evidence.
3. Make failures diagnosable.
4. Harden RC evidence.
5. Only then expand hardware policy.
