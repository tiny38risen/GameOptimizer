> Status: NeedsReview  
> 이 문서는 권위, 자동 생성 경로, 또는 현재 기준 여부 확인이 필요합니다. 현재 기준은 `docs/DOCUMENT_REGISTER.md`를 따릅니다.

# Game Verification Matrix

## Purpose

This worksheet records real-game validation observations before they are copied into `Game_Verification_Matrix.json`.
It is an evidence collection form, not a substitute for `verify_real_game_validation.py`.

Use one row per game/process and mode. Keep all placeholder cells until a real run has produced evidence.
For RC approval, every recorded row must link to a real evidence artifact and the final JSON matrix must pass validation.

## Required Fields

- `game/process`
- `mode`
- `duration`
- `Access Denied`
- `fallback`
- `Raw Input`
- `IRQ support`
- `DPC/RTT telemetry`
- `shutdown reason`
- `rollback result`
- `BLOCKER/WARN/INFO`

## Matrix

| game/process | mode | duration | evidence artifact | Access Denied | fallback | Raw Input | IRQ support | DPC/RTT telemetry | shutdown reason | rollback result | BLOCKER/WARN/INFO | decision |
|---|---|---:|---|---|---|---|---|---|---|---|---:|---|
| Game A / `<process.exe>` | dry-run | 30m | `<path/to/rc_evidence_report.json>` | none / observed | monitoring-only / rollback evidence / none needed | detected / not detected / low confidence | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / other | no preserved state / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |
| Game A / `<process.exe>` | soft-apply | 60m | `<path/to/rc_evidence_report.json>` | none / observed | baseline only / monitoring-only / rollback evidence | detected / not detected / low confidence | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / other | no preserved state / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |
| Game A / `<process.exe>` | apply-limited | limited | `<path/to/rc_evidence_report.json>` | none required | explicit rollback evidence | high-confidence ConcreteTid / not eligible | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / operator stop / other | rollback audit passed / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |
| Game B / `<process.exe>` | dry-run | 30m | `<path/to/rc_evidence_report.json>` | none / observed | monitoring-only / rollback evidence / none needed | detected / not detected / low confidence | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / other | no preserved state / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |
| Game B / `<process.exe>` | soft-apply | 60m | `<path/to/rc_evidence_report.json>` | none / observed | baseline only / monitoring-only / rollback evidence | detected / not detected / low confidence | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / other | no preserved state / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |
| Game B / `<process.exe>` | apply-limited | limited | `<path/to/rc_evidence_report.json>` | none required | explicit rollback evidence | high-confidence ConcreteTid / not eligible | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / operator stop / other | rollback audit passed / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |
| Game C / `<process.exe>` | dry-run | 30m | `<path/to/rc_evidence_report.json>` | none / observed | monitoring-only / rollback evidence / none needed | detected / not detected / low confidence | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / other | no preserved state / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |
| Game C / `<process.exe>` | soft-apply | 60m | `<path/to/rc_evidence_report.json>` | none / observed | baseline only / monitoring-only / rollback evidence | detected / not detected / low confidence | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / other | no preserved state / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |
| Game C / `<process.exe>` | apply-limited | limited | `<path/to/rc_evidence_report.json>` | none required | explicit rollback evidence | high-confidence ConcreteTid / not eligible | supported / unsupported monitoring-only | DPC=`n`, RTT=`summary` | MaxRuntimeSoftTimeout / operator stop / other | rollback audit passed / failure | B=0 / W=0 / I=0 | PASS / BLOCKER |

## Recording Rules

- Record at least two, preferably three, real game/process targets before RC approval.
- Dry-run and soft-apply rows are required before any `apply-limited` row is accepted.
- `Access Denied` is WARN only when fallback or rollback evidence is present.
- `Raw Input` with low confidence blocks input-thread pinning and blocks apply eligibility.
- Unsupported `IRQ support` is WARN only when the runtime stays monitoring-only.
- `DPC/RTT telemetry` must record observed counts or summaries, not qualitative guesses.
- `rollback result` must explicitly state whether preserved rollback state remained.
- `BLOCKER/WARN/INFO` must use the final evidence counts from the linked artifact.
- Any BLOCKER row prevents RC approval.

## JSON Handoff

After this worksheet is filled with real evidence, copy the observations into:

```text
docs/release/Game_Verification_Matrix.json
```

Then run:

```bat
python verify_real_game_validation.py --matrix docs\release\Game_Verification_Matrix.json
```
