GameOptimizer v3.0 Runtime Build

Toolchain:
MSVC v143 / Visual Studio 2022 compatible project settings, built with C++20+ /std:c++latest. Modern standard library use is allowed only when supported by the active MSVC toolchain; unsupported runtime/OS paths must degrade to WARN/fallback behavior.

Build:
cl /std:c++latest /O2 /W4 /WX /permissive- main.cpp ProcessFinder.cpp ThreadTracker.cpp TopologyAnalyzer.cpp RollbackManager.cpp SchedulerController.cpp TrackingWatchdog.cpp PolicyDispatcher.cpp LatencyDecisionLayer.cpp LatencyMetricsCollector.cpp NetworkInterruptController.cpp TimerResolutionController.cpp InputLatencyController.cpp AppliedPolicyTracker.cpp BackgroundController.cpp RuntimeValidationMonitor.cpp ApplyGuard.cpp

Run:
GameOptimizer.exe <process-name.exe> [--dry-run|--apply] [--background-filter <path>] [--latency-ping <ipv4-or-host>] [--background-detail-log] [--thread-detail-log] [--thread-log-interval <cycles>] [--max-runtime-seconds <seconds>]

Current scope:
1. ThreadTracker performs SRS-style multi-sampling, EMA scoring, stickiness, and migration feedback.
2. TopologyAnalyzer builds a validated group-aware main-thread policy mask from Win32 topology data.
3. SchedulerController applies or validates thread group affinity and priority through ApplyGuard.
4. BackgroundController restricts explicitly configured background targets and saves process rollback state first.
5. RollbackManager restores thread and process scheduling state with identity checks for stale targets.
6. LatencyDecisionLayer emits bounded PolicyCommand decisions with hysteresis and cooldowns.
7. NetworkInterruptController samples DPC rate through PDH and keeps unsupported interrupt-affinity environments in monitoring-only mode.
8. PolicyDispatcher routes IRQ_REPIN to NetworkInterruptController; unsupported interrupt-affinity control is a non-fatal WARN path.
9. TimerResolutionController requests NtSetTimerResolution only in apply mode and releases it during shutdown rollback.
10. InputLatencyController queries same-process Raw Input registrations when possible, records remote-process detection as unsupported WARN/fallback, separates Raw Input detection from pinning eligibility, and keeps input thread pinning disabled unless Raw Input plus a high-confidence concrete input TID are detected.
11. RuntimeValidationMonitor records runtime health samples and shutdown summaries.
12. TrackingWatchdog and LatencyMetricsCollector use std::jthread/std::stop_token plus interruptible condition-variable waits instead of fixed sleep polling.
13. ThreadTracker multi-sampling accepts stop_token cancellation between samples.
14. TopologyAnalyzer parses Win32 topology buffers through span-bounded record walking.
15. Logger formatting templates are concept-constrained before std::vformat dispatch.
16. TopologyAnalyzer exposes a deterministic process-affinity fallback helper with group-preserving mask provenance so HEDT/processor-group behavior can be regression-tested without 64+ core hardware.
17. BackgroundController blocks process-wide restriction for processor group 1+, records the blocked group in the non-fatal summary, and leaves thread-level group affinity support in SchedulerController.
18. Group 1+ process-wide background restriction is an explicit safety limitation, not an implementation defect; per-thread group-aware background restriction is a LOW-priority Future Phase item.
19. Development priority is tracked in DevelopmentRoadmap.md: RC stabilization first, release hardening second, future HEDT/hybrid/NUMA architecture third.
18. Long soak RC gate requires both a 30-minute dry-run and a 60-minute soft-apply run, with hang detection, runtime timeline monotonicity checks, runtime validation summary, and runtime validation failure exit-code gating.
19. Release gate smoke and soak runs create run-id scoped RC evidence reports with logs, exit codes, schema version, git commit, build hash, GameOptimizer.exe SHA-256, and BLOCKER/WARN/INFO severity summaries.
20. Anti-cheat/access-denied Win32 boundaries are classified as recoverable limitations and fall back to monitoring-only behavior when no safe mutation can be verified.

Validation:
1. Run run_rc_gate.bat <target.exe> from an x64 Native Tools Command Prompt for the full RC gate.
2. Run run_regression_tests.bat for regression-only validation.
3. Run run_release_gate_static_checks.py before merging.
4. Run run_release_gate_smoke.bat <target.exe> for smoke validation.
5. Run run_long_soak_presets.bat <target.exe> [30m|60m|both] for extended soak validation. The default RC path is both.
6. Run release_gate_evidence.py verify-rc --target <target.exe> to verify that current-commit smoke and soak evidence reports both exist and passed. Schema, git commit, or exe SHA-256 mismatch is a BLOCKER. WARN is allowed but reported, and INFO is tracking data.
7. Inspect release_gate_logs\<run-id>\rc_evidence_report.txt or .json for the final release decision evidence.

Important:
Apply mode can modify target thread scheduling and configured background process scheduling until shutdown rollback. Use --dry-run first, then soft-apply, then --apply only with a reviewed background filter.
