# MDS-008 PolicyDispatcher

## 1. 문서 개요

문서명: MDS-008 PolicyDispatcher

버전: v1.0

대상 모듈: PolicyDispatcher

작성 목적: 검증된 PolicyCandidate를 적절한 Controller로 전달하고, 정책 충돌과 cooldown을 처리하는 Policy Layer 모듈 경계를 정의한다.

적용 범위: policy type validation, required controller validation, risk level 확인, cooldown 확인, conflict 확인, controller handoff, rollback request dispatch, monitoring-only dispatch

비적용 범위: Win32 mutation, ApplyGuard 직접 우회, RollbackManager state 직접 조작, PerformanceEngine 판단 생성

상위 문서:

- `docs/performance/PolicySpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`

관련 모듈:

- PerformanceEngine
- PolicyResolver
- SchedulerController
- BackgroundController
- TimerResolutionController
- RollbackManager

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

PolicyDispatcher의 목적은 검증된 PolicyCandidate를 적절한 Controller로 전달하고, policy conflict, cooldown, risk 조건을 적용 경계에서 확인하는 것이다.

PolicyDispatcher는 정책 실행자가 아니라 정책 전달과 검증 경계다.

## 3. 책임 범위

이 모듈의 책임:

1. policy type validation
2. required controller validation
3. risk level 확인
4. cooldown 확인
5. conflict 확인
6. controller handoff
7. rollback request dispatch
8. monitoring-only dispatch

이 모듈의 비책임:

1. Win32 mutation 직접 수행
2. ApplyGuard 생성
3. RollbackManager state 직접 조작
4. PerformanceEngine 분석 수행
5. Evidence final classification

## 4. 입력

Input: PolicyCandidate

- Source: PerformanceEngine / PolicyResolver
- Required: Yes
- Validation: policyType, requiredController, rollbackScope, evidenceFields 필요
- Missing behavior: reject policy

Input: cooldown state

- Source: Policy Layer state
- Required: Yes for repeated policy
- Validation: same policy thrashing 방지
- Missing behavior: conservative block

Input: runtime validation state

- Source: RuntimeValidationMonitor
- Required: Yes
- Validation: BLOCKER 여부 확인
- Missing behavior: no aggressive policy

Input: active policy state

- Source: RuntimeContext / PolicyDispatcher state
- Required: Yes
- Validation: conflict 판단
- Missing behavior: policy reject

## 5. 출력

Output: dispatch result

- Consumer: RuntimeStateMachine, Evidence Layer
- Meaning: controller handoff 성공/실패
- 신뢰도: validation completeness에 의존
- Failure behavior: PolicyRejected

Output: rejected policy reason

- Consumer: PerformanceEvidencePlanner
- Meaning: 정책 거부 사유
- 신뢰도: rejection rule coverage에 의존
- Failure behavior: unknown rejection

Output: controller handoff result

- Consumer: Controller, Evidence Layer
- Meaning: 어떤 Controller로 전달되었는지
- 신뢰도: requiredController validation에 의존
- Failure behavior: no handoff

Output: cooldown block result

- Consumer: Evidence Layer
- Meaning: cooldown 때문에 차단된 정책
- 신뢰도: policy timestamp state에 의존
- Failure behavior: conservative block

Output: conflict result

- Consumer: RuntimeStateMachine
- Meaning: 정책 간 충돌 판단
- 신뢰도: active policy state completeness
- Failure behavior: MonitoringOnly 또는 PolicyRejected

## 6. 내부 상태

State: active policy registry

- 목적: 현재 active policy와 conflict 판단
- 수명: runtime session
- 초기화 시점: startup
- 갱신 시점: policy dispatch/disable/rollback
- 보존 여부: policy timeline evidence

State: cooldown tracker

- 목적: repeated policy thrashing 방지
- 수명: runtime session
- 초기화 시점: first policy dispatch
- 갱신 시점: dispatch/reject/rollback
- 보존 여부: evidence summary

State: last dispatch result

- 목적: RuntimeValidation과 Evidence 연결
- 수명: cycle
- 초기화 시점: dispatch attempt
- 갱신 시점: handoff/reject
- 보존 여부: Evidence

