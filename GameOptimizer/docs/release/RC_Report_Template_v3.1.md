# GameOptimizer v3.1 RC Report Template

## 1. 템플릿 개요

문서명: GameOptimizer v3.1 RC Report Template

템플릿 버전: v1.0

대상 제품 버전: GameOptimizer v3.1

작성 목적: GameOptimizer v3.1의 릴리즈 후보 보고서(Release Candidate Report)가 따라야 할 구조, 필수 필드, 판정 규칙, Evidence 참조 방식, 누락 처리 기준을 정의한다. 본 문서는 실제 RC 검증 결과가 아니며, RC Gate 실행 후 생성되는 실제 보고서의 템플릿이다.

적용 범위:

- Build Identity 기록 기준
- Session Identity와 Test Environment 기록 기준
- Game Profile Summary 기록 기준
- Gate Summary와 Detailed Gate Results 기록 기준
- Static Safety, Runtime Validation, Rollback, Performance, Aggressive Mode, Long Soak 요약 기준
- Evidence Completeness와 Source Traceability 기준
- WARN / REGRESSION / BLOCKER / FATAL findings 기록 기준
- Final Decision, Approval, Artifact Index 기준
- Markdown 보고서와 구조화된 보고서 mapping 기준

비적용 범위:

- 실제 RC 결과 작성
- 실제 테스트 PASS 판정
- RC Report Generator 구현
- JSON Schema 구현
- Release Gate script 구현
- CI/CD 구현
- 자동 배포 구현
- 설치 프로그램 작성
- 공식 지원 게임 선언
- 성능 향상 보장

상위 문서:

- `GameOptimizer_v3_SRS_Rev1_3_CPP23_Safety_Update.docx`
- `docs/vision/PVD_v1.0.md`
- `docs/proposals/performance/PerformanceEngineSpec.md`
- `docs/proposals/performance/PolicySpecification.md`
- `docs/proposals/performance/GameProfileSpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/validation/RegressionMatrix_v3.1.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/implementation/ImplementationPlan_v3.1.md`
- GitHub Issues

아카이브 참고 문서:

- `docs/archive/patch-plans/PatchPlan_Phase1.md`
- `docs/archive/patch-plans/PatchPlan_Phase2.md`
- `docs/archive/patch-plans/PatchPlan_Phase3.md`
- `docs/archive/patch-plans/PatchPlan_Phase4.md`
- `docs/archive/patch-plans/PatchPlan_Phase5.md`
- `docs/archive/patch-plans/PatchPlan_Phase6.md`
- `docs/archive/patch-plans/PatchPlan_Phase7.md`
- `docs/archive/patch-plans/PatchPlan_Phase8.md`
- `docs/archive/patch-plans/PatchPlan_Phase9.md`
- `docs/archive/patch-plans/PatchPlan_Phase10.md`

출력 형식:

- Markdown RC Report
- 구조화된 RC Report JSON 또는 `TBD`

문서 경로 기준: `GameOptimizer/docs/`

RC Report의 목적은 프로그램이 잘 동작했다는 인상을 주는 것이 아니라, 어떤 조건에서 어떤 검증이 수행되었고 어떤 근거로 Release Candidate 판정이 내려졌는지를 재현 가능하게 기록하는 것이다.

핵심 전제:

1. 보고서 생성 성공은 RC PASS를 의미하지 않는다.
2. BLOCKER 또는 FATAL이 존재하면 PASS를 기록하지 않는다.
3. RuntimeValidation BLOCKER가 존재하면 PASS를 기록하지 않는다.
4. Rollback failure가 존재하면 PASS를 기록하지 않는다.
5. failed rollback state가 보존되지 않았으면 PASS를 기록하지 않는다.
6. 필수 Evidence 누락이 있으면 PASS를 기록하지 않는다.
7. Release binary의 EXE SHA-256 누락은 BLOCKER다.
8. Gate 내부 FAIL을 최종 판정에서 누락하지 않는다.
9. 보고서의 핵심 필드는 원본 Evidence와 연결되어야 한다.
10. 확인되지 않은 값은 추측해 채우지 않는다.
11. 테스트되지 않은 항목을 PASS로 기록하지 않는다.
12. Manual 항목은 승인자와 확인 Evidence가 있어야 한다.
13. 실제 게임 검증은 공식 지원 선언으로 기록하지 않는다.
14. NEUTRAL은 성능 향상을 의미하지 않는다.
15. Markdown 보고서와 구조화된 보고서의 최종 판정은 일치해야 한다.

## 2. 템플릿 사용 규칙

