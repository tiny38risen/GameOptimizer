# GameOptimizer v3.1 Release Checklist

## 1. 문서 개요

문서명: GameOptimizer v3.1 Release Checklist

버전: v1.0

작성 목적: GameOptimizer v3.1을 릴리즈 후보(Release Candidate) 또는 정식 릴리즈 후보로 인정하기 위해 필요한 문서, 빌드, 안전성, 성능 증거(Performance Evidence), 롤백 결과(Rollback Result), 런타임 검증(Runtime Validation), 게임 프로파일(Game Profile) 기준을 정의한다.

적용 범위:

- v3.1 Release Candidate 승인 기준
- v3.1 정식 Release 승인 기준
- Release Blocker와 severity 기준
- Evidence Bundle 완전성 기준
- Rollback, Runtime Validation, Game Profile 검증 기준

비적용 범위:

- Release Gate 스크립트 구현
- Python validator 구현
- CI/CD 설정 구현
- 설치 프로그램 제작
- UI release 화면 설계
- 성능 향상 보장
- 공식 지원 게임 목록 확정
- 자동 배포 파이프라인 구현

상위 문서:

- `docs/vision/PVD_v1.0.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/performance/GameProfileSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/evidence/EvidenceSpecification.md`
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

- `docs/release/RC_Runbook_v3.1.md`
- `docs/implementation/ImplementationPlan_v3.1.md`

문서 경로 기준: `GameOptimizer/docs/`

Release Checklist의 목적은 v3.1이 가장 빠른 프로그램인지 증명하는 것이 아니라, v3.1이 안전하게 적용되고, 실패 시 복구되며, 성능 주장에 필요한 Evidence를 남기는지 확인하는 것이다.

## 2. 릴리즈 승인 원칙

v3.1 릴리즈 승인 원칙은 다음과 같다.

1. 안전성은 성능보다 우선한다.
2. Rollback 가능성은 필수 조건이다.
3. Evidence 완전성은 릴리즈 승인 조건이다.
4. Runtime Validation은 PASS 판정의 필수 조건이다.
5. 성능 개선은 Before/After로만 주장할 수 있다.
6. Game Profile은 Candidate / Validated / Restricted / MonitoringOnly 상태를 구분해야 한다.
7. Anti-Cheat boundary 의심 시 mutation은 차단되어야 한다.
8. Release Candidate는 재현 가능한 build hash와 binary hash를 가져야 한다.

성능 지표가 개선되어도 Rollback 실패, RuntimeValidation BLOCKER, Evidence fatal missing이 있으면 Release Candidate가 될 수 없다.

GameOptimizer v3.1은 모든 게임을 빠르게 만드는 범용 FPS Booster가 아니다. Release Checklist는 성능 개선 주장보다 안전한 적용, 검증 가능한 Evidence, 실패 시 복구 가능성, 검증되지 않은 Game Profile 차단을 우선한다.

## 3. 릴리즈 체크리스트 분류

릴리즈 체크리스트(Release Checklist)는 다음 범주로 나눈다.

1. Documentation Checklist
2. Build Checklist
3. Static Safety Checklist
4. Runtime Safety Checklist
5. Rollback Checklist
6. Performance Evidence Checklist
7. Game Profile Checklist
8. Validation Checklist
9. Release Evidence Checklist
10. Final Approval Checklist

각 범주는 다음 양식으로 작성한다.

```text
목적
필수 항목
PASS 기준
WARN 기준
BLOCKER 기준
Evidence 요구사항
```

체크리스트 결과는 `PASS`, `PASS_WITH_WARNINGS`, `WARN`, `REGRESSION`, `BLOCKER`, `FATAL` 중 하나로 분류할 수 있다. Release Candidate 승인에는 `BLOCKER 0`과 `FATAL 0`이 필수다.

## 4. Documentation Checklist

목적:

v3.1 릴리즈 기준 문서가 충분히 준비되었는지 확인한다.

필수 항목:

