> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# GameOptimizer Release Performance Checklist

## Build
- Configuration: x64 Release
- C++ standard: C++23 or latest
- Warnings: /W4 /WX /permissive-

## Runtime measurement
- Use dry-run or soft-apply for baseline measurement unless a limited apply validation has already satisfied the ADR apply-mode policy.
- Run target game/process for at least 10 minutes.
- Measure GameOptimizer CPU usage while idle and while BG_RESTRICT_UP triggers.
- Target: idle <= 0.1%, active <= 1.0%.

## Log checks
- Default mode should not print per-process protected skip spam.
- `--background-detail-log` should restore per-process diagnostics.
- ICMP transient misses should be rate-limited.
- Policy feedback rearm should be cooldown-limited.

## Rollback checks
- Main thread affinity/priority audit passes.
- Background process rollback distinguishes stale identity from alive same-identity failure.
- Exit code is 0 when all non-stale rollback targets are restored.

## Release gate logging mode
- Default run must not call the display-only top-thread logging path.
- Use `--thread-detail-log` only during diagnostics.
- Use `--thread-log-interval <cycles>` to reduce diagnostic log frequency.

Recommended limited apply validation command:
`GameOptimizer.exe target.exe --apply --background-filter background_filter_example.txt --latency-ping 8.8.8.8`

Recommended diagnostic command:
`GameOptimizer.exe target.exe --max-runtime-seconds 120 --thread-detail-log --thread-log-interval 4`
