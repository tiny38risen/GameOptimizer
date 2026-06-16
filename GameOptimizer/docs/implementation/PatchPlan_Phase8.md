# GameOptimizer v3.1 Patch Plan — Phase 8: Performance Validation Flow

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 8

버전: v1.0

작성 목적: GameOptimizer v3.1의 여덟 번째 구현 단계인 Phase 8 — Performance Validation Flow를 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, 성능 검증 흐름(Performance Validation Flow)을 기준 구간(Baseline Window), 적용 구간(Applied Window), 전후 비교(Before/After Comparison), 성능 분류(Performance Classification), 회귀 판정(Regression Detection), 장시간 검증(Long Soak Validation), 릴리즈 증거(Release Evidence) 연결 단위로 정리하는 작업 계획 문서다.

적용 범위:

- Baseline Window 수집 기준 정의
- Applied Window 수집 기준 정의
- Before/After Comparison 기준 정의
- metric normalization과 completeness 기준 정의
- Performance Summary 생성 기준 정의
- Classification Result 생성 기준 정의
- Regression Detection 기준 정의
- Safety Override Rule 정의
- dry-run / soft-apply / apply validation mode별 제한 정의
- Long Soak Validation 기준 정의
- Actual Game Validation Recording 기준 정의
- RC Evidence Bundle 및 RC Report 연결 기준 정의

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 새로운 성능 정책 추가
- Aggressive Single-Core Mode 구현
- SchedulerController mutation 경로 변경
- RollbackManager rollback execution 변경
- GameProfileRegistry 구조 변경
- 외부 FPS/frame time 측정 도구 직접 구현
- UI 성능 리포트 구현
- 공식 지원 게임 목록 확정
- 성능 향상 보장 문구 작성
- 자동 벤치마크 도구 전체 구현

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/implementation/PatchPlan_Phase1.md`
- `docs/implementation/PatchPlan_Phase2.md`
- `docs/implementation/PatchPlan_Phase3.md`
- `docs/implementation/PatchPlan_Phase4.md`
- `docs/implementation/PatchPlan_Phase5.md`
- `docs/implementation/PatchPlan_Phase6.md`
- `docs/implementation/PatchPlan_Phase7.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/performance/GameProfileSpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-009_RuntimeOrchestrator.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`

후속 문서:

- `docs/implementation/PatchPlan_Phase9.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 8의 목표는 성능 향상을 보장하는 것이 아니라, 성능 변화가 실제로 관측되었는지, 안전성 위반 없이 재현 가능한 Evidence로 설명 가능한지 검증하는 것이다.

## 2. Phase 8 목표

Phase 8의 핵심 목표는 성능 주장을 릴리즈 가능한 Evidence로 검증하는 것이다. 성능 변화가 좋아 보이는지보다, 동일한 target identity, 동일한 profile, 동일한 validation mode에서 Baseline Window와 Applied Window가 존재하고, 그 전후 비교가 안전성 결과와 충돌하지 않는지가 중요하다.

필수 목표:

1. Baseline Window 수집 기준 정의
2. Applied Window 수집 기준 정의
3. Before/After Comparison 기준 정의
4. Performance Summary 생성 기준 정의
5. Classification Result 생성 기준 정의
6. Regression Detection 기준 정의
7. Rollback / RuntimeValidation 결과와 성능 판정 연결
8. Long Soak Validation 기준 정의
9. Actual Game Validation 기록 기준 정의
10. RC Evidence Bundle 연결 기준 정의

핵심 원칙:

1. Baseline Window 없이 성능 개선 판정 금지.
2. Applied Window 없이 성능 개선 판정 금지.
3. Before/After Comparison 없이 improvement 판정 금지.
4. RuntimeValidation BLOCKER가 있으면 PASS 금지.
5. Rollback failure가 있으면 PASS 금지.
6. Evidence fatal missing이 있으면 PASS 금지.
7. NEUTRAL은 실패가 아니지만 성능 향상 주장도 아니다.
8. REGRESSION은 안전성과 별개로 성능 회귀로 기록한다.
9. 실제 게임 검증은 Candidate Profile 검증 자료일 뿐 공식 지원 선언이 아니다.
10. 외부 FPS/frame time 측정값이 없으면 선택 지표 누락 또는 WARN으로 분류한다.

정량 기준 후보:

- Baseline Window 유효성은 최소 시간 `TBD`와 최소 cycle 수 `TBD`를 모두 만족해야 한다.
- Applied Window 유효성은 최소 시간 `TBD`, 최소 cycle 수 `TBD`, apply/verify 결과 연결을 모두 만족해야 한다.
- `RuntimeValidation BLOCKER`, `rollback failure`, `evidence fatal missing` 중 하나라도 있으면 final PASS count는 0이어야 한다.
- Before/After Comparison 없이 `performanceImprovementClaimAllowed=true`가 생성되면 BLOCKER로 분류한다.
- 외부 FPS/frame time metric은 v3.1에서 필수 여부가 확정될 때까지 optional metric 후보로 두고, 누락 시 WARN 후보로 분류한다.

## 3. Phase 8 비목표

Phase 8은 성능 정책을 추가하는 단계가 아니라, 이미 적용된 정책의 결과를 안전성·증거·전후 비교 기준으로 판정하는 단계다.

비목표:

1. 새로운 성능 정책 추가
2. Aggressive Single-Core Mode 구현
3. SchedulerController mutation 경로 변경
4. RollbackManager rollback execution 변경
5. GameProfileRegistry 구조 변경
6. 외부 FPS/frame time 측정 도구 직접 구현
7. UI 성능 리포트 구현
8. 공식 지원 게임 목록 확정
9. 성능 향상 보장 문구 작성
10. 자동 벤치마크 도구 전체 구현

Phase 8에서 성능 결과가 좋아 보이더라도 안전성 blocker, rollback failure, evidence fatal missing이 존재하면 PASS 또는 PASS_WITH_WARNINGS로 분류하지 않는다.

## 4. 영향 모듈

