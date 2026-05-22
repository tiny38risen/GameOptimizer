# Real Game Validation Runbook

## Purpose

This runbook records the manual validation required before proposing `v3.0-rc1`. It does not replace `run_rc_gate.bat`; it proves that the release candidate behaves safely against real game processes and anti-cheat/access-boundary conditions.

## Required targets

Validate 2-3 representative game processes. Prefer one low-risk local game, one launcher-backed online game, and one anti-cheat-protected title when available.

## Required order per target

1. Dry-run:

```bat
GameOptimizer.exe <target.exe> --dry-run --max-runtime-seconds 300 --thread-detail-log --thread-log-interval 20
```

2. Soft-apply:

```bat
GameOptimizer.exe <target.exe> --max-runtime-seconds 300 --thread-detail-log --thread-log-interval 20
```

3. Apply is allowed only after dry-run and soft-apply have no BLOCKER findings and the background filter is explicit:

```bat
GameOptimizer.exe <target.exe> --apply --background-filter background_filter_example.txt --max-runtime-seconds 300
```

## Validation table

Create one row per target and mode.

| Target | Mode | Exit code | Runtime validation | Rollback evidence | Access Denied / anti-cheat fallback | DPC / IRQ evidence | Raw Input / TID confidence | Processor Group evidence | Decision |
|---|---|---:|---|---|---|---|---|---|---|
| `<target.exe>` | dry-run |  | PASS / FAILED | none expected | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |
| `<target.exe>` | soft-apply |  | PASS / FAILED | baseline validated | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |
| `<target.exe>` | apply |  | PASS / FAILED | rollback audit passed | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |

## Severity rules

- BLOCKER: runtime validation FAILED, rollback failure, missing rollback/fallback evidence, hang detection failure, timeline monotonicity failure, evidence mismatch, concrete TID confidence violation, or unsupported IRQ path treated as ERROR/FAIL.
- WARN: Access Denied with fallback evidence, IRQ unsupported monitoring-only, Raw Input unavailable fallback, Processor Group 1+ process-wide background monitoring-only.
- INFO: dry-run decisions, telemetry, unsupported feature observations with no attempted mutation.

## Apply safety rule

Apply mode must stay disabled for a target until:

1. dry-run and soft-apply both exit successfully,
2. runtime validation has no FAILED result,
3. shutdown rollback evidence is clean,
4. Access Denied and anti-cheat cases have fallback evidence,
5. input pinning is not attempted without High confidence and `ConcreteTid`,
6. Processor Group 1+ process-wide background restriction remains monitoring-only.
