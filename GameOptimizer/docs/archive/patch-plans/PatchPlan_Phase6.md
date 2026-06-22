# GameOptimizer v3.1 Patch Plan — Phase 6: Evidence & Runtime Validation Integration
> Archive notice: This Phase Patch Plan is historical. Active implementation work and execution status are tracked in GitHub Issues. This file is not a current source of truth for work ordering, completion status, or release approval.

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 6

버전: v1.0

작성 목적: GameOptimizer v3.1의 여섯 번째 구현 단계인 Phase 6 — Evidence & Runtime Validation Integration을 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, Evidence Recorder, RuntimeValidationMonitor, RuntimeSnapshotRecorder, PolicyTimelineRecorder, Evidence Completeness, RC Evidence Bundle을 어떤 순서로 연결할지 정의하는 작업 계획 문서다.

적용 범위:

- 증거 번들(Evidence Bundle) 구조 정리
- 런타임 스냅샷(Runtime Snapshot) latest/final 생성 기준
- 정책 타임라인(Policy Timeline) 정합성 기준
- 성능 증거(Performance Evidence), rollback summary, runtime validation summary 통합
- 증거 완전성(Evidence Completeness) 검사와 severity 분류
- 최종 판정(Final Classification) 기준
- shutdown evidence flush와 RC report input bundle 기준

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 성능 개선 수치 보장
- 외부 FPS/frame time 측정 도구 구현
- SchedulerController mutation 경로 변경
- RollbackManager rollback execution 대규모 변경
- Aggressive Single-Core Mode 구현
- GameProfileRegistry 구현
- UI 리포트 구현
- 자동 배포 구현

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/archive/patch-plans/PatchPlan_Phase1.md`
- `docs/archive/patch-plans/PatchPlan_Phase2.md`
- `docs/archive/patch-plans/PatchPlan_Phase3.md`
- `docs/archive/patch-plans/PatchPlan_Phase4.md`
- `docs/archive/patch-plans/PatchPlan_Phase5.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/modules/MDS-009_RuntimeOrchestrator.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`

후속 문서:

- `docs/archive/patch-plans/PatchPlan_Phase7.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 6의 목표는 성능 향상을 주장하는 것이 아니라, GameOptimizer의 정책 적용·검증·롤백 결과가 릴리즈 판단 가능한 Evidence로 남도록 만드는 것이다.

## 2. Phase 6 목표

Phase 6의 핵심 목표는 이전 Phase에서 생성된 observation, topology, confidence, policy, scheduler, rollback 결과를 릴리즈 판단 가능한 Evidence 흐름으로 연결하는 것이다.

필수 목표:

1. Evidence Bundle 구조 정리
2. session manifest 필수 필드 정리
3. runtime snapshot latest/final 생성 기준 정리
4. policy timeline monotonicity 검증 기준 정리
5. performance summary 생성 기준 정리
6. rollback summary 생성 기준 정리
7. runtime validation summary 생성 기준 정리
8. evidence completeness report 생성 기준 정리
9. classification result 생성 기준 정리
10. RuntimeValidation BLOCKER와 RC 판정 연결

Phase 6은 성능 정책을 새로 추가하는 단계가 아니다. 이미 존재하거나 이전 Phase에서 생성된 결과를 검증 가능한 Evidence로 연결하는 단계다.

## 3. Phase 6 비목표

다음은 Phase 6에서 구현하지 않는다.

```text
ThreadTracker observation 로직 변경
TopologyAnalyzer group-aware 로직 변경
MainThreadConfidenceAnalyzer classification 로직 변경
PolicyDispatcher validation 구조 변경
SchedulerController mutation 경로 변경
RollbackManager rollback execution 로직 대규모 변경
GameProfileRegistry runtime integration
Aggressive Single-Core Mode 구현
외부 FPS/frame time 측정 도구 구현
UI 리포트 구현
```

Phase 6은 각 모듈의 정책 판단이나 mutation 동작을 바꾸는 단계가 아니라, 그 결과를 검증 가능하게 기록하고 판정하는 단계다.

Phase 6에서 각 모듈의 core behavior를 바꿔야 한다면 해당 Phase로 되돌려 분리한다.

## 4. 영향 모듈

Phase 6에서 영향을 받을 수 있는 모듈은 직접 영향, 간접 영향, 변경 금지 또는 최소 변경 대상으로 구분한다.

### 4.1 직접 영향 모듈

```text
RuntimeSnapshotRecorder
PerformanceEvidenceRecorder
PolicyTimelineRecorder
RuntimeValidationMonitor
EvidenceCompletenessValidator
RCReportGenerator
RuntimeOrchestrator
ShutdownPipeline
```

### 4.2 간접 영향 모듈

```text
ThreadTracker
TopologyAnalyzer
MainThreadConfidenceAnalyzer
PerformanceEngine
PolicyDispatcher
SchedulerController
RollbackManager
ApplyGuard
BackgroundController
```