### 4.1 직접 영향 모듈

```text
PerformanceEvidenceRecorder
PerformanceEvidencePlanner
RuntimeSnapshotRecorder
PolicyTimelineRecorder
RuntimeValidationMonitor
EvidenceCompletenessValidator
Classification module
RCReportGenerator
Validation scripts
```

### 4.2 간접 영향 모듈

```text
ThreadTracker
MainThreadConfidenceAnalyzer
TopologyAnalyzer
PerformanceEngine
PolicyDispatcher
SchedulerController
RollbackManager
GameProfileRegistry
RuntimeOrchestrator
ShutdownPipeline
```

### 4.3 변경 금지 또는 최소 변경 모듈

```text
ThreadTracker core sampling logic
TopologyAnalyzer topology detection logic
SchedulerController apply internals
RollbackManager rollback execution internals
ApplyGuard transaction internals
GameProfileRegistry lookup internals
AggressiveSingleCoreController
```

Phase 8에서 성능 검증 때문에 core behavior를 바꿔야 한다면 해당 Phase로 되돌려 분리한다.

## 5. 패치 단위 개요

Phase 8은 다음 Patch ID 구조를 사용한다.

```text
P8-01 Performance Validation Contract Alignment
P8-02 Baseline Window Collection
P8-03 Applied Window Collection
P8-04 Before / After Comparison
P8-05 Metric Normalization and Completeness
P8-06 Performance Summary Generation
P8-07 Classification Result Mapping
P8-08 Regression Detection
P8-09 Safety Override Rule
P8-10 Dry-run / Soft-apply / Apply Validation Modes
P8-11 Long Soak Validation Flow
P8-12 Actual Game Validation Recording
P8-13 RC Evidence and Report Integration
P8-14 Phase 8 Regression Test and Evidence
```

각 Patch Unit은 다음 항목을 반드시 포함한다.

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

패치 하나가 performance metric collection, classification rule, release report override를 동시에 변경해서는 안 된다.

## 6. P8-01 Performance Validation Contract Alignment

Patch ID: `P8-01`

목적:

PerformanceValidationPlan과 EvidenceSpecification 기준으로 성능 검증의 입력, 출력, 판정 기준을 정렬한다.

작업 범위:

```text
Baseline Window 필수성 확인
Applied Window 필수성 확인
Before/After Comparison 필수성 확인
Safety Override Rule 확인
Performance Summary 필드 매핑
Classification Result 필드 매핑
RC Report 연결 필드 확인
metric owner 확인
```

수정 가능 파일:

```text
docs/implementation/PatchPlan_Phase8.md
docs/implementation/MIGRATION_NOTES_Phase8.md
Performance validation 관련 문서
PerformanceEvidencePlanner placeholder
EvidenceSpecification 관련 performance field mapping
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager internals
ThreadTracker sampling internals
GameProfileRegistry internals
```

구현 규칙:

```text
성능 검증 필드는 생성 주체와 소비 주체를 가져야 한다.
소유 모듈이 불명확한 metric은 필수 지표로 확정하지 않는다.
Safety Override Rule은 성능 metric보다 높은 우선순위를 가진다.
Performance Summary는 원본 Evidence field와 traceable해야 한다.
```

완료 기준:

```text
Phase 8에서 생성해야 할 성능 Evidence 항목과 소유 모듈이 명확하다.
성능 판정에 필요한 입력 누락 기준이 식별된다.
classification input과 RC report input이 연결된다.
```

필수 테스트:

```text
performance validation contract review test
metric owner map review test
classification input field review test
rc performance field mapping review test
safety override priority review test
```

필수 Evidence:

```text
performanceValidationContractSummary
performanceMetricOwnerMap
classificationInputFieldMap
rcPerformanceFieldMap
blockingQuestionList
```

리스크:

```text
metric owner가 불명확하면 Performance Summary가 원본 Evidence 없이 계산될 수 있다.
Safety Override 우선순위가 contract에 없으면 성능 개선 수치가 안전성 실패를 덮을 수 있다.
```

Rollback 계획:

```text
소유 모듈이 불명확한 metric은 optional 또는 TBD로 낮춘다.
contract alignment가 완료될 때까지 classification은 INVALID_RUN 또는 BLOCKER 후보로 제한한다.
```

## 7. P8-02 Baseline Window Collection

Patch ID: `P8-02`

목적:

정책 적용 전 성능 기준 구간인 Baseline Window 수집 기준을 정의한다.

작업 범위:

```text
baseline start condition
baseline end condition
minimum baseline duration
baseline cycle count
main thread confidence during baseline
migration count during baseline
RTT jitter during baseline
DPC spike during baseline
policy activation disabled state 확인
baseline identity and profile snapshot
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
RuntimeSnapshotRecorder
RuntimeOrchestrator validation point
EvidenceCompletenessValidator
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
PolicyDispatcher mutation dispatch semantics
ThreadTracker core sampling logic
TopologyAnalyzer topology detection logic
```

구현 규칙:

```text
Baseline Window 중 mutation policy를 적용하지 않는다.
Baseline Window가 없으면 improvement classification을 생성하지 않는다.
Baseline Window가 부족하면 INVALID_RUN 또는 WARN으로 분류한다.
baseline target identity와 profile은 comparison input으로 저장한다.
```

완료 기준:

```text
baselineWindow가 성능 비교 입력으로 기록된다.
baselineWindow의 유효성 여부가 Evidence에 포함된다.
baseline 중 mutation absence가 검증된다.
```

필수 테스트:

```text
baseline window generated test
baseline minimum duration test
baseline mutation absence test
missing baseline blocks improvement test
baseline metric completeness test
baseline identity recorded test
```

필수 Evidence:

```text
baselineWindowStartUtc
baselineWindowEndUtc
baselineCycleCount
baselineMainThreadConfidence
baselineMigrationCount
baselineRttJitter
baselineDpcSpikeCount
baselineTargetIdentity
baselineProfileId
baselineValid
baselineInvalidReason
```

리스크:

```text
baseline 없이 성능 개선 판정을 만들면 정책 적용 전 상태와 비교할 수 없다.
baseline 중 mutation이 발생하면 비교 기준이 오염된다.
```

Rollback 계획:

```text
baseline collection이 불완전하면 improvement classification을 금지한다.
baseline validity가 불명확하면 INVALID_RUN으로 제한한다.
```

BLOCKER 기준:

```text
Baseline Window 없이 improvement classification 생성
Baseline 중 mutation policy 적용
baseline validity 누락
```

## 8. P8-03 Applied Window Collection

Patch ID: `P8-03`

목적:

정책 적용 후 Applied Window 수집 기준을 정의한다.

작업 범위:

```text
applied window start condition
applied window end condition
policy apply result 확인
verify result 확인
active policy list 기록
main thread confidence during applied window
migration count during applied window
RTT jitter during applied window
DPC spike during applied window
rollback readiness 확인
applied identity and profile snapshot
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
RuntimeSnapshotRecorder
PolicyTimelineRecorder consumer
SchedulerController evidence consumer
EvidenceCompletenessValidator
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback execution internals
ApplyGuard transaction internals
ThreadTracker sampling internals
```

구현 규칙:

```text
Applied Window는 apply/verify 결과와 연결되어야 한다.
verify 실패 상태에서 Applied Window를 성공 구간으로 기록하지 않는다.
Rollback readiness가 없는 Applied Window는 PASS 후보가 아니다.
active policy list는 comparison과 rollback summary에서 참조 가능해야 한다.
```

완료 기준:

```text
appliedWindow가 성능 비교 입력으로 기록된다.
적용된 policy 목록과 verify 결과가 연결된다.
rollback readiness 상태가 Evidence에 남는다.
```

필수 테스트:

```text
applied window generated test
applied window requires policy apply test
verify failure blocks valid applied window test
active policy list recorded test
applied metric completeness test
rollback readiness required test
```

필수 Evidence:

```text
appliedWindowStartUtc
appliedWindowEndUtc
appliedCycleCount
appliedPolicyList
appliedVerifyResult
appliedMainThreadConfidence
appliedMigrationCount
appliedRttJitter
appliedDpcSpikeCount
appliedRollbackReady
appliedTargetIdentity
appliedProfileId
appliedValid
appliedInvalidReason
```

리스크:

```text
verify 실패 구간을 applied success로 기록하면 성능 비교가 거짓 성공처럼 보인다.
active policy list가 없으면 어떤 정책의 결과인지 추적할 수 없다.
```

Rollback 계획:

```text
Applied Window 연결이 불완전하면 improvement classification을 금지한다.
verify 결과가 없거나 rollback readiness가 없으면 INVALID_RUN 또는 BLOCKER 후보로 제한한다.
```

BLOCKER 기준:

```text
Applied Window 없이 improvement classification 생성
verify 실패 구간을 successful applied window로 기록
active policy list 누락
```

## 9. P8-04 Before / After Comparison

Patch ID: `P8-04`

목적:

Baseline Window와 Applied Window를 비교하여 성능 변화 요약을 생성한다.

작업 범위:

```text
confidence 변화 비교
migration count 변화 비교
thread switch count 변화 비교
RTT jitter 변화 비교
DPC spike 변화 비교
policy activation 효과 비교
external FPS/frame time optional comparison
comparison confidence 수준 기록
target identity / profile / mode match 확인
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
PerformanceEvidencePlanner
classification module
EvidenceCompletenessValidator
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
Runtime policy mutation semantics
ThreadTracker sampling internals
GameProfileRegistry lookup internals
```

구현 규칙:

```text
Before/After Comparison은 같은 target identity, 같은 profile, 같은 mode 조건에서만 유효하다.
비교 조건이 다르면 INVALID_RUN 또는 WARN으로 분류한다.
개선/악화/중립을 수치 없이 단정하지 않는다.
metric 누락을 improvement로 처리하지 않는다.
```

완료 기준:

```text
comparisonSummary가 생성된다.
각 metric별 improved / neutral / regressed / missing 상태가 기록된다.
comparison validity가 Evidence에 남는다.
```

필수 테스트:

```text
before after comparison generated test
different target identity invalid comparison test
missing metric comparison warning test
migration improved comparison test
rtt jitter regressed comparison test
neutral comparison test
comparison without baseline blocked test
comparison without applied blocked test
```

필수 Evidence:

```text
comparisonSummary
comparisonTargetIdentityMatch
comparisonProfileMatch
comparisonModeMatch
metricComparisonResults
comparisonConfidence
comparisonValid
comparisonInvalidReason
```

리스크:

```text
비교 조건이 다르면 정책 효과와 환경 변화를 구분할 수 없다.
metric 누락을 개선으로 처리하면 성능 주장이 왜곡된다.
```

Rollback 계획:

```text
comparison validity를 보장할 수 없으면 improvement claim을 금지한다.
identity, profile, mode 중 하나라도 불일치하면 INVALID_RUN으로 제한한다.
```

BLOCKER 기준:

```text
Baseline/Applied 조건 불일치 무시
comparison 없이 improvement classification
metric 누락을 improvement로 처리
```

## 10. P8-05 Metric Normalization and Completeness

Patch ID: `P8-05`

목적:

성능 지표 단위와 누락 상태를 일관되게 처리한다.

작업 범위:

```text
migration count 단위 정의
thread switch count 단위 정의
RTT jitter 단위 정의
DPC spike count 단위 정의
confidence history 단위 정의
cycle 기준 정규화
missing metric 처리
optional metric 처리
unit mismatch 처리
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
EvidenceCompletenessValidator
classification module
PerformanceEvidencePlanner
관련 테스트
```

수정 금지 파일:

```text
ThreadTracker core sampling logic
MainThreadConfidenceAnalyzer scoring internals
TopologyAnalyzer detection internals
SchedulerController apply internals
```

구현 규칙:

