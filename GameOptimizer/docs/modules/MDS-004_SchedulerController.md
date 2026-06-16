# MDS-004 SchedulerController

## 1. 문서 개요

문서명: MDS-004 SchedulerController

버전: v1.0

대상 모듈: SchedulerController

작성 목적: 검증된 정책에 따라 thread-level priority/affinity mutation과 scheduler-owned process priority mutation 후보를 수행하는 Control Layer 모듈의 책임, 안전 조건, rollback/evidence 요구사항을 정의한다.

적용 범위: thread priority apply, thread group affinity apply, scheduler-owned process priority apply 후보, apply 전 state query, RollbackManager state save 요청, ApplyGuard 기반 guarded mutation, verify, rollback 요청

비적용 범위: ThreadTracker observation, PerformanceEngine 판단, background process restriction ownership, topology analysis ownership, protected/background process restriction

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
- BackgroundController
- RuntimeValidationMonitor

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

SchedulerController는 검증된 thread scheduling policy에 따라 thread priority와 thread group affinity를 적용하는 Control Layer 모듈이다.

SchedulerController는 scheduling mutation의 소유자지만, ApplyGuard와 RollbackManager 계약 없이는 mutation할 수 없다.

## 3. 책임 범위

이 모듈의 책임:

1. thread priority apply
2. thread group affinity apply
3. scheduler-owned process priority apply 후보
4. apply 전 original state query
5. RollbackManager state save 요청
6. ApplyGuard 기반 guarded mutation
7. post-apply verify
8. apply/verify failure 시 rollback 요청
9. policy apply evidence 제공

이 모듈의 비책임:

1. ThreadTracker observation 수행
2. PerformanceEngine 판단 수행
3. PolicyCandidate 생성
4. background process restriction ownership
5. topology 정보 생성
6. release evidence final classification

## 4. 입력

Input: validated scheduling policy

- Source: PolicyDispatcher
- Required: Yes
- Validation: policyType, target thread identity, processorGroup, affinity mask 또는 priority 정보가 필요
- Missing behavior: policy reject

Input: target thread identity

- Source: ThreadTracker / PolicyCandidate
- Required: Yes
- Validation: apply 직전 identity revalidation 필요
- Missing behavior: no mutation, evidence 기록

Input: target process identity

- Source: RuntimeContext / PolicyCandidate
- Required: Yes for scheduler-owned process priority policy
- Validation: process identity revalidation 필요
- Missing behavior: no process priority mutation, evidence 기록

Input: TopologyResult

- Source: TopologyAnalyzer
- Required: Yes for affinity policy
- Validation: processorGroup과 KAFFINITY가 함께 유효해야 함
- Missing behavior: group 0 보정 금지, policy reject

Input: RollbackManager

- Source: RuntimeContext
- Required: Yes
- Validation: original state save 가능해야 함
- Missing behavior: no mutation

Input: SchedulerMode

- Source: RuntimeContext / CLI
- Required: Yes
- Validation: apply 허용 상태인지 확인
- Missing behavior: dry-run 또는 monitoring-only

## 5. 출력

Output: apply result

- Consumer: PolicyDispatcher, Evidence Layer, RuntimeValidationMonitor
- Meaning: mutation attempt 결과
- 신뢰도: Win32 result와 post-failure audit에 의존
- Failure behavior: RollbackPending

Output: verify result

- Consumer: RuntimeStateMachine, Evidence Layer
- Meaning: expected state와 actual state 비교 결과
- 신뢰도: post-apply query success에 의존
- Failure behavior: rollback request

Output: rollback state save result

- Consumer: ApplyGuard, Evidence Layer
- Meaning: mutation 전 원상 상태 저장 결과
- 신뢰도: RollbackManager response
- Failure behavior: no mutation

Output: policy apply evidence

- Consumer: PerformanceEvidencePlanner, release evidence
- Meaning: controller name, target identity, policy type, apply/verify result
- 신뢰도: required evidence completeness에 의존
- Failure behavior: missing evidence warning/blocker candidate

Output: process priority apply result

- Consumer: PolicyDispatcher, Evidence Layer
- Meaning: scheduler-owned process priority policy 적용 결과
- 신뢰도: rollback state save와 verify result에 의존
- Failure behavior: rollback request 또는 policy rejected

## 6. 내부 상태

State: active scheduler transaction

- 목적: apply/verify/commit 또는 rollback boundary 추적
- 수명: single apply transaction
- 초기화 시점: rollback state save 이후
- 갱신 시점: arm/apply/verify/commit/rollback
- 보존 여부: Evidence summary

State: last apply target identity

- 목적: stale target 방지
- 수명: transaction
- 초기화 시점: policy handoff
- 갱신 시점: identity revalidation
- 보존 여부: failure evidence

