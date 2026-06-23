# GameOptimizer Validation Runbook

## 1. 목적

이 문서는 개인 프로젝트 기준의 빌드, 테스트, 검증, 릴리즈 체크를 한 화면에 모은다.
상세 Evidence schema와 RC 절차는 Reference 문서로 보존하되, 실제 확인 순서는 이 문서를 우선한다.

## 2. Build Check

| 확인 | PASS 기준 |
|---|---|
| Configuration | Release x64 build 성공 |
| Toolchain | MSVC v143 / Visual Studio 2022 |
| Binary identity | binary path와 SHA-256 기록 |
| Commit identity | git commit 기록 |

Release build 실패, binary hash 누락, commit 누락은 BLOCKER다.

## 3. Static Check

| 확인 | PASS 기준 |
|---|---|
| ThreadTracker | mutation API 없음 |
| SchedulerController | affinity / priority mutation owner 유지 |
| ApplyGuard | Save → Arm → Apply → Verify 흐름 유지 |
| RollbackManager | failed state preservation 유지 |
| Processor Group | group-aware 경로에서 group 0 hardcoding 없음 |
| `std::expected` | 사용 전 Assign → Check → Bind 흐름 |

static safety check 실패는 runtime validation 전 BLOCKER다.

## 4. Regression Test

- safety regression test 통과
- rollback 관련 test 통과
- evidence validator / selftest 통과
- release gate marker 순서 검증
- `git diff --check` 통과

테스트 실패는 원인 분류 후 PASS/WARN/BLOCKER로 기록한다.

## 5. Runtime Validation

| 확인 | 기준 |
|---|---|
| Dry-run | mutation 없이 observation과 evidence 생성 |
| Soft-apply | rollback state 저장, ApplyGuard arm, verify 기록 |
| Apply | 명시적 조건에서만 제한 수행 |
| RuntimeValidation | BLOCKER 없음 |
| Shutdown | 신규 mutation 중지, final evidence flush |

RuntimeValidation BLOCKER가 있으면 RC PASS 불가다.

## 6. Rollback Validation

| 확인 | BLOCKER 조건 |
|---|---|
| rollback state 저장 | 저장 없이 mutation |
| identity 재검증 | mismatch를 success 처리 |
| thread/process 원복 | living same identity rollback 실패 |
| failed state preservation | 실패 상태 폐기 |
| shutdown rollback | preserved state 은폐 |

Rollback 실패는 WARN으로 낮추지 않는다.

## 7. Evidence Checklist

필수 Evidence는 반복 필드가 아니라 체크리스트로 관리한다.

- build configuration
- binary / commit identity
- target process identity
- runtime validation summary
- rollback summary
- blocker / warn / info count
- processor group summary
- ThreadTracker telemetry
- final RC decision reason

필수 Evidence 누락, false PASS, hash mismatch는 BLOCKER다.

## 8. RC Decision

| 결과 | 조건 |
|---|---|
| PASS | BLOCKER 0, rollback 성공, required Evidence 존재 |
| PASS_WITH_WARNINGS | BLOCKER 0, WARN 사유 명확 |
| NEUTRAL | 안전성 통과, 성능 개선 주장 금지 |
| REGRESSION | 성능 또는 안정성 악화 |
| BLOCKER | unsafe mutation, rollback 실패, Evidence fatal missing |
| INVALID_RUN | 환경 구성 실패 또는 target identity 불명확 |

## 9. Known Limitations

- 성능 향상은 보장하지 않는다.
- Access Denied와 Anti-Cheat 경계는 public user-mode fallback으로 처리한다.
- group 1+ process-wide background restriction은 제한될 수 있다.
- 실제 게임 검증은 지원 선언이 아니라 후보 검증이다.
- Proposal 문서는 current implementation 근거가 아니다.