1. `<REQUIRED>` 표시는 반드시 채워야 한다.
2. `<OPTIONAL>` 표시는 적용 가능한 경우에만 채운다.
3. `<TBD>`는 미확정 설계값에만 사용한다.
4. 실제 RC 실행에서 값을 알 수 없으면 `MISSING`으로 기록한다.
5. 실행되지 않은 Gate는 `NOT_RUN`으로 기록한다.
6. 선행 Gate 실패로 실행되지 못한 Gate는 `SKIPPED_WITH_REASON`으로 기록한다.
7. 적용되지 않는 항목은 `NOT_APPLICABLE`과 사유를 기록한다.
8. 누락값을 0, false, PASS 같은 기본값으로 대체하지 않는다.
9. 보고서 생성기가 값을 추정하거나 재계산할 경우 계산식을 기록한다.
10. 모든 시간은 UTC를 기본으로 하며 필요한 경우 local timezone을 병기한다.

보고서 생성 성공은 RC PASS를 의미하지 않는다. 실제 PASS는 Final Decision 규칙, Gate Summary, Findings, Evidence Completeness, Source Traceability가 모두 일관될 때만 기록할 수 있다.

## 3. 상태값 정의

### Gate Status

다음 값을 사용한다.

```text
NOT_RUN
RUNNING
PASS
PASS_WITH_WARNINGS
FAIL
BLOCKED
SKIPPED_WITH_REASON
INVALID_RUN
```

### Severity

다음 값을 사용한다.

```text
INFO
WARN
REGRESSION
BLOCKER
FATAL
```

### Final Decision

다음 값을 사용한다.

```text
PASS
PASS_WITH_WARNINGS
NEUTRAL
REGRESSION
BLOCKER
INVALID_RUN
FATAL
```

### 규칙

```text
BLOCKER 또는 FATAL이 1개 이상이면 PASS / PASS_WITH_WARNINGS 금지
RuntimeValidation BLOCKER가 있으면 PASS 금지
Rollback failure가 있으면 PASS 금지
Evidence fatal missing이 있으면 PASS 금지
INVALID_RUN은 실행 조건이 성립하지 않아 판정할 수 없는 상태
NEUTRAL은 안전성은 통과했지만 성능 개선이 확인되지 않은 상태
```

## 4. 보고서 헤더

실제 RC Report는 다음 헤더를 사용한다.

```text
# GameOptimizer v3.1 RC Evidence Report

Report Version: <REQUIRED>
Run ID: <REQUIRED>
Report Generated UTC: <REQUIRED>
Target Version: GameOptimizer v3.1
Test Kind: <REQUIRED>
Final Decision: <REQUIRED>
Final Severity: <REQUIRED>
```

Test Kind 후보:

```text
dry-run
soft-apply
apply
aggressive
rollback
soak
release-candidate
```

필수 규칙:

```text
Run ID가 없으면 INVALID_RUN
Final Decision이 없으면 BLOCKER
Report Version이 없으면 WARN 또는 INVALID
```

## 5. Executive Summary

다음 필드를 포함한다.

```text
Final Decision
Decision Reason
BLOCKER Count
FATAL Count
REGRESSION Count
WARN Count
INFO Count
Passed Gate Count
Failed Gate Count
Skipped Gate Count
Overall Evidence Completeness
Rollback Result
RuntimeValidation Result
Performance Classification
```

템플릿:

```text
## Executive Summary

- Final Decision: <REQUIRED>
- Decision Reason: <REQUIRED>
- Gate Summary:
  - PASS: <REQUIRED>
  - PASS_WITH_WARNINGS: <REQUIRED>
  - FAIL: <REQUIRED>
  - BLOCKED: <REQUIRED>
  - SKIPPED_WITH_REASON: <REQUIRED>
- Severity Summary:
  - FATAL: <REQUIRED>
  - BLOCKER: <REQUIRED>
  - REGRESSION: <REQUIRED>
  - WARN: <REQUIRED>
  - INFO: <REQUIRED>
- Evidence Completeness: <REQUIRED>
- Rollback Result: <REQUIRED>
- RuntimeValidation Result: <REQUIRED>
- Performance Classification: <REQUIRED>
```

본 요약은 아래 Gate 결과와 원본 Evidence를 축약한 것이며, 원본 Evidence와 충돌할 경우 원본 Evidence 및 Gate 집계 규칙을 우선한다.

## 6. Build Identity

다음 필드를 포함한다.

```text
gitCommit
gitBranch
workingTreeState
buildConfiguration
compiler
compilerVersion
toolset
cppStandard
buildTimestampUtc
binaryPath
buildHash
exeSha256
configHash
profileHash
```

템플릿:

```text
## Build Identity

| Field | Value | Source Artifact | Status |
|---|---|---|---|
| Git Commit | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Git Branch | <OPTIONAL> | <REQUIRED> | <REQUIRED> |
| Working Tree State | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Build Configuration | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Compiler / Toolset | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| C++ Standard | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Build Timestamp UTC | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Binary Path | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Build Hash | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| EXE SHA-256 | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Config Hash | <OPTIONAL> | <REQUIRED> | <REQUIRED> |
| Profile Hash | <OPTIONAL> | <REQUIRED> | <REQUIRED> |
```

