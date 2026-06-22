# GameOptimizer v3.1 Evidence Specification

Status: Active
Authority: Future v3.1 evidence design
TargetVersion: v3.1
ImplementationStatus: Design only; not the current machine-enforced RC schema
VerificationStatus: Planned/manual design review
Owner: Performance evidence design

## 1. 문서 개요

문서명: GameOptimizer v3.1 Evidence Specification

버전: v1.0

작성 목적: GameOptimizer v3.1의 성능 검증, 정책 적용, 롤백(Rollback), 런타임 검증(Runtime Validation) 결과를 어떤 증거(Evidence)로 기록할지 정의한다. 본 문서는 성능 기능이 실제로 효과가 있었는지, 안전하게 적용되었는지, 실패 시 제대로 복구되었는지 판단하기 위한 Evidence 구조, 필수 필드(Required Field), 누락 필드(Missing Field), 심각도(Severity), release-facing 판정 기준을 정의한다.

적용 범위:

- Evidence 목적과 분류
- Session / Environment / Runtime / Policy / Performance / Rollback / Validation / Release / Failure Evidence
- 필수 필드와 선택 필드
- 누락 시 WARN/BLOCKER/FATAL 기준
- Classification 기준
- Evidence Completeness 규칙
- Evidence 저장 위치 설계

비적용 범위:

- 구체 JSON schema 최종 구현
- Python validator 구현
- Release Gate 스크립트 구현
- UI 리포트 구현
- 로그 포맷 최종 확정
- 자동 벤치마크 도구 구현
- 외부 FPS 측정 도구 구현

상위 문서:

- `docs/validation/PerformanceValidationPlan.md`
- `docs/proposals/performance/PerformanceEngineSpec.md`
- `docs/proposals/performance/PolicySpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-005_RollbackManager.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/modules/MDS-009_RuntimeOrchestrator.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Release_Gate_Spec.md`

후속 문서:

- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/proposals/performance/GameProfileSpecification.md`

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

본 문서는 코드 구현 문서가 아니다. 기존 `docs/release/Evidence_Schema.md`는 v3.0 RC evidence 계약으로 유지하며, 본 문서는 v3.1 성능 기능을 위한 설계 기준을 정의한다.

## 2. Evidence 목적

Evidence는 홍보 자료가 아니라 정책 판단, 성능 검증, 릴리즈 승인, 실패 분석의 근거다.

Evidence의 목적은 성능 향상을 주장하는 것이 아니라, 성능 정책이 안전하고 검증 가능한 방식으로 동작했는지 설명하는 것이다.

필수 목적:

1. 성능 개선 여부 판단
2. 정책 적용 결과 검증
3. 롤백 성공/실패 검증
4. RuntimeValidation 결과 기록
5. PolicyCandidate 생성/거부 사유 기록
6. Monitoring-only 전환 사유 기록
7. Release Candidate 승인 근거 제공
8. 실패 발생 시 원인 분석 근거 제공

반드시 유지할 원칙:

```text
1. Evidence 없는 성능 성공 판정 금지.
2. Rollback 실패를 WARN/INFO로 낮추기 금지.
3. RuntimeValidation BLOCKER가 있으면 PASS 금지.
4. Before/After 비교 없이 성능 개선 판정 금지.
5. 필수 Evidence 누락 시 WARN 또는 BLOCKER 후보로 분류.
6. target process identity가 불명확하면 적용 성공으로 분류 금지.
7. Processor Group 정보 누락 상태에서 group 0 보정 적용은 BLOCKER 후보.
```

## 3. Evidence 분류

Evidence는 다음 범주로 나눈다.

```text
1. Session Evidence
2. Environment Evidence
3. Runtime State Evidence
4. Policy Evidence
5. Performance Evidence
6. Rollback Evidence
7. Runtime Validation Evidence
8. Release Evidence
9. Failure Evidence
```

### 3.1 Session Evidence

목적: 실행 세션을 식별한다.

필수 필드: `runId`, `sessionId`, `startedUtc`, `targetProcessName`, `mode`, `gitCommit`, `buildHash`, `exeSha256`

생성 시점: startup / evidence session initialization

소비 문서/모듈: PerformanceValidationPlan, ReleaseChecklist, RC Runbook, Evidence validator

누락 시 처리: run identity 누락은 BLOCKER 또는 INVALID_RUN 후보

### 3.2 Environment Evidence

목적: 성능 검증 환경을 재현 가능하게 기록한다.

필수 필드: `osVersion`, `cpuModel`, `processorGroupCount`, `smtEnabled`, `testMode`

생성 시점: Initializing

소비 문서/모듈: PerformanceValidationPlan, ReleaseChecklist, failure analysis

누락 시 처리: 안전 관련 필드 누락은 BLOCKER 후보, 일반 환경 필드 누락은 WARN

### 3.3 Runtime State Evidence

목적: RuntimeStateMachine의 상태 전이를 기록한다.

필수 필드: `cycleId`, `stateBefore`, `stateAfter`, `transitionReason`, `timestampUtc`

생성 시점: every runtime state transition

소비 문서/모듈: RuntimeStateMachine, RuntimeOrchestrator, RuntimeValidationMonitor

누락 시 처리: shutdown/fatal transition 누락은 BLOCKER 후보

### 3.4 Policy Evidence

목적: 정책 후보 생성, 검증, 전달, 적용 결과를 기록한다.

필수 필드: `policyType`, `policyId`, `confidenceLevel`, `requiredController`, `riskLevel`, `dispatchResult`

생성 시점: PolicyCandidate 생성부터 Controller handoff/verify까지

소비 문서/모듈: PolicySpecification, PolicyDispatcher, PerformanceEvidencePlanner

누락 시 처리: mutation policy의 rollback scope 누락은 BLOCKER

### 3.5 Performance Evidence

목적: 성능 정책 적용 전후의 변화를 비교한다.

필수 필드: `baselineWindow`, `appliedWindow`, `comparisonSummary`, `classification`

생성 시점: Baseline/Applied Window 종료 후

소비 문서/모듈: PerformanceValidationPlan, ReleaseChecklist, RC evidence bundle

누락 시 처리: Before/After 누락 시 성능 개선 판정 금지

### 3.6 Rollback Evidence

목적: 정책 적용 전 상태 저장과 실패/종료 시 원상 복구 결과를 기록한다.

필수 필드: `rollbackReason`, `rollbackScope`, `rollbackStateSaved`, `rollbackResult`, `identityValidationResult`

생성 시점: save state, rollback request, shutdown rollback

소비 문서/모듈: RollbackManager, ApplyGuard, ShutdownPipeline, ReleaseChecklist

누락 시 처리: rollback failure 관련 필드 누락은 BLOCKER

### 3.7 Runtime Validation Evidence

목적: RuntimeValidation 결과와 BLOCKER 여부를 기록한다.

필수 필드: `runtimeValidationState`, `blockerCount`, `warnCount`, `validationCycle`

생성 시점: runtime validation cycle and shutdown summary

소비 문서/모듈: RuntimeValidationMonitor, RuntimeStateMachine, ReleaseChecklist

누락 시 처리: summary 누락은 BLOCKER 후보

### 3.8 Release Evidence

목적: RC 또는 Release Candidate 승인에 필요한 최종 Evidence Bundle을 정의한다.

필수 필드: binary/git identity, final snapshot, rollback summary, runtime validation summary, classification result

생성 시점: ShuttingDown / release bundle creation

소비 문서/모듈: ReleaseChecklist, RC Runbook, Release Gate

누락 시 처리: release-facing 필수 artifact 누락은 BLOCKER 후보

### 3.9 Failure Evidence

목적: 실패를 숨기지 않고 원인 분석 가능하게 기록한다.

필수 필드: `failureType`, `severity`, `timestampUtc`, `affectedModule`, `fallbackAction`

생성 시점: failure detection

소비 문서/모듈: RuntimeStateMachine, ReleaseChecklist, failure analysis

누락 시 처리: fatal failure evidence 누락은 BLOCKER

## 4. Session Evidence

목적:

```text
한 번의 GameOptimizer 실행 세션을 식별한다.
```

필수 필드:

```text
runId
sessionId
startedUtc
finishedUtc
targetProcessName
targetProcessId
targetProcessCreationTime
gameCandidateName
mode
gitCommit
buildHash
exeSha256
configHash
adminPrivilege
```

누락 시 처리:

```text
runId 없음: BLOCKER
startedUtc 없음: BLOCKER
finishedUtc 없음: WARN 또는 BLOCKER 후보
gitCommit/buildHash/exeSha256 없음: Release 단계에서 BLOCKER 후보
target process identity 없음: BLOCKER
```

Session Evidence는 다른 모든 Evidence를 연결하는 root identity다. runId 또는 target process identity가 불명확하면 적용 성공 또는 성능 성공으로 분류할 수 없다.

## 5. Environment Evidence

목적:

```text
성능 검증 환경을 재현 가능하게 기록한다.
```

필수 필드:

```text
osVersion
cpuModel
physicalCoreCount
logicalProcessorCount
processorGroupCount
smtEnabled
ramSize
gpuModel
powerPlan
timerResolutionState
networkAdapterName
testMode
```

선택 필드:

```text
biosVersion
chipsetDriverVersion
gpuDriverVersion
gameVersion
serverRegion
networkType
```

누락 시 처리:

```text
cpuModel 없음: WARN
processorGroupCount 없음: BLOCKER 후보
smtEnabled 없음: WARN
testMode 없음: BLOCKER
```

Processor Group과 SMT 정보는 v3.1 정책 안전성에 직접 영향을 준다. 이 정보가 없으면 topology 기반 정책의 적용 성공을 인정할 수 없다.

## 6. Runtime State Evidence

목적:

```text
RuntimeStateMachine의 상태 전이를 기록한다.
```

필수 필드:

```text
cycleId
stateBefore
stateAfter
transitionReason
timestampUtc
runtimeValidationState
policyType
rollbackRequired
monitoringOnlyReason
fatalReason
```

필수 전이 기록:

```text
Initializing → Observing
Observing → Evaluating
Evaluating → PolicyPending
PolicyPending → Applying
Applying → Verifying
Verifying → Running
Running → RollbackPending
ShutdownRequested → ShuttingDown
ShuttingDown → Terminated
FatalError → Terminated
```

누락 시 처리:

```text
stateBefore/stateAfter 없음: BLOCKER 후보
transitionReason 없음: WARN
FatalError evidence 없음: BLOCKER
Shutdown transition 누락: BLOCKER 후보
```

Runtime State Evidence는 정책이 안전한 상태에서만 적용되었는지 검증하는 근거다. ShutdownRequested 이후 신규 mutation이 발생했다면 BLOCKER 후보로 기록한다.

## 7. Policy Evidence

목적:

```text
정책 후보 생성, 검증, 전달, 적용 결과를 기록한다.
```

필수 필드:

```text
policyType
policyId
source
confidenceLevel
activationReason
deactivationCondition
requiredController
requiredRollbackScope
requiredEvidenceFields
riskLevel
cooldownState
fallbackBehavior
monitoringOnlyReason
dispatchResult
applyResult
verifyResult
```

정책 후보 생성 시 필수 기록:

```text
candidateCreated
candidateRejected
rejectionReason
conflictResult
cooldownBlockReason
requiredControllerValidation
rollbackScopeValidation
evidenceFieldValidation
```

누락 시 처리:

```text
policyType 없음: BLOCKER 후보
requiredController 없음: BLOCKER 후보
rollbackScope 없음 + mutation policy: BLOCKER
evidenceFields 없음: WARN 또는 BLOCKER 후보
verifyResult 없음 + applied policy: BLOCKER
```

Policy Evidence는 “왜 적용했는가”와 “왜 적용하지 않았는가”를 모두 설명해야 한다. Monitoring-only와 PolicyRejected도 정상적인 Evidence 대상이다.

## 8. Performance Evidence

목적:

```text
성능 정책 적용 전후의 변화를 비교한다.
```

필수 필드:

```text
baselineWindow
appliedWindow
comparisonSummary
mainThreadConfidenceHistory
mainThreadMigrationCount
mainThreadSwitchCount
coreRelocationCount
policyActivationCount
policyRejectionCount
aggressiveModeActiveDuration
rttJitterSummary
dpcSpikeSummary
runtimeValidationSummary
rollbackSummary
classification
```

Before/After 구조:

```text
Baseline Window:
- 정책 적용 전 관찰 구간

