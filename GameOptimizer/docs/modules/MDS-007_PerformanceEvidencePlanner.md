# MDS-007 PerformanceEvidencePlanner

## 1. 문서 개요

문서명: MDS-007 PerformanceEvidencePlanner

버전: v1.0

대상 모듈: PerformanceEvidencePlanner

작성 목적: 성능 정책의 Before/After 비교와 release-facing 판단에 필요한 Evidence 요구사항을 계획하는 모듈 경계를 정의한다.

적용 범위: baseline window 요구사항, applied window 요구사항, required evidence fields, missing evidence 판단, performance success classification 금지 조건 정의

비적용 범위: Evidence file writer 구현, release final classification, Controller apply, Win32 mutation, FPS/frame time 직접 계측 방식 확정

상위 문서:

- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/release/Evidence_Schema.md`

관련 모듈:

- PerformanceEngine
- PolicyDispatcher
- SchedulerController
- BackgroundController
- RuntimeValidationMonitor
- PerformanceEvidenceRecorder

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

PerformanceEvidencePlanner의 목적은 성능 정책이 성공으로 분류되기 위해 필요한 Evidence를 사전에 정의하고, Before/After 비교 가능성을 판단하는 것이다.

PerformanceEvidencePlanner는 홍보 지표가 아니라 정책 판단과 릴리즈 판단의 근거를 만든다.

## 3. 책임 범위

이 모듈의 책임:

1. baseline window 요구사항 정의
2. applied window 요구사항 정의
3. required evidence fields 정의
4. missing evidence 판단
5. performance success classification 금지 조건 정의
6. policy별 Evidence requirement 제공

이 모듈의 비책임:

1. Evidence file write
2. Release final WARN/BLOCKER 확정
3. Win32 mutation 수행
4. Controller apply 결과 생성
5. FPS/frame time 직접 계측 방식 확정

## 4. 입력

Input: PolicyCandidate

- Source: PerformanceEngine / PolicyDispatcher
- Required: Yes
- Validation: policyType과 requiredEvidenceFields 필요
- Missing behavior: missing evidence requirement

Input: controller apply/verify result

- Source: SchedulerController, BackgroundController, TimerResolutionController
- Required: Yes for applied policy
- Validation: applyResult와 verifyResult 모두 필요
- Missing behavior: success classification 금지

Input: rollback result

- Source: RollbackManager / Controller
- Required: Yes if rollback path occurred
- Validation: rollback scope와 result 필요
- Missing behavior: release-facing BLOCKER candidate

Input: runtime validation summary

- Source: RuntimeValidationMonitor
- Required: Yes
- Validation: BLOCKER 여부 확인
- Missing behavior: inconclusive classification

Input: Before/After samples

- Source: ThreadTracker, latency/DPC metrics, policy timeline
- Required: Yes for performance claim
- Validation: baseline and applied window 구분 필요
- Missing behavior: no performance success

## 5. 출력

Output: required evidence fields

- Consumer: PolicyResolver, Evidence Recorder
- Meaning: 정책 결과 판단에 필요한 field set
- 신뢰도: policy type mapping에 의존
- Failure behavior: missing requirement

Output: missing evidence list

- Consumer: Evidence Layer, Release/Validation Layer
- Meaning: 누락된 Evidence field 목록
- 신뢰도: field completeness check
- Failure behavior: policy success forbidden

Output: before/after comparison requirement

- Consumer: PerformanceEvidenceRecorder
- Meaning: baseline/applied window 비교 조건
- 신뢰도: window availability에 의존
- Failure behavior: comparison unavailable

Output: classification eligibility

- Consumer: Policy Layer, Release evidence
- Meaning: 성능 성공 판정 가능 여부
- 신뢰도: Evidence completeness에 의존
- Failure behavior: not eligible

## 6. 내부 상태

State: policy evidence requirement map

- 목적: policyType별 필수 Evidence 정의
- 수명: runtime session
- 초기화 시점: startup
- 갱신 시점: policy spec update 시
- 보존 여부: document contract

State: baseline window state

- 목적: Before/After 비교 가능성 판단
- 수명: policy evaluation window
- 초기화 시점: policy candidate 생성 전
- 갱신 시점: observation cycle
- 보존 여부: Evidence summary

State: applied window state

- 목적: 적용 후 관찰 구간 추적
- 수명: active policy duration
- 초기화 시점: verified apply 후
- 갱신 시점: Running state
- 보존 여부: Evidence summary

State: missing evidence accumulator

- 목적: success classification 차단 사유 기록
- 수명: policy lifecycle
- 초기화 시점: candidate evaluation
- 갱신 시점: apply/verify/rollback/result collection
- 보존 여부: Evidence

## 7. 소유권 규칙

PerformanceEvidencePlanner는 Evidence 요구사항과 missing evidence 판단을 소유한다.

PerformanceEvidencePlanner는 Evidence file writer, Controller apply, rollback execution, release final decision을 소유하지 않는다.

## 8. 허용 동작

- required evidence field definition
- missing evidence detection
- baseline/applied window requirement generation
- classification eligibility 판단
- monitoring-only reason 보강
- release-facing evidence input 제공

## 9. 금지 동작

- Evidence 없는 성능 성공 판정 금지
- Before/After 없는 개선 판정 금지
- rollback failure severity 하향 금지
- RuntimeValidation BLOCKER 무시 금지
- Win32 mutation 금지
- Controller 직접 호출 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Observing | Yes | baseline requirement planning |
| Evaluating | Yes | evidence requirement planning |
| PolicyPending | Yes | required field validation |
| Applying | Limited | expected evidence tracking |
| Verifying | Limited | verify result requirement |
| Running | Yes | before/after comparison requirement |
| MonitoringOnly | Yes | monitoring-only evidence requirement |
| RollbackPending | Yes | rollback evidence requirement |
| ShutdownRequested | Limited | final evidence checklist |
| ShuttingDown | Limited | final summary requirement |

## 11. 실패 처리

Failure: missing required evidence field

- Severity: WARN/BLOCKER candidate
- Fallback: success classification 금지
- Evidence: missingEvidenceList
- Next State: PolicyRejected 또는 MonitoringOnly

Failure: no baseline window

- Severity: WARN
- Fallback: performance comparison unavailable
- Evidence: baselineWindowState
- Next State: Running without success classification

Failure: rollback failure

- Severity: BLOCKER candidate
- Fallback: preserve failure evidence
- Evidence: rollbackResult, preservedStateCount
- Next State: RollbackFailedPreserved

Failure: RuntimeValidation BLOCKER

- Severity: BLOCKER
- Fallback: aggressive policy success 금지
- Evidence: runtimeValidationState
- Next State: RollbackPending

## 12. Evidence 요구사항

- required evidence fields
- missing evidence list
- baseline window status
- applied window status
- before/after comparison requirement
- classification eligibility
- policy type
- rollback result
- runtime validation state

Evidence 없는 성공 판정은 금지한다.

## 13. 테스트 기준

Unit Test:

- policyType별 required evidence field mapping
- missing evidence detection
- baseline/applied window availability

Integration Test:

- PolicyDispatcher, Controller apply result, RuntimeValidation summary와 연결

Regression Test:

- rollback failure가 lower severity로 classification되지 않는지 확인

Failure Test:

- missing applyResult
- missing verifyResult
- no baseline window
- rollback failure

Evidence Test:

- missingEvidenceList와 classification eligibility가 artifact로 남는지 검증

## 14. Non-Goals

- Evidence JSON schema 최종 확정
- Evidence file writer 구현
- FPS/frame time 직접 계측 구현
- Controller apply 구현
- Release Gate script 구현

## 15. Acceptance Criteria

1. Evidence planning 책임과 비책임이 명확하다.
2. Before/After 비교 요구사항이 정의되어 있다.
3. missing evidence 판단이 정의되어 있다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. Evidence 없는 성능 성공 판정 금지가 포함되어 있다.

## 16. Open Questions

1. PerformanceEvidencePlanner와 PerformanceEvidenceRecorder를 분리할 것인가?
2. FPS/frame time은 외부 입력으로 받을 것인가, 직접 계측할 것인가?
3. missing evidence가 WARN인지 BLOCKER인지는 어느 문서에서 최종 확정할 것인가?
4. before/after window duration은 PerformanceValidationPlan에서 확정할 것인가?
