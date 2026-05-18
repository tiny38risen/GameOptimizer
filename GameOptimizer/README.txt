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
10. InputLatencyController records Raw Input detection misses as WARN/fallback and keeps input thread pinning disabled unless Raw Input and a concrete input TID are detected.
11. RuntimeValidationMonitor records runtime health samples and shutdown summaries.
12. TrackingWatchdog and LatencyMetricsCollector use std::jthread/std::stop_token plus interruptible condition-variable waits instead of fixed sleep polling.
13. ThreadTracker multi-sampling accepts stop_token cancellation between samples.
14. TopologyAnalyzer parses Win32 topology buffers through span-bounded record walking.
15. Logger formatting templates are concept-constrained before std::vformat dispatch.

Validation:
1. Run run_regression_tests.bat from an x64 Native Tools Command Prompt for VS.
2. Run run_release_gate_static_checks.py before merging.
3. Run run_release_gate_smoke.bat <target.exe> from the directory containing GameOptimizer.exe for smoke validation.

Important:
Apply mode can modify target thread scheduling and configured background process scheduling until shutdown rollback. Use --dry-run first, then soft-apply, then --apply only with a reviewed background filter.
