# GameOptimizer v3.1 Patch Plan — Phase 2: Main Thread Confidence Engine

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 2

버전: v1.0

작성 목적: GameOptimizer v3.1의 두 번째 구현 단계인 Phase 2 — Main Thread Confidence Engine을 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, Phase 2에서 어떤 순서로 어떤 모듈을 수정하고, MainThreadConfidenceAnalyzer를 어떤 기준으로 도입하며, 각 패치의 완료 기준(Definition of Done), 테스트 기준, Evidence 기준을 정의하는 작업 계획 문서다.

적용 범위:

- MainThreadConfidenceAnalyzer 책임 경계 정의
- ThreadTracker raw observation signal의 confidence input 변환 기준
- 메인 스레드 신뢰도(Main Thread Confidence) 단계 분류
- 신뢰도 단계(Confidence Level)와 정책 적격성(Policy Eligibility) 연결
- 관찰 전용 사유(Monitoring-only Reason)와 불충분 증거(Insufficient Evidence) 처리
- Confidence history와 Runtime Validation 연계

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 성능 개선 수치 보장
- SchedulerController mutation 변경
- RollbackManager 변경
- ApplyGuard 변경
- Aggressive Single-Core Mode 구현
- GameProfileRegistry 구현
- Release Gate 자동화

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/implementation/PatchPlan_Phase1.md`
- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`

후속 문서:

- `docs/implementation/PatchPlan_Phase3.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 2의 목표는 메인 스레드를 직접 고정하는 것이 아니라, 메인 스레드 후보가 정책 적용 대상으로 신뢰 가능한지 판단하는 기준을 만드는 것이다.

## 2. Phase 2 목표

Phase 2의 핵심 목표는 ThreadTracker가 제공한 raw observation signal을 기반으로 Main Thread Confidence를 계산하고, 정책 후보 생성 전 단계에서 안전하게 차단 또는 허용 후보를 분류할 수 있는 판단 기준을 만드는 것이다.

필수 목표:

1. MainThreadConfidenceAnalyzer 책임 경계 확정
2. Confidence input contract 정의
3. Low / Medium / High / VeryHigh confidence level 정의
4. Confidence 상승/하락 reason 정의
5. Insufficient evidence reason 정의
6. Monitoring-only reason 제공
7. Policy eligibility 제공
8. Confidence history Evidence 생성
9. RuntimeValidation과 연계 가능한 confidence state 제공

Phase 2는 Phase 1의 observation/topology foundation을 소비한다. Phase 2에서 생성되는 결과는 policy apply 명령이 아니라 policy eligibility와 monitoring-only 판단 입력이다.

## 3. Phase 2 비목표

다음은 Phase 2에서 구현하지 않는다.

```text
SchedulerController mutation 변경
PolicyDispatcher 최종 구현
PolicyCandidate dispatch 구현
Aggressive Single-Core Mode 구현
Background restriction 확장
GameProfileRegistry runtime integration
Before/After performance classification
Release Gate 자동화
```

Phase 2에서 Win32 mutation을 수행하지 않는다.

Phase 2의 PASS는 정책 적용 성공이 아니다. Phase 2 PASS는 confidence 판단 체계가 안전하게 준비되었다는 의미다.

## 4. 영향 모듈

Phase 2에서 영향을 받을 수 있는 모듈은 직접 영향, 간접 영향, 변경 금지 또는 최소 변경 대상으로 구분한다.

### 4.1 직접 영향 모듈

```text
MainThreadConfidenceAnalyzer
PerformanceEngine
ThreadTracker output consumer
RuntimeValidationMonitor
PerformanceEvidenceRecorder
```

### 4.2 간접 영향 모듈

```text
PolicySpecification
PolicyDispatcher
SchedulerController
GameProfileRegistry
RuntimeContext
```

간접 영향 모듈은 confidence result를 읽거나 후속 Phase에서 사용할 수 있지만, Phase 2에서 mutation 책임을 새로 가져서는 안 된다.

### 4.3 변경 금지 또는 최소 변경 모듈

```text
SchedulerController
BackgroundController
RollbackManager
ApplyGuard
TopologyAnalyzer
```

주의:

Phase 2에서 SchedulerController, RollbackManager, ApplyGuard의 동작을 변경해야 하는 경우, 별도 Phase로 분리한다.

## 5. 패치 단위 개요

Phase 2는 다음 Patch ID 구조를 사용한다.

```text
P2-01 Confidence Contract Alignment
P2-02 Confidence Input Model
P2-03 Confidence Level Classification
P2-04 Confidence Reason & Downgrade Reason
P2-05 Insufficient Evidence / Monitoring-only Reason
P2-06 Policy Eligibility Output
P2-07 Confidence History Evidence
P2-08 RuntimeValidation Confidence Checks
P2-09 PerformanceEngine Integration Boundary
P2-10 Phase 2 Regression Test & Evidence
```

각 패치는 다음 양식을 따른다.

```text
Patch ID
목적
작업 범위
수정 가능 파일
수정 금지 파일
구현 규칙
완료 기준
필수 테스트
필수 Evidence
리스크
Rollback 계획
```

패치 하나가 confidence calculation, policy dispatch, mutation, rollback, release gate를 동시에 변경해서는 안 된다.

## 6. P2-01 Confidence Contract Alignment

Patch ID: `P2-01`

목적:

MainThreadConfidenceAnalyzer의 책임과 비책임을 코드/문서 기준으로 정렬한다.

작업 범위:

```text
MDS-002 기준 확인
PerformanceEngineSpec 기준 확인
현재 코드에 confidence 관련 로직이 분산되어 있는지 확인
Phase 2에서 분리할 범위 정의
```

수정 가능 파일:

```text
docs/implementation/PatchPlan_Phase2.md
docs/implementation/MIGRATION_NOTES_Phase2.md
MainThreadConfidenceAnalyzer 관련 문서 또는 placeholder 파일
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
BackgroundController
```

구현 규칙:

```text
MainThreadConfidenceAnalyzer는 판단 계층이다.
MainThreadConfidenceAnalyzer는 Win32 mutation을 수행하지 않는다.
MainThreadConfidenceAnalyzer는 SchedulerController를 직접 호출하지 않는다.
확정되지 않은 threshold는 TBD로 남긴다.
```

완료 기준:

```text
MainThreadConfidenceAnalyzer의 책임과 비책임이 명확하다.
Phase 2에서 mutation을 수행하지 않는다는 계약이 고정된다.
Confidence 관련 blocking question이 식별된다.
```

필수 테스트:

```text
confidence contract review
mutation ownership review
MDS-002 alignment review
blocking question review
```

필수 Evidence:

```text
confidenceContractSummary
outOfScopeMutationList
blockingQuestionList
```

리스크:

```text
Confidence 책임이 PerformanceEngine 내부에 과도하게 섞이면 Phase 3 policy candidate 경계가 흐려질 수 있다.
threshold를 조기 확정하면 검증 전 성능 특성을 단정할 수 있다.
```

Rollback 계획:

```text
책임 경계가 모호하면 문서/placeholder 변경만 유지하고 코드 변경은 후속 패치로 분리한다.
threshold 관련 변경이 포함되었다면 TBD 상태로 되돌리고 evidence 기반 결정으로 남긴다.
```

## 7. P2-02 Confidence Input Model

Patch ID: `P2-02`

목적:

ThreadTracker raw signal을 confidence 계산 입력으로 사용할 수 있게 입력 모델을 정리한다.

작업 범위:

```text
ThreadSampleSnapshot 소비 기준 정의
ThreadRuntimeStats 소비 기준 정의
CPU delta 입력 기준 정의
EMA input 기준 정의
migration count 입력 기준 정의
candidate switch count 입력 기준 정의
access failure count 입력 기준 정의
topology safety input 연결 여부 정의
```

수정 가능 파일:

```text
MainThreadConfidenceAnalyzer 관련 소스/헤더
PerformanceEngine 관련 소스/헤더
ThreadTracker output consumer
관련 테스트
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
BackgroundController
PolicyDispatcher mutation 경로
```

구현 규칙:

```text
Confidence input은 observation result여야 한다.
Confidence input은 policy apply result가 아니다.
Input이 불완전하면 confidence 상승에 사용하지 않는다.
ThreadTracker output을 policy eligibility로 직접 해석하지 않는다.
```

완료 기준:

```text
Confidence 계산에 필요한 입력이 명확히 정의된다.
partial snapshot은 insufficient evidence로 분류 가능하다.
Input rejected reason이 Evidence로 기록 가능하다.
```

필수 테스트:

```text
valid confidence input test
empty snapshot input test
partial snapshot input test
access failure input test
migration input test
```

필수 Evidence:

```text
confidenceInputSampleCount
inputCompleteness
accessFailureCount
migrationCount
candidateSwitchCount
inputRejectedReason
```

리스크:

```text
ThreadTracker snapshot 계약이 아직 불안정하면 confidence input model이 잦게 변경될 수 있다.
Input completeness와 topology safety가 섞이면 Phase 1/Phase 2 책임 경계가 흐려질 수 있다.
```

Rollback 계획:

```text
입력 모델이 불안정하면 raw observation adapter를 두고 analyzer 내부 판단은 최소화한다.
Topology safety 연결이 위험하면 monitoring-only reason 입력만 남기고 eligibility 연결은 후속 패치로 분리한다.
```

## 8. P2-03 Confidence Level Classification

Patch ID: `P2-03`

목적:

Main Thread Confidence를 Low / Medium / High / VeryHigh로 분류한다.

작업 범위:

```text
confidence level enum 또는 문서상 분류 기준 정리
Low 조건 정의
Medium 조건 정의
High 조건 정의
VeryHigh 조건 정의
level transition 기준 정의
threshold는 확정되지 않았으면 TBD로 둠
```

수정 가능 파일:

```text
MainThreadConfidenceAnalyzer 관련 소스/헤더
PerformanceEngine 관련 소스/헤더
관련 테스트
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
PolicyDispatcher dispatch 경로
```

구현 규칙:

```text
Low: 관찰만 허용
Medium: monitoring-only 또는 dry-run/soft-apply validation 후보
High: 제한적 affinity/priority policy eligibility 가능
VeryHigh: aggressive mode eligibility 후보 가능
VeryHigh는 aggressive mode 즉시 활성화가 아니다.
```

완료 기준:

```text
confidence level이 안정적으로 계산된다.
각 level이 Evidence에 기록된다.
threshold 미확정 항목은 TBD로 남는다.
```

필수 테스트:

```text
low confidence classification test
medium confidence classification test
high confidence classification test
very high confidence classification test
confidence level transition test
```

필수 Evidence:

```text
confidenceLevel
confidenceScoreOrBucket
classificationReason
previousConfidenceLevel
confidenceTransitionReason
```

리스크:

```text
threshold를 너무 낮게 잡으면 잘못된 High/VeryHigh 승격이 발생할 수 있다.
bucket-only 분류가 너무 거칠면 policy eligibility 설명력이 부족할 수 있다.
```

Rollback 계획:

```text
분류 기준이 불안정하면 confidenceScoreOrBucket을 diagnostic evidence로만 남기고 policy eligibility는 false로 제한한다.
VeryHigh 승격 문제가 발생하면 VeryHigh를 High 이하로 downgrade하고 aggressive candidate를 차단한다.
```

## 9. P2-04 Confidence Reason & Downgrade Reason

Patch ID: `P2-04`

목적:

Confidence 상승/하락 이유를 명확히 기록한다.

작업 범위:

```text
confidence increase reason 정의
confidence downgrade reason 정의
candidate instability reason 정의
migration instability reason 정의
access failure downgrade reason 정의
short observation reason 정의
```

수정 가능 파일:

```text
MainThreadConfidenceAnalyzer
PerformanceEvidenceRecorder
RuntimeSnapshotRecorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
BackgroundController
```

구현 규칙:

```text
Confidence 결과는 level만 기록하면 안 된다.
왜 해당 level인지 reason을 함께 제공해야 한다.
불안정한 후보는 High/VeryHigh로 승격하지 않는다.
```

완료 기준:

```text
Confidence 상승/하락 이유가 Evidence로 기록된다.
불안정한 후보는 High/VeryHigh로 승격되지 않는다.
downgrade reason이 monitoring-only reason과 연결 가능하다.
```

필수 테스트:

```text
confidence increase reason test
migration downgrade reason test
candidate switch downgrade test
access failure downgrade test
short observation downgrade test
```

필수 Evidence:

```text
confidenceReason
downgradeReason
candidateInstabilityReason
migrationInstabilityReason
accessFailureReason
```

리스크:

```text
Reason taxonomy가 너무 세분화되면 evidence schema와 report가 복잡해질 수 있다.
Reason이 누락되면 confidence 결과를 release evidence에서 설명할 수 없다.
```

Rollback 계획:

```text
세부 reason이 불안정하면 공통 downgradeReason과 insufficientEvidenceReason으로 축소한다.
Evidence schema 연동이 지연되면 runtime snapshot diagnostic field로 먼저 보존한다.
```

## 10. P2-05 Insufficient Evidence / Monitoring-only Reason

Patch ID: `P2-05`

목적:

신뢰도가 부족하거나 입력이 불완전할 때 monitoring-only 전환 사유를 제공한다.

작업 범위:

```text
insufficient evidence reason 정의
monitoring-only reason 정의
topology incomplete 연계
access failure 연계
short observation 연계
candidate unstable 연계
```

수정 가능 파일:

```text
MainThreadConfidenceAnalyzer
PerformanceEngine
RuntimeValidationMonitor
Evidence recorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
BackgroundController
```

구현 규칙:

```text
Insufficient evidence 상태에서 policy apply 후보를 생성하지 않는다.
Monitoring-only는 실패가 아니라 안전한 비적용 상태다.
Monitoring-only reason은 Release Evidence에 남길 수 있어야 한다.
```

완료 기준:

```text
confidence 부족 시 monitoring-only reason이 제공된다.
policy eligibility가 false로 제한된다.
insufficient evidence가 RuntimeValidation과 Evidence로 전달 가능하다.
```

필수 테스트:

```text
insufficient evidence blocks policy eligibility test
monitoring-only reason propagation test
topology incomplete reason propagation test
short observation monitoring-only test
```

필수 Evidence:

```text
insufficientEvidence
insufficientEvidenceReason
monitoringOnlyReason
policyEligibilityBlockedReason
```

리스크:

```text
Monitoring-only reason이 누락되면 사용자는 정책이 왜 적용되지 않았는지 알 수 없다.
Insufficient evidence 상태가 policy candidate로 새면 안전 계약을 위반한다.
```

Rollback 계획:

```text
reason taxonomy가 불안정하면 broad monitoringOnlyReason으로 축소하되 policy eligibility false는 유지한다.
PerformanceEngine 연동이 불안정하면 analyzer result만 기록하고 policy gate 연결은 P2-09로 이동한다.
```

## 11. P2-06 Policy Eligibility Output

Patch ID: `P2-06`

목적:

Confidence 결과를 policy layer가 사용할 수 있는 eligibility 정보로 제공한다.

작업 범위:

```text
priority policy eligibility 정의
affinity policy eligibility 정의
core isolation eligibility 정의
aggressive mode eligibility 후보 정의
eligibility block reason 정의
```

수정 가능 파일:

```text
MainThreadConfidenceAnalyzer
PerformanceEngine
PolicyCandidate input type
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation 경로
RollbackManager
ApplyGuard
BackgroundController
PolicyDispatcher dispatch 경로
```

구현 규칙:

```text
Confidence < High이면 affinity/priority policy eligibility는 false다.
Aggressive mode eligibility는 VeryHigh만으로 충분하지 않으며 추가 안전 조건이 필요하다.
Phase 2에서는 aggressive mode를 실제 활성화하지 않는다.
Eligibility는 policy candidate 생성 전 gate로 사용된다.
```

완료 기준:

```text
Policy eligibility가 confidence level에 따라 제한된다.
Eligibility와 block reason이 Evidence에 기록된다.
VeryHigh는 aggressiveModeCandidate만 표시하고 enable하지 않는다.
```

필수 테스트:

```text
low blocks all mutation eligibility test
medium blocks affinity priority eligibility test
high allows limited eligibility test
very high marks aggressive candidate only test
eligibility block reason test
```

필수 Evidence:

```text
priorityPolicyEligible
affinityPolicyEligible
coreIsolationEligible
aggressiveModeCandidate
eligibilityBlockedReason
```

리스크:

```text
Eligibility output이 policy dispatch와 결합되면 Phase 3 범위를 침범할 수 있다.
VeryHigh를 enable로 오해하면 aggressive mode가 너무 일찍 열린다.
```

Rollback 계획:

```text
Eligibility 연결이 위험하면 analyzer result에 candidate flag만 남기고 policy input 연결을 차단한다.
High/VeryHigh 판정이 불안정하면 all mutation eligibility를 false로 fallback한다.
```

## 12. P2-07 Confidence History Evidence

Patch ID: `P2-07`

목적:

Confidence 변화 이력을 Evidence로 기록한다.

작업 범위:

```text
confidence history summary 정의
confidence level transition 기록
confidence stability duration 기록
candidate switch count 연결
migration count 연결
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
RuntimeSnapshotRecorder
PolicyTimelineRecorder if applicable
MainThreadConfidenceAnalyzer
관련 테스트
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
BackgroundController
```

구현 규칙:

```text
단일 cycle의 confidence만으로 성능 판단하지 않는다.
confidence history는 PerformanceValidationPlan의 Before/After 비교에 사용 가능해야 한다.
history는 policy apply 성공을 의미하지 않는다.
```

완료 기준:

```text
confidence history가 runtime snapshot 또는 performance evidence에 포함된다.
transition count와 stability duration이 기록된다.
history evidence가 누락되면 confidence validation이 WARN 또는 BLOCKER 후보가 된다.
```

필수 테스트:

```text
confidence history recorded test
confidence transition recorded test
confidence stability duration test
confidence evidence completeness test
```

필수 Evidence:

```text
confidenceHistory
confidenceTransitionCount
confidenceStableDuration
highestConfidenceLevel
lowestConfidenceLevel
```

리스크:

```text
history 보관 길이를 조기 확정하면 메모리와 evidence 크기 문제가 생길 수 있다.
history가 너무 짧으면 confidence 안정성을 설명하기 어렵다.
```

Rollback 계획:

```text
history 저장 방식이 불안정하면 summary-only evidence로 축소한다.
보관 길이는 TBD로 두고 configurable 또는 bounded summary로 유지한다.
```

## 13. P2-08 RuntimeValidation Confidence Checks

Patch ID: `P2-08`

목적:

Confidence 관련 이상 상태를 RuntimeValidation에서 감지할 수 있게 한다.

작업 범위:

```text
confidence input freshness validation
confidence level consistency validation
High/VeryHigh eligibility consistency validation
insufficient evidence handling validation
monitoring-only reason validation
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
MainThreadConfidenceAnalyzer
관련 테스트
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
BackgroundController
```

구현 규칙:

```text
RuntimeValidation은 mutation을 수행하지 않는다.
RuntimeValidation은 confidence inconsistency를 WARN 또는 BLOCKER 후보로 분류한다.
Eligibility inconsistency는 safety-facing issue로 다룬다.
```

완료 기준:

```text
Confidence 관련 validation 결과가 RuntimeValidation Summary에 포함된다.
High/VeryHigh eligibility inconsistency가 감지된다.
monitoring-only reason 누락이 validation 결과로 드러난다.
```

필수 테스트:

```text
confidence input stale warning test
eligibility inconsistency blocker test
missing monitoring-only reason test
high confidence without evidence blocker test
```

필수 Evidence:

```text
confidenceValidationState
confidenceInputFreshness
eligibilityConsistency
confidenceValidationWarnCount
confidenceValidationBlockerCount
```

리스크:

```text
RuntimeValidation rule이 과도하면 정상 dry-run도 BLOCKER가 될 수 있다.
반대로 eligibility inconsistency를 WARN으로 낮추면 unsafe policy 후보가 통과할 수 있다.
```

Rollback 계획:

```text
초기 rule은 confidence freshness와 eligibility inconsistency에 집중한다.
과도한 rule은 WARN으로 낮추되 Confidence < High eligibility true는 BLOCKER로 유지한다.
```

## 14. P2-09 PerformanceEngine Integration Boundary

Patch ID: `P2-09`

목적:

MainThreadConfidenceAnalyzer가 PerformanceEngine과 연결되되 mutation 책임을 침범하지 않도록 한다.

작업 범위:

```text
PerformanceEngine이 confidence result를 소비하는 경계 정의
PolicyCandidate 생성 전 confidence gate 연결
PerformanceEngine direct Win32 mutation 금지 확인
SchedulerController 직접 호출 금지 확인
```

수정 가능 파일:

```text
PerformanceEngine
MainThreadConfidenceAnalyzer
관련 테스트
정적 검사 스크립트
```

수정 금지 파일:

```text
SchedulerController mutation 경로
RollbackManager
ApplyGuard
BackgroundController
```

구현 규칙:

```text
PerformanceEngine은 판단 계층이다.
PerformanceEngine은 Win32 mutation을 수행하지 않는다.
PerformanceEngine은 SchedulerController를 직접 호출하지 않는다.
PerformanceEngine은 confidence gate 실패 시 monitoring-only 또는 reject reason을 제공한다.
```

완료 기준:

```text
PerformanceEngine이 confidence result를 기반으로 policy eligibility를 판단할 수 있다.
mutation은 여전히 Controller Layer에만 존재한다.
confidence gate 결과가 Evidence로 기록된다.
```

필수 테스트:

```text
performance engine no Win32 mutation static check
performance engine no scheduler direct call test
confidence result consumed test
policy candidate gated by confidence test
```

필수 Evidence:

```text
performanceEngineConfidenceInput
policyGateReason
confidenceGateResult
mutationBoundaryCheckResult
```

리스크:

```text
PerformanceEngine 통합 시 policy candidate 생성 책임과 confidence 판단 책임이 섞일 수 있다.
SchedulerController 직접 호출이 생기면 safety architecture를 위반한다.
```

Rollback 계획:

```text
PerformanceEngine 통합이 위험하면 analyzer result를 diagnostic evidence로만 남기고 policy gate 연결은 Phase 3로 이동한다.
정적 검사에서 mutation boundary 위반이 발견되면 즉시 해당 연결을 제거한다.
```

## 15. P2-10 Phase 2 Regression Test & Evidence

Patch ID: `P2-10`

목적:

Phase 2 패치 묶음의 회귀 테스트와 Evidence 기준을 정리한다.

작업 범위:

```text
Phase 2 test list 작성
Phase 2 static check list 작성
Phase 2 runtime evidence list 작성
Phase 2 release blocker list 작성
RegressionMatrix_v3.1 연결
```

수정 가능 파일:

```text
docs/validation/RegressionMatrix_v3.1.md
docs/release/ReleaseChecklist_v3.1.md
validation scripts if existing
tests/*
```

수정 금지 파일:

```text
SchedulerController mutation behavior
RollbackManager behavior
ApplyGuard behavior
AggressiveSingleCoreController
```

구현 규칙:

```text
Phase 2 PASS는 정책 적용 성공이 아니다.
Phase 2 PASS는 confidence 판단 체계가 안전하게 준비되었다는 의미다.
Regression test는 confidence classification, insufficient evidence, policy eligibility, evidence, RuntimeValidation을 분리해 기록한다.
```

완료 기준:

```text
Phase 2 regression checklist가 존재한다.
Phase 2 blocker 조건이 ReleaseChecklist와 연결된다.
Phase 2 Evidence Bundle 최소 항목이 정의된다.
```

필수 테스트:

```text
MainThreadConfidenceAnalyzer classification suite
insufficient evidence suite
policy eligibility suite
confidence evidence suite
RuntimeValidation confidence suite
```

필수 Evidence:

```text
phase2RegressionSummary
confidenceClassificationSummary
policyEligibilitySummary
confidenceEvidenceSummary
runtimeValidationSummary
```

리스크:

```text
RegressionMatrix_v3.1.md가 아직 없으면 테스트 목록이 문서상 계획에 머물 수 있다.
ReleaseChecklist_v3.1.md 변경이 과도하면 릴리즈 기준과 Phase 2 기준이 섞일 수 있다.
```

Rollback 계획:

```text
신규 regression 문서가 준비되지 않았으면 PatchPlan_Phase2.md의 Evidence 기준을 우선 source of truth로 유지한다.
ReleaseChecklist 수정은 Phase 2 blocker 항목만 최소 반영한다.
```

## 16. Phase 2 완료 기준

Phase 2 전체 완료 기준은 다음과 같다.

1. MainThreadConfidenceAnalyzer의 책임과 비책임이 명확하다.
2. Confidence input model이 정의되어 있다.
3. Low / Medium / High / VeryHigh confidence level이 계산된다.
4. Confidence reason과 downgrade reason이 기록된다.
5. Insufficient evidence와 monitoring-only reason이 제공된다.
6. Confidence < High 상태에서 affinity/priority eligibility가 차단된다.
7. VeryHigh는 aggressive mode 후보일 뿐, 즉시 활성화가 아니다.
8. Confidence history가 Evidence로 기록된다.
9. RuntimeValidation이 confidence inconsistency를 감지할 수 있다.
10. PerformanceEngine은 confidence result를 소비하되 직접 mutation하지 않는다.
11. Phase 2 regression test가 정의되어 있다.

완료 판단은 정책 적용 성공이 아니라 confidence 판단 체계의 안전성과 evidence 가능성을 기준으로 한다.

## 17. Phase 2 BLOCKER 조건

다음 조건은 Phase 2 BLOCKER로 분류한다.

```text
MainThreadConfidenceAnalyzer에서 Win32 mutation 호출
PerformanceEngine에서 Win32 mutation 호출
PerformanceEngine이 SchedulerController 직접 호출
Confidence < High인데 affinity/priority eligibility true
VeryHigh만으로 aggressive mode 즉시 활성화
insufficient evidence 상태에서 policy apply 후보 생성
confidence reason 누락
RuntimeValidation이 confidence inconsistency를 무시
Phase 2 Evidence 누락
```

BLOCKER가 발생하면 후속 patch보다 해당 blocker 제거를 우선한다. 특히 P2-06과 P2-09의 BLOCKER는 Phase 2 전체 진행을 차단한다.

## 18. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P2-01 Confidence Contract Alignment
2. P2-02 Confidence Input Model
3. P2-03 Confidence Level Classification
4. P2-04 Confidence Reason & Downgrade Reason
5. P2-05 Insufficient Evidence / Monitoring-only Reason
6. P2-06 Policy Eligibility Output
7. P2-07 Confidence History Evidence
8. P2-08 RuntimeValidation Confidence Checks
9. P2-09 PerformanceEngine Integration Boundary
10. P2-10 Phase 2 Regression Test & Evidence
```

주의:

```text
P2-02와 P2-06은 Phase 2의 핵심 계약 패치다.
P2-06에서 Confidence < High 상태의 affinity/priority eligibility가 열리면 즉시 수정한다.
P2-09에서 PerformanceEngine이 mutation owner를 침범하면 Phase 2를 중단한다.
```

## 19. 패치 작성 규칙

각 실제 코드 패치는 다음 규칙을 따른다.

1. `[PATCH]` 모드로 작성한다.
2. 변경 전 실제 헤더 시그니처를 확인한다.
3. 인터페이스 변경 시 선언, 정의, 호출부, 테스트를 모두 수정한다.
4. `std::expected`는 Assign -> Check -> Bind 패턴을 따른다.
5. expected 결과를 검사 없이 `return func(...)` 형태로 전파하지 않는다.
6. Windows header include 순서를 깨지 않는다.
7. main/Test build target을 분리한다.
8. B9 Gate 검증 결과를 기록한다.
9. BEFORE/AFTER 코드 문맥을 제공한다.
10. Conventional Commit 메시지를 제안한다.

패치 작성자는 Phase 2 범위를 벗어난 mutation, rollback behavior 변경, aggressive mode 구현을 같은 패치에 섞지 않는다.

## 20. Non-Goals

다음은 PatchPlan_Phase2.md의 비목표(Non-Goals)다.

```text
구체 C++ 코드 작성
패치 파일 직접 생성
테스트 코드 직접 작성
성능 개선 수치 보장
SchedulerController mutation 변경
RollbackManager 변경
ApplyGuard 변경
Aggressive Single-Core Mode 구현
GameProfileRegistry 구현
Release Gate 자동화
```

Phase 2는 메인 스레드 신뢰도(Main Thread Confidence) 판단 체계를 고정하는 문서다.

## 21. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 2의 목적과 비목표가 명확하다.
2. Phase 2 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P2-01부터 P2-10까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 완료 기준, 테스트, Evidence가 정의되어 있다.
5. Confidence Level과 Policy Eligibility의 관계가 명확하다.
6. Confidence < High에서 affinity/priority eligibility가 차단된다.
7. VeryHigh와 Aggressive Mode 즉시 활성화가 구분된다.
8. Phase 2 BLOCKER 조건이 정의되어 있다.
9. 패치 순서가 정의되어 있다.
10. 후속 `docs/implementation/PatchPlan_Phase3.md` 작성에 필요한 기준이 충분하다.

## 22. Open Questions

다음 질문은 Phase 2 실제 패치 착수 전 또는 P2-01에서 분류한다.

```text
1. MainThreadConfidenceAnalyzer는 신규 파일로 분리할 것인가, PerformanceEngine 내부 하위 모듈로 둘 것인가?
2. Confidence score는 수치형으로 둘 것인가, bucket-only로 둘 것인가?
3. Low / Medium / High / VeryHigh threshold는 v3.1에서 확정할 것인가, TBD로 둘 것인가?
4. Confidence history 보관 길이는 몇 cycle로 둘 것인가?
5. migration count와 candidate switch count의 가중치는 어느 문서에서 확정할 것인가?
6. access failure가 반복될 때 confidence 강등 기준은 어디서 확정할 것인가?
7. VeryHigh confidence의 최소 유지 시간은 얼마로 둘 것인가?
8. Phase 2 완료 전 실제 게임 dry-run을 수행할 것인가?
```

Open Questions가 해결되기 전까지 관련 구현 범위와 threshold는 `TBD`로 유지한다. 단, MainThreadConfidenceAnalyzer에 mutation 책임을 부여하지 않는 것과 Confidence < High에서 affinity/priority 가능 판정을 허용하지 않는 것은 Open Question이 아니라 Phase 2 안전 계약이다.