```text
docs/vision/PVD_v1.0.md
docs/performance/PerformanceEngineSpec.md
docs/performance/PolicySpecification.md
docs/performance/GameProfileSpecification.md
docs/validation/PerformanceValidationPlan.md
docs/evidence/EvidenceSpecification.md
docs/architecture/SAD_v1.0.md
docs/architecture/RuntimeStateMachine.md
docs/modules/MDS-001_ThreadTracker.md
docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md
docs/modules/MDS-003_TopologyAnalyzer.md
docs/modules/MDS-004_SchedulerController.md
docs/modules/MDS-005_RollbackManager.md
docs/modules/MDS-006_BackgroundController.md
docs/modules/MDS-007_PerformanceEvidencePlanner.md
docs/modules/MDS-008_PolicyDispatcher.md
docs/modules/MDS-009_RuntimeOrchestrator.md
```

PASS 기준:

```text
필수 문서가 존재한다.
문서 간 경로 기준이 docs/... 형식으로 통일되어 있다.
각 문서의 Open Questions가 릴리즈 영향 여부로 분류되어 있다.
Safety, Rollback, Evidence, Runtime Validation의 책임 경계가 문서화되어 있다.
```

WARN 기준:

```text
비핵심 Open Questions가 남아 있다.
문서 간 용어가 일부 불일치하지만 릴리즈 승인 판단에는 영향이 없다.
후속 문서가 Planned 상태이나 RC 승인 조건에는 직접 영향을 주지 않는다.
```

BLOCKER 기준:

```text
Safety/Rollback/Evidence 관련 기준 문서 누락
Performance Engine이 직접 Win32 mutation을 수행하는 구조로 문서화됨
ThreadTracker에 mutation 책임이 부여됨
SchedulerController ownership이 불명확함
Release Checklist가 Rollback 또는 RuntimeValidation 실패를 차단 조건으로 정의하지 않음
```

Evidence 요구사항:

```text
documentationChecklistResult
requiredDocumentList
missingDocumentList
pathConsistencyResult
blockingOpenQuestions
documentationBlockers
```

## 5. Build Checklist

목적:

Release Candidate로 사용할 바이너리가 재현 가능한 빌드 산출물인지 확인한다.

필수 항목:

```text
Release x64 build 성공
MSVC v143 / Visual Studio 2022 기준 빌드
C++20 이상 또는 프로젝트 기준 C++23 설정 확인
경고 정책 확인
빌드 산출물 경로 기록
git commit hash 기록
build hash 기록
EXE SHA-256 기록
```

PASS 기준:

```text
Release x64 빌드 성공
build hash와 exeSha256이 Evidence에 기록됨
git commit hash와 build configuration이 재현 가능하게 기록됨
```

WARN 기준:

```text
Debug 빌드 검증은 성공했으나 Release 빌드의 부가 산출물 일부가 누락됨
비차단 경고가 존재하지만 runtime safety와 rollback에 영향 없음
symbol 또는 map 파일이 누락되었으나 RC 승인 핵심 기준은 충족함
```

BLOCKER 기준:

```text
Release build 실패
EXE SHA-256 누락
git commit hash 누락
빌드 산출물 식별 불가
Release Candidate binary와 Evidence의 hash 불일치
```

Evidence 요구사항:

```text
buildConfiguration
compilerVersion
gitCommitHash
buildHash
exeSha256
artifactPath
buildLogPath
buildChecklistResult
```

## 6. Static Safety Checklist

목적:

코드와 문서가 핵심 안전 계약(Safety Contract)을 위반하지 않는지 정적 검토한다.

필수 항목:

```text
ThreadTracker observation-only 유지
Performance Engine direct Win32 mutation 없음
Policy Layer direct Win32 mutation 없음
SchedulerController mutation ownership 유지
RollbackManager state preservation 책임 유지
ApplyGuard 우회 없음
std::expected Assign -> Check -> Bind 패턴 준수
uninspected expected propagation 금지
Processor Group group 0 hardcoding 금지
```

PASS 기준:

```text
mutation ownership이 SchedulerController로 제한됨
ThreadTracker가 observation-only 책임만 가짐
Rollback state 저장 전 mutation 가능 경로 없음
ApplyGuard arm 전 mutation 가능 경로 없음
Processor Group 정보 누락 시 안전 fallback이 문서와 코드에서 일치함
```

WARN 기준:

