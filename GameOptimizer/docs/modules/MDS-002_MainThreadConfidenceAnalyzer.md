# MDS-002 MainThreadConfidenceAnalyzer

## 1. 문서 개요

문서명: MDS-002 MainThreadConfidenceAnalyzer

버전: v1.0

대상 모듈: MainThreadConfidenceAnalyzer

작성 목적: ThreadTracker의 observation snapshot을 기반으로 메인 스레드 후보의 신뢰도(Main Thread Confidence)를 계산하는 모듈 경계를 정의한다.

적용 범위: confidence score 또는 bucket 계산, Low/Medium/High/VeryHigh 분류, confidence 사유 기록, policy eligibility 판단, insufficient evidence 판단

비적용 범위: Win32 mutation, SchedulerController 호출, RollbackManager 호출, PolicyDispatcher 호출, 최종 threshold 수치 확정

상위 문서:

- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`

관련 모듈:

- ThreadTracker
- PerformanceEngine
- PolicyResolver
- PerformanceEvidencePlanner
- RuntimeValidationMonitor

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

MainThreadConfidenceAnalyzer의 목적은 ThreadTracker가 제공하는 observation snapshot을 분석하여 메인 스레드 후보가 실제 최적화 대상인지 판단하는 것이다.

이 모듈은 confidence를 계산하지만 정책을 적용하지 않는다. Confidence는 Policy Candidate eligibility의 입력일 뿐 직접 mutation trigger가 아니다.

핵심 원칙:

```text
Confidence < High이면 affinity/priority 정책 적용 후보를 만들 수 없다.
```

## 3. 책임 범위

이 모듈의 책임:

1. confidence score 또는 bucket 계산
2. Low/Medium/High/VeryHigh 분류
3. confidence 상승/하락 사유 기록
4. policy eligibility 판단
5. insufficient evidence 판단
6. monitoring-only reason 생성

이 모듈의 비책임:

1. Win32 mutation 수행
2. SchedulerController 직접 호출
3. RollbackManager 직접 호출
4. PolicyDispatcher 직접 호출
5. Threshold 최종 수치 확정
6. Evidence final classification 결정

## 4. 입력

Input: ThreadSampleSnapshot

- Source: ThreadTracker
- Required: Yes
- Validation: sample count와 timestamp continuity가 최소 조건을 만족해야 함
- Missing behavior: Low confidence + insufficient evidence reason

Input: ThreadRuntimeStats

- Source: ThreadTracker
- Required: Yes
- Validation: CPU delta, EMA, lifetime field가 관찰 가능해야 함
- Missing behavior: confidence 상향 금지

Input: migration count

- Source: ThreadTracker
- Required: No
- Validation: topology 정보와 연결 가능해야 함
- Missing behavior: migration stability unknown

Input: access failure count

- Source: ThreadTracker / RuntimeValidationMonitor
- Required: Yes
- Validation: query failure가 confidence 품질에 반영되어야 함
- Missing behavior: confidence 상향 제한

Input: threshold configuration

- Source: PerformanceValidationPlan 또는 policy configuration
- Required: No
- Validation: High/VeryHigh threshold는 TBD
- Missing behavior: default candidate threshold 사용 금지, conservative bucket 사용

## 5. 출력

Output: MainThreadConfidenceLevel

- Consumer: PerformanceEngine, PolicyResolver
- Meaning: Low/Medium/High/VeryHigh confidence bucket
- 신뢰도: input quality와 observation duration에 의존
- Failure behavior: Low

Output: confidence reason

- Consumer: PerformanceEvidencePlanner, PolicyResolver
- Meaning: confidence 산출 또는 강등 사유
- 신뢰도: evidence trace로 사용
- Failure behavior: insufficient evidence

Output: policy eligibility

- Consumer: PerformanceEngine, Policy Layer
- Meaning: priority/affinity/aggressive 후보 가능 여부
- 신뢰도: confidence level에 직접 의존
- Failure behavior: no eligibility

Output: monitoring-only reason

- Consumer: RuntimeStateMachine, Evidence Layer
- Meaning: 정책 후보를 만들지 않는 사유
- 신뢰도: fallback 판단에 사용
- Failure behavior: unknown confidence reason

## 6. 내부 상태

State: candidate confidence history

- 목적: confidence 안정성 추적
- 수명: observation session
- 초기화 시점: first candidate detected
- 갱신 시점: Evaluating state
- 보존 여부: Evidence summary로 보존

State: candidate stickiness

- 목적: main thread 후보 유지 시간 추적
- 수명: candidate lifetime
- 초기화 시점: candidate 변경
- 갱신 시점: sample evaluation
- 보존 여부: summary만 보존

State: insufficient evidence flags

- 목적: confidence 상향 차단 조건 기록
- 수명: evaluation window
- 초기화 시점: evaluation start
- 갱신 시점: missing input 발생
- 보존 여부: Evidence로 보존

## 7. 소유권 규칙

MainThreadConfidenceAnalyzer는 confidence 판단 결과와 그 사유를 소유한다.

MainThreadConfidenceAnalyzer는 ThreadTracker raw observation data를 소유하지 않는다.

MainThreadConfidenceAnalyzer는 policy dispatch, rollback state, Win32 mutation, controller state를 소유하지 않는다.

## 8. 허용 동작

- ThreadSampleSnapshot read
- EMA / CPU delta / lifetime / migration signal 분석
- confidence bucket 계산
- confidence reason 생성
- policy eligibility flag 생성
- monitoring-only reason 생성
- insufficient evidence 판단

## 9. 금지 동작

- Win32 mutation 금지
- SetThreadAffinityMask 호출 금지
- SetThreadGroupAffinity 호출 금지
- SetThreadPriority 호출 금지
- SetPriorityClass 호출 금지
- SchedulerController 직접 호출 금지
- RollbackManager 직접 호출 금지
- PolicyDispatcher 직접 호출 금지
- ApplyGuard 생성 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Observing | Limited | snapshot quality check |
| Evaluating | Yes | confidence calculation |
| PolicyPending | Limited | confidence result read |
| Applying | No | mutation forbidden |
| Verifying | No | mutation forbidden |
| Running | Yes | confidence drift monitoring |
| MonitoringOnly | Yes | recovery condition analysis |
| RollbackPending | No | rollback 소유권 없음 |
| ShutdownRequested | Limited | final confidence summary |
| ShuttingDown | Limited | final summary only |

## 11. 실패 처리

Failure: insufficient sample count

- Severity: INFO/WARN
- Fallback: Low 또는 Medium 유지
- Evidence: sample count, observation duration
- Next State: Observing 또는 MonitoringOnly

Failure: candidate switch frequency high

- Severity: WARN
- Fallback: confidence 상향 차단
- Evidence: mainThreadSwitchCount
- Next State: Evaluating 또는 MonitoringOnly

Failure: access failure count high

- Severity: WARN
- Fallback: monitoring-only reason 생성
- Evidence: accessDeniedCount, confidenceQuality
- Next State: MonitoringOnly

Failure: threshold missing

- Severity: INFO
- Fallback: TBD threshold를 확정하지 않고 conservative 판단
- Evidence: thresholdState=TBD
- Next State: Evaluating

## 12. Evidence 요구사항

- confidence level
- confidence score 또는 bucket
- confidence reason
- observation duration
- sample count
- main thread switch count
- migration stability
- access failure count
- policy eligibility
- monitoring-only reason

Evidence가 없는 confidence 상향은 정책 후보 생성 근거가 될 수 없다.

## 13. 테스트 기준

Unit Test:

- Low/Medium/High/VeryHigh bucket 분류 시나리오
- insufficient evidence 시 confidence 상향 차단
- candidate switch 시 confidence 강등

Integration Test:

- ThreadTracker snapshot을 입력으로 받아 PerformanceEngine policy eligibility에 연결

Regression Test:

- Win32 mutation API, RollbackManager, PolicyDispatcher 호출이 추가되지 않았는지 static check

Failure Test:

- missing snapshot
- high access failure count
- unstable candidate lifetime

Evidence Test:

- confidence reason과 monitoring-only reason이 evidence field로 남는지 검증

## 14. Non-Goals

- 구체 C++ 함수 시그니처 확정
- High/VeryHigh threshold 최종 수치 확정
- PolicyCommand enum 확정
- Controller apply 구현
- UI 설계

## 15. Acceptance Criteria

1. confidence 계산 책임과 비책임이 명확하다.
2. 입력과 출력이 정의되어 있다.
3. Win32 mutation 금지가 명확하다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. Confidence < High 정책 차단 원칙이 포함되어 있다.

## 16. Open Questions

1. High/VeryHigh threshold는 어떤 validation plan에서 확정할 것인가?
2. confidence score를 numeric으로 둘 것인가, bucket-only로 둘 것인가?
3. candidate switch frequency 기준은 cycle count로 둘 것인가, 시간 기반으로 둘 것인가?
4. confidence reason의 final evidence field 이름은 어디서 확정할 것인가?
