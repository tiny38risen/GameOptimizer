# PATCH SUMMARY - RG-6 Function-Scope Sequence Gate

## Changes

- Replaced global first-occurrence ApplyGuard ordering checks with function-body scoped ordering checks.
- Added lightweight function body extraction for:
  - `SchedulerController::applyMainThreadPolicy`
  - `BackgroundController::applyRestriction`
- Kept RG-6 as a conservative static heuristic, not a control-flow proof.
- Updated ReleaseGateRunbook, ReleaseRegressionMatrix, and ApplyGuardRegressionMatrix to avoid overstating the gate.

## Verification

- `python run_release_gate_static_checks.py`
- Result: PASS