```text
정적 검토 자동화가 미완료이나 수동 검토 evidence가 존재함
비핵심 helper에서 naming 불일치가 있으나 mutation 경로에 영향 없음
테스트 보강 필요 항목이 있으나 Safety Contract 위반은 아님
```

BLOCKER 기준:

```text
Rollback state 저장 전 mutation 가능 경로 존재
ApplyGuard arm 전 mutation 가능 경로 존재
ThreadTracker에서 SetThreadAffinityMask 또는 SetThreadPriority 호출
PerformanceEngine에서 Win32 mutation 호출
Processor Group 정보 누락 시 group 0 fallback 강제
std::expected 무검사 전파
Policy Layer가 Win32 mutation을 직접 수행
```

Evidence 요구사항:

```text
staticSafetyReviewResult
mutationOwnershipScan
threadTrackerObservationOnlyResult
directWin32MutationScan
applyGuardPathReview
expectedHandlingReview
processorGroupSafetyReview
staticSafetyBlockers
```

## 7. Runtime Safety Checklist

목적:

실행 중 안전 상태 전이와 Runtime Validation이 정상인지 확인한다.

필수 항목:

```text
RuntimeStateMachine 상태 전이 Evidence 존재
ShutdownRequested 이후 신규 policy apply 없음
RuntimeValidation BLOCKER 0
watchdog heartbeat 정상
snapshot freshness 정상
policy timeline monotonicity 정상
active policy와 rollback state 일치
target process identity 검증 정상
```

PASS 기준:

```text
RuntimeValidation BLOCKER 0
정상 종료 또는 안전 종료 Evidence 존재
RuntimeStateMachine transition이 허용된 경로만 사용함
active policy와 rollback state가 일치함
```

WARN 기준:

```text
일부 runtime snapshot interval이 지연되었으나 freshness 기준을 위반하지 않음
비차단 telemetry 누락이 있으나 RuntimeValidation Summary는 완전함
shutdown 중 신규 apply는 없고 cleanup evidence만 지연됨
```

BLOCKER 기준:

```text
RuntimeValidation BLOCKER 존재
shutdown 중 신규 mutation 발생
target identity mismatch 상태에서 mutation 발생
active policy와 rollback state 불일치
timeline monotonicity 실패
watchdog heartbeat fatal stale
```

Evidence 요구사항:

```text
runtimeStateMachineTrace
runtimeValidationSummary
watchdogHeartbeatSummary
snapshotFreshnessSummary
policyTimeline
activePolicyRollbackStateMatch
targetIdentityValidationResult
runtimeSafetyBlockers
```

## 8. Rollback Checklist

목적:

적용된 모든 mutation이 실패 또는 종료 시 원상 복구 가능한지 확인한다.

필수 항목:

```text
rollback scope 기록
rollback state save result 기록
thread priority rollback result
thread affinity rollback result
process priority rollback result
background restriction rollback result
timer resolution rollback result if modified
failed rollback state preservation
identity validation result
```

PASS 기준:

```text
적용된 모든 mutation에 rollback state가 존재한다.
종료 시 rollback 성공 또는 recoverable stale/missing identity로 분류된다.
failed rollback state가 폐기되지 않고 보존된다.
```

WARN 기준:

```text
대상 thread/process가 종료되어 stale identity로 분류됨
복구 대상이 이미 사라져 no-op rollback으로 분류됨
수동 확인이 필요한 rollback warning이 있으나 living same identity 실패는 아님
```

BLOCKER 기준:

```text
rollback state 없이 mutation 발생
living same identity rollback 실패
failed rollback state 폐기
rollback failure를 WARN/INFO로 낮춤
identity validation 없이 rollback 성공 처리
rollback summary 누락
```

Evidence 요구사항:

```text
rollbackScope
rollbackStateSaveResult
rollbackSummary
rollbackFailurePreservationResult
identityValidationResult
livingSameIdentityRollbackResult
rollbackChecklistResult
rollbackBlockers
```

## 9. Performance Evidence Checklist

목적:

성능 개선 주장이 Evidence로 설명 가능한지 확인한다.

필수 항목:

```text
Baseline Window
Applied Window
Comparison Summary
Main Thread Confidence History
Main Thread Migration Count
Main Thread Switch Count
Policy Activation Count
RTT Jitter Summary
DPC Spike Summary
Rollback Summary
Runtime Validation Summary
classification result
```