```text
서로 다른 측정 단위를 직접 비교하지 않는다.
필수 metric 누락은 BLOCKER 또는 INVALID_RUN 후보로 분류한다.
선택 metric 누락은 WARN으로 둘 수 있다.
외부 FPS/frame time metric은 필수 여부가 확정될 때까지 optional candidate로 둔다.
```

완료 기준:

```text
performance metric completeness가 판정된다.
필수/선택 지표 누락이 분리된다.
unit mismatch가 comparison validity에 반영된다.
```

필수 테스트:

```text
metric normalization test
missing required metric blocker test
missing optional metric warning test
unit mismatch invalid comparison test
cycle normalized comparison test
external fps missing optional warning test
```

필수 Evidence:

```text
metricCompletenessState
normalizedMetricList
missingRequiredMetrics
missingOptionalMetrics
unitMismatchList
metricCompletenessSeverity
externalMetricAvailability
```

리스크:

```text
정규화되지 않은 지표를 비교하면 회귀와 개선이 뒤섞인다.
optional metric 누락을 fatal로 처리하면 실제 사용 가능한 검증 흐름이 과도하게 막힐 수 있다.
```

Rollback 계획:

```text
unit mismatch가 있으면 해당 metric comparison을 invalid로 둔다.
필수 여부가 확정되지 않은 metric은 optional 또는 TBD로 유지한다.
```

## 11. P8-06 Performance Summary Generation

Patch ID: `P8-06`

목적:

성능 검증 결과를 `performance_summary`로 통합한다.

작업 범위:

```text
baselineWindow 요약
appliedWindow 요약
comparisonSummary 요약
confidenceHistory 요약
migrationSummary 요약
rttJitterSummary 요약
dpcSpikeSummary 요약
policyActivationSummary 요약
metricCompleteness 요약
Rollback Summary reference
RuntimeValidation Summary reference
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
RuntimeSnapshotRecorder
RC report input
EvidenceCompletenessValidator
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback execution internals
RuntimeValidationMonitor blocker semantics
성능 개선 판정 공식 임의 완화
```

구현 규칙:

```text
Performance Summary는 Rollback Summary와 RuntimeValidation Summary를 참조 가능해야 한다.
Performance Summary만으로 RC PASS를 결정하지 않는다.
comparison summary가 없으면 improvement claim을 생성하지 않는다.
summary는 원본 metric source field와 traceable해야 한다.
```

완료 기준:

```text
performance_summary가 생성된다.
RC Report에서 성능 검증 입력으로 사용할 수 있다.
Rollback Summary와 RuntimeValidation Summary reference가 포함된다.
```

필수 테스트:

```text
performance summary generated test
performance summary includes baseline test
performance summary includes applied window test
performance summary includes comparison test
performance summary references safety summaries test
performance summary traceability test
```

필수 Evidence:

```text
performanceSummaryPath
baselineSummary
appliedSummary
comparisonSummary
metricCompletenessState
rollbackSummaryReference
runtimeValidationSummaryReference
performanceSummaryReady
```

리스크:

```text
Performance Summary가 safety summary를 참조하지 않으면 성능 결과가 안전성 결과와 분리된다.
summary가 원본 Evidence 없이 자체 계산되면 RC review에서 추적이 어렵다.
```

Rollback 계획:

```text
summary 통합이 불안정하면 baseline, applied, comparison, metric completeness 최소 요약만 유지한다.
Rollback Summary 또는 RuntimeValidation Summary가 없으면 PASS를 차단한다.
```

BLOCKER 기준:

```text
performance summary 누락
comparison summary 누락
필수 metric 누락을 숨김
```

## 12. P8-07 Classification Result Mapping

Patch ID: `P8-07`

목적:

성능 검증 결과를 `PASS`, `PASS_WITH_WARNINGS`, `NEUTRAL`, `REGRESSION`, `BLOCKER`, `INVALID_RUN`으로 분류한다.

작업 범위:

```text
PASS 기준
PASS_WITH_WARNINGS 기준
NEUTRAL 기준
REGRESSION 기준
BLOCKER 기준
INVALID_RUN 기준
classification reason 기록
warning count 기록
blocker override 연결
improvement claim allowed flag 정의
```

수정 가능 파일:

```text
classification module
PerformanceEvidenceRecorder
EvidenceCompletenessValidator
RCReportGenerator input
관련 테스트
```

수정 금지 파일:

```text
RuntimeValidation BLOCKER semantics
RollbackManager failure semantics
SchedulerController apply internals
Release PASS 우회 rule
```

구현 규칙:

```text
성능 개선이 있어도 safety blocker가 있으면 PASS 금지.
NEUTRAL은 성능 향상 주장 금지.
INVALID_RUN은 비교 구간 또는 필수 identity가 성립하지 않을 때 사용한다.
classification reason 없이 final classification을 생성하지 않는다.
```

완료 기준:

```text
classificationResult가 일관된 기준으로 생성된다.
classificationReason이 Evidence에 기록된다.
performanceImprovementClaimAllowed가 classification과 일치한다.
```

필수 테스트:

```text
pass classification test
pass with warnings classification test
neutral classification test
regression classification test
invalid run classification test
blocker classification test
classification reason required test
neutral no improvement claim test
```

필수 Evidence:

```text
classificationResult
classificationReason
performanceImprovementClaimAllowed
warningCount
blockerCount
invalidRunReason
classificationInputSummary
```

리스크:

```text
NEUTRAL을 성능 개선으로 오해하면 release note가 과장될 수 있다.
INVALID_RUN 조건을 PASS로 처리하면 검증 자체가 성립하지 않는 실행이 승인될 수 있다.
```

Rollback 계획:

```text
classification input이 불완전하면 INVALID_RUN으로 제한한다.
classification reason 누락 시 PASS 또는 PASS_WITH_WARNINGS를 생성하지 않는다.
```

BLOCKER 기준:

```text
classification reason 누락
NEUTRAL에서 improvement claim 허용
INVALID_RUN 조건을 PASS로 처리
```

## 13. P8-08 Regression Detection

Patch ID: `P8-08`

