# GameOptimizer Source Bands

## Purpose

This file maps source files to functional ownership bands.
Use it with `Architecture_Decision_Record.md` when adding or moving behavior.

## Runtime Orchestration

- `main.cpp`
- `RuntimeOrchestrator.*`
- `RuntimeContext.h`
- `RuntimeSignalState.h`
- `StartupPipeline.*`
- `ShutdownPipeline.*`
- `WatchdogCycleRunner.*`
- `TrackingWatchdog.*`

Owns startup, shutdown, watchdog cycle boundaries, signal state, and rollback-safe stop ordering.

## Scheduling And Rollback

- `ThreadTracker.*`
- `ThreadInfo.h`
- `SchedulerController.*`
- `ApplyGuard.*`
- `RollbackManager.*`
- `PolicyCommand.h`
- `PolicyDispatcher.*`
- `AppliedPolicyTracker.*`

Owns observation, policy dispatch, thread-level scheduling mutation, process-level rollback state, and transaction cleanup.
Mutation must satisfy ADR-001, ADR-002, ADR-003, ADR-005, ADR-007, ADR-008, and ADR-010.

## Background Restriction

- `BackgroundController.*`
- `background_filter_example.txt`
- `BackgroundControllerSafetyTests.cpp`

Owns process-level background restriction, protected-process filtering, deny/restrict configuration, and group 1+ monitoring-only behavior.

## Topology And Processor Groups

- `TopologyAnalyzer.*`
- `TopologyAnalyzerTests.cpp`
- `ProcessorGroupHedtEvidenceTests.cpp`

Owns topology discovery, processor-group provenance, fallback masks, and HEDT evidence.

## Latency, Network, Timer, And Input

- `LatencyDecisionLayer.*`
- `LatencyMetricsCollector.*`
- `NetworkInterruptController.*`
- `TimerResolutionController.*`
- `InputLatencyController.*`
- `IcmpHandle.h`
- `TimerInputControllerTests.cpp`
- `NetworkInterruptControllerTests.cpp`
- `LatencyDecisionLayerTests.cpp`

Owns latency metrics, policy decisions, DPC/IRQ observation, timer resolution requests, Raw Input detection, and input pinning eligibility.

## Validation And Evidence

- `RuntimeValidationMonitor.*`
- `RuntimeValidationMonitorTests.cpp`
- `release_gate_evidence.py`
- `release_gate_evidence_selftest.py`
- `verify_rc_candidate.py`
- `verify_real_game_validation.py`
- `create_rc_evidence_bundle.py`
- `run_release_gate_static_checks.py`
- `run_release_gate_log_assertions.py`
- `run_release_gate_smoke.bat`
- `run_long_soak_presets.bat`
- `run_soak_with_hang_detection.py`
- `run_rc_gate.bat`
- `run_regression_tests.bat`
- `build_decision_layer_tests.bat`

Owns RC evidence, static gates, log assertions, soak automation, and release candidate verification.

## Shared Utilities

- `Logger.h`
- `LogString.h`
- `ErrorCode.h`
- `WinApiError.h`
- `WinHandle.h`
- `ProcessFinder.*`
- `CliOptions.*`

Owns shared logging, error mapping, handles, process lookup, and CLI parsing.

## Documentation Bands

- `docs/architecture`: accepted contracts and source banding.
- `docs/design`: subsystem design and regression matrices.
- `docs/operations`: runbooks, roadmap, release matrix, and performance checklist.
- `docs/release`: machine-checked release contracts and schemas.
- `docs/history`: historical patch and build context.