간접 영향 모듈은 Phase 6에서 생성되는 Evidence의 source가 될 수 있지만, Phase 6에서 core behavior나 mutation semantics를 변경하지 않는다.

### 4.3 변경 금지 또는 최소 변경 모듈

```text
ThreadTracker core sampling logic
TopologyAnalyzer topology detection logic
SchedulerController apply internals
RollbackManager rollback execution internals
ApplyGuard transaction internals
AggressiveSingleCoreController
```

주의:

Phase 6에서 각 모듈의 core behavior를 바꿔야 한다면 해당 Phase로 되돌려 분리한다.

## 5. 패치 단위 개요

Phase 6은 다음 Patch ID 구조를 사용한다.

```text
P6-01 Evidence Contract Alignment
P6-02 Session Manifest Integration
P6-03 Runtime Snapshot Latest / Final
P6-04 Policy Timeline Integration
P6-05 Performance Summary Integration
P6-06 Rollback Summary Integration
P6-07 Runtime Validation Summary
P6-08 Evidence Completeness Validator
P6-09 Classification Result Mapping
P6-10 Evidence Write Failure Handling
P6-11 Shutdown Evidence Flush
P6-12 RC Report Input Bundle
P6-13 Phase 6 Regression Test & Evidence
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

패치 하나가 Evidence integration과 module core behavior 변경을 동시에 수행해서는 안 된다.

## 6. P6-01 Evidence Contract Alignment

Patch ID: `P6-01`

목적:

EvidenceSpecification 기준으로 Phase 6에서 생성해야 할 Evidence 항목과 소유 모듈을 정렬한다.

작업 범위:

```text
Session Evidence 필드 매핑
Runtime State Evidence 필드 매핑
Policy Evidence 필드 매핑
Performance Evidence 필드 매핑
Rollback Evidence 필드 매핑
Runtime Validation Evidence 필드 매핑
Release Evidence 필드 매핑
각 Evidence 소유 모듈 정의
```

수정 가능 파일:

```text
docs/archive/patch-plans/PatchPlan_Phase6.md
docs/implementation/MIGRATION_NOTES_Phase6.md
Evidence 관련 문서
Evidence recorder placeholder
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback internals
ThreadTracker sampling internals
```

구현 규칙:

```text
Evidence 필드는 생성 주체와 소비 주체를 가져야 한다.
소유 모듈이 불명확한 Evidence는 구현 대상으로 확정하지 않는다.
Evidence ownership collision은 blocking question으로 분류한다.
```

완료 기준:

```text
Phase 6에서 생성해야 할 Evidence 항목과 소유 모듈이 명확하다.
Evidence ownership collision이 식별된다.
필수 Evidence와 선택 Evidence가 구분된다.
```

필수 테스트:

```text
evidence owner map review test
missing owner classification test
required evidence list review test
ownership collision review test
```

필수 Evidence:

```text
evidenceContractSummary
evidenceOwnerMap
missingEvidenceOwnerList
blockingQuestionList
```

리스크:

```text
Evidence ownership이 불분명하면 같은 값을 여러 recorder가 다르게 기록할 수 있다.
소비 주체가 없는 Evidence를 구현하면 RC report와 completeness 검사에서 사용되지 않을 수 있다.
```

Rollback 계획:

```text
ownership이 불분명한 Evidence는 TBD로 남기고 구현 범위에서 제외한다.
중복 owner가 있으면 single source of truth를 정할 때까지 summary reference만 남긴다.
```

## 7. P6-02 Session Manifest Integration

Patch ID: `P6-02`

목적:

한 번의 실행 세션을 식별할 수 있는 session manifest를 정리한다.

작업 범위:

```text
runId
sessionId
startedUtc
finishedUtc
targetProcessName
targetProcessId
targetProcessCreationTime
selectedProfile
mode
gitCommit
buildHash
exeSha256
configHash
adminPrivilege
```

수정 가능 파일:

```text
RuntimeOrchestrator
RuntimeSnapshotRecorder
SessionManifestWriter
RC report input
관련 테스트
```

수정 금지 파일:

```text
ThreadTracker core sampling logic
SchedulerController apply internals
RollbackManager rollback internals
PolicyDispatcher validation structure
```

구현 규칙:

```text
Release 검증에서 gitCommit, buildHash, exeSha256 누락은 BLOCKER 후보로 분류한다.
target process identity 누락은 BLOCKER 후보로 분류한다.
session manifest는 runId 기준으로 Evidence Bundle의 root identity가 된다.
```

완료 기준:

```text
session manifest가 runId 기준으로 생성된다.
Release binary와 runtime session을 연결할 수 있다.
manifest timestamp consistency를 검증할 수 있다.
```

필수 테스트:

```text
session manifest generated test
runId required test
target identity required test
missing binary hash release blocker test
manifest timestamp consistency test
```

필수 Evidence:

```text
sessionManifestPath
runId
startedUtc
finishedUtc
gitCommit
buildHash
exeSha256
targetProcessIdentity
```

리스크:

```text
build identity와 runtime identity가 분리되면 RC binary 추적이 불가능해진다.
targetProcessCreationTime 확보가 실패하면 target process identity가 불완전해질 수 있다.
```

Rollback 계획:

```text
manifest writer가 불안정하면 runId, timestamps, target identity, binary identity 최소 필드만 유지한다.
target identity가 불완전하면 Release classification을 INVALID_RUN 또는 BLOCKER 후보로 제한한다.
```

BLOCKER 기준:

```text
Release 검증에서 EXE SHA-256 누락
runId 누락
target process identity 누락
session manifest 생성 실패
```

## 8. P6-03 Runtime Snapshot Latest / Final

Patch ID: `P6-03`

목적:

`runtime_snapshot_latest`와 `runtime_snapshot_final`을 생성하고 freshness 기준을 정리한다.

작업 범위:

```text
runtime_snapshot_latest 생성
runtime_snapshot_final 생성
cycleId 기록
active policy 상태 기록
confidence 상태 기록
topology 상태 기록
rollback state 상태 기록
snapshot freshness 판단
final snapshot shutdown 기준 기록
```

수정 가능 파일:

```text
RuntimeSnapshotRecorder
RuntimeValidationMonitor
ShutdownPipeline
관련 테스트
```

수정 금지 파일:

```text
ThreadTracker core sampling logic
TopologyAnalyzer topology detection logic
SchedulerController apply internals
RollbackManager rollback execution internals
```

구현 규칙:

```text
latest snapshot은 runtime freshness 검증 대상이다.
final snapshot은 shutdown 완료 상태와 session end 기준으로 검증한다.
final snapshot을 latest freshness 규칙으로 오판하지 않는다.
```

완료 기준:

```text
latest/final snapshot이 구분되어 생성된다.
snapshot freshness와 final snapshot consistency를 검증할 수 있다.
final snapshot이 shutdown summary와 일치한다.
```

필수 테스트:

```text
latest snapshot generated test
final snapshot generated test
latest snapshot freshness test
final snapshot shutdown consistency test
missing final snapshot blocker test
```

필수 Evidence:

```text
runtimeSnapshotLatestPath
runtimeSnapshotFinalPath
latestCycleId
finalCycleId
snapshotFreshness
finalSnapshotConsistency
```

리스크:

```text
latest와 final snapshot semantics가 섞이면 shutdown 완료 상태가 stale로 오분류될 수 있다.
final snapshot 누락은 RC report에서 session end 상태를 검증할 수 없게 만든다.
```

Rollback 계획:

```text
snapshot split이 불안정하면 final snapshot writer를 별도 path로 분리하고 latest update와 독립시킨다.
final snapshot 실패 시 classification을 BLOCKER 후보로 제한한다.
```

BLOCKER 기준:

```text
final snapshot 누락
latest snapshot freshness 실패
final snapshot과 shutdown summary 불일치
```

## 9. P6-04 Policy Timeline Integration

Patch ID: `P6-04`

목적:

PolicyCandidate 생성, 차단, 전달, 적용, rollback request를 policy timeline으로 기록한다.

작업 범위:

```text
policy candidate created event
policy rejected event
confidence gate block event
profile restriction block event
cooldown block event
conflict resolution event
controller handoff event
apply result event
verify result event
rollback request event
timeline monotonicity 검증
```

수정 가능 파일:

```text
PolicyTimelineRecorder
PolicyDispatcher
SchedulerController event boundary
RollbackManager event boundary
RuntimeValidationMonitor
관련 테스트
```

수정 금지 파일:

```text
PolicyDispatcher validation structure
SchedulerController apply internals
RollbackManager rollback internals
ApplyGuard transaction internals
```

구현 규칙:

```text
policy event는 시간 순서가 보장되어야 한다.
timeline monotonicity 실패는 BLOCKER 후보로 분류한다.
policy success는 apply result와 verify result를 구분해 기록한다.
```

완료 기준:

```text
policy lifecycle이 timeline에 기록된다.
timeline monotonicity를 검증할 수 있다.
apply, verify, rollback request event가 분리되어 기록된다.
```

필수 테스트:

```text
policy candidate event test
policy reject event test
controller handoff event test
apply verify event order test
rollback request event test
timeline monotonicity failure blocker test
```

필수 Evidence:

```text
policyTimelinePath
policyTimelineEventCount
policyCreatedCount
policyRejectedCount
policyDispatchedCount
policyRollbackRequestCount
timelineMonotonicity
```

리스크:

```text
event source가 여러 모듈에 분산되면 ordering이 깨질 수 있다.
apply result와 verify result를 합치면 scheduler failure 원인 추적이 어려워진다.
```

Rollback 계획:

```text
timeline ordering이 불안정하면 single recorder queue 또는 monotonic sequence id를 우선 적용한다.
detailed event가 불안정하면 lifecycle summary만 유지하되 rollback request event는 보존한다.
```

BLOCKER 기준:

```text
timeline monotonicity 실패
applied policy event 누락
verify result event 누락
rollback request event 누락
```

## 10. P6-05 Performance Summary Integration

Patch ID: `P6-05`

목적:

PerformanceValidationPlan 기준으로 performance summary를 생성한다.

작업 범위:

```text
baselineWindow
appliedWindow
comparisonSummary
mainThreadConfidenceHistory
mainThreadMigrationCount
mainThreadSwitchCount
policyActivationCount
rttJitterSummary
dpcSpikeSummary
classificationInput
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
RuntimeSnapshotRecorder
PolicyTimelineRecorder consumer
관련 테스트
```

수정 금지 파일:

```text
PerformanceEngine policy 판단 로직
SchedulerController apply internals
RollbackManager rollback internals
MainThreadConfidenceAnalyzer classification logic
```

구현 규칙:

```text
Baseline Window와 Applied Window 없이 성능 개선 판정 금지.
performance summary는 rollback summary와 runtime validation summary를 참조해야 한다.
성능 지표가 개선되어도 safety blocker가 있으면 PASS가 아니다.
```

완료 기준:

```text
performance_summary가 생성된다.
Before/After 비교 가능 여부가 명시된다.
classification input completeness를 판단할 수 있다.
```

필수 테스트:

```text
baseline window required test
applied window required test
comparison summary generated test
missing before after blocks improvement classification test
performance summary evidence completeness test
```

필수 Evidence:

```text
performanceSummaryPath
baselineWindow
appliedWindow
comparisonSummary
mainThreadConfidenceHistory
migrationSummary
classificationInputCompleteness
```

리스크:

```text
dry-run 또는 monitoring-only에서도 performance summary 기대치가 모호할 수 있다.
Before/After가 없는 상태에서 improvement로 분류되면 release claim이 왜곡된다.
```

Rollback 계획:

```text
comparison summary가 불완전하면 classification을 NEUTRAL 또는 INVALID_RUN으로 제한한다.
Before/After 입력이 없으면 improvement classification을 차단한다.
```

BLOCKER 기준:

```text
Baseline Window 없이 improvement classification
Applied Window 없이 improvement classification
Comparison Summary 누락
```

## 11. P6-06 Rollback Summary Integration

Patch ID: `P6-06`

목적:

RollbackManager와 ApplyGuard 결과를 release-facing rollback summary로 통합한다.

작업 범위:

```text
rollbackStateSaveResult
rollbackScope
rollbackResult
failedRollbackTarget
failedStatePreserved
identityValidationResult
rollbackReleaseSeverity
shutdownRollbackResult
```

수정 가능 파일:

```text
Rollback summary recorder
RuntimeSnapshotRecorder
RuntimeValidationMonitor
RC report input
관련 테스트
```

수정 금지 파일:

```text
RollbackManager rollback execution internals
ApplyGuard transaction internals
SchedulerController apply internals
BackgroundController restriction internals
```

구현 규칙:

```text
Rollback Summary 누락은 Release 단계에서 BLOCKER 후보.
rollback failure를 WARN/INFO로 하향 금지.
failedStatePreserved=false는 BLOCKER 후보.
```

완료 기준:

```text
rollback_summary가 생성된다.
rollback failure와 failed state preservation이 release-facing evidence에 반영된다.
identity validation result가 rollback summary에 포함된다.
```

필수 테스트:

```text
rollback summary generated test
rollback failure severity test
failed state preserved evidence test
identity validation evidence test
missing rollback summary blocker test
```

필수 Evidence:

```text
rollbackSummaryPath
rollbackStateSaveResult
rollbackResult
rollbackReleaseSeverity
failedStatePreserved
identityValidationResult
shutdownRollbackResult
```

리스크:

```text
rollback execution 결과와 summary 결과가 불일치하면 RC 판정이 잘못될 수 있다.
failed state detail을 과도하게 기록하면 artifact 크기와 민감 정보 위험이 생길 수 있다.
```

Rollback 계획:

```text
summary integration이 불안정하면 rollback result, severity, failedStatePreserved 최소 필드를 primary로 유지한다.
detail field는 reference id 중심으로 축소한다.
```

BLOCKER 기준:

```text
rollback summary 누락
rollback failure severity 하향
failed rollback state 폐기
identity validation result 누락
```

## 12. P6-07 Runtime Validation Summary

Patch ID: `P6-07`

목적:

RuntimeValidationMonitor의 결과를 `runtime_validation_summary`로 정리한다.

작업 범위:

```text
blockerCount
warnCount
infoCount
heartbeatProgression
snapshotFreshness
timelineMonotonicity
activePolicyConsistency
rollbackStateConsistency
shutdownCleanliness
phase-specific validation summary
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
runtime validation summary writer
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback internals
ApplyGuard transaction internals
PolicyDispatcher validation structure
```

구현 규칙:

```text
RuntimeValidation은 mutation을 수행하지 않는다.
RuntimeValidation BLOCKER는 RC PASS를 차단해야 한다.
BLOCKER가 있는데 PASS classification을 만들면 안 된다.
```

완료 기준:

```text
runtime_validation_summary가 생성된다.
RuntimeValidation BLOCKER가 classification과 RC report에 반영된다.
phase-specific validation summary가 누락되지 않는다.
```

필수 테스트:

```text
runtime validation summary generated test
blocker count blocks pass test
snapshot freshness blocker test
timeline monotonicity blocker test
rollback consistency blocker test
shutdown cleanliness test
```

필수 Evidence:

```text
runtimeValidationSummaryPath
runtimeValidationState
blockerCount
warnCount
infoCount
heartbeatProgression
snapshotFreshness
timelineMonotonicity
```

리스크:

```text
RuntimeValidation summary가 여러 source를 병합하면서 stale state를 포함할 수 있다.
BLOCKER count가 classification에 반영되지 않으면 RC PASS가 잘못 생성될 수 있다.
```

Rollback 계획:

```text
summary merge가 불안정하면 blocker/warn/info count와 핵심 safety fields만 우선 기록한다.
BLOCKER가 있으면 classification PASS를 차단하는 rule은 유지한다.
```

BLOCKER 기준:

```text
runtime validation summary 누락
RuntimeValidation BLOCKER가 PASS를 차단하지 않음
active policy와 rollback state 불일치
shutdown cleanliness 실패
```

## 13. P6-08 Evidence Completeness Validator

Patch ID: `P6-08`

목적:

필수 Evidence 누락 여부를 검사하고 WARN/BLOCKER/FATAL로 분류한다.

작업 범위:

```text
session completeness
environment completeness
runtime snapshot completeness
policy timeline completeness
performance completeness
rollback completeness
runtime validation completeness
release evidence completeness
classification input completeness
```

수정 가능 파일:

```text
EvidenceCompletenessValidator
release validation script if existing
RC report input
관련 테스트
```

수정 금지 파일:

```text
ThreadTracker core sampling logic
SchedulerController apply internals
RollbackManager rollback internals
RuntimeValidationMonitor mutation-free contract
```

구현 규칙:

```text
안전 관련 필드 누락은 BLOCKER 후보로 분류한다.
선택 지표 누락은 WARN으로 둘 수 있다.
필수 release evidence 누락은 RC PASS를 차단한다.
```

완료 기준:

```text
evidence_completeness_report가 생성된다.
누락 필드가 severity와 함께 분류된다.
필수 release evidence 누락이 classification input으로 전달된다.
```

필수 테스트:

```text
complete evidence report test
missing optional field warning test
missing rollback summary blocker test
missing final snapshot blocker test
missing binary hash release blocker test
invalid evidence format test
```

필수 Evidence:

```text
evidenceCompletenessReportPath
evidenceCompletenessState
missingRequiredFields
missingOptionalFields
evidenceCompletenessSeverity
```

리스크:

```text
required/optional 기준이 불명확하면 정상 dry-run이 BLOCKER로 오분류될 수 있다.
반대로 safety evidence 누락을 WARN으로 낮추면 release approval이 위험해진다.
```

Rollback 계획:

```text
초기 required set은 release safety evidence 중심으로 제한한다.
optional metrics는 WARN으로 분류하되 rollback/runtime validation/final snapshot 누락은 BLOCKER로 유지한다.
```

BLOCKER 기준:

```text
rollback summary 누락
runtime validation summary 누락
final snapshot 누락
binary hash 누락
classification result 누락
```

## 14. P6-09 Classification Result Mapping

Patch ID: `P6-09`

목적:

PerformanceValidationPlan과 EvidenceSpecification 기준으로 최종 classification을 생성한다.

작업 범위:

```text
PASS
PASS_WITH_WARNINGS
NEUTRAL
REGRESSION
BLOCKER
INVALID_RUN
classification input validation
blocker override rule
neutral handling
regression handling
```

수정 가능 파일:

```text
PerformanceEvidenceRecorder
EvidenceCompletenessValidator
RC report generator
classification module if existing
관련 테스트
```

수정 금지 파일:

```text
PerformanceEngine policy 판단 로직
SchedulerController apply internals
RollbackManager rollback internals
PolicyDispatcher validation structure
```

구현 규칙:

```text
Rollback 실패가 있으면 PASS 금지.
RuntimeValidation BLOCKER가 있으면 PASS 금지.
Evidence fatal missing이 있으면 PASS 금지.
Before/After 없이 improvement classification 금지.
```

완료 기준:

```text
classification result가 일관된 기준으로 생성된다.
BLOCKER override rule이 적용된다.
classification reason이 Evidence에 기록된다.
```

필수 테스트:

```text
pass classification test
pass with warnings classification test
neutral classification test
regression classification test
rollback failure overrides improvement test
runtime blocker overrides improvement test
missing evidence invalid run test
```

필수 Evidence:

```text
classificationResult
classificationReason
blockerOverrideApplied
warningCount
blockerCount
invalidRunReason
```

리스크:

```text
performance improvement와 release approval classification이 섞이면 safety blocker가 묻힐 수 있다.
BLOCKER override rule이 빠지면 위험한 PASS가 생성될 수 있다.
```

Rollback 계획:

```text
classification mapping이 불안정하면 BLOCKER/INVALID_RUN 우선 rule만 유지하고 성능 개선 분류는 NEUTRAL로 제한한다.
rollback failure, RuntimeValidation BLOCKER, Evidence fatal missing은 항상 PASS를 차단한다.
```

BLOCKER 기준:

```text
BLOCKER 존재 상태에서 PASS 생성
rollback failure 상태에서 PASS 생성
RuntimeValidation BLOCKER 상태에서 PASS 생성
Evidence fatal missing 상태에서 PASS 생성
```

## 15. P6-10 Evidence Write Failure Handling

Patch ID: `P6-10`

목적:

Evidence 파일 생성/쓰기 실패를 명확히 분류하고 RC 판정에 반영한다.

작업 범위:

```text
manifest write failure
snapshot write failure
timeline write failure
performance summary write failure
rollback summary write failure
runtime validation summary write failure
completeness report write failure
partial write detection
atomic replace 가능 여부
```

수정 가능 파일:

```text
Evidence writers
RuntimeSnapshotRecorder
PolicyTimelineRecorder
PerformanceEvidenceRecorder
Rollback summary writer
관련 테스트
```

수정 금지 파일:

```text
ThreadTracker core sampling logic
SchedulerController apply internals
RollbackManager rollback internals
PolicyDispatcher validation structure
```

구현 규칙:

```text
Evidence write failure를 조용히 무시하지 않는다.
필수 Evidence write failure는 BLOCKER 또는 FATAL 후보로 분류한다.
partial write를 success로 처리하지 않는다.
```

완료 기준:

```text
Evidence write failure가 severity와 함께 기록된다.
필수 evidence write failure가 RC PASS를 차단한다.
partial write detection 결과가 Evidence에 남는다.
```

필수 테스트:

```text
manifest write failure test
snapshot write failure test
rollback summary write failure blocker test
partial write detection test
atomic replace failure classification test
```

필수 Evidence:

```text
evidenceWriteResult
failedEvidencePath
evidenceWriteFailureReason
partialWriteDetected
evidenceWriteSeverity
```

리스크:

```text
Evidence write 실패 처리 중 추가 write를 시도하면 failure loop가 발생할 수 있다.
partial file을 success로 취급하면 RC report가 깨진 evidence를 소비할 수 있다.
```

Rollback 계획:

```text
atomic replace가 불안정하면 temp file + checksum 또는 write failure summary만 유지한다.
필수 evidence write failure는 PASS 차단 rule을 유지한다.
```

BLOCKER 기준:

```text
필수 Evidence write 실패 무시
partial write를 success로 처리
write failure 상태에서 PASS 생성
```

## 16. P6-11 Shutdown Evidence Flush

Patch ID: `P6-11`

목적:

ShutdownRequested / ShuttingDown 상태에서 final Evidence를 안전하게 flush한다.

작업 범위:

```text
new policy apply blocked after ShutdownRequested
final runtime snapshot 생성
rollback summary flush
runtime validation summary flush
evidence completeness report flush
session manifest finishedUtc 기록
clean shutdown classification
```

수정 가능 파일:

```text
ShutdownPipeline
RuntimeOrchestrator
RuntimeSnapshotRecorder
RuntimeValidationMonitor
Evidence writers
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback internals
ApplyGuard transaction internals
PolicyDispatcher validation structure
```

구현 규칙:

```text
ShutdownRequested 이후 신규 policy apply 금지.
shutdown 중 final evidence flush 실패는 RC 판정에 반영한다.
clean shutdown은 rollback/evidence 상태와 일치해야 한다.
```

완료 기준:

```text
shutdown 시 final evidence bundle이 생성된다.
shutdown cleanliness가 RuntimeValidation에 반영된다.
finishedUtc가 session manifest에 기록된다.
```

필수 테스트:

```text
shutdown blocks new policy apply test
final snapshot flush test
rollback summary flush test
runtime validation summary flush test
shutdown evidence failure blocker test
clean shutdown consistency test
```

필수 Evidence:

```text
shutdownEvidenceFlushStarted
shutdownEvidenceFlushResult
finalSnapshotWritten
finalRollbackSummaryWritten
finalRuntimeValidationSummaryWritten
finishedUtc
cleanShutdown
```

리스크:

```text
shutdown 중 rollback과 evidence flush 순서가 꼬이면 final snapshot과 rollback summary가 불일치할 수 있다.
clean shutdown을 너무 쉽게 true로 만들면 rollback failure가 숨겨질 수 있다.
```

Rollback 계획:

```text
flush ordering이 불안정하면 final snapshot, rollback summary, runtime validation summary 순서를 고정하고 실패 시 cleanShutdown=false로 제한한다.
ShutdownRequested 이후 신규 apply 차단은 유지한다.
```

BLOCKER 기준:

```text
ShutdownRequested 이후 신규 apply
final snapshot 누락
shutdown rollback summary 누락
clean shutdown 위장
```

## 17. P6-12 RC Report Input Bundle

Patch ID: `P6-12`

목적:

RC report generator가 사용할 입력 bundle을 정리한다.

작업 범위:

```text
session manifest input
runtime snapshot input
policy timeline input
performance summary input
rollback summary input
runtime validation summary input
evidence completeness report input
classification result input
binary hash input
```

수정 가능 파일:

```text
RCReportGenerator
EvidenceCompletenessValidator
release scripts if existing
관련 테스트
```

수정 금지 파일:

```text
Evidence source module core behavior
SchedulerController apply internals
RollbackManager rollback internals
RuntimeValidationMonitor mutation-free contract
```

구현 규칙:

```text
RC report는 원본 Evidence 없이 임의 값을 생성하지 않는다.
report에 들어간 값은 Evidence Bundle의 필드에서 추적 가능해야 한다.
missing input은 report readiness를 false로 만들어야 한다.
```

완료 기준:

```text
RC report 입력 bundle이 정의된다.
report 필드와 Evidence 원본 필드가 추적 가능하다.
rcReportReady를 판정할 수 있다.
```

필수 테스트:

```text
rc report input bundle complete test
rc report missing rollback summary blocker test
rc report missing binary hash blocker test
rc report field traceability test
```

필수 Evidence:

```text
rcReportInputBundle
rcReportSourceFieldMap
rcReportMissingInputList
rcReportReady
```

리스크:

```text
report generator가 원본 Evidence 대신 자체 계산을 시작하면 source of truth가 분산된다.
field traceability가 없으면 RC Report의 값 검증이 어렵다.
```

Rollback 계획:

```text
report generation이 불안정하면 input bundle과 source field map까지만 생성한다.
missing required input이 있으면 rcReportReady=false를 유지한다.
```

## 18. P6-13 Phase 6 Regression Test & Evidence

Patch ID: `P6-13`

목적:

Phase 6 패치 묶음의 회귀 테스트와 Evidence 기준을 정리한다.

작업 범위:

```text
Phase 6 test list 작성
Phase 6 static check list 작성
Phase 6 runtime evidence list 작성
Phase 6 release blocker list 작성
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
ThreadTracker core sampling logic
SchedulerController mutation 경로
RollbackManager rollback execution 대규모 변경
AggressiveSingleCoreController
GameProfileRegistry runtime integration
```

구현 규칙:

```text
Phase 6 PASS는 성능 향상 PASS가 아니다.
Phase 6 PASS는 Evidence와 RuntimeValidation 체계가 릴리즈 판정 가능하다는 의미다.
Regression test는 Evidence Bundle, RuntimeValidation, completeness, classification, shutdown flush, RC report input을 분리해 기록한다.
```

완료 기준:

```text
Phase 6 regression checklist가 존재한다.
Phase 6 blocker 조건이 ReleaseChecklist와 RC_Runbook에 연결된다.
Phase 6 Evidence Bundle 최소 항목이 정의된다.
```

필수 테스트:

```text
Evidence Bundle generation suite
RuntimeValidation summary suite
Evidence completeness suite
Classification mapping suite
Shutdown evidence flush suite
RC report input suite
```

필수 Evidence:

```text
phase6RegressionSummary
evidenceBundleSummary
runtimeValidationSummary
evidenceCompletenessSummary
classificationSummary
shutdownEvidenceSummary
```

리스크:

```text
RegressionMatrix_v3.1.md가 아직 없으면 테스트 목록이 문서상 계획에 머물 수 있다.
ReleaseChecklist_v3.1.md와 RC_Runbook_v3.1.md 변경이 과도하면 릴리즈 기준과 Phase 6 기준이 섞일 수 있다.
```

Rollback 계획:

```text
신규 regression 문서가 준비되지 않았으면 PatchPlan_Phase6.md의 Evidence 기준을 우선 source of truth로 유지한다.
ReleaseChecklist와 RC_Runbook 수정은 Phase 6 blocker 항목만 최소 반영한다.
```

## 19. Phase 6 완료 기준

Phase 6 전체 완료 기준은 다음과 같다.

1. Session Manifest가 생성된다.
2. `runtime_snapshot_latest`와 `runtime_snapshot_final`이 생성된다.
3. Policy Timeline이 생성되고 monotonicity를 검증할 수 있다.
4. Performance Summary가 생성된다.
5. Rollback Summary가 생성된다.
6. Runtime Validation Summary가 생성된다.
7. Evidence Completeness Report가 생성된다.
8. Classification Result가 생성된다.
9. RuntimeValidation BLOCKER가 PASS를 차단한다.
10. Rollback failure가 PASS를 차단한다.
11. Evidence fatal missing이 PASS를 차단한다.
12. Shutdown 시 final evidence flush가 수행된다.
13. RC Report Input Bundle이 준비된다.
14. Phase 6 regression test가 정의되어 있다.

완료 판단은 성능 향상 여부가 아니라 릴리즈 판단 가능한 Evidence와 RuntimeValidation 체계의 완전성을 기준으로 한다.

## 20. Phase 6 BLOCKER 조건

다음 조건은 Phase 6 BLOCKER로 분류한다.

```text
session manifest 누락
runId 누락
target process identity 누락
final runtime snapshot 누락
policy timeline monotonicity 실패
performance summary 누락
rollback summary 누락
runtime validation summary 누락
RuntimeValidation BLOCKER가 PASS를 차단하지 않음
rollback failure가 PASS를 차단하지 않음
Evidence fatal missing이 PASS를 차단하지 않음
partial write를 success로 처리
ShutdownRequested 이후 신규 apply 허용
RC report가 Evidence 원본 없이 임의 값을 생성
```

BLOCKER가 발생하면 후속 patch보다 해당 blocker 제거를 우선한다. 특히 P6-02, P6-03, P6-06, P6-07, P6-08, P6-09의 BLOCKER는 Phase 6 전체 진행을 차단한다.

## 21. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P6-01 Evidence Contract Alignment
2. P6-02 Session Manifest Integration
3. P6-03 Runtime Snapshot Latest / Final
4. P6-04 Policy Timeline Integration
5. P6-05 Performance Summary Integration
6. P6-06 Rollback Summary Integration
7. P6-07 Runtime Validation Summary
8. P6-08 Evidence Completeness Validator
9. P6-09 Classification Result Mapping
10. P6-10 Evidence Write Failure Handling
11. P6-11 Shutdown Evidence Flush
12. P6-12 RC Report Input Bundle
13. P6-13 Phase 6 Regression Test & Evidence
```