목적:

성능 회귀와 정책 thrashing을 감지한다.

작업 범위:

```text
migration count regression
thread switch count regression
RTT jitter regression
DPC spike regression
confidence downgrade regression
policy activation thrashing
cooldown violation
rollback frequency abnormal
regression severity mapping
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
PolicyTimelineRecorder consumer
RuntimeValidationMonitor
classification module
관련 테스트
```

수정 금지 파일:

```text
PolicyDispatcher mutation semantics
SchedulerController apply internals
RollbackManager execution internals
ThreadTracker sampling internals
```

구현 규칙:

```text
회귀 지표가 명확하면 REGRESSION으로 분류한다.
policy thrashing은 단순 WARN이 아니라 BLOCKER 후보가 될 수 있다.
Regression이 있는 상태에서 성능 향상 주장 금지.
regression reason은 metric별로 기록한다.
```

완료 기준:

```text
regressionDetectionResult가 생성된다.
회귀 사유가 metric별로 기록된다.
policy thrashing과 cooldown violation이 classification input으로 연결된다.
```

필수 테스트:

```text
migration regression test
rtt jitter regression test
dpc spike regression test
confidence downgrade regression test
policy thrashing detection test
cooldown violation regression test
regression no improvement claim test
```

필수 Evidence:

```text
regressionDetectionResult
regressedMetrics
regressionReason
policyThrashingDetected
cooldownViolationDetected
rollbackFrequencyAbnormal
regressionSeverity
```

리스크:

```text
회귀 판정이 없으면 성능 개선이 일부 metric 악화를 덮을 수 있다.
policy thrashing을 WARN으로만 처리하면 long soak에서 불안정성이 누락될 수 있다.
```

Rollback 계획:

```text
regression threshold가 TBD이면 해당 metric을 improvement 근거로 사용하지 않는다.
policy thrashing 판단이 불명확하면 BLOCKER 후보로 제한한다.
```

BLOCKER 기준:

```text
policy thrashing 무시
명확한 regression 상태에서 PASS 생성
regression reason 누락
```

## 14. P8-09 Safety Override Rule

Patch ID: `P8-09`

목적:

성능 결과보다 안전성 결과가 우선하도록 classification override 규칙을 정의한다.

작업 범위:

```text
RuntimeValidation BLOCKER override
Rollback failure override
Evidence fatal missing override
ApplyGuard unresolved transaction override
active policy / rollback state mismatch override
shutdown cleanliness failure override
override reason mapping
```

수정 가능 파일:

```text
classification module
RuntimeValidationMonitor consumer
EvidenceCompletenessValidator
RCReportGenerator input
PerformanceEvidenceRecorder
관련 테스트
```

수정 금지 파일:

```text
RuntimeValidationMonitor blocker definition 임의 완화
RollbackManager failure definition 임의 완화
ApplyGuard transaction internals
SchedulerController apply internals
```

구현 규칙:

```text
성능 metric이 개선되어도 safety blocker가 있으면 PASS 또는 PASS_WITH_WARNINGS 금지.
RuntimeValidation BLOCKER, Rollback failure, Evidence fatal missing은 improvement보다 우선한다.
Safety Override가 적용되면 override reason을 Evidence에 기록한다.
override reason 없이 final PASS를 생성하지 않는다.
```

완료 기준:

```text
Safety Override Rule이 classification에 반영된다.
overrideApplied와 overrideReason이 기록된다.
finalClassificationAfterOverride가 생성된다.
```

필수 테스트:

```text
runtime blocker overrides improvement test
rollback failure overrides improvement test
evidence fatal missing overrides improvement test
apply guard unresolved overrides improvement test
shutdown failure overrides improvement test
override reason required test
```

필수 Evidence:

```text
safetyOverrideApplied
safetyOverrideReason
runtimeValidationBlockerCount
rollbackFailureCount
evidenceFatalMissingCount
applyGuardUnresolvedCount
shutdownCleanlinessResult
finalClassificationAfterOverride
```

리스크:

```text
Safety Override가 classification 이후에 누락되면 위험한 PASS가 생성될 수 있다.
override reason이 없으면 RC review에서 PASS 차단 근거를 확인할 수 없다.
```

Rollback 계획:

```text
Safety Override 판단이 불안정하면 safety blocker 존재 시 BLOCKER를 우선 적용한다.
override input이 불완전하면 PASS 또는 PASS_WITH_WARNINGS를 생성하지 않는다.
```

BLOCKER 기준:

```text
RuntimeValidation BLOCKER 상태에서 PASS 생성
Rollback failure 상태에서 PASS 생성
Evidence fatal missing 상태에서 PASS 생성
override reason 누락
```

## 15. P8-10 Dry-run / Soft-apply / Apply Validation Modes

Patch ID: `P8-10`

목적:

검증 모드별 성능 Evidence 생성 기준과 제한을 정의한다.

작업 범위:

```text
dry-run validation
soft-apply validation
apply validation
aggressive validation placeholder
mode-specific allowed classification
mode-specific evidence requirement
mode mismatch 처리
profile mode compatibility 확인
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
RuntimeContext
GameProfile consumer
classification module
EvidenceCompletenessValidator
관련 테스트
```

수정 금지 파일:

```text
GameProfileRegistry lookup internals
PolicyDispatcher mutation semantics
SchedulerController apply internals
AggressiveSingleCoreController implementation
```

구현 규칙:

```text
dry-run은 mutation 성능 개선 판정을 만들지 않는다.
soft-apply는 제한적 정책 검증으로 기록한다.
apply mode만 실제 policy effect comparison 후보가 될 수 있다.
aggressive validation은 Phase 9 이전에 placeholder로만 둔다.
mode mismatch는 INVALID_RUN 또는 WARN으로 기록한다.
```

완료 기준:

```text
검증 모드별 Evidence와 classification 제한이 적용된다.
mode mismatch가 INVALID_RUN 또는 WARN으로 기록된다.
aggressive validation placeholder가 실제 적용으로 해석되지 않는다.
```

