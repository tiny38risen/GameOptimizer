# Real Game Validation Runbook

## Purpose

This runbook records the manual validation required before proposing `v3.0-rc1`. It does not replace `run_rc_gate.bat`; it proves that the release candidate behaves safely against real game processes and anti-cheat/access-boundary conditions.

## Required targets

Validate exactly three representative game processes before proposing `v3.0-rc1`:

- Game A: anti-cheat 없음 / legacy or single-core 성향
- Game B: 네트워크 민감 / RTT, DPC telemetry 확인
- Game C: Access Denied 또는 anti-cheat 가능성 / fallback 확인

## Required order per target

1. Dry-run 30m:

```bat
GameOptimizer.exe <target.exe> --dry-run --max-runtime-seconds 1800 --thread-detail-log --thread-log-interval 20
```

2. Soft-apply 60m:

```bat
GameOptimizer.exe <target.exe> --max-runtime-seconds 3600 --thread-detail-log --thread-log-interval 40
```

3. Apply is allowed for only one game after dry-run and soft-apply have no BLOCKER findings and the background filter is explicit:

```bat
GameOptimizer.exe <target.exe> --apply --background-filter background_filter_example.txt --max-runtime-seconds 300
```

Apply mode must be explicitly requested with `--apply`. The default mode is `soft-apply`; `--dry-run` is the non-mutating planning mode.

## Required JSON record

Copy `docs/release/Game_Verification_Matrix.example.json` to:

```text
docs/release/Game_Verification_Matrix.json
```

Then replace every example value with real observations. Synthetic or placeholder values must not be used for RC approval.

Required fields per game:

- `process_name`
- `admin`
- `scheduler_mode`
- `access_denied`
- `fallback`
- `thread_tracker_confidence`
- `raw_input_tid_confidence`
- `migration_count`
- `dpc_spike_count`
- `rtt_jitter_summary`
- `raw_input_detection`
- `irq_support`
- `background_mode`
- `shutdown_reason`
- `rollback_result`
- `rollback_state_saved`
- `apply_guard_audit`
- `group_aware_thread_path_verified`
- `anti_cheat_suspected`
- `requires_group1_process_wide_background_restriction`
- `soft_apply_warn_count`
- `rollback_preserved_state_count`
- `blocker_count`
- `warn_count`
- `info_count`
- `runs`

## Validation table

Create one row per target and mode.

| Role | Target | Mode | Duration | Exit code | Runtime validation | Rollback evidence | Access Denied / anti-cheat fallback | DPC / IRQ evidence | Raw Input / TID confidence | Processor Group evidence | Decision |
|---|---|---|---:|---:|---|---|---|---|---|---|---|
| Game A | `<target.exe>` | dry-run | 30m |  | PASS / FAILED | none expected | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |
| Game A | `<target.exe>` | soft-apply | 60m |  | PASS / FAILED | baseline validated | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |
| Game B | `<target.exe>` | dry-run | 30m |  | PASS / FAILED | none expected | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |
| Game B | `<target.exe>` | soft-apply | 60m |  | PASS / FAILED | baseline validated | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |
| Game C | `<target.exe>` | dry-run | 30m |  | PASS / FAILED | none expected | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |
| Game C | `<target.exe>` | soft-apply | 60m |  | PASS / FAILED | baseline validated | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |
| One game only | `<target.exe>` | apply | limited |  | PASS / FAILED | rollback audit passed | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | WARN / INFO / BLOCKER | PASS / BLOCKER |

## Severity rules

- BLOCKER: runtime validation FAILED, rollback failure, missing rollback/fallback evidence, hang detection failure, timeline monotonicity failure, evidence mismatch, concrete TID confidence violation, or unsupported IRQ path treated as ERROR/FAIL.
- WARN: Access Denied with fallback evidence, IRQ unsupported monitoring-only, Raw Input unavailable fallback, Processor Group 1+ process-wide background monitoring-only.
- INFO: dry-run decisions, telemetry, unsupported feature observations with no attempted mutation.

## Apply safety rule

Apply mode must stay disabled for a target until:

1. dry-run and soft-apply both exit successfully,
2. runtime validation has no FAILED result,
3. shutdown rollback evidence is clean,
4. Access Denied is not observed,
5. anti-cheat is not suspected,
6. rollback state save succeeds,
7. ThreadTracker confidence is sufficient,
8. ApplyGuard audit is `PASS`,
9. group-aware thread path is verified,
10. Raw Input TID confidence is not Low,
11. Processor Group 1+ process-wide background restriction is not required,
12. soft-apply WARN count is not high.

Apply mode is forbidden when:

- anti-cheat is suspected,
- Access Denied repeats,
- rollback state save fails,
- Raw Input TID confidence is Low,
- group 1+ process-wide background restriction is needed,
- soft-apply records many WARN findings.

## Distribution PASS criteria

- all 3 games have `BLOCKER=0`
- all 3 games have no abnormal exit
- all 3 games have `rollback_preserved_state_count=0`
- at least 2 games complete 60m soft-apply
- exactly 1 or more limited apply-mode validation exists, and only after clean dry-run/soft-apply
- at least 1 game records policy decision telemetry

Verify the matrix with:

```bat
python verify_real_game_validation.py --matrix docs\release\Game_Verification_Matrix.json
```