주의:

```text
P6-02, P6-03, P6-06, P6-07, P6-08, P6-09는 Phase 6의 핵심 안전 패치다.
P6-09에서 BLOCKER override rule이 깨지면 Phase 6을 중단한다.
P6-10에서 필수 Evidence write failure가 무시되면 Phase 6을 중단한다.
```

## 22. 패치 작성 규칙

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

패치 작성자는 Phase 6 범위를 벗어난 core sampling, policy decision, mutation, rollback execution, aggressive mode 구현을 같은 패치에 섞지 않는다.

## 23. Non-Goals

다음은 PatchPlan_Phase6.md의 비목표(Non-Goals)다.

```text
구체 C++ 코드 작성
패치 파일 직접 생성
테스트 코드 직접 작성
성능 개선 수치 보장
외부 FPS/frame time 측정 도구 구현
SchedulerController mutation 경로 변경
RollbackManager rollback execution 대규모 변경
Aggressive Single-Core Mode 구현
GameProfileRegistry 구현
UI 리포트 구현
자동 배포 구현
```

Phase 6은 Evidence와 RuntimeValidation 체계를 릴리즈 판정 가능하게 만드는 문서다.

## 24. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 6의 목적과 비목표가 명확하다.
2. Phase 6 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P6-01부터 P6-13까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 완료 기준, 테스트, Evidence가 정의되어 있다.
5. Evidence Bundle 구성 요소가 명확하다.
6. RuntimeValidation Summary와 Classification Result 연결이 명확하다.
7. Rollback failure / RuntimeValidation BLOCKER / Evidence fatal missing이 PASS를 차단한다는 기준이 명확하다.
8. Shutdown Evidence Flush 기준이 정의되어 있다.
9. Phase 6 BLOCKER 조건이 정의되어 있다.
10. 패치 순서가 정의되어 있다.
11. 후속 `docs/archive/patch-plans/PatchPlan_Phase7.md` 작성에 필요한 기준이 충분하다.