BLOCKER 기준:

```text
gitCommit 누락
binaryPath 누락
buildHash 누락
EXE SHA-256 누락
manifest와 report의 hash 불일치
```

## 7. Session Identity

다음 필드를 포함한다.

```text
runId
sessionId
startedUtc
finishedUtc
duration
targetProcessName
targetProcessId
targetProcessCreationTime
targetIdentityValidationResult
selectedProfileId
validationMode
administratorPrivilege
artifactRoot
```

템플릿:

```text
## Session Identity

| Field | Value | Source Artifact |
|---|---|---|
| Run ID | <REQUIRED> | <REQUIRED> |
| Session ID | <OPTIONAL> | <REQUIRED> |
| Started UTC | <REQUIRED> | <REQUIRED> |
| Finished UTC | <REQUIRED> | <REQUIRED> |
| Duration | <REQUIRED> | <REQUIRED> |
| Target Process Name | <REQUIRED> | <REQUIRED> |
| Target Process ID | <REQUIRED> | <REQUIRED> |
| Target Creation Time | <REQUIRED> | <REQUIRED> |
| Identity Validation | <REQUIRED> | <REQUIRED> |
| Selected Profile | <REQUIRED> | <REQUIRED> |
| Validation Mode | <REQUIRED> | <REQUIRED> |
| Administrator Privilege | <REQUIRED> | <REQUIRED> |
| Artifact Root | <REQUIRED> | <REQUIRED> |
```

BLOCKER 기준:

```text
Run ID 누락
target process identity 누락
identity mismatch 상태에서 mutation 수행
startedUtc 또는 finishedUtc 누락
```

## 8. Test Environment

다음 필드를 포함한다.

```text
osName
osVersion
osBuild
cpuModel
physicalCoreCount
logicalProcessorCount
processorGroupCount
selectedProcessorGroup
smtState
hybridArchitecture
memoryTotal
networkType
administratorPrivilege
antiCheatPresence
targetApplication
```

템플릿:

```text
## Test Environment

| Field | Value | Validation State |
|---|---|---|
| OS | <REQUIRED> | <REQUIRED> |
| OS Build | <REQUIRED> | <REQUIRED> |
| CPU Model | <REQUIRED> | <REQUIRED> |
| Physical Cores | <REQUIRED> | <REQUIRED> |
| Logical Processors | <REQUIRED> | <REQUIRED> |
| Processor Groups | <REQUIRED> | <REQUIRED> |
| Selected Processor Group | <REQUIRED> | <REQUIRED> |
| SMT State | <REQUIRED> | <REQUIRED> |
| Hybrid Architecture | <OPTIONAL> | <REQUIRED> |
| Memory | <REQUIRED> | <REQUIRED> |
| Network Type | <OPTIONAL> | <REQUIRED> |
| Anti-Cheat Presence | <REQUIRED> | <REQUIRED> |
```

규칙:

```text
확인하지 못한 Anti-Cheat 정보는 Unknown으로 기록한다.
Unknown을 Low Risk로 자동 변환하지 않는다.
Processor Group 정보 누락을 group 0으로 대체하지 않는다.
```

## 9. Game Profile Summary

다음 필드를 포함한다.

```text
profileId
profileName
profileType
profileStatus
profileRiskLevel
profileSelectionReason
targetBindingResult
allowedPolicies
blockedPolicies
conditionalPolicies
validationRequired
monitoringOnlyReason
profileFallbackApplied
profileFallbackReason
```

템플릿:

```text
## Game Profile Summary

- Profile ID: <REQUIRED>
- Profile Name: <REQUIRED>
- Profile Type: <REQUIRED>
- Profile Status: <REQUIRED>
- Risk Level: <REQUIRED>
- Selection Reason: <REQUIRED>
- Target Binding Result: <REQUIRED>
- Validation Required: <REQUIRED>
- Fallback Applied: <REQUIRED>
- Fallback Reason: <REQUIRED OR NOT_APPLICABLE>

### Allowed Policies

<REQUIRED>

### Blocked Policies

<REQUIRED>

### Conditional Policies

<REQUIRED>

### Monitoring-only Reason

<REQUIRED OR NOT_APPLICABLE>
```

BLOCKER 기준:

```text
Candidate를 Validated로 기록
Unknown 또는 MonitoringOnly Profile에서 mutation 발생
AntiCheatSensitive Profile에서 mutation 발생
blocked policy 적용
profile fallback reason 누락
```

## 10. Gate Summary

다음 Gate를 반드시 포함한다.

```text
Preflight Gate
Build Identity Gate
Documentation Gate
Static Safety Gate
Dry-run Gate
Soft-apply Gate
Runtime Safety Gate
Rollback Gate
Game Profile Gate
Performance Validation Gate
Evidence Completeness Gate
Long Soak Gate
RC Report Consistency Gate
```