PASS 기준:

```text
Before/After 비교 가능
Rollback 정상
RuntimeValidation BLOCKER 없음
성능 지표가 개선 또는 최소한 악화되지 않음
classification result가 Evidence와 일치함
```

WARN 기준:

```text
성능 개선 폭이 작음
선택 지표 일부 누락
FPS/frame time 외부 측정값 없음
NEUTRAL classification이나 안전성 기준은 충족함
```

BLOCKER 기준:

```text
Baseline Window 없음
Applied Window 없음
Rollback Summary 없음
RuntimeValidation Summary 없음
Evidence 없이 성능 개선 판정
성능 개선이 있어도 안전성 BLOCKER 존재
Before/After 없이 IMPROVED로 분류
```

Evidence 요구사항:

```text
baselineWindow
appliedWindow
comparisonSummary
mainThreadConfidenceHistory
migrationSummary
switchCountSummary
policyActivationCount
rttJitterSummary
dpcSpikeSummary
rollbackSummary
runtimeValidationSummary
classificationResult
performanceEvidenceChecklistResult
```

## 10. Game Profile Checklist

목적:

게임별 profile이 안전한 정책 제한 기준으로 동작하는지 확인한다.

필수 항목:

```text
selectedProfile
profileType
profileStatus
profileRiskLevel
allowedPolicies
blockedPolicies
conditionalPolicies
profileSelectionReason
validationRequired
monitoringOnlyReason if applicable
```

PASS 기준:

```text
Unknown game은 Unknown Default Profile 또는 MonitoringOnly로 처리된다.
Candidate profile은 Validated로 위장되지 않는다.
blockedPolicies가 allowedPolicies보다 우선한다.
profileStatus와 defaultMode가 GameProfileSpecification 기준과 일치한다.
```

WARN 기준:

```text
Candidate profile의 일부 optional evidence가 누락되었으나 mutation은 차단됨
profile validation 수치가 TBD이나 Release Candidate 승인에는 직접 영향 없음
Unknown Default Profile이 적용되어 성능 정책이 제한됨
```

BLOCKER 기준:

```text
검증되지 않은 게임을 Validated로 분류
targetProcessName을 검증 없이 확정
AntiCheatSensitive profile에서 mutation 허용
Unknown profile에서 aggressive mode 허용
blockedPolicies 무시
profileStatus 누락
```

Evidence 요구사항:

```text
selectedProfile
profileType
profileStatus
profileRiskLevel
profileSelectionReason
allowedPolicies
blockedPolicies
conditionalPolicies
validationRequired
monitoringOnlyReason
profileFallbackReason
gameProfileChecklistResult
```

## 11. Validation Checklist

목적:

`docs/validation/PerformanceValidationPlan.md` 기준으로 dry-run, soft-apply, apply, rollback, soak 검증이 수행되었는지 확인한다.

필수 항목:

```text
dry-run result
soft-apply result
apply result if applicable
rollback test result
long soak result if applicable
actual game validation result if applicable
classification result
warnings
blockers
```

PASS 기준:

```text
dry-run과 soft-apply가 정상 수행됨
rollback test 성공
BLOCKER 없음
classification result가 evidence와 일치함
```

WARN 기준:

```text
apply mode 검증 미완료
long soak 미완료
실제 게임 검증 미완료
성능 개선이 NEUTRAL
Candidate Game Profile이 아직 Validated로 승격되지 않음
```

BLOCKER 기준:

```text
rollback test 실패
RuntimeValidation BLOCKER
Evidence completeness failure
unsafe mutation detected
validation result와 release classification 불일치
```

Evidence 요구사항:

```text
dryRunResult
softApplyResult
applyResult
rollbackTestResult
longSoakResult
actualGameValidationResult
validationClassificationResult
validationWarnings
validationBlockers
validationChecklistResult
```

## 12. Release Evidence Checklist

목적:

릴리즈 승인에 필요한 Evidence Bundle이 완전한지 확인한다.

필수 산출물:

```text
session_manifest.json 또는 txt
runtime_snapshot_latest
runtime_snapshot_final
policy_timeline.log
performance_summary.json
rollback_summary.json
runtime_validation_summary.json
evidence_completeness_report.json
rc_report.md
binary hash
git commit
classification result
```

