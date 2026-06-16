# MDS-005 RollbackManager

## 1. 문서 개요

문서명: MDS-005 RollbackManager

버전: v1.0

대상 모듈: RollbackManager

작성 목적: mutation 전 원상 상태를 저장하고, 실패 또는 종료 시 안전하게 rollback을 수행하는 모듈의 책임과 보존 계약을 정의한다.

적용 범위: thread/process original state save, timer original state 가능 범위 정의, identity revalidation, rollback execution, failed rollback state preservation, rollback result evidence

비적용 범위: policy decision, thread observation, topology analysis, release final decision, Controller apply 판단

상위 문서:

- `docs/contracts/Safety_Contract.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/release/Evidence_Schema.md`

관련 모듈:

- SchedulerController
- BackgroundController
- TimerResolutionController
- ApplyGuard
- ShutdownPipeline
- RuntimeValidationMonitor

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

RollbackManager의 목적은 mutation 전 원상 상태를 저장하고, 실패 또는 종료 시 안전하게 rollback을 수행하는 것이다.

Rollback 실패는 release-facing evidence에서 낮은 severity로 숨기지 않는다.

## 3. 책임 범위

이 모듈의 책임:

1. thread original state save
2. process original state save
3. timer original state save 가능 범위 정의
4. target identity revalidation
5. rollback execution
6. failed rollback state preservation
7. rollback result evidence 제공

이 모듈의 비책임:

1. policy candidate 생성
2. ThreadTracker observation
3. Performance Engine 판단
4. ApplyGuard transaction decision 대체
5. release final BLOCKER/WARN classification 단독 결정

## 4. 입력

Input: save state request

- Source: SchedulerController, BackgroundController, TimerResolutionController
- Required: Yes
- Validation: target identity와 original state query result 필요
- Missing behavior: save failure, mutation block

Input: rollback request

- Source: Controller, ApplyGuard, ShutdownPipeline
- Required: Yes
- Validation: saved state와 current identity revalidation 필요
- Missing behavior: no rollback target evidence

Input: target identity

- Source: Controller / RuntimeContext
- Required: Yes
- Validation: process/thread identity hash 또는 equivalent check
- Missing behavior: stale target, rollback block or preserved state

Input: save state disposition

- Source: RollbackManager internal ownership result
- Required: Yes
- Validation: CreatedNewState/ReusedExistingState 구분 필요
- Missing behavior: unsafe discard 금지

## 5. 출력

Output: save state result

- Consumer: Controller, ApplyGuard, Evidence Layer
- Meaning: original state save 성공/실패와 ownership disposition
- 신뢰도: query와 insert/update 결과
- Failure behavior: no mutation

Output: rollback result

- Consumer: ApplyGuard, ShutdownPipeline, Evidence Layer
- Meaning: state restore 결과
- 신뢰도: post-rollback verification 가능성에 의존
- Failure behavior: failed state preserved

Output: preserved failed state

- Consumer: ShutdownPipeline, release evidence
- Meaning: rollback 실패 후 보존된 state
- 신뢰도: state preservation record
- Failure behavior: FatalError candidate

Output: identity validation result

- Consumer: Controller, Evidence Layer
- Meaning: stale target 여부
- 신뢰도: identity check completeness
- Failure behavior: rollback blocked/preserved

## 6. 내부 상태

State: saved rollback states

- 목적: original state 보존
- 수명: mutation 이후 shutdown 또는 discard까지
- 초기화 시점: save request
- 갱신 시점: save/reuse/rollback/preserve
- 보존 여부: rollback failure 시 보존

State: rollback state ownership disposition

- 목적: ApplyGuard cleanup 결정 지원
- 수명: transaction
- 초기화 시점: save result
- 갱신 시점: commit/discard/rollback
- 보존 여부: evidence로 요약

State: preserved failed rollback state