표 형식:

```text
## Gate Summary

| Gate | Status | Severity | Check Count | Fail Count | Evidence | Reason |
|---|---|---:|---:|---:|---|---|
| Preflight | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Build Identity | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Documentation | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Static Safety | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Dry-run | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Soft-apply | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Runtime Safety | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Rollback | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Game Profile | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Performance | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Evidence Completeness | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Long Soak | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Report Consistency | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
```

규칙:

```text
Gate가 실행되지 않았으면 PASS 대신 NOT_RUN 또는 SKIPPED_WITH_REASON을 사용한다.
Check Count가 0인 필수 Gate는 PASS가 될 수 없다.
Fail Count가 1 이상이면 Gate PASS가 될 수 없다.
```

## 11. Detailed Gate Results

각 Gate는 다음 공통 템플릿을 사용한다.

```text
### <Gate Name>

- Status: <REQUIRED>
- Severity: <REQUIRED>
- Started UTC: <REQUIRED>
- Finished UTC: <REQUIRED>
- Command: <REQUIRED OR MANUAL>
- Exit Code: <REQUIRED>
- Required Evidence: <REQUIRED>
- Evidence Present: <REQUIRED>
- Failure Count: <REQUIRED>
- Decision Reason: <REQUIRED>
```

Check 표:

```text
| Check ID | Description | Expected | Observed | Status | Severity | Evidence |
|---|---|---|---|---|---|---|
| <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
```

규칙:

```text
Check 내부 FAIL이 있으면 Gate 결과에 반드시 반영한다.
중첩 Check의 FAIL을 최종 Gate Status에서 무시하지 않는다.
```

## 12. Static Safety Results

다음 검사 결과를 별도 요약한다.

```text
ThreadTracker observation-only
PerformanceEngine direct mutation 금지
PolicyDispatcher direct mutation 금지
AggressiveSingleCoreController direct mutation 금지
SchedulerController mutation ownership
ApplyGuard 우회
rollback state 없는 mutation
std::expected Assign → Check → Bind
uninspected expected propagation
Processor Group group 0 hardcoding
Anti-Cheat 우회 구현
```

템플릿:

```text
## Static Safety Results

| Contract | Status | Findings | Severity | Evidence |
|---|---|---:|---|---|
| ThreadTracker Observation-only | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Controller Mutation Ownership | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| ApplyGuard Contract | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| std::expected Contract | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Processor Group Contract | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Anti-Cheat Safety Boundary | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
```

## 13. Runtime Validation Summary

다음 필드를 포함한다.

```text
runtimeValidationState
blockerCount
warnCount
infoCount
heartbeatProgression
snapshotFreshness
timelineMonotonicity
activePolicyConsistency
rollbackStateConsistency
targetIdentityConsistency
shutdownCleanliness
```

템플릿:

```text
## Runtime Validation Summary

- State: <REQUIRED>
- BLOCKER Count: <REQUIRED>
- WARN Count: <REQUIRED>
- INFO Count: <REQUIRED>
- Heartbeat Progression: <REQUIRED>
- Snapshot Freshness: <REQUIRED>
- Timeline Monotonicity: <REQUIRED>
- Active Policy Consistency: <REQUIRED>
- Rollback State Consistency: <REQUIRED>
- Target Identity Consistency: <REQUIRED>
- Shutdown Cleanliness: <REQUIRED>
```

BLOCKER 기준:

```text
RuntimeValidation BLOCKER 존재
heartbeat 중단
latest snapshot stale
timeline monotonicity 실패
active policy와 rollback state 불일치
identity mismatch mutation
clean shutdown 위장
```

## 14. Policy Timeline Summary

다음 필드를 포함한다.

```text
policyTimelinePath
eventCount
firstEventUtc
lastEventUtc
timelineMonotonicity
policyCreatedCount
policyRejectedCount
policyDispatchedCount
policyAppliedCount
policyVerifiedCount
rollbackRequestCount
monitoringOnlyCount
```

템플릿:

```text
## Policy Timeline Summary

| Metric | Value |
|---|---:|
| Event Count | <REQUIRED> |
| Policy Created | <REQUIRED> |
| Policy Rejected | <REQUIRED> |
| Policy Dispatched | <REQUIRED> |
| Policy Applied | <REQUIRED> |
| Policy Verified | <REQUIRED> |
| Rollback Requests | <REQUIRED> |
| Monitoring-only Events | <REQUIRED> |
| Timeline Monotonicity | <REQUIRED> |
```

규칙:

```text
필수 실행 모드에서 eventCount가 0이면 명시적 정당화 없이는 PASS 금지.
Apply 이벤트가 존재하면 Verify 또는 Rollback 이벤트가 연결되어야 한다.
```

## 15. Scheduler and ApplyGuard Summary

다음 필드를 포함한다.