필수 테스트:

```text
dry-run no improvement claim test
soft-apply limited classification test
apply validation comparison test
mode mismatch invalid run test
aggressive placeholder only test
profile mode compatibility test
```

필수 Evidence:

```text
validationMode
modeSpecificEvidenceRequirement
modeSpecificClassificationLimit
modeMismatchDetected
profileModeCompatibility
modeValidationResult
```

리스크:

```text
dry-run 결과를 mutation improvement로 해석하면 실제 정책 효과를 검증하지 못한다.
aggressive placeholder가 실제 기능처럼 기록되면 Phase 9 범위를 침범한다.
```

Rollback 계획:

```text
mode 판단이 불명확하면 dry-run 또는 soft-apply 결과를 improvement claim에서 제외한다.
aggressive mode는 Phase 9 전까지 placeholder evidence만 허용한다.
```

## 16. P8-11 Long Soak Validation Flow

Patch ID: `P8-11`

목적:

장시간 실행 중 안정성, evidence 지속성, rollback state 보존, policy thrashing 여부를 검증한다.

작업 범위:

```text
soak duration
heartbeat progression
snapshot freshness over time
policy activation count
policy rejection count
cooldown behavior
memory growth
CPU overhead
rollback state consistency
evidence flush progression
shutdown cleanliness
watchdog hang detection
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
PerformanceEvidenceRecorder
EvidenceCompletenessValidator
validation scripts if existing
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback execution internals
ApplyGuard transaction internals
ThreadTracker sampling internals
```

구현 규칙:

```text
Soak 테스트는 성능 개선 증명이 아니라 장시간 안정성 검증이다.
watchdog hang, snapshot freshness failure, timeline monotonicity failure는 BLOCKER 후보로 분류한다.
rollback state loss와 shutdown evidence flush failure는 PASS를 차단한다.
soak 기본 시간은 확정 전까지 TBD로 둔다.
```

완료 기준:

```text
soakValidationSummary가 생성된다.
장시간 실행 중 주요 안정성 지표가 기록된다.
soak blocker가 classification input으로 연결된다.
```

필수 테스트:

```text
soak heartbeat progression test
soak snapshot freshness test
soak policy thrashing test
soak evidence flush test
soak shutdown cleanliness test
soak memory growth warning test
soak rollback state consistency test
```

필수 Evidence:

```text
soakValidationSummary
soakDuration
heartbeatProgression
snapshotFreshnessOverTime
policyActivationCount
policyRejectionCount
memoryGrowthSummary
cpuOverheadSummary
rollbackStateConsistency
shutdownCleanliness
soakBlockerCount
```

리스크:

```text
짧은 검증만으로는 policy thrashing, evidence flush 실패, rollback state 손실을 놓칠 수 있다.
soak 결과를 성능 개선 증거로 과장하면 장시간 안정성 검증의 목적이 흐려진다.
```

Rollback 계획:

```text
soak automation이 준비되지 않았으면 manual runbook과 evidence checklist를 먼저 사용한다.
soak blocker가 하나라도 있으면 final classification을 BLOCKER 후보로 제한한다.
```

BLOCKER 기준:

```text
watchdog hang
snapshot freshness failure
timeline monotonicity failure
rollback state loss
shutdown evidence flush failure
```

## 17. P8-12 Actual Game Validation Recording

Patch ID: `P8-12`

목적:

실제 게임 검증 결과를 Candidate Profile 검증 자료로 기록한다.

작업 범위:

```text
target game display name
profileId
profileStatus
test scenario
test duration
baseline window
applied window
subjective notes
objective metrics
known limitations
validation result
official support claim guard
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
GameProfile evidence consumer
RCReportGenerator input
validation report template
관련 테스트
```

수정 금지 파일:

```text
GameProfileRegistry lookup internals
특정 게임 Validated profile 선언
공식 지원 게임 목록
Anti-Cheat 우회 정책
```

구현 규칙:

```text
실제 게임 검증은 공식 지원 선언이 아니다.
targetProcessName, antiCheatRisk, engineType이 검증되지 않았으면 TBD 또는 Unknown으로 둔다.
주관적 체감은 보조 자료이며 객관 지표를 대체하지 않는다.
객관 지표 없이 성능 개선을 확정하지 않는다.
```

완료 기준:

```text
actualGameValidationRecord가 생성된다.
Candidate Profile 검증 자료로 연결된다.
공식 지원 선언 금지 기준이 문서화된다.
```

필수 테스트:

```text
actual game validation record generated test
candidate profile validation evidence test
subjective note does not replace metric test
unknown target info remains TBD test
official support claim forbidden test
objective metric required for improvement claim test
```

필수 Evidence:

```text
actualGameValidationRecord
profileId
profileStatus
testScenario
testDuration
objectiveMetricSummary
subjectiveNote
knownLimitations
validationResult
officialSupportClaimPresent
```

리스크:

```text
실제 게임 검증 결과가 공식 지원 선언처럼 작성되면 Phase 7의 profile safety 원칙과 충돌한다.
주관적 체감을 객관 지표 대신 사용하면 성능 검증의 재현성이 사라진다.
```

Rollback 계획:

```text
target 정보가 검증되지 않았으면 TBD 또는 Unknown을 유지한다.
objective metric이 없으면 improvement claim을 금지하고 Candidate Profile 보조 자료로만 기록한다.
```

BLOCKER 기준:

```text
실제 게임 검증을 공식 지원 선언으로 기록
TBD 정보를 확정값으로 위장
객관 지표 없이 성능 개선 확정
```

## 18. P8-13 RC Evidence and Report Integration

Patch ID: `P8-13`

목적:

Performance Validation 결과를 RC Evidence Bundle과 RC Report 입력으로 연결한다.

작업 범위:

```text
performance summary input
classification result input
regression result input
soak validation input
actual game validation input
safety override input
RC final decision input
source field traceability
```

수정 가능 파일:

```text
RCReportGenerator
EvidenceCompletenessValidator
PerformanceEvidenceRecorder
release scripts if existing
관련 테스트
```