## 25. Open Questions

다음 질문은 Phase 6 실제 패치 착수 전 또는 P6-01에서 분류한다.

```text
1. Evidence 파일 포맷은 JSON으로 통일할 것인가, 일부 log/markdown을 유지할 것인가?
2. session_manifest는 json을 primary로 둘 것인가, txt와 병행할 것인가?
3. policy_timeline은 기존 log 형식을 유지할 것인가, JSONL로 전환할 것인가?
4. EvidenceCompletenessValidator는 C++ 내부 구현인가, 외부 Python 스크립트인가?
5. final snapshot 누락은 모든 모드에서 BLOCKER인가, Release 모드에서만 BLOCKER인가?
6. Performance Summary는 dry-run에서도 생성할 것인가?
7. Classification Result는 Runtime에서 생성할 것인가, RC report 단계에서 생성할 것인가?
8. Evidence write failure는 언제 FATAL로 승격할 것인가?
9. RC Report Generator는 Phase 6에서 입력 bundle까지만 할 것인가, 보고서 생성까지 포함할 것인가?
```

Open Questions가 해결되기 전까지 관련 구현 범위와 포맷은 `TBD`로 유지한다. 단, Evidence 없는 성능 성공 판정을 허용하지 않는 것, RuntimeValidation BLOCKER가 있으면 PASS로 분류하지 않는 것, Rollback failure가 있으면 PASS로 분류하지 않는 것은 Open Question이 아니라 Phase 6 안전 계약이다.
