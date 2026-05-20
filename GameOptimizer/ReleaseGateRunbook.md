# GameOptimizer Release Gate Runbook

## Goal
Verify that the default Release path avoids display-only diagnostic work while preserving opt-in diagnostics.

## RC candidate command

The full RC entry command is:

```bat
run_rc_gate.bat <target.exe>
```

After the gate passes, save the final regression output and run:

```bat
verify_rc_candidate.py --target <target.exe> --regression-log <log>
```

Do not create the `v3.0-rc1` tag until this candidate check passes.

## RC candidate required artifacts

- Runbook: this file and the operational safety runbook must describe the release flow.
- Blocker list: `ReleaseRegressionMatrix.md` must list release blockers.
- evidence bundle: current-commit smoke and soak reports must exist under `release_gate_logs`.
- Final regression result: a saved regression log must contain `failed=0` and `[PASS] all regression tests passed`.

Any missing item is a `BLOCKER`.

## Default compact run
```bat
GameOptimizer.exe target.exe --apply --background-filter background_filter_example.txt --latency-ping 8.8.8.8
```
Expected:
- Startup prints `thread detail log disabled by default`.
- Watchdog does not print `observed top thread count`.
- Main thread candidate and confirmed main state are still printed.
- BG_RESTRICT_UP summary and rollback logs still appear.

## Diagnostic run
```bat
GameOptimizer.exe target.exe --apply --background-filter background_filter_example.txt --latency-ping 8.8.8.8 --thread-detail-log --thread-log-interval 2
```
Expected:
- Startup prints `thread detail log enabled: interval=2 watchdog cycle(s)`.
- Top thread rows appear once per 2 watchdog cycles.
- `observed top thread count` remains <= 8.

## Failure checks
- `--thread-log-interval 0` must not crash and must fall back to 1.
- Invalid interval text must not throw and must fall back to 1.
- Shutdown rollback must still return exit code 0 unless a live same-identity rollback failure occurs.


## Automated smoke gate

Run from the directory containing `GameOptimizer.exe` and the repository scripts:

```bat
run_release_gate_smoke.bat target.exe
```

The script captures logs under `release_gate_logs/` and validates them with:

- `run_release_gate_static_checks.py`
- `run_release_gate_log_assertions.py`

Do not approve a release if either script fails, even when the executable itself exits with code 0.

## v3.0-rc1 tag preparation

Before tagging:

1. `run_rc_gate.bat <target.exe>` passes.
2. `verify_rc_candidate.py --target <target.exe> --regression-log <log>` passes.
3. Smoke and soak `rc_evidence_report.json` files match the current git commit.
4. `GameOptimizer.exe` SHA-256 matches the evidence report.
5. The final regression log reports `failed=0`.


## ApplyGuard sequence gate

Run `python run_release_gate_static_checks.py` before every merge. The gate verifies a minimum function-scope lexical ordering heuristic for the concrete SchedulerController and BackgroundController ApplyGuard transaction paths, including save-before-arm and commit-after-apply marker ordering inside the target function body. Treat the gate as mandatory, but do not treat it as a control-flow proof; branch-level rollbackNow/discardSavedState paths still require manual review.