수정 금지 파일:

```text
RuntimeValidation BLOCKER semantics
Rollback failure semantics
성능 개선 PASS 기준 임의 완화
원본 Evidence 없이 RC 판정 생성하는 fallback
```

구현 규칙:

```text
RC Report는 원본 Evidence 없이 성능 판정을 생성하지 않는다.
Performance Summary와 Classification Result는 traceable해야 한다.
Safety Override 적용 결과가 RC Report에 반영되어야 한다.
missing performance summary는 PASS 후보가 아니다.
```

완료 기준:

```text
RC Report에서 성능 검증 결과와 안전성 override 결과를 확인할 수 있다.
performance evidence source field mapping이 가능하다.
RC final decision input이 classification after override를 참조한다.
```

필수 테스트:

```text
rc report performance input test
rc report classification input test
rc report safety override input test
rc report missing performance summary blocker test
rc report field traceability test
rc final decision uses override test
```

필수 Evidence:

```text
rcPerformanceInputBundle
rcPerformanceSourceFieldMap
rcClassificationInput
rcSafetyOverrideInput
rcPerformanceReportReady
rcFinalDecisionPerformanceReference
```

리스크:

```text
RC Report가 원본 Evidence 없이 성능 판정을 생성하면 검증과 보고가 분리된다.
Safety Override가 report에 반영되지 않으면 안전성 실패 상태에서 PASS처럼 보일 수 있다.
```

Rollback 계획:

```text
RC integration이 불안정하면 source field map과 input bundle까지만 생성한다.
원본 Evidence가 없으면 rcPerformanceReportReady=false를 유지한다.
```

## 19. P8-14 Phase 8 Regression Test and Evidence

Patch ID: `P8-14`

목적:

Phase 8 패치 묶음의 회귀 테스트와 Evidence 기준을 정리한다.

작업 범위:

```text
Phase 8 test list 작성
Phase 8 static check list 작성
Phase 8 runtime evidence list 작성
Phase 8 release blocker list 작성
RegressionMatrix_v3.1 연결
ReleaseChecklist_v3.1 연결
RC_Runbook_v3.1 연결
```

수정 가능 파일:

```text
docs/validation/RegressionMatrix_v3.1.md
docs/release/ReleaseChecklist_v3.1.md
docs/release/RC_Runbook_v3.1.md
validation scripts if existing
tests/*
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback execution internals
GameProfileRegistry internals
성능 개선 수치 보장 문구
```

구현 규칙:

```text
Phase 8 PASS는 성능 향상 PASS가 아니다.
Phase 8 PASS는 성능 검증 흐름이 Evidence와 safety override 기준을 만족한다는 의미다.
RegressionMatrix는 safety override와 comparison blocker를 포함해야 한다.
```

완료 기준:

```text
Phase 8 regression checklist가 존재한다.
Phase 8 BLOCKER 조건이 ReleaseChecklist와 RC Runbook에 연결된다.
후속 Phase 9 작성에 필요한 evidence 기준이 남는다.
```

필수 테스트:

```text
Baseline Window suite
Applied Window suite
Before/After Comparison suite
Metric Completeness suite
Classification Mapping suite
Regression Detection suite
Safety Override suite
Long Soak Validation suite
Actual Game Validation Record suite
RC Performance Evidence suite
```

필수 Evidence:

```text
phase8RegressionSummary
baselineAppliedWindowSummary
comparisonSummary
classificationSummary
regressionDetectionSummary
safetyOverrideSummary
soakValidationSummary
actualGameValidationSummary
runtimeValidationSummary
```

리스크:

```text
회귀 테스트가 comparison happy path에만 집중하면 safety override 실패를 놓칠 수 있다.
ReleaseChecklist에 Phase 8 blocker가 없으면 RC에서 잘못된 PASS가 생성될 수 있다.
```

Rollback 계획:

```text
별도 regression matrix가 준비되지 않았으면 PatchPlan_Phase8.md의 완료 기준을 source of truth로 유지한다.
ReleaseChecklist와 RC_Runbook 수정은 Phase 8 blocker 항목만 최소 반영한다.
```

## 20. Phase 8 완료 기준

Phase 8 전체 완료 기준은 다음과 같다.

1. Baseline Window 수집 기준이 정의되어 있다.
2. Applied Window 수집 기준이 정의되어 있다.
3. Before/After Comparison 기준이 정의되어 있다.
4. metric normalization과 completeness 기준이 정의되어 있다.
5. Performance Summary가 생성된다.
6. Classification Result가 생성된다.
7. Regression Detection 기준이 정의되어 있다.
8. Safety Override Rule이 classification에 반영된다.
9. dry-run / soft-apply / apply mode별 검증 제한이 정의되어 있다.
10. Long Soak Validation 기준이 정의되어 있다.
11. Actual Game Validation Recording 기준이 정의되어 있다.
12. RC Evidence와 Report 연결 기준이 정의되어 있다.
13. Phase 8 regression test가 정의되어 있다.

완료 기준은 성능 향상 보장이 아니라 성능 변화, 안전성, rollback, evidence completeness를 함께 판정할 수 있는 재현 가능한 검증 흐름이다.

## 21. Phase 8 BLOCKER 조건

다음 조건은 Phase 8 BLOCKER로 분류한다.

```text
Baseline Window 없이 improvement classification 생성
Applied Window 없이 improvement classification 생성
Before/After Comparison 없이 improvement 판정
RuntimeValidation BLOCKER 상태에서 PASS 생성
Rollback failure 상태에서 PASS 생성
Evidence fatal missing 상태에서 PASS 생성
verify failure 구간을 successful Applied Window로 기록
policy thrashing 무시
metric 누락을 improvement로 처리
실제 게임 검증을 공식 지원 선언으로 기록
TBD 정보를 확정값으로 위장
RC Report가 원본 Evidence 없이 성능 판정 생성
```

BLOCKER는 성능 수치와 무관하게 release PASS를 차단한다.