- 목적: shutdown retry와 release-facing evidence
- 수명: failure 이후 shutdown까지
- 초기화 시점: rollback failure
- 갱신 시점: retry/final evidence
- 보존 여부: yes

## 7. 소유권 규칙

RollbackManager는 rollback state ownership의 source of truth다.

RollbackManager는 Controller가 요청한 save/rollback을 수행하지만, 어떤 정책을 적용할지는 소유하지 않는다.

RollbackManager는 failed rollback state를 임의 폐기하지 않는다.

## 8. 허용 동작

- original thread/process state 저장
- timer original state 저장 가능 범위 정의
- identity revalidation
- rollback execution
- rollback state preservation
- rollback result evidence 제공
- save state disposition 제공

## 9. 금지 동작

- identity mismatch 무시 금지
- rollback failure를 WARN/INFO로 낮추기 금지
- failed state 폐기 금지
- 성공 rollback으로 위장 금지
- policy candidate 생성 금지
- observation-only module에서 직접 호출되도록 경계 허물기 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Applying | Yes | save original state |
| Verifying | Limited | rollback 준비 |
| Running | Limited | active saved state 유지 |
| RollbackPending | Yes | rollback execution |
| RollbackFailedPreserved | Yes | failed state preservation |
| ShutdownRequested | Limited | rollback required 판단 지원 |
| ShuttingDown | Yes | final rollback retry |
| FatalError | Limited | last known state evidence |
| Observing | No | direct call forbidden |
| Evaluating | No | direct call forbidden |

## 11. 실패 처리

Failure: save state failed

- Severity: BLOCKER for mutation
- Fallback: mutation 금지
- Evidence: saveStateResult=false
- Next State: PolicyRejected 또는 MonitoringOnly

Failure: identity mismatch

- Severity: WARN/BLOCKER depending mutation state
- Fallback: rollback 차단 또는 failed state preservation
- Evidence: identityValidationResult
- Next State: RollbackFailedPreserved 또는 FatalError

Failure: rollback failed

- Severity: BLOCKER candidate
- Fallback: preserve failed state, shutdown retry
- Evidence: rollbackResult, preservedStateCount
- Next State: RollbackFailedPreserved

Failure: rollback state missing

- Severity: BLOCKER if mutation occurred
- Fallback: Safe Shutdown / FatalError candidate
- Evidence: missingRollbackState
- Next State: FatalError 또는 RollbackFailedPreserved

## 12. Evidence 요구사항

- save state result
- save state disposition
- rollback result
- rollback reason
- rollback scope
- identity validation result
- preserved failed state
- preserved state count
- shutdown retry eligibility

Rollback Evidence 없는 mutation success는 허용하지 않는다.

## 13. 테스트 기준

Unit Test:

- save state disposition
- identity revalidation
- rollback success/failure state preservation

Integration Test:

- SchedulerController/BackgroundController와 ApplyGuard transaction 연동

Regression Test:

- rollback failure가 WARN/INFO로 낮아지지 않는지 release evidence selftest

Failure Test:

- missing state
- stale identity
- rollback API failure

Evidence Test:

- rollback preserved state count와 failure transfer evidence 연결 검증

## 14. Non-Goals

- Controller apply 구현
- PolicyResolver 구현
- ThreadTracker observation
- Release Gate script 구현
- 구체 C++ 함수 시그니처 확정

## 15. Acceptance Criteria

1. RollbackManager의 state ownership이 명확하다.
2. identity mismatch 처리 원칙이 포함되어 있다.
3. rollback failure preservation이 정의되어 있다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. rollback 없는 mutation 금지 원칙을 지원한다.

## 16. Open Questions

1. timer original state 저장은 RollbackManager가 직접 소유할 것인가, TimerResolutionController가 소유할 것인가?
2. preserved failed state의 final artifact format은 Evidence Schema에서 어떻게 정의할 것인가?
3. identity validation field set은 어느 문서에서 확정할 것인가?
4. rollback retry 횟수와 timeout은 어디서 정의할 것인가?