Applied Window:
- 정책 적용 후 관찰 구간

Comparison Summary:
- migration delta
- confidence stability delta
- switch count delta
- RTT jitter delta
- DPC spike delta
- policy activation stability
```

선택 필드:

```text
fpsAverage
frameTimeAverage
frameTimeP95
frameTimeP99
onePercentLow
zeroPointOnePercentLow
externalBenchmarkResult
subjectiveUserNote
```

누락 시 처리:

```text
baselineWindow 없음: 성능 개선 판정 금지
appliedWindow 없음: 성능 개선 판정 금지
comparisonSummary 없음: 성능 개선 판정 금지
rollbackSummary 없음: BLOCKER 후보
runtimeValidationSummary 없음: BLOCKER 후보
```

subjectiveUserNote는 보조 정보다. 체감 기록은 Performance Evidence를 대체하지 않는다.

## 9. Rollback Evidence

목적:

```text
정책 적용 전 상태 저장과 실패/종료 시 원상 복구 결과를 기록한다.
```

필수 필드:

```text
rollbackReason
rollbackScope
savedStateCount
rollbackStateSaved
rollbackResult
failedRollbackTarget
failedStatePreserved
retryEligibility
identityValidationResult
shutdownRollbackResult
releaseSeverity
```

필수 규칙:

```text
rollbackStateSaved=false 상태에서 mutation이 발생하면 BLOCKER.
rollbackResult 실패를 WARN/INFO로 낮추지 않는다.
failedStatePreserved=false이고 rollback 실패가 있으면 BLOCKER.
identityValidation 실패 상태에서 성공 rollback으로 처리하지 않는다.
```

누락 시 처리:

```text
rollbackScope 없음: BLOCKER 후보
rollbackResult 없음: BLOCKER 후보
identityValidationResult 없음: BLOCKER 후보
failedStatePreserved 누락 + rollback failure: BLOCKER
```

Rollback Evidence는 safety evidence다. 성능 결과가 좋아 보여도 rollback evidence가 실패하면 PASS가 될 수 없다.

## 10. Runtime Validation Evidence

목적:

```text
런타임 검증 결과와 BLOCKER 여부를 기록한다.
```

필수 필드:

```text
runtimeValidationState
blockerCount
warnCount
infoCount
validationCycle
heartbeatProgression
snapshotFreshness
timelineMonotonicity
activePolicyConsistency
rollbackStateConsistency
shutdownCleanliness
```

BLOCKER 조건:

```text
runtime validation BLOCKER 발생
heartbeat progression 정지
snapshot freshness 실패
timeline monotonicity 실패
active policy와 rollback state 불일치
shutdown 중 active mutation 잔존
```

RuntimeValidation BLOCKER가 존재하면 성능 지표가 개선되어도 PASS가 될 수 없다.

Runtime Validation Evidence는 active policy 상태와 rollback state의 일관성을 확인해야 한다.

## 11. Release Evidence

목적:

```text
RC 또는 Release Candidate 승인에 필요한 최종 Evidence Bundle을 정의한다.
```

필수 구성:

```text
session_manifest.txt 또는 json
runtime_snapshot_latest
runtime_snapshot_final
policy_timeline
performance_summary
rollback_summary
runtime_validation_summary
evidence_completeness_report
binary_hash
git_commit
classification_result
```

필수 판정:

```text
Release 단계에서 binary hash 누락은 BLOCKER 후보.
final snapshot 누락은 BLOCKER 후보.
rollback summary 누락은 BLOCKER 후보.
runtime validation summary 누락은 BLOCKER 후보.
```

Release Evidence는 기존 v3.0 RC evidence contract와 충돌하지 않아야 한다. v3.1에서 새 필드를 추가하려면 ReleaseChecklist와 RC Runbook에서 compatibility를 정의해야 한다.

## 12. Failure Evidence

목적:

```text
실패를 숨기지 않고 원인 분석 가능하게 기록한다.
```

필수 실패 유형:

```text
accessDenied
identityMismatch
targetProcessExited
topologyIncomplete
processorGroupUnsupported
rollbackStateSaveFailure
applyFailure
verifyFailure
rollbackFailure
evidenceWriteFailure
runtimeValidationBlocker
antiCheatBoundarySuspicion
```

각 실패는 다음 필드를 가져야 한다.

```text
failureType
severity
timestampUtc
cycleId
affectedModule
affectedPolicy
fallbackAction
releaseFacing
diagnosticMessage
```

Failure Evidence는 fallback을 설명해야 한다. 실패가 발생했는데 fallbackAction이 없으면 release-facing WARN 또는 BLOCKER 후보로 분류한다.

## 13. Severity 기준

다음 severity를 정의한다.

```text
INFO
WARN
REGRESSION
BLOCKER
FATAL
```

### INFO

```text
정상 동작 또는 참고용 정보.
```

예: Monitoring-only 진입 사유, policy rejected but safe.

### WARN

```text
안전성은 유지되지만 일부 Evidence 또는 성능 판단이 불완전한 상태.
```

예: 선택 환경 필드 누락, unsupported path with safe fallback.

### REGRESSION

```text
성능 지표가 Before/After 비교에서 악화된 상태.
```

예: migration 증가, RTT jitter 악화, DPC spike 증가, policy thrashing.

### BLOCKER

```text
릴리즈 또는 PASS 판정을 막아야 하는 상태.
```

BLOCKER 예시:

```text
rollback 실패
runtime validation BLOCKER
rollback state 없이 mutation
ApplyGuard 우회
identity mismatch 상태에서 mutation
Processor Group 정보 누락 상태에서 group 0 hardcoding 적용
final evidence 누락
```

### FATAL

```text
프로그램이 안전하게 계속 실행할 수 없는 치명 상태.
```

예: unrecoverable identity mismatch, evidence write fatal failure, runtime contract violation.

## 14. Classification 기준

검증 결과 분류를 정의한다.

```text
PASS
PASS_WITH_WARNINGS
NEUTRAL
REGRESSION
BLOCKER
INVALID_RUN
```

### PASS

조건:

```text
필수 Evidence 존재
Rollback 성공
RuntimeValidation BLOCKER 없음
Before/After 비교 가능
주요 안정성 지표 개선 또는 악화 없음
```

### PASS_WITH_WARNINGS

조건:

```text
안전 조건은 통과
경미한 Evidence 누락
성능 개선 폭 작거나 일부 지표 불명확
Rollback과 RuntimeValidation은 정상
```

### NEUTRAL

조건:

```text
안전성 문제 없음
성능 개선 불명확
Before/After 차이 미미
```

### REGRESSION

조건:

```text
성능 지표 악화
migration 증가
RTT jitter 악화
DPC spike 증가
policy thrashing 발생
```

### BLOCKER

조건:

```text
Rollback 실패
RuntimeValidation BLOCKER
unsafe mutation
Evidence fatal missing
identity mismatch mutation
```

### INVALID_RUN

조건:

```text
테스트 환경 구성 실패
필수 세션 식별자 없음
대상 프로세스 검증 실패
비교 구간 자체가 성립하지 않음
```

Classification은 Evidence Completeness 결과와 Safety/Rollback 결과를 먼저 반영한 뒤 성능 지표를 해석한다.

## 15. Evidence Completeness 규칙

Evidence 완전성 판단 규칙을 정의한다.

필수 검사:

```text
session completeness
environment completeness
policy completeness
performance completeness
rollback completeness
runtime validation completeness
release completeness
```

완전성 결과:

```text
Complete
Partial
MissingRequiredField
InvalidFormat
InconsistentState
```

누락 규칙:

```text
필수 필드 누락은 문서별 severity 기준에 따라 WARN/BLOCKER/FATAL로 분류한다.
선택 필드 누락은 WARN으로만 처리할 수 있다.
안전 관련 필드 누락은 BLOCKER 후보로 본다.
```

InconsistentState 예시는 다음과 같다.

- stateAfter가 Running인데 verifyResult가 없음
- rollbackResult가 failure인데 failedStatePreserved가 없음
- policy applied인데 rollbackScope가 없음
- Processor Group 정보가 unknown인데 affinity apply success로 기록됨

## 16. Evidence 저장 위치

권장 저장 위치:

```text
artifacts/
└─ <runId>/
   ├─ session_manifest.json
   ├─ runtime_snapshot_latest.json
   ├─ runtime_snapshot_final.json
   ├─ policy_timeline.log
   ├─ performance_summary.json
   ├─ rollback_summary.json
   ├─ runtime_validation_summary.json
   ├─ evidence_completeness_report.json
   └─ rc_report.md