## 22. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P8-01 Performance Validation Contract Alignment
2. P8-02 Baseline Window Collection
3. P8-03 Applied Window Collection
4. P8-04 Before / After Comparison
5. P8-05 Metric Normalization and Completeness
6. P8-06 Performance Summary Generation
7. P8-07 Classification Result Mapping
8. P8-08 Regression Detection
9. P8-09 Safety Override Rule
10. P8-10 Dry-run / Soft-apply / Apply Validation Modes
11. P8-11 Long Soak Validation Flow
12. P8-12 Actual Game Validation Recording
13. P8-13 RC Evidence and Report Integration
14. P8-14 Phase 8 Regression Test and Evidence
```

주의:

```text
P8-02, P8-03, P8-04, P8-07, P8-09는 Phase 8의 핵심 안전 패치다.
P8-09에서 safety override가 깨지면 Phase 8을 중단한다.
P8-12에서 실제 게임 검증을 공식 지원 선언처럼 기록하면 Phase 8을 중단한다.
```

## 23. 패치 작성 규칙

각 실제 코드 패치는 다음 규칙을 따른다.

```text
1. [PATCH] 모드로 작성한다.
2. 변경 전 실제 헤더 시그니처를 확인한다.
3. 인터페이스 변경 시 선언, 정의, 호출부, 테스트를 모두 수정한다.
4. std::expected는 Assign → Check → Bind 패턴을 따른다.
5. expected 결과를 검사 없이 return func(...) 형태로 전파하지 않는다.
6. Windows header include 순서를 깨지 않는다.
7. main/Test build target을 분리한다.
8. B9 Gate 검증 결과를 기록한다.
9. BEFORE/AFTER 코드 문맥을 제공한다.
10. Conventional Commit 메시지를 제안한다.
11. 성능 개선 판정은 Evidence source field와 연결한다.
12. Safety Override 적용 여부를 classification에 반영한다.
```

문서 작성 규칙:

```text
1. 구현 코드를 작성하지 않는다.
2. 구체 함수 시그니처를 확정하지 않는다.
3. 확정되지 않은 수치는 TBD로 둔다.
4. 성능 향상을 보장하지 않는다.
5. 특정 게임을 공식 지원한다고 쓰지 않는다.
6. Anti-Cheat 우회 정책을 작성하지 않는다.
7. Baseline/Applied Window 없이 improvement 판정을 허용하지 않는다.
8. RuntimeValidation BLOCKER가 있으면 PASS로 분류하지 않는다.
9. Rollback failure가 있으면 PASS로 분류하지 않는다.
10. 실제 게임 검증을 공식 지원 선언으로 작성하지 않는다.
11. Markdown 형식으로 작성한다.
```

## 24. Non-Goals

다음은 PatchPlan_Phase8.md의 비목표다.

```text
구체 C++ 코드 작성
패치 파일 직접 생성
테스트 코드 직접 작성
성능 개선 수치 보장
새로운 최적화 정책 추가
Aggressive Single-Core Mode 구현
외부 FPS/frame time 측정 도구 구현
공식 지원 게임 목록 확정
GameProfileRegistry 구조 변경
SchedulerController mutation 경로 변경
RollbackManager 내부 변경
UI 리포트 구현
자동 배포 구현
```

Non-Goals를 위반하는 변경은 Phase 8 범위 밖이며 별도 승인 문서가 필요하다.

## 25. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 8의 목적과 비목표가 명확하다.
2. Phase 8 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P8-01부터 P8-14까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 구현 규칙, 완료 기준, 테스트, Evidence, 리스크, Rollback 계획이 정의되어 있다.
5. Baseline / Applied Window / Before-After Comparison 기준이 명확하다.
6. Classification Result와 Safety Override Rule의 관계가 명확하다.
7. Regression Detection과 Long Soak Validation 기준이 정의되어 있다.
8. Actual Game Validation Recording이 공식 지원 선언이 아님을 명확히 한다.
9. Phase 8 BLOCKER 조건이 정의되어 있다.
10. 패치 순서가 정의되어 있다.
11. 후속 PatchPlan_Phase9.md 작성에 필요한 기준이 충분하다.

Approval Criteria:

1. Baseline Window, Applied Window, Before/After Comparison 없이 improvement claim을 허용하는 문장이 없어야 한다.
2. RuntimeValidation BLOCKER, Rollback failure, Evidence fatal missing이 PASS를 차단한다는 기준이 명확해야 한다.
3. NEUTRAL이 성능 향상 주장이 아님을 분명히 해야 한다.
4. 실제 게임 검증이 Candidate Profile 검증 자료이며 공식 지원 선언이 아님을 분명히 해야 한다.
5. 확정되지 않은 threshold, duration, cycle count는 `TBD`로 남아 있어야 한다.

## 26. Open Questions

### Blocking

1. Baseline Window의 최소 시간과 최소 cycle 수는 얼마로 둘 것인가?
2. Applied Window의 최소 시간과 최소 cycle 수는 얼마로 둘 것인가?
3. RTT jitter와 DPC spike의 regression threshold는 고정값인가, profile별 값인가?
4. Main Thread Migration 감소를 improvement로 인정하는 최소 기준은 무엇인가?
5. NEUTRAL을 RC 허용으로 둘 것인가?
6. Classification Result는 runtime 중 생성할 것인가, shutdown 후 RC report 단계에서 생성할 것인가?
7. Safety Override Rule은 RuntimeValidationMonitor가 소유할 것인가, Classification module이 소유할 것인가?

### Non-blocking

1. 실제 게임 검증은 RC 필수 조건인가, Candidate Profile 보조 자료인가?
2. Long Soak 기본 시간을 몇 분으로 둘 것인가?
3. FPS/frame time 외부 측정값은 v3.1에서 선택 지표인가 필수 지표인가?

Blocking 질문은 Phase 8 구현 전 결정해야 한다. Non-blocking 질문은 안전 override와 improvement claim 금지 기준을 유지하는 조건에서 `TBD`로 이월할 수 있다.