PASS 기준:

```text
필수 release evidence가 모두 존재한다.
Evidence completeness가 Complete 또는 허용 가능한 Partial이다.
Evidence Bundle의 hash와 build hash가 일치한다.
```

WARN 기준:

```text
선택 산출물이 누락되었으나 필수 승인 evidence는 완전함
Partial evidence가 있으나 PASS_WITH_WARNINGS 범위로 설명 가능함
외부 FPS/frame time 자료가 없으나 내부 evidence는 완전함
```

BLOCKER 기준:

```text
session manifest 누락
final snapshot 누락
rollback summary 누락
runtime validation summary 누락
binary hash 누락
classification result 누락
Evidence Bundle과 Release Candidate binary 식별 불일치
```

Evidence 요구사항:

```text
sessionManifest
runtimeSnapshotLatest
runtimeSnapshotFinal
policyTimelineLog
performanceSummary
rollbackSummary
runtimeValidationSummary
evidenceCompletenessReport
rcReport
binaryHash
gitCommit
classificationResult
releaseEvidenceChecklistResult
```

## 13. Final Approval Checklist

목적:

Release Candidate와 정식 Release 승인 조건을 명확히 분리한다.

필수 항목:

```text
Documentation Checklist result
Build Checklist result
Static Safety Checklist result
Runtime Safety Checklist result
Rollback Checklist result
Performance Evidence Checklist result
Game Profile Checklist result
Validation Checklist result
Release Evidence Checklist result
BLOCKER count
FATAL count
```

Release Candidate 승인 조건:

```text
Documentation Checklist PASS
Build Checklist PASS
Static Safety Checklist PASS
Runtime Safety Checklist PASS
Rollback Checklist PASS
Performance Evidence Checklist PASS 또는 PASS_WITH_WARNINGS
Game Profile Checklist PASS
Validation Checklist PASS 또는 PASS_WITH_WARNINGS
Release Evidence Checklist PASS
BLOCKER 0
FATAL 0
```

정식 Release 승인 조건:

```text
Release Candidate 승인 조건 충족
최소 1개 Candidate Game Profile에 대한 dry-run/soft-apply 검증 완료
rollback 성공률 100%
RuntimeValidation BLOCKER 0
Release Evidence Bundle 완성
```

PASS 기준:

```text
Release Candidate 승인 조건 또는 정식 Release 승인 조건을 모두 충족함
승인 결과가 rc_report.md에 기록됨
승인자, build hash, binary hash, git commit이 추적 가능함
```

WARN 기준:

```text
성능 지표가 NEUTRAL이나 안전성과 Evidence가 완전함
실제 게임 검증이 미완료이나 RC 승인 범위에서 허용됨
PASS_WITH_WARNINGS 항목이 존재하지만 BLOCKER 또는 FATAL은 없음
```

BLOCKER 기준:

```text
하나 이상의 필수 체크리스트 BLOCKER
FATAL 1개 이상
Rollback 실패
RuntimeValidation BLOCKER
Evidence fatal missing
```

Evidence 요구사항:

```text
finalApprovalResult
releaseCandidateApprovalResult
formalReleaseApprovalResult
checklistSummary
blockerCount
fatalCount
approvalCriteriaResult
approver
approvedAt
rcReport
```

주의:

성능 지표가 NEUTRAL이어도 안전성과 Evidence가 완전하면 RC는 가능하다.
그러나 성능 개선을 홍보하거나 Validated profile로 승격해서는 안 된다.

## 14. Severity 기준

릴리즈 severity는 다음과 같이 정의한다.

```text
INFO
WARN
REGRESSION
BLOCKER
FATAL
```

`INFO`: 릴리즈 승인에 영향을 주지 않는 참고 정보다.

`WARN`: 승인 가능하지만 release note, rc_report, 후속 task에 남겨야 하는 경고다. WARN은 `PASS_WITH_WARNINGS`로 집계될 수 있다.

`REGRESSION`: 기존 기준 대비 후퇴가 확인된 상태다. 안전성과 rollback에 영향이 없고 허용 범위가 명확하면 RC에서 WARN으로 다룰 수 있으나, 안전 경로와 연결되면 BLOCKER가 된다.