```text
currentStateQueryResult
rollbackStateSaveResult
applyGuardArmResult
schedulerApplyResult
schedulerVerifyResult
applyGuardCommitResult
rollbackRequestResult
processorGroupSafetyResult
```

템플릿:

```text
## Scheduler / ApplyGuard Summary

| Stage | Result | Target | Evidence | Failure Reason |
|---|---|---|---|---|
| Query | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED OR N/A> |
| Save | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED OR N/A> |
| Arm | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED OR N/A> |
| Apply | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED OR N/A> |
| Verify | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED OR N/A> |
| Commit / Rollback | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED OR N/A> |
```

BLOCKER 기준:

```text
Save 전 mutation
Arm 전 mutation
Verify 없는 success
Verify 실패 후 Commit
Processor Group unsafe 상태의 affinity apply
```

## 16. Rollback Summary

다음 필드를 포함한다.

```text
rollbackStateSaveResult
rollbackScope
rollbackResult
rollbackClassification
rollbackReleaseSeverity
identityValidationResult
staleOrMissingIdentity
failedStatePreserved
failedRollbackTargetList
shutdownRollbackResult
mutationRollbackCoverage
```

템플릿:

```text
## Rollback Summary

- Overall Result: <REQUIRED>
- Classification: <REQUIRED>
- Release Severity: <REQUIRED>
- Identity Validation: <REQUIRED>
- Failed State Preserved: <REQUIRED>
- Shutdown Rollback Result: <REQUIRED>
- Mutation Rollback Coverage: <REQUIRED>

### Rollback Targets

| Target | Mutation Type | Original State Saved | Rollback Result | Identity Result | State Preserved | Evidence |
|---|---|---|---|---|---|---|
| <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
```

BLOCKER 기준:

```text
Rollback Summary 누락
living same identity rollback failure
failedStatePreserved=false
rollback state 없는 mutation
identity validation 없는 rollback 성공
```

## 17. Performance Validation Summary

다음 필드를 포함한다.

```text
validationMode
baselineWindowValid
appliedWindowValid
comparisonValid
metricCompleteness
regressionDetectionResult
safetyOverrideApplied
performanceClassification
performanceImprovementClaimAllowed
```

템플릿:

```text
## Performance Validation Summary

- Validation Mode: <REQUIRED>
- Baseline Window Valid: <REQUIRED>
- Applied Window Valid: <REQUIRED>
- Comparison Valid: <REQUIRED>
- Metric Completeness: <REQUIRED>
- Regression Result: <REQUIRED>
- Safety Override Applied: <REQUIRED>
- Performance Classification: <REQUIRED>
- Improvement Claim Allowed: <REQUIRED>
```

Metric Comparison:

```text
| Metric | Baseline | Applied | Difference | Result | Completeness |
|---|---:|---:|---:|---|---|
| Main Thread Confidence | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Migration Count | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Main Thread Switch Count | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| RTT Jitter | <REQUIRED OR N/A> | <REQUIRED OR N/A> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| DPC Spike Count | <REQUIRED OR N/A> | <REQUIRED OR N/A> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| FPS / Frame Time | <OPTIONAL> | <OPTIONAL> | <OPTIONAL> | <OPTIONAL> | <REQUIRED> |
```

규칙:

```text
Baseline 또는 Applied Window가 없으면 improvement claim 금지.
NEUTRAL이면 improvement claim 금지.
REGRESSION이면 PASS 금지 또는 REGRESSION 최종 판정.
Safety Override가 적용되면 성능 개선 여부보다 Safety 결과를 우선한다.
```

## 18. Aggressive Mode Summary

Aggressive Mode가 비활성인 경우에도 `NOT_APPLICABLE` 또는 `NOT_ENABLED`로 명시한다.

다음 필드를 포함한다.

```text
aggressiveModeEnabled
candidateCount
activationCount
activationGateResult
failedActivationGates
policyBundle
activeDuration
deactivationReason
rollbackResult
cooldownState
runtimeValidationResult
```

템플릿:

```text
## Aggressive Mode Summary

- Enabled: <REQUIRED>
- Candidate Count: <REQUIRED>
- Activation Count: <REQUIRED>
- Activation Gate Result: <REQUIRED>
- Failed Gates: <REQUIRED OR NONE>
- Policy Bundle: <REQUIRED OR NOT_APPLICABLE>
- Active Duration: <REQUIRED OR NOT_APPLICABLE>
- Deactivation Reason: <REQUIRED OR NOT_APPLICABLE>
- Rollback Result: <REQUIRED OR NOT_APPLICABLE>
- Cooldown State: <REQUIRED OR NOT_APPLICABLE>
- Runtime Validation Result: <REQUIRED>
```

BLOCKER 기준:

```text
Verify 전 Active
Unknown / MonitoringOnly / AntiCheatSensitive Profile에서 Active
부분 적용 후 Active
RuntimeValidation BLOCKER 상태에서 Active 유지
Aggressive Mode Evidence 누락
```

