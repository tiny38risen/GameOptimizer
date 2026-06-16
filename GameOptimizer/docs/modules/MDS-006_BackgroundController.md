# MDS-006 BackgroundController

## 1. 문서 개요

문서명: MDS-006 BackgroundController

버전: v1.0

대상 모듈: BackgroundController

작성 목적: 게임 실행 코어 주변의 background process interference를 줄이기 위해 제한적인 background restriction을 수행하는 Control Layer 모듈의 책임과 안전 경계를 정의한다.

적용 범위: background process 후보 식별, protected process 제외, anti-cheat 의심 process 제외, background restriction apply, verify, rollback, fallback evidence

비적용 범위: ThreadTracker observation, main thread confidence 계산, thread-level scheduling mutation ownership, processor topology source ownership

상위 문서:

- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/performance/PolicySpecification.md`
- `docs/contracts/Safety_Contract.md`

관련 모듈:

- PolicyDispatcher
- TopologyAnalyzer
- RollbackManager
- ApplyGuard
- SchedulerController
- PerformanceEvidencePlanner

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

BackgroundController의 목적은 게임 실행 코어 주변의 background process interference를 줄이기 위해 제한적인 background restriction을 수행하는 것이다.

Background restriction은 공격적 최적화가 아니라 제한적 간섭 감소 정책이다.

## 3. 책임 범위

이 모듈의 책임:

1. background process 후보 식별
2. protected process 제외
3. anti-cheat 의심 process 제외
4. identity revalidation
5. background restriction apply
6. restriction verify
7. restriction rollback
8. fallback reason 기록

이 모듈의 비책임:

1. thread-level scheduling mutation
2. main thread confidence 계산
3. topology source 생성
4. PolicyCandidate 생성
5. release final classification

## 4. 입력

Input: CoreIsolationPlan

- Source: CoreIsolationPlanner / PolicyCandidate
- Required: Yes
- Validation: processorGroup, excludedCoreMask, isolationStrength 필요
- Missing behavior: no restriction

Input: background process candidates

- Source: configuration / RuntimeContext / policy input
- Required: Yes for restriction
- Validation: identity 확인 및 protected/anti-cheat 제외 필요
- Missing behavior: monitoring-only

Input: TopologyResult

- Source: TopologyAnalyzer
- Required: Yes
- Validation: Processor Group 1+ process-wide safety 확인
- Missing behavior: no process-wide restriction

Input: RollbackManager

- Source: RuntimeContext
- Required: Yes
- Validation: process original state save 가능해야 함
- Missing behavior: no mutation

## 5. 출력

Output: restricted process list

- Consumer: Evidence Layer, RuntimeValidationMonitor
- Meaning: restriction 적용 대상
- 신뢰도: identity validation에 의존
- Failure behavior: empty list

Output: skipped process list

- Consumer: Evidence Layer
- Meaning: protected/anti-cheat/identity failure 등으로 제외된 대상
- 신뢰도: skip reason completeness에 의존
- Failure behavior: unknown skipped reason

Output: restriction result

- Consumer: PolicyDispatcher, Evidence Layer
- Meaning: apply/verify result
- 신뢰도: post-apply query에 의존
- Failure behavior: rollback request

Output: fallback reason

- Consumer: RuntimeStateMachine, Evidence Layer
- Meaning: monitoring-only 또는 no-op 사유
- 신뢰도: safety check completeness
- Failure behavior: policy rejected

Output: rollback result

- Consumer: RollbackManager, Evidence Layer
- Meaning: process state restore 결과
- 신뢰도: identity revalidation에 의존
- Failure behavior: RollbackFailedPreserved

## 6. 내부 상태

State: candidate background process set

- 목적: restriction 후보 목록 관리
- 수명: policy cycle
- 초기화 시점: policy handoff
- 갱신 시점: identity/protection filtering
- 보존 여부: evidence summary

State: active restriction state

- 목적: applied restriction tracking
- 수명: apply 이후 rollback/commit/shutdown까지
- 초기화 시점: successful apply
- 갱신 시점: verify/rollback
- 보존 여부: evidence

State: skipped process reason map

- 목적: 안전 제외 사유 기록
- 수명: policy cycle
- 초기화 시점: filtering
- 갱신 시점: access/protection check
- 보존 여부: evidence

## 7. 소유권 규칙

BackgroundController는 process-level background restriction의 소유자다.

BackgroundController는 thread-level scheduling mutation을 소유하지 않는다.

BackgroundController는 protected process와 anti-cheat 의심 process를 restriction 대상에서 제외한다.

Processor Group 1+ process-wide restriction 안전 경로가 없으면 mutation하지 않는다.

## 8. 허용 동작

- background process candidate filtering
- protected process check
- anti-cheat boundary safety check
- process original state save 요청
- ApplyGuard guarded process restriction
- restriction verify
- restriction rollback 요청
- fallback evidence 기록

## 9. 금지 동작

- protected process mutation 금지
- anti-cheat 의심 process mutation 금지
- identity 불명확 process mutation 금지
- Processor Group 1+ unsafe process-wide restriction 금지
- rollback state 없는 restriction 금지
- ApplyGuard arm 전 mutation 금지
- ThreadTracker 책임 침범 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Evaluating | No | 직접 판단 없음 |
| PolicyPending | Limited | policy validation input |
| Applying | Yes | guarded restriction apply |
| Verifying | Yes | restriction verify |
| Running | Limited | active restriction status |
| MonitoringOnly | No | mutation forbidden |
| RollbackPending | Yes | restriction rollback |
| RollbackFailedPreserved | Limited | failed state context |
| ShutdownRequested | No | new restriction forbidden |
| ShuttingDown | Limited | final rollback retry |

## 11. 실패 처리

Failure: protected process candidate

- Severity: WARN/INFO
- Fallback: skip process
- Evidence: skippedProcessList, skipReason
- Next State: Applying if remaining targets, otherwise MonitoringOnly

Failure: Access Denied

- Severity: WARN with fallback evidence
- Fallback: skip or monitoring-only
- Evidence: accessDeniedCount, fallbackReason
- Next State: MonitoringOnly 또는 PolicyRejected

Failure: Processor Group 1+ unsupported path

- Severity: WARN
- Fallback: process-wide restriction blocked
- Evidence: processorGroupMode, backgroundRestrictionMode
- Next State: MonitoringOnly

Failure: verify mismatch

- Severity: BLOCKER candidate
- Fallback: rollback
- Evidence: expected/actual restriction state
- Next State: RollbackPending

## 12. Evidence 요구사항

- restricted process list
- skipped process list
- skip reason
- restriction level
- processor group mode
- background restriction mode
- rollbackStateSaved
- apply result
- verify result
- rollback result
- fallback reason

Evidence 없는 restriction success 판정은 금지한다.

## 13. 테스트 기준

Unit Test:

- protected process filtering
- anti-cheat boundary filtering
- Processor Group 1+ unsupported path block

Integration Test:

- PolicyDispatcher handoff와 RollbackManager/ApplyGuard transaction 연동

Regression Test:

- process mutation API가 BackgroundController/RollbackManager 외부로 이동하지 않았는지 static gate 검증

Failure Test:

- Access Denied
- identity mismatch
- verify mismatch
- rollback failure

Evidence Test:

- skipped process, fallback reason, rollback result evidence completeness

## 14. Non-Goals

- thread-level scheduling mutation
- background process auto-discovery without explicit safety filter
- protected process bypass
- Anti-Cheat 우회
- 구체 C++ 함수 시그니처 확정

## 15. Acceptance Criteria

1. BackgroundController의 restriction ownership이 명확하다.
2. protected/anti-cheat process 제외 원칙이 포함되어 있다.
3. Processor Group 1+ unsafe process-wide restriction 금지가 명시되어 있다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. 후속 구현 작업에서 background restriction 경계를 판단할 수 있다.

## 16. Open Questions

1. Background Restriction 단계는 몇 단계로 나눌 것인가?
2. explicit allow/restrict configuration의 format은 어디서 확정할 것인가?
3. priority-only process rollback은 v3.1 범위에 포함할 것인가?
4. per-thread background restriction은 future phase로 유지할 것인가?