## 7. 소유권 규칙

PolicyDispatcher는 policy handoff와 dispatch result를 소유한다.

PolicyDispatcher는 Win32 mutation, rollback state, ApplyGuard transaction을 소유하지 않는다.

PolicyDispatcher는 검증 실패 정책을 Controller로 넘기지 않는다.

## 8. 허용 동작

- policy type validation
- controller mapping validation
- risk level validation
- cooldown check
- conflict check
- validated policy handoff
- rollback request dispatch
- monitoring-only dispatch
- dispatch evidence 생성

## 9. 금지 동작

- Win32 mutation 직접 수행 금지
- ApplyGuard 직접 우회 금지
- RollbackManager 직접 상태 조작 금지
- 검증 실패 정책 전달 금지
- Evidence field 없는 정책 성공 처리 금지
- RuntimeValidation BLOCKER 상태에서 aggressive policy 전달 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Evaluating | Limited | policy candidate read |
| PolicyPending | Yes | validation and dispatch |
| PolicyRejected | Yes | rejection evidence |
| Applying | No | controller owns apply |
| Verifying | No | controller owns verify |
| Running | Limited | active policy/cooldown update |
| MonitoringOnly | Yes | monitoring-only dispatch |
| RollbackPending | Yes | rollback request dispatch |
| ShutdownRequested | No | new policy dispatch forbidden |
| ShuttingDown | Limited | rollback/shutdown dispatch only |

## 11. 실패 처리

Failure: invalid policy type

- Severity: WARN
- Fallback: reject policy
- Evidence: rejectedPolicyType, rejectionReason
- Next State: PolicyRejected

Failure: required controller missing

- Severity: BLOCKER candidate for policy
- Fallback: reject policy
- Evidence: requiredControllerState
- Next State: PolicyRejected

Failure: cooldown violation

- Severity: INFO/WARN
- Fallback: block policy for this cycle
- Evidence: cooldownBlockResult
- Next State: PolicyRejected 또는 Observing

Failure: conflict detected

- Severity: WARN
- Fallback: MonitoringOnly or reject lower-priority policy
- Evidence: conflictResult
- Next State: MonitoringOnly 또는 PolicyRejected

Failure: RuntimeValidation BLOCKER

- Severity: BLOCKER
- Fallback: block aggressive policy, dispatch rollback if needed
- Evidence: runtimeValidationState
- Next State: RollbackPending

## 12. Evidence 요구사항

- policy type
- dispatch result
- rejected policy reason
- required controller
- risk level
- cooldown result
- conflict result
- controller handoff result
- monitoring-only reason
- rollback request reason

Evidence 없는 dispatch success 판정은 금지한다.

## 13. 테스트 기준

Unit Test:

- policy type validation
- cooldown block
- conflict detection
- missing controller rejection

Integration Test:

- SchedulerController/BackgroundController/TimerResolutionController handoff
- RuntimeValidation BLOCKER → rollback request

Regression Test:

- PolicyDispatcher가 Win32 mutation API를 직접 호출하지 않는지 static check

Failure Test:

- invalid PolicyCandidate
- missing rollback scope
- missing evidence fields

Evidence Test:

- dispatch/reject/conflict/cooldown evidence completeness

## 14. Non-Goals

- Controller apply 구현
- Win32 API 호출
- ApplyGuard transaction 구현
- PolicyCandidate 최종 C++ 구조 확정
- UI 설계

## 15. Acceptance Criteria

1. PolicyDispatcher의 전달/검증 경계가 명확하다.
2. 직접 mutation 금지가 명시되어 있다.
3. requiredController, cooldown, conflict, risk 검증이 정의되어 있다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. 후속 구현 작업에서 PolicyDispatcher의 모듈 경계를 판단할 수 있다.

## 16. Open Questions

1. PolicyResolver를 별도 모듈로 둘 것인가, PolicyDispatcher 내부 책임으로 둘 것인가?
2. PolicyCandidate와 기존 PolicyCommand는 어떻게 연결할 것인가?
3. cooldown state는 PolicyDispatcher가 소유할 것인가, RuntimeContext가 소유할 것인가?
4. conflict resolution priority table은 어느 문서에서 확정할 것인가?