## 19. Long Soak Summary

다음 필드를 포함한다.

```text
soakExecuted
soakDuration
heartbeatProgression
snapshotFreshnessOverTime
timelineMonotonicity
policyActivationCount
policyRejectionCount
policyThrashingDetected
memoryGrowthSummary
cpuOverheadSummary
rollbackStateConsistency
shutdownCleanliness
```

템플릿:

```text
## Long Soak Summary

- Executed: <REQUIRED>
- Duration: <REQUIRED OR NOT_RUN>
- Heartbeat Progression: <REQUIRED OR NOT_RUN>
- Snapshot Freshness: <REQUIRED OR NOT_RUN>
- Timeline Monotonicity: <REQUIRED OR NOT_RUN>
- Policy Thrashing: <REQUIRED OR NOT_RUN>
- Memory Growth: <REQUIRED OR NOT_RUN>
- CPU Overhead: <REQUIRED OR NOT_RUN>
- Rollback State Consistency: <REQUIRED OR NOT_RUN>
- Shutdown Cleanliness: <REQUIRED OR NOT_RUN>
```

## 20. Evidence Completeness Summary

필수 artifact를 표로 작성한다.

```text
## Evidence Completeness Summary

| Artifact | Required | Present | Valid | Hash | Severity if Missing | Path |
|---|---:|---:|---:|---|---|---|
| Session Manifest | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Runtime Snapshot Latest | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Runtime Snapshot Final | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Policy Timeline | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Performance Summary | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Rollback Summary | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Runtime Validation Summary | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Evidence Completeness Report | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Classification Result | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
| Artifact Manifest | Yes | <REQUIRED> | <REQUIRED> | <REQUIRED> | BLOCKER | <REQUIRED> |
```

규칙:

```text
Present=true만으로 Valid=true가 되지 않는다.
파일 존재, 포맷, 필수 필드, hash, 시간 정합성을 분리해 검증한다.
```

## 21. WARN / REGRESSION / BLOCKER / FATAL 목록

각 severity를 별도 표로 작성한다.

템플릿:

```text
## Findings

### FATAL

| ID | Source Gate | Description | Evidence | Resolution |
|---|---|---|---|---|
| <REQUIRED OR NONE> | | | | |

### BLOCKER

| ID | Source Gate | Description | Evidence | Resolution |
|---|---|---|---|---|
| <REQUIRED OR NONE> | | | | |

### REGRESSION

| ID | Metric / Source | Description | Evidence | Resolution |
|---|---|---|---|---|
| <REQUIRED OR NONE> | | | | |

### WARN

| ID | Source | Description | Evidence | Accepted Reason |
|---|---|---|---|---|
| <REQUIRED OR NONE> | | | | |
```

규칙:

```text
BLOCKER가 NONE이면 명시적으로 `None`을 기록한다.
빈 표를 BLOCKER 0의 근거로 사용하지 않는다.
집계 수치와 Finding 행 수가 일치해야 한다.
```

## 22. Regression Matrix Summary

다음 필드를 포함한다.

```text
totalRequirementCount
mappedRequirementCount
unmappedRequirementCount
totalTestCount
executedTestCount
passedTestCount
failedTestCount
notRunTestCount
notImplementedTestCount
blockerRequirementWithoutTestCount
requiredEvidenceMappedCount
coverageState
```

템플릿:

```text
## Regression Matrix Summary

- Coverage State: <REQUIRED>
- Requirements:
  - Total: <REQUIRED>
  - Mapped: <REQUIRED>
  - Unmapped: <REQUIRED>
- Tests:
  - Total: <REQUIRED>
  - Executed: <REQUIRED>
  - PASS: <REQUIRED>
  - FAIL: <REQUIRED>
  - NOT_RUN: <REQUIRED>
  - NOT_IMPLEMENTED: <REQUIRED>
- BLOCKER Requirements Without Test: <REQUIRED>
- Required Evidence Mapped: <REQUIRED>
```

BLOCKER 기준:

```text
BLOCKER requirement without test > 0
BLOCKER test NOT_IMPLEMENTED
필수 Evidence와 연결되지 않은 BLOCKER test
```

## 23. False Pass Validation Summary

다음 필드를 포함한다.

```text
falsePassScenarioCount
executedScenarioCount
detectedScenarioCount
missedScenarioCount
unexpectedExitCodeCount
reportMismatchCount
```

템플릿:

```text
## False Pass Validation Summary

| Scenario | Injected Failure | Expected Decision | Expected Exit Code | Observed Decision | Observed Exit Code | Status |
|---|---|---|---:|---|---:|---|
| FP-001 | Missing EXE SHA-256 | BLOCKER | Non-zero | <REQUIRED> | <REQUIRED> | <REQUIRED> |
```

필수 시나리오:

```text
missing EXE SHA-256
timeline event count zero
timeline monotonicity failure
missing rollback summary
rollback failure
RuntimeValidation BLOCKER
Evidence write failure
nested check FAIL
child process non-zero exit
report decision mismatch
Unknown Profile mutation
Aggressive Mode partial apply
failed rollback state discarded
Processor Group group 0 hardcoding
```

BLOCKER 기준:

```text
주입한 BLOCKER가 PASS로 종료
BLOCKER 주입에서 exit code 0
Markdown / 구조화 보고서 판정 불일치
```

## 24. Source Traceability

보고서의 핵심 필드가 어느 artifact에서 왔는지 기록한다.

템플릿:

```text
## Source Traceability

| Report Field | Source Artifact | Source Field | Artifact Hash | Validation Result |
|---|---|---|---|---|
| Final Decision | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| EXE SHA-256 | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Rollback Result | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| RuntimeValidation State | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
| Performance Classification | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
```

규칙:

```text
핵심 필드의 source artifact 또는 source field가 없으면 해당 값을 승인 근거로 사용하지 않는다.
```

## 25. Manual Verification

자동화되지 않은 항목을 별도로 기록한다.

템플릿:

```text
## Manual Verification

| Manual Check ID | Description | Procedure | Reviewer | Reviewed UTC | Result | Evidence |
|---|---|---|---|---|---|---|
| <REQUIRED OR NONE> | | | | | | |
```

규칙:

```text
Reviewer가 없으면 Manual PASS로 인정하지 않는다.
Procedure가 없으면 재현 가능한 검증으로 인정하지 않는다.
```

## 26. Final Decision

최종 판정 섹션은 다음 형식을 사용한다.

```text
## Final Decision

- Final Decision: <REQUIRED>
- Final Severity: <REQUIRED>
- Exit Code: <REQUIRED>
- Approval Eligible: <REQUIRED>
- Performance Improvement Claim Allowed: <REQUIRED>
- Profile Promotion Allowed: <REQUIRED>
- Decision Reasons:
  1. <REQUIRED>
  2. <OPTIONAL>
- Blocking Findings:
  - <REQUIRED OR NONE>
- Accepted Warnings:
  - <REQUIRED OR NONE>
```

### PASS 조건

```text
FATAL 0
BLOCKER 0
Rollback 성공
RuntimeValidation BLOCKER 0
필수 Evidence 완전
Release binary 식별 가능
Gate 내부 FAIL 0
exit code 0
```

### PASS_WITH_WARNINGS 조건

```text
FATAL 0
BLOCKER 0
Rollback 성공
RuntimeValidation BLOCKER 0
필수 Evidence 완전
허용된 WARN만 존재
```

### NEUTRAL 조건

```text
안전성 Gate 통과
성능 개선 불명확
성능 개선 주장 금지
Validated Profile 승격 금지
```

### REGRESSION 조건

```text
성능 회귀 확인
안전성 PASS 여부와 별개로 성능 승인 불가
```

### BLOCKER 조건

```text
Rollback failure
RuntimeValidation BLOCKER
Evidence fatal missing
ApplyGuard 우회
Processor Group unsafe fallback
Profile mutation violation
Gate aggregation false pass
```

### INVALID_RUN 조건

```text
Build identity 불명확
target identity 불명확
Baseline / Applied 비교 구간 불성립
필수 테스트 환경 구성 실패
```

### FATAL 조건

```text
Gate 실행 자체 실패
판정 데이터 손상
필수 Evidence writer 전체 실패
보고서 신뢰성 확보 불가
```

## 27. Approval

실제 RC 승인자가 사용할 섹션을 작성한다.

```text
## Approval

- Technical Reviewer: <REQUIRED>
- Safety Reviewer: <REQUIRED>
- Release Approver: <REQUIRED>
- Approval Status: <REQUIRED>
- Approval UTC: <REQUIRED>
- Approval Notes: <OPTIONAL>
```

Approval Status:

```text
APPROVED
APPROVED_WITH_WARNINGS
REJECTED
PENDING
```

규칙:

```text
Final Decision이 BLOCKER / INVALID_RUN / FATAL이면 APPROVED 금지.
```

## 28. Artifact Index

모든 산출물을 나열한다.

템플릿:

```text
## Artifact Index

| Artifact | Path | SHA-256 | Created UTC | Required | Validation Result |
|---|---|---|---|---:|---|
| <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> | <REQUIRED> |
```

## 29. Machine-readable Mapping

구조화된 보고서와 연결할 필드 목록을 정의한다.

필수 최상위 필드:

```text
reportVersion
runId
targetVersion
testKind
buildIdentity
sessionIdentity
environment
gameProfile
gateResults
runtimeValidation
rollback
performance
aggressiveMode
soak
evidenceCompleteness
findings
regressionCoverage
falsePassValidation
traceability
finalDecision
approval
artifactIndex
```