State: last scheduler apply result

- 목적: runtime validation과 evidence 연결
- 수명: runtime cycle 또는 transaction
- 초기화 시점: apply attempt
- 갱신 시점: apply/verify 완료
- 보존 여부: Evidence

## 7. 소유권 규칙

SchedulerController는 thread-level scheduling mutation을 소유한다. scheduler-owned process priority policy가 명시적으로 정의된 경우에도 ApplyGuard와 RollbackManager 계약을 따라야 한다.

SchedulerController는 background process restriction을 소유하지 않는다. background process restriction과 protected/background process mutation filtering은 BackgroundController 소유다.

SchedulerController는 RollbackManager에 state save를 요청하고, ApplyGuard transaction boundary 안에서만 mutation한다.

## 8. 허용 동작

- original thread state query
- scheduler-owned process priority original state query
- RollbackManager state save 요청
- ApplyGuard for thread transaction 생성
- SetThreadGroupAffinity guarded call
- SetThreadPriority guarded call
- scheduler-owned SetPriorityClass guarded call
- post-apply verify
- rollback request
- apply/verify evidence 기록

## 9. 금지 동작

- rollback state 저장 전 mutation 금지
- ApplyGuard arm 전 mutation 금지
- ThreadTracker 책임 침범 금지
- PerformanceEngine 판단 직접 수행 금지
- Processor Group 정보 없을 때 group 0 보정 금지
- post-apply verify 없이 commit 금지
- BackgroundController의 process background restriction 소유권 침범 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Observing | No | 없음 |
| Evaluating | No | 없음 |
| PolicyPending | Limited | policy validation input read |
| Applying | Yes | guarded mutation |
| Verifying | Yes | post-apply verification |
| Running | Limited | active policy status 제공 |
| MonitoringOnly | No | mutation forbidden |
| RollbackPending | Yes | rollback request path |
| RollbackFailedPreserved | Limited | preserved failure context |
| ShutdownRequested | No | new mutation forbidden |
| ShuttingDown | Limited | rollback retry support |

## 11. 실패 처리

Failure: rollback state save failed

- Severity: BLOCKER for apply path
- Fallback: mutation 금지
- Evidence: rollbackStateSaved=false, policyType
- Next State: PolicyRejected 또는 MonitoringOnly

Failure: SetThreadGroupAffinity failed

- Severity: WARN/BLOCKER depending audit
- Fallback: post-failure audit, rollback if mutation escaped
- Evidence: applyResult, auditResult
- Next State: RollbackPending 또는 PolicyRejected

Failure: verify mismatch

- Severity: BLOCKER candidate
- Fallback: rollback
- Evidence: expected state, actual state
- Next State: RollbackPending

Failure: identity mismatch

- Severity: WARN/BLOCKER depending active mutation
- Fallback: no mutation or rollback
- Evidence: target identity
- Next State: PolicyRejected 또는 RollbackPending

## 12. Evidence 요구사항

- controller name
- policy type
- thread id
- process identity
- processorGroup
- affinity mask before/after
- priority before/after
- process priority before/after when scheduler-owned policy is active
- rollbackStateSaved
- apply result
- verify result
- rollback result if failed

Evidence 없는 apply success 판정은 금지한다.

## 13. 테스트 기준

Unit Test:

- save-before-apply ordering
- apply-before-verify ordering
- verify failure rollback path

Integration Test:

- PolicyDispatcher handoff 후 SchedulerController apply/verify evidence 연결

Regression Test:

- mutation API가 SchedulerController/RollbackManager 외부로 이동하지 않았는지 static gate 검증

Failure Test:

- rollback save failure
- SetThreadGroupAffinity failure with audit failure
- identity mismatch

Evidence Test:

- apply/verify/rollback evidence fields completeness

## 14. Non-Goals

- ThreadTracker 구현
- PerformanceEngine confidence threshold 확정
- BackgroundController process restriction 구현
- 구체 C++ 함수 시그니처 확정
- UI 설계

## 15. Acceptance Criteria

1. SchedulerController의 mutation ownership이 명확하다.
2. ApplyGuard/RollbackManager 계약이 포함되어 있다.
3. Processor Group 정보 누락 시 group 0 보정 금지가 명시되어 있다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. 후속 구현 작업에서 scheduling mutation 경계를 판단할 수 있다.

## 16. Open Questions

1. SchedulerController가 process priority를 직접 소유할 범위는 v3.1에서 어디까지인가?
2. MAIN_THREAD_PRIORITY_UP과 affinity stabilize를 같은 transaction으로 묶을 것인가?
3. verify mismatch의 재시도는 허용할 것인가?
4. thread-level background restriction은 v3.1 범위에 포함할 것인가?
