GameOptimizer Advanced Dry-Run Build

Build:
cl /std:c++latest /O2 /W4 /WX /permissive- main.cpp ProcessFinder.cpp ThreadTracker.cpp TopologyAnalyzer.cpp RollbackManager.cpp SchedulerController.cpp TrackingWatchdog.cpp

Run:
GameOptimizer.exe <process-name.exe>

Current scope:
1. ThreadTracker now uses SRS-style multi-sampling: 3 samples with a 50 ms interval, then averages the delta before EMA update.
2. TopologyAnalyzer prepares a validated main-thread mask from Win32 topology data:
   - process affinity is queried first,
   - SMT sibling pressure is reduced by selecting one representative logical processor per physical core,
   - L3 cache locality is preferred when available,
   - the final mask is validated against the target process affinity mask.
3. TrackingWatchdog runs the tracking loop on a joinable worker thread.
4. Ctrl+C requests watchdog shutdown, joins the worker thread, and then calls rollbackAll().
5. SchedulerMode remains DryRun by default.
6. DryRun does not call OpenThread, GetThreadPriority, SetThreadAffinityMask, or SetThreadPriority from SchedulerController/RollbackManager.

Important:
Real Apply mode is still intentionally disabled by default. Enable it only after validating the topology mask and rollback behavior on a safe test process.