규칙:

```text
구체 JSON Schema는 본 문서에서 확정하지 않아도 된다.
단, Markdown 섹션과 구조화된 필드가 1:1로 매핑 가능해야 한다.
```

## 30. Template Validation Rules

RC Report Template 자체의 검증 규칙을 정의한다.

1. 필수 섹션 누락 금지
2. 필수 필드 placeholder 유지 여부 검사
3. 실제 보고서 생성 시 `<REQUIRED>` 잔존 금지
4. 집계 수치와 상세 행 수 일치
5. Gate Status와 Final Decision 일치
6. Severity Count와 Finding 목록 일치
7. Final Decision과 Exit Code 일치
8. Markdown과 구조화된 보고서 판정 일치
9. Artifact Hash와 Source Traceability 일치
10. Approval Status와 Final Decision 일치

실제 보고서에 `<REQUIRED>`가 남아 있으면:

```text
INVALID_RUN 또는 BLOCKER 후보
```

## 31. Non-Goals

다음은 RC_Report_Template_v3.1.md의 비목표다.

```text
실제 RC 결과 작성
실제 테스트 PASS 판정
RC Report Generator 구현
JSON Schema 구현
Release Gate script 구현
CI/CD 구현
자동 배포 구현
설치 프로그램 작성
공식 지원 게임 선언
성능 향상 보장
```

## 32. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. RC Report의 필수 구조가 정의되어 있다.
2. Build Identity 섹션이 정의되어 있다.
3. Session Identity와 Test Environment 섹션이 정의되어 있다.
4. Game Profile Summary가 정의되어 있다.
5. Gate Summary와 Detailed Gate Results가 정의되어 있다.
6. Static Safety Results가 정의되어 있다.
7. RuntimeValidation Summary가 정의되어 있다.
8. Policy Timeline Summary가 정의되어 있다.
9. Scheduler / ApplyGuard Summary가 정의되어 있다.
10. Rollback Summary가 정의되어 있다.
11. Performance Validation Summary가 정의되어 있다.
12. Aggressive Mode Summary가 정의되어 있다.
13. Long Soak Summary가 정의되어 있다.
14. Evidence Completeness Summary가 정의되어 있다.
15. Finding 목록이 severity별로 정의되어 있다.
16. Regression Matrix Summary가 정의되어 있다.
17. False Pass Validation Summary가 정의되어 있다.
18. Source Traceability가 정의되어 있다.
19. Manual Verification과 Approval 섹션이 정의되어 있다.
20. Final Decision 규칙이 정의되어 있다.
21. Artifact Index가 정의되어 있다.
22. Markdown과 구조화된 보고서 mapping이 정의되어 있다.
23. Template 자체의 validation rule이 정의되어 있다.
24. BLOCKER가 존재할 때 PASS를 생성하지 않는 원칙이 포함되어 있다.

## 33. Open Questions

1. 구조화된 RC Report 형식은 JSON으로 확정할 것인가?
2. Markdown과 JSON 중 어느 형식을 source of truth로 둘 것인가?
3. RC Report 생성기는 C++ runtime 내부인가, 외부 Python script인가?
4. Technical Reviewer와 Release Approver를 동일 인물로 허용할 것인가?
5. PASS_WITH_WARNINGS의 승인 권한은 누구에게 둘 것인가?
6. Long Soak가 NOT_RUN인 RC를 허용할 것인가?
7. Actual Game Validation이 NOT_RUN인 RC를 허용할 것인가?
8. NEUTRAL 결과에서 Approval Eligible을 true로 둘 것인가?
9. Candidate Profile의 Validated 승격 여부를 RC Report에서 결정할 것인가?
10. Artifact Index에 모든 로그 파일을 포함할 것인가, 필수 artifact만 포함할 것인가?
11. Source Traceability를 모든 보고서 필드에 적용할 것인가, 핵심 필드에만 적용할 것인가?
12. Manual Verification 결과에 전자서명 또는 별도 승인 기록이 필요한가?
13. 실제 보고서에 `<REQUIRED>`가 남으면 BLOCKER인가 INVALID_RUN인가?
14. Report Template 버전과 Product Version을 별도로 관리할 것인가?

## 작성 규칙

1. 실제 RC 결과를 작성하지 않는다.
2. placeholder를 실제 값처럼 채우지 않는다.
3. BLOCKER를 WARN으로 낮추지 않는다.
4. 확인되지 않은 값을 추측하지 않는다.
5. 보고서 생성 성공을 RC PASS로 간주하지 않는다.
6. 실행되지 않은 Gate를 PASS로 기록하지 않는다.
7. Evidence 없는 결과를 VERIFIED로 기록하지 않는다.
8. Candidate Profile을 Validated로 자동 승격하지 않는다.
9. 실제 게임 검증을 공식 지원 선언으로 작성하지 않는다.
10. Markdown 형식으로 작성한다.
