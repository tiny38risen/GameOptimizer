# GameOptimizer v3.0-rc1 Known Limitations

## Processor Group / HEDT

- Thread-level group affinity is supported.
- Process-wide background restriction is limited to group 0.
- Group 1+ process-wide background affinity is `monitoring-only`.
- Group 1+ process-wide background affinity mutation is intentionally excluded from `v3.0-rc1`.
- Group 1+ background restriction must be WARN + monitoring-only, not ERROR/FAIL.

## Input Latency

- Input thread pinning is disabled unless Raw Input detection and High confidence `ConcreteTid` evidence both exist.
- Raw Input not detected means input thread pinning is forbidden.
- Remote Raw Input usage is not considered confirmable through the current public Win32 path.
- Low-confidence input observations are monitoring-only.

## Network / IRQ

- IRQ_REPIN mutation is not forced when interrupt affinity is unsupported.
- IRQ repin is available only when the NIC/driver/runtime path supports interrupt affinity control.
- Unsupported IRQ paths are WARN + monitoring-only.
- DPC spike observation does not imply IRQ mutation support.

## Anti-cheat and Access Denied

- Win32 API access may be denied by anti-cheat or protected processes.
- Anti-cheat Access Denied is a normal fallback path when fallback or rollback evidence exists.
- Access Denied is not a release blocker when fallback or rollback evidence exists.
- Apply mode must not be forced in anti-cheat environments without explicit fallback evidence.

## Non-invasive Boundary

- GameOptimizer does not modify game memory.
- GameOptimizer does not use DLL injection.
- GameOptimizer does not use kernel patching.
- GameOptimizer does not use driver patching.
- GameOptimizer does not patch anti-cheat, drivers, the kernel, or game binaries.
- `v3.0-rc1` is limited to public user-mode Windows API observation and bounded scheduling/timer requests.

## Apply Mode

- Apply mode remains conservative.
- Default execution is soft-apply unless `--dry-run` or explicit `--apply` is passed.
- Broad Apply mode requires an explicit background deny/restrict configuration.
- Apply mode should be used only after clean dry-run and soft-apply evidence for the same target.
- Apply mode requires rollback state save success, sufficient ThreadTracker confidence, ApplyGuard audit PASS, and verified group-aware thread path.
- Apply mode is blocked by anti-cheat suspicion, repeated Access Denied, rollback state save failure, Low Raw Input TID confidence, group 1+ process-wide background restriction need, or many soft-apply WARN findings.
- Apply mode remains in limited validation status for `v3.0-rc1`.
- Aggressive self-tuning is outside `v3.0-rc1`.