```

주의:

```text
본 문서는 저장 위치를 설계 수준에서 정의한다.
구체 파일 포맷과 스크립트 구현은 후속 Release/Implementation 문서에서 확정한다.
```

Existing v3.0 release artifacts와 v3.1 artifacts의 위치 충돌을 피해야 한다. 후속 ReleaseChecklist에서 compatibility 또는 migration path를 정의한다.

## 17. Non-Goals

다음은 EvidenceSpecification의 비목표다.

```text
구체 JSON schema 최종 구현
Python validator 구현
Release Gate 스크립트 구현
UI 리포트 구현
로그 포맷 최종 확정
성능 향상 보장
자동 벤치마크 도구 구현
외부 FPS 측정 도구 구현
```

본 문서는 Evidence 설계 기준을 정의한다. 구현 schema와 validator는 후속 ImplementationPlan 또는 ReleaseChecklist에서 확정한다.

## 18. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

```text
1. Evidence 분류가 정의되어 있다.
2. Session / Environment / Runtime / Policy / Performance / Rollback / Release Evidence 필드가 정의되어 있다.
3. 필수 필드와 선택 필드가 구분되어 있다.
4. Evidence 누락 시 WARN/BLOCKER/FATAL 기준이 정의되어 있다.
5. Rollback 실패가 낮은 severity로 숨겨지지 않는다.
6. RuntimeValidation BLOCKER가 PASS를 막는다는 원칙이 포함되어 있다.
7. Before/After 없이 성능 개선 판정을 금지한다.
8. Classification 기준이 정의되어 있다.
9. Evidence 저장 위치가 설계 수준에서 정의되어 있다.
10. 후속 ReleaseChecklist_v3.1.md 작성에 필요한 Evidence 기준이 충분하다.
```

Acceptance Criteria는 이 문서의 승인 기준이다. JSON schema 구현 완료나 Release Gate 반영 완료를 의미하지 않는다.

## 19. Open Questions

1. Evidence 파일 포맷은 JSON으로 통일할 것인가, Markdown 요약을 병행할 것인가?
2. session_manifest는 txt와 json 중 무엇을 v3.1 기준으로 삼을 것인가?
3. policy_timeline은 log 형식을 유지할 것인가, structured JSONL로 전환할 것인가?
4. Evidence completeness validator는 Release Gate에 필수로 포함할 것인가?
5. FPS/frame time 외부 측정값은 어떤 schema로 입력받을 것인가?
6. Evidence 보관 기간 또는 cleanup 정책은 둘 것인가?
7. WARN과 BLOCKER의 최종 기준은 ReleaseChecklist에서 확정할 것인가?
8. fatal evidence write failure 발생 시 exit code mapping은 어디서 정의할 것인가?