`BLOCKER`: Release Candidate 승인을 차단하는 문제다. 해결되거나 명시적으로 non-release scope로 제외되기 전에는 RC가 될 수 없다.

`FATAL`: 안전성, 복구 가능성, evidence 신뢰성을 근본적으로 훼손하는 문제다. FATAL은 RC와 정식 Release를 모두 차단한다.

BLOCKER 예시:

```text
Rollback 실패
RuntimeValidation BLOCKER
Evidence fatal missing
ApplyGuard 우회
rollback state 없이 mutation
identity mismatch 상태에서 mutation
Processor Group group 0 hardcoding
Anti-Cheat boundary 우회 시도
```

Severity 분류는 `docs/evidence/EvidenceSpecification.md`의 evidence classification과 일치해야 한다. Rollback 실패와 RuntimeValidation BLOCKER는 성능 개선 여부와 무관하게 release approval을 차단한다.

## 15. Non-Goals

다음은 ReleaseChecklist_v3.1.md의 비목표(Non-Goals)다.

```text
Release Gate 스크립트 구현
Python validator 구현
CI/CD 설정 구현
설치 프로그램 제작
UI release 화면 설계
성능 향상 보장
공식 지원 게임 목록 확정
자동 배포 파이프라인 구현
```

이 문서는 “무엇을 만들 것인가”가 아니라 “무엇을 만족해야 릴리즈 후보로 인정할 것인가”를 정의한다.

## 16. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. 릴리즈 승인 원칙이 정의되어 있다.
2. Documentation / Build / Static Safety / Runtime Safety / Rollback / Performance Evidence / Game Profile / Validation / Release Evidence 체크리스트가 정의되어 있다.
3. 각 체크리스트의 PASS / WARN / BLOCKER 기준이 정의되어 있다.
4. Release Candidate 승인 조건이 정의되어 있다.
5. 정식 Release 승인 조건이 정의되어 있다.
6. Rollback 실패와 RuntimeValidation BLOCKER가 릴리즈를 차단한다는 원칙이 포함되어 있다.
7. Evidence 없는 성능 개선 주장을 금지한다.
8. Candidate Profile과 Validated Profile을 구분한다.
9. 후속 `docs/release/RC_Runbook_v3.1.md` 작성에 필요한 실행 기준이 충분하다.

## 17. Open Questions

다음 질문은 후속 문서 또는 release governance에서 결정한다.

| 분류 | 질문 | 릴리즈 영향 |
|---|---|---|
| Blocking | RC 승인을 위해 실제 게임 검증을 필수로 둘 것인가, soft-apply까지만 필수로 둘 것인가? | RC 승인 범위 결정 필요 |
| Blocking | Performance Evidence가 NEUTRAL일 때 RC를 허용할 것인가? | Final Approval 기준 결정 필요 |
| Blocking | PASS_WITH_WARNINGS의 허용 범위를 어디까지 둘 것인가? | RC 승인 가능 범위 결정 필요 |
| Blocking | binary hash 누락은 모든 빌드에서 BLOCKER인가, Release 빌드에서만 BLOCKER인가? | Build Checklist 적용 범위 결정 필요 |
| Blocking | ReleaseChecklist를 사람이 수동으로 확인할 것인가, Release Gate 스크립트와 연결할 것인가? | 승인 운영 방식 결정 필요 |
| Non-blocking | Long Soak Test 기본 시간을 몇 분으로 둘 것인가? | 구체 수치는 TBD, RC 전에는 최소 기준 필요 |
| Non-blocking | Release Evidence Bundle의 최종 저장 위치는 어디로 둘 것인가? | 문서/운영 경로 결정 필요 |
| Non-blocking | Candidate Game Profile을 Validated로 승격하는 기준은 ReleaseChecklist에서 확정할 것인가, 별도 ProfileValidation 문서에서 확정할 것인가? | 정식 Release 전 세부화 필요 |

Open Questions가 해결되기 전까지 확정되지 않은 수치는 `TBD`로 유지한다. Blocking 질문은 Release Candidate 승인 전에 owner와 결정 근거를 남겨야 한다.
