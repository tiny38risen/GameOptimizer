> Status: Archived  
> 이 문서는 과거 기록입니다. 현재 기준 문서가 아닙니다.

# GameOptimizer v3.1 Patch Plan — Phase 7: Game Profile Integration
> Archive notice: This Phase Patch Plan is historical. Active implementation work and execution status are tracked in GitHub Issues. This file is not a current source of truth for work ordering, completion status, or release approval.

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 7

버전: v1.0

작성 목적: GameOptimizer v3.1의 일곱 번째 구현 단계인 Phase 7 — Game Profile Integration을 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, Game Profile 정보를 런타임 정책 결정과 Evidence 체계에 안전하게 연결하기 위한 작업 계획 문서다.

적용 범위:

- Game Profile 런타임 계약(Runtime Contract) 정렬
- profile data model과 validation boundary 정리
- profile registry lookup과 fallback 흐름 정의
- target identity와 profile binding 기준 정리
- Unknown Default Profile fallback 기준 정리
- profile status별 정책 허용 범위 정의
- allowedPolicies, blockedPolicies, conditionalPolicies 적용 순서 정리
- confidence, topology, rollback requirement gate와 profile gate 연결
- MonitoringOnly 및 AntiCheatSensitive 안전 fallback 정리
- profile selection Evidence와 RuntimeValidation check 정리
- Phase 7 회귀 테스트와 Evidence 기준 정리

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 특정 게임의 공식 지원 선언
- 특정 게임의 실행 파일명 확정
- 특정 게임의 engine structure 확정
- Anti-Cheat 위험도 확정
- 성능 개선 수치 보장
- registry mutation 정책 신규 도입
- SchedulerController apply internals 변경
- group 0 fallback로 profile 실패를 우회하는 동작 도입

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/archive/patch-plans/PatchPlan_Phase6.md`
- `docs/proposals/performance/GameProfileSpecification.md`
- `docs/proposals/performance/PolicySpecification.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/modules/MDS-009_RuntimeOrchestrator.md`

후속 문서:

- `docs/archive/patch-plans/PatchPlan_Phase8.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 7의 목표는 게임별 성능 향상을 보장하는 것이 아니라, 검증된 범위 안에서만 정책을 허용하고 정보가 부족하면 안전하게 비적용하는 런타임 제한 체계를 만드는 것이다.

## 2. Phase 7 목표

Phase 7의 핵심 목표는 Game Profile을 런타임 정책 결정의 제한 조건으로 연결하는 것이다. profile은 성능 개선을 주장하는 문서가 아니라, 어떤 정책이 허용 가능한지, 어떤 조건에서 차단해야 하는지, 정보가 부족할 때 어떤 안전 fallback을 선택해야 하는지 결정하는 제한 계약이다.

필수 목표:

1. GameProfileSpecification과 Runtime Contract의 용어를 정렬한다.
2. profile data model의 필수 필드와 validation boundary를 정리한다.
3. registry lookup 실패 시 `Unknown Default Profile`을 선택한다.
4. target process identity가 불명확하면 mutation policy를 생성하지 않는다.
5. `profileStatus`가 `Candidate`이면 `Validated`보다 보수적으로 처리한다.
6. `profileStatus`가 `MonitoringOnly`이면 mutation policy를 생성하거나 전달하지 않는다.
7. `AntiCheatSensitive` profile은 모든 mutation을 차단한다.
8. `blockedPolicies`가 항상 `allowedPolicies`보다 우선하도록 한다.
9. `conditionalPolicies`는 evidence와 runtime condition을 모두 만족할 때만 허용한다.
10. confidence, topology, rollback requirement gate 실패를 profile fallback과 연결한다.
11. profile selection과 policy rejection 사유를 Evidence로 남긴다.
12. RuntimeValidationMonitor가 profile 상태와 실제 mutation 결과를 검증한다.

정량 기준 후보:

- `MonitoringOnly` 또는 `AntiCheatSensitive` 상태에서 생성된 mutation policy count는 0이어야 한다.
- profile lookup 실패는 100% `Unknown Default Profile` evidence로 기록되어야 한다.
- `blockedPolicies` 충돌이 있는 policy dispatch count는 0이어야 한다.
- target identity가 `TBD` 또는 `Unknown`인 상태에서 apply mode policy dispatch count는 0이어야 한다.
- profile fallback reason 누락은 RC PASS를 차단하는 BLOCKER 후보로 분류한다.

## 3. Phase 7 비목표

Phase 7에서 특정 게임의 성능 특성, 실행 파일명, Anti-Cheat 위험도를 검증 없이 확정하지 않는다.

비목표:

1. 특정 게임을 `Validated`로 자동 승격하지 않는다.
2. 특정 게임의 targetProcessName을 추정값으로 확정하지 않는다.
3. 특정 게임의 engineType, bottleneck type, Anti-Cheat risk를 단정하지 않는다.
4. profile 문서를 공식 지원 게임 목록으로 해석하지 않는다.
5. performance gain 수치를 산출하거나 보장하지 않는다.
6. profile selection 실패를 aggressive default policy로 복구하지 않는다.
7. group 0 제한을 profile fallback 대체 수단으로 사용하지 않는다.
8. `Unknown Default Profile`에서 mutation을 허용하지 않는다.
9. `Candidate` profile을 `Validated` profile과 같은 권한으로 처리하지 않는다.
10. Anti-Cheat 우회 또는 회피 로직을 설계하지 않는다.

검증되지 않은 게임 정보는 `TBD` 또는 `Unknown`으로 유지한다. Game Profile은 런타임 제한 계약이며, 성능 주장 또는 공식 지원 선언이 아니다.

## 4. 영향 모듈

직접 영향 모듈:

```text
GameProfileRegistry
GameProfileLoader
GameProfileValidator
RuntimeContext
RuntimeOrchestrator
PerformanceEngine
PolicyResolver
PolicyDispatcher
RuntimeValidationMonitor
RuntimeSnapshotRecorder
PerformanceEvidenceRecorder
```

간접 영향 모듈:

```text
ThreadTracker
MainThreadConfidenceAnalyzer
TopologyAnalyzer
RollbackManager
PolicyTimelineRecorder
EvidenceCompletenessValidator
RCReportGenerator
```

영향 문서:

```text
docs/proposals/performance/GameProfileSpecification.md
docs/proposals/performance/PolicySpecification.md
docs/evidence/EvidenceSpecification.md
docs/validation/PerformanceValidationPlan.md
docs/release/ReleaseChecklist_v3.1.md
docs/release/RC_Runbook_v3.1.md
docs/modules/MDS-007_PerformanceEvidencePlanner.md
docs/modules/MDS-008_PolicyDispatcher.md
docs/modules/MDS-009_RuntimeOrchestrator.md
```

Phase 7은 module core observation logic보다 profile-driven policy gating boundary에 집중한다.

## 5. 패치 단위 개요

Phase 7은 다음 Patch ID 구조를 사용한다.

```text
P7-01 Game Profile Runtime Contract Alignment
P7-02 Profile Data Model and Validation Boundary
P7-03 Profile Registry and Lookup
P7-04 Target Identity and Profile Binding
P7-05 Unknown Default Profile Fallback
P7-06 Profile Status Enforcement
P7-07 Allowed / Blocked Policy Enforcement
P7-08 Conditional Policy Gate
P7-09 Confidence / Topology / Rollback Requirement Gate
P7-10 Monitoring-only and Anti-Cheat Safety Fallback
P7-11 Profile Selection Evidence Integration
P7-12 RuntimeValidation Profile Checks
P7-13 Phase 7 Regression Test and Evidence
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

패치 하나가 profile data contract, runtime mutation path, release classification을 동시에 변경해서는 안 된다.

## 6. P7-01 Game Profile Runtime Contract Alignment

Patch ID: `P7-01`

목적:

GameProfileSpecification의 용어와 Runtime Contract에서 사용하는 profile-related 상태값을 정렬한다.

작업 범위:

```text
profileId 용어 정렬
profileType 용어 정렬
profileStatus 용어 정렬
defaultMode 용어 정렬
allowedPolicies / blockedPolicies / conditionalPolicies 의미 정렬
Unknown Default Profile 의미 정렬
MonitoringOnly mutation 금지 의미 정렬
AntiCheatSensitive mutation 금지 의미 정렬
```

수정 가능 파일:

```text
docs/proposals/performance/GameProfileSpecification.md
docs/proposals/performance/PolicySpecification.md
docs/evidence/EvidenceSpecification.md
docs/modules/MDS-008_PolicyDispatcher.md
docs/modules/MDS-009_RuntimeOrchestrator.md
관련 계약 문서
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager execution internals
ThreadTracker sampling internals
구체 게임별 validated profile 선언
```

구현 규칙:

```text
문서 간 동일한 profile term은 같은 의미로 사용한다.
profileStatus는 자동 승격 가능한 runtime flag로 취급하지 않는다.
Candidate는 검증 후보이며 Validated와 동일하게 취급하지 않는다.
MonitoringOnly는 mutation policy를 생성하지 않는 상태로 정의한다.
AntiCheatSensitive는 모든 mutation을 차단하는 상태로 정의한다.
```

완료 기준:

```text
profile 용어 목록이 문서 간 충돌 없이 정렬된다.
Runtime Contract에서 사용하는 profile state와 GameProfileSpecification 상태가 매핑된다.
Unknown, Candidate, Validated, Restricted, MonitoringOnly 의미가 분리된다.
```

필수 테스트:

```text
profile terminology review test
runtime contract mapping review test
status meaning collision review test
monitoring-only mutation prohibition review test
```

필수 Evidence:

```text
profileContractMappingSummary
profileTerminologyReviewResult
profileStatusMeaningMap
contractAlignmentDecisionLog
```

리스크:

```text
같은 용어가 문서별로 다른 의미를 가지면 profile gate가 우회될 수 있다.
Candidate를 Validated와 동일하게 취급하면 검증되지 않은 mutation이 발생할 수 있다.
```

Rollback 계획:

```text
용어 충돌이 발견되면 GameProfileSpecification을 source of truth로 유지한다.
불명확한 status는 Candidate 또는 MonitoringOnly보다 높은 권한으로 해석하지 않는다.
```

## 7. P7-02 Profile Data Model and Validation Boundary

Patch ID: `P7-02`

목적:

Game Profile data model의 필수 필드와 validation boundary를 정리하고, schema 누락 또는 불완전 profile을 안전하게 차단한다.

작업 범위:

```text
필수 profile field 목록 정리
profileStatus validation rule 정리
targetProcessName TBD 허용 조건 정리
antiCheatRisk Unknown 허용 조건 정리
allowedPolicies / blockedPolicies / conditionalPolicies 필수성 정리
validationRequired 처리 기준 정리
profile corruption 처리 기준 정리
```

수정 가능 파일:

```text
GameProfileValidator
GameProfileLoader
GameProfileRegistry
docs/proposals/performance/GameProfileSpecification.md
docs/evidence/EvidenceSpecification.md
관련 테스트
```

수정 금지 파일:

```text
PolicyDispatcher mutation call path
SchedulerController apply internals
특정 게임별 확정 profile 값
RC PASS classification rule
```

구현 규칙:

```text
필수 필드 누락은 안전 fallback 또는 BLOCKER 후보로 분류한다.
targetProcessName이 TBD인 profile은 target binding에 사용할 수 없다.
antiCheatRisk가 Unknown이면 mutation 허용 조건을 강화한다.
blockedPolicies 필드 누락은 허용 정책 목록보다 위험하게 분류한다.
validationRequired=true는 자동 apply 허용 근거가 아니다.
```

완료 기준:

```text
필수 profile field 목록이 정의된다.
profile validation 실패가 Unknown Default Profile 또는 MonitoringOnly로 연결된다.
profile corruption이 aggressive default로 복구되지 않는다.
```

필수 테스트:

```text
missing profileId validation test
missing blockedPolicies validation test
TBD targetProcessName validation test
Unknown antiCheatRisk conservative gate test
corrupted profile fallback test
```

필수 Evidence:

```text
profileValidationResult
profileValidationErrorList
profileRequiredFieldStatus
profileFallbackReason
profileCorruptionDetected
```

리스크:

```text
필수 필드를 WARN으로만 처리하면 mutation gate가 잘못 열릴 수 있다.
schema가 느슨하면 검증되지 않은 profile이 Validated처럼 동작할 수 있다.
```

Rollback 계획:

```text
validation rule이 불안정하면 profile을 MonitoringOnly로 강제 fallback한다.
필수 필드 판단이 불명확하면 mutation policy 생성을 차단한다.
```

## 8. P7-03 Profile Registry and Lookup

Patch ID: `P7-03`

목적:

GameProfileRegistry lookup 흐름을 정리하고, profile not found 상태를 `Unknown Default Profile`로 안전하게 연결한다.

작업 범위:

```text
profile registry source 정리
lookup key 우선순위 정리
lookup 실패 reason 분류
duplicate profile 처리 기준 정리
registry load failure 처리 기준 정리
Unknown Default Profile fallback 연결
```

수정 가능 파일:

```text
GameProfileRegistry
GameProfileLoader
GameProfileValidator
RuntimeContext
RuntimeOrchestrator
관련 테스트
```

수정 금지 파일:

```text
PolicyResolver aggressive default rule
PolicyDispatcher mutation execution path
SchedulerController apply internals
game-specific official support list
```

구현 규칙:

```text
profile not found는 오류 은폐가 아니라 Unknown Default Profile selection으로 기록한다.
duplicate profile은 임의 선택하지 않는다.
registry load failure는 mutation 허용 근거가 될 수 없다.
lookup 실패 후 group 0 또는 default aggressive policy로 복구하지 않는다.
```

완료 기준:

```text
lookup success, not found, duplicate, load failure가 구분된다.
lookup 실패 시 Unknown Default Profile이 선택된다.
lookup failure reason이 Evidence에 남는다.
```

필수 테스트:

```text
profile lookup success test
profile not found fallback test
duplicate profile block test
registry load failure fallback test
lookup failure no aggressive policy test
```

필수 Evidence:

```text
profileLookupResult
profileLookupKey
profileLookupFailureReason
selectedProfileId
unknownDefaultProfileApplied
```

리스크:

```text
lookup 실패를 silent default로 처리하면 실제로 어떤 profile이 적용되었는지 추적할 수 없다.
중복 profile을 임의 선택하면 runtime 재현성이 깨질 수 있다.
```

Rollback 계획:

```text
registry lookup이 불안정하면 Unknown Default Profile만 허용한다.
duplicate 또는 load failure 상태에서는 mutation policy 생성을 차단한다.
```

## 9. P7-04 Target Identity and Profile Binding

Patch ID: `P7-04`

목적:

target process identity와 selected profile을 안전하게 binding하고, identity가 불명확하면 mutation을 허용하지 않는다.

작업 범위:

```text
target process identity source 정리
profile targetProcessName binding rule 정리
exe identity evidence 연결
process identity mismatch 처리
TBD targetProcessName 처리
profile binding freshness 기준 정리
```

수정 가능 파일:

```text
RuntimeContext
RuntimeOrchestrator
GameProfileRegistry
RuntimeSnapshotRecorder
PerformanceEvidenceRecorder
관련 테스트
```

수정 금지 파일:

```text
ThreadTracker process discovery internals
SchedulerController apply internals
특정 게임 targetProcessName 확정값
```

구현 규칙:

```text
targetProcessName이 TBD이면 확정 binding으로 사용하지 않는다.
target identity mismatch는 profile fallback 또는 mutation block으로 처리한다.
profile binding은 process identity evidence와 함께 기록한다.
identity가 불완전하면 Candidate라도 mutation을 허용하지 않는다.
```

완료 기준:

```text
target identity와 selected profile binding 결과가 생성된다.
TBD 또는 Unknown identity에서 mutation policy가 생성되지 않는다.
identity mismatch가 Evidence에 남는다.
```

필수 테스트:

```text
target identity binding success test
TBD targetProcessName mutation block test
identity mismatch fallback test
stale identity binding rejection test
profile binding evidence test
```

필수 Evidence:

```text
targetProcessIdentity
profileBindingResult
profileBindingReason
profileBindingFreshness
identityMismatchDetected
```

리스크:

```text
process identity가 약하면 다른 process에 profile 제한을 잘못 적용할 수 있다.
TBD 값을 실제 target identity로 취급하면 검증되지 않은 게임 정보가 확정값처럼 동작한다.
```

Rollback 계획:

```text
identity binding이 불완전하면 selected profile을 Unknown Default Profile로 제한한다.
identity mismatch 상태에서는 모든 mutation policy dispatch를 차단한다.
```

## 10. P7-05 Unknown Default Profile Fallback

Patch ID: `P7-05`

목적:

profile selection 실패, identity 불명확, evidence 부족, profile corruption 상태에서 `Unknown Default Profile`을 적용하는 fallback 흐름을 정리한다.

작업 범위:

```text
Unknown Default Profile entry 정의 확인
fallback trigger 목록 정리
monitoring-only 또는 dry-run mode 연결
aggressive policy 차단 rule 연결
fallback reason Evidence 연결
fallback after profile corruption 처리
```

수정 가능 파일:

```text
GameProfileRegistry
GameProfileValidator
RuntimeContext
PolicyResolver
PolicyDispatcher
PerformanceEvidenceRecorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback execution internals
validated profile promotion logic
```

구현 규칙:

```text
profile not found는 Unknown Default Profile로 fallback한다.
Unknown Default Profile은 monitoring-only 또는 dry-run만 허용한다.
Unknown Default Profile은 aggressive mode를 허용하지 않는다.
Unknown Default Profile은 group 0 제한을 자동 적용하지 않는다.
fallback reason은 Evidence에 반드시 남긴다.
```

완료 기준:

```text
fallback trigger별 Unknown Default Profile 적용 결과가 정의된다.
Unknown Default Profile에서 mutation policy dispatch count가 0이다.
fallback reason 누락이 BLOCKER 후보로 분류된다.
```

필수 테스트:

```text
profile not found unknown fallback test
identity unknown fallback test
profile corruption unknown fallback test
unknown default no mutation test
unknown default no group zero fallback test
```

필수 Evidence:

```text
unknownDefaultProfileApplied
unknownDefaultFallbackReason
unknownDefaultAllowedMode
mutationPolicyBlockedByUnknownDefault
profileSelectionFailureDetail
```

리스크:

```text
Unknown Default Profile이 느슨하면 정보 부족 상태에서 mutation이 실행될 수 있다.
fallback reason이 없으면 release review에서 비적용 판단을 검증할 수 없다.
```

Rollback 계획:

```text
Unknown fallback 구현이 불안정하면 모든 unknown profile 상태를 MonitoringOnly로 고정한다.
fallback reason 기록 실패 시 RC PASS를 차단한다.
```

## 11. P7-06 Profile Status Enforcement

Patch ID: `P7-06`

목적:

`Draft`, `Candidate`, `Validated`, `Restricted`, `MonitoringOnly`, `Deprecated` 상태별 런타임 권한을 명확히 적용한다.

작업 범위:

```text
profileStatus별 default mode 정리
Candidate conservative behavior 정리
Validated gate 조건 정리
Restricted policy 제한 정리
MonitoringOnly mutation 금지 정리
Deprecated profile 차단 정리
auto-promotion 금지 rule 정리
```

수정 가능 파일:

```text
GameProfileValidator
RuntimeContext
PolicyResolver
PolicyDispatcher
RuntimeValidationMonitor
관련 테스트
```

수정 금지 파일:

```text
GameProfileSpecification의 특정 게임 Validated 선언
SchedulerController apply internals
Release PASS classification 우회 rule
```

구현 규칙:

```text
Candidate는 Validated보다 보수적으로 동작한다.
Candidate는 자동으로 Validated로 승격되지 않는다.
MonitoringOnly는 mutation policy를 생성하거나 전달하지 않는다.
Deprecated는 적용 대상에서 제외한다.
Validated라도 blockedPolicies, rollback requirement, RuntimeValidation gate를 통과해야 한다.
```

완료 기준:

```text
profileStatus별 허용 동작이 분리된다.
Candidate 자동 승격 경로가 없다.
MonitoringOnly에서 mutation policy count가 0이다.
Deprecated profile이 runtime apply 대상이 아니다.
```

필수 테스트:

```text
Candidate conservative gate test
Candidate no auto promotion test
Validated still requires gates test
MonitoringOnly no mutation policy test
Deprecated profile rejected test
```

필수 Evidence:

```text
profileStatus
profileStatusEnforcementResult
profileStatusPolicyLimit
candidateConservativeGateApplied
monitoringOnlyMutationBlocked
```

리스크:

```text
status enforcement가 느슨하면 Candidate가 Validated와 같은 권한으로 실행될 수 있다.
MonitoringOnly enforcement가 PolicyDispatcher 이후에만 적용되면 mutation candidate가 잘못 생성될 수 있다.
```

Rollback 계획:

```text
status enforcement가 불안정하면 Candidate, Restricted, MonitoringOnly, Deprecated를 모두 mutation 금지로 제한한다.
Validated도 required evidence가 없으면 MonitoringOnly로 fallback한다.
```

## 12. P7-07 Allowed / Blocked Policy Enforcement

Patch ID: `P7-07`

목적:

profile의 `allowedPolicies`와 `blockedPolicies`를 정책 생성 및 dispatch 전에 적용하고, `blockedPolicies`가 항상 우선하도록 한다.

작업 범위:

```text
allowedPolicies matching rule 정리
blockedPolicies matching rule 정리
blocked-over-allowed precedence 정리
policy rejection reason 정리
blocked policy Evidence 연결
policy resolver와 dispatcher boundary 정리
```

수정 가능 파일:

```text
PolicyResolver
PolicyDispatcher
GameProfileValidator
RuntimeContext
PolicyTimelineRecorder
PerformanceEvidenceRecorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager state save internals
Profile registry lookup algorithm beyond gate input
```

구현 규칙:

```text
blockedPolicies는 항상 allowedPolicies보다 우선한다.
blocked policy는 생성하지 않거나 dispatch 전에 반드시 reject한다.
blocked policy rejection은 policy timeline과 Evidence에 남긴다.
allowedPolicies에 있더라도 blockedPolicies와 충돌하면 차단한다.
blockedPolicies 누락 profile은 BLOCKER 후보로 분류한다.
```

완료 기준:

```text
blocked-over-allowed precedence가 적용된다.
blocked policy dispatch count가 0이다.
policy rejection reason이 Evidence에 남는다.
```

필수 테스트:

```text
blocked overrides allowed test
allowed policy permitted after gate test
blocked policy no dispatch test
missing blockedPolicies blocker test
policy rejection evidence test
```

필수 Evidence:

```text
allowedPolicyList
blockedPolicyList
policyBlockedByProfile
policyBlockReason
blockedPolicyDispatchCount
```

리스크:

```text
allowedPolicies를 먼저 신뢰하면 blockedPolicies가 우회될 수 있다.
policy rejection reason이 없으면 비적용 판단이 회귀 테스트에서 검증되지 않는다.
```

Rollback 계획:

```text
precedence 판단이 불명확하면 해당 policy를 차단한다.
blockedPolicies 필드가 없으면 profile 전체를 MonitoringOnly로 fallback한다.
```

## 13. P7-08 Conditional Policy Gate

Patch ID: `P7-08`

목적:

`conditionalPolicies`를 required evidence와 runtime condition이 모두 충족된 경우에만 허용하도록 한다.

작업 범위:

```text
conditional policy 목록 정리
required evidence 매핑 정리
runtime condition 매핑 정리
condition evaluation result 기록
condition missing fallback 정리
policy hold / reject reason 정리
```

수정 가능 파일:

```text
PolicyResolver
RuntimeContext
RuntimeValidationMonitor
PerformanceEvidenceRecorder
PolicyTimelineRecorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
ThreadTracker sampling internals
TopologyAnalyzer calculation internals
```

구현 규칙:

```text
conditionalPolicies는 evidence와 runtime condition을 모두 만족해야 한다.
required evidence가 없으면 condition은 false로 처리한다.
runtime condition이 stale이면 condition은 false로 처리한다.
conditional policy hold는 apply 성공으로 기록하지 않는다.
condition 충족 여부는 Evidence에 남긴다.
```

완료 기준:

```text
conditional policy별 required evidence와 runtime condition이 매핑된다.
조건 불충족 policy가 dispatch되지 않는다.
condition evaluation result가 Evidence에 남는다.
```

필수 테스트:

```text
conditional policy evidence missing test
conditional policy runtime condition false test
conditional policy stale condition test
conditional policy allowed when all conditions true test
conditional policy evidence recording test
```

필수 Evidence:

```text
conditionalPolicyList
conditionalPolicyEvaluationResult
conditionalPolicyMissingEvidence
conditionalPolicyRuntimeCondition
conditionalPolicyRejectedReason
```

리스크:

```text
condition 결과가 stale이면 과거 상태를 근거로 mutation이 실행될 수 있다.
missing evidence를 pass로 처리하면 검증 전 정책이 열릴 수 있다.
```

Rollback 계획:

```text
condition freshness를 보장할 수 없으면 conditionalPolicies 전체를 reject한다.
required evidence 매핑이 불완전하면 해당 policy를 MonitoringOnly로 제한한다.
```

## 14. P7-09 Confidence / Topology / Rollback Requirement Gate

Patch ID: `P7-09`

목적:

profile policy gate를 confidence, topology, rollback requirement와 연결하여 profile만으로 mutation이 허용되지 않도록 한다.

작업 범위:

```text
confidence gate와 profile gate 연결
topology gate와 profile gate 연결
rollback requirement gate 연결
Processor Group 안전 조건 연결
gate failure reason 통합
profile fallback decision 연결
```

수정 가능 파일:

```text
RuntimeContext
PolicyResolver
PolicyDispatcher
RuntimeValidationMonitor
PerformanceEvidenceRecorder
관련 테스트
```

수정 금지 파일:

```text
MainThreadConfidenceAnalyzer scoring internals
TopologyAnalyzer topology detection internals
RollbackManager rollback execution internals
SchedulerController apply internals
```

구현 규칙:

```text
profile allowed 상태라도 Confidence < High이면 위험 mutation을 차단한다.
topology가 unsafe이면 profile allowed 상태라도 mutation을 차단한다.
rollback state를 저장할 수 없는 policy는 apply 대상이 아니다.
Processor Group 1+ 환경에서 안전하지 않은 process-wide restriction은 차단한다.
gate failure는 profile fallback reason과 policy rejection reason에 연결한다.
```

완료 기준:

```text
profile gate가 confidence, topology, rollback requirement와 함께 평가된다.
rollback 불가 policy가 dispatch되지 않는다.
unsafe topology에서 aggressive policy가 차단된다.
```

필수 테스트:

```text
profile allowed but low confidence block test
unsafe topology policy block test
missing rollback requirement block test
processor group unsafe restriction block test
gate failure evidence test
```

필수 Evidence:

```text
profileGateResult
confidenceGateResult
topologyGateResult
rollbackRequirementGateResult
processorGroupSafetyGateResult
combinedGateFailureReason
```

리스크:

```text
profile만 통과하면 정책을 적용하는 구조가 되면 runtime safety gate가 약해진다.
rollback requirement 누락을 허용하면 Phase 5의 rollback safety 기준이 훼손된다.
```

Rollback 계획:

```text
combined gate evaluation이 불안정하면 가장 보수적인 gate result를 적용한다.
confidence, topology, rollback requirement 중 하나라도 불명확하면 mutation을 차단한다.
```

## 15. P7-10 Monitoring-only and Anti-Cheat Safety Fallback

Patch ID: `P7-10`

목적:

MonitoringOnly 및 AntiCheatSensitive 상태에서 mutation policy가 생성되거나 dispatch되지 않도록 안전 fallback을 적용한다.

작업 범위:

```text
MonitoringOnly mutation policy 생성 금지
AntiCheatSensitive mutation 전체 차단
anti-cheat suspicion fallback 정리
access denied 증가 fallback 정리
protected process suspicion fallback 정리
safety fallback Evidence 연결
```

수정 가능 파일:

```text
RuntimeContext
PolicyResolver
PolicyDispatcher
RuntimeValidationMonitor
PerformanceEvidenceRecorder
PolicyTimelineRecorder
관련 테스트
```

수정 금지 파일:

```text
Anti-Cheat 우회 또는 회피 로직
SchedulerController apply internals
protected process access behavior 변경
특정 게임 antiCheatRisk 확정값
```

구현 규칙:

```text
MonitoringOnly는 mutation policy를 생성하거나 전달하지 않는다.
AntiCheatSensitive는 모든 mutation을 차단한다.
anti-cheat suspicion이 있으면 안전 fallback을 우선한다.
access denied 증가를 성능 최적화 신호로 해석하지 않는다.
Anti-Cheat 위험도 Unknown 상태는 보수적으로 처리한다.
```

완료 기준:

```text
MonitoringOnly mutation policy count가 0이다.
AntiCheatSensitive mutation policy count가 0이다.
safety fallback reason이 Evidence에 남는다.
```

필수 테스트:

```text
MonitoringOnly no mutation creation test
AntiCheatSensitive all mutation blocked test
anti-cheat suspicion fallback test
access denied conservative fallback test
unknown antiCheatRisk conservative behavior test
```

필수 Evidence:

```text
monitoringOnlyReason
antiCheatSensitiveDetected
antiCheatSuspicionReason
mutationBlockedBySafetyFallback
accessDeniedFallbackReason
```

리스크:

```text
MonitoringOnly가 dispatcher 직전에서만 차단되면 upstream policy candidate가 혼란을 만들 수 있다.
Anti-Cheat 관련 신호를 과소평가하면 제품 범위를 벗어난 위험 동작이 발생할 수 있다.
```

Rollback 계획:

```text
safety fallback 판단이 불명확하면 mutation 전체를 차단한다.
Anti-Cheat 관련 값이 Unknown이면 MonitoringOnly로 제한한다.
```

## 16. P7-11 Profile Selection Evidence Integration

Patch ID: `P7-11`

목적:

profile selection, fallback, gate result, policy rejection 사유를 Evidence Bundle에 통합한다.

작업 범위:

```text
profile selection event 기록
profile validation result 기록
profile lookup result 기록
profile binding result 기록
policy gate result 기록
fallback reason 기록
profile fields in runtime snapshot 정리
```

수정 가능 파일:

```text
RuntimeSnapshotRecorder
PerformanceEvidenceRecorder
PolicyTimelineRecorder
RuntimeValidationMonitor
EvidenceCompletenessValidator
관련 테스트
```

수정 금지 파일:

```text
PolicyDispatcher mutation execution semantics
SchedulerController apply internals
성능 개선 판정 공식
```

구현 규칙:

```text
profile selection은 selectedProfileId와 selection reason을 함께 기록한다.
fallback은 fallback reason 없이 성공으로 기록하지 않는다.
policy rejection은 profile gate reason과 연결한다.
Evidence 누락은 PASS를 만들기 위한 default 값으로 채우지 않는다.
```

완료 기준:

```text
profile selection Evidence가 생성된다.
profile fallback reason이 생성된다.
profile gate result가 runtime snapshot과 policy timeline에 연결된다.
```

필수 테스트:

```text
profile selection evidence test
profile fallback evidence test
profile gate result timeline test
profile evidence completeness test
missing fallback reason blocker test
```

필수 Evidence:

```text
selectedProfileId
selectedProfileType
profileStatus
profileSelectionReason
profileFallbackReason
profileGateResult
profilePolicyRejectionSummary
```

리스크:

```text
Evidence가 없으면 profile이 왜 선택되었는지, 왜 policy가 차단되었는지 검증할 수 없다.
fallback reason 누락은 safe non-apply와 silent failure를 구분하지 못하게 한다.
```

Rollback 계획:

```text
상세 Evidence 통합이 불안정하면 selectedProfileId, profileStatus, fallbackReason, gateResult 최소 필드를 우선 기록한다.
fallback reason이 없으면 release classification을 BLOCKER 후보로 제한한다.
```

## 17. P7-12 RuntimeValidation Profile Checks

Patch ID: `P7-12`

목적:

RuntimeValidationMonitor가 profile 제한과 실제 policy 생성·dispatch·apply 결과의 일관성을 검증하도록 한다.

작업 범위:

```text
MonitoringOnly mutation count validation
AntiCheatSensitive mutation count validation
blocked policy dispatch validation
conditional policy condition validation
Unknown Default Profile mutation validation
Candidate auto-promotion validation
profile evidence completeness validation
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
PolicyTimelineRecorder
EvidenceCompletenessValidator
RCReportGenerator
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
GameProfileRegistry lookup semantics
Release PASS 우회 조건
```

구현 규칙:

```text
MonitoringOnly 상태에서 mutation policy가 있으면 RuntimeValidation BLOCKER로 분류한다.
AntiCheatSensitive 상태에서 mutation policy가 있으면 RuntimeValidation BLOCKER로 분류한다.
blocked policy dispatch가 있으면 RuntimeValidation BLOCKER로 분류한다.
Candidate 자동 승격이 감지되면 BLOCKER 후보로 분류한다.
profile Evidence 누락은 severity와 함께 기록한다.
```

완료 기준:

```text
profile-related RuntimeValidation check 목록이 정의된다.
mutation 금지 상태에서 mutation 발생 시 BLOCKER가 생성된다.
profile validation summary가 RC input으로 연결된다.
```

필수 테스트:

```text
MonitoringOnly mutation runtime blocker test
AntiCheatSensitive mutation runtime blocker test
blocked policy dispatch runtime blocker test
Candidate auto promotion detection test
profile evidence missing severity test
```

필수 Evidence:

```text
profileRuntimeValidationSummary
profileRuntimeValidationBlockerList
profileMutationViolationCount
profileEvidenceCompletenessResult
profileValidationRcInput
```

리스크:

```text
RuntimeValidation이 profile 위반을 검출하지 못하면 release PASS가 잘못 생성될 수 있다.
validation check가 Evidence와 연결되지 않으면 CI 또는 RC에서 재현 검증이 어렵다.
```

Rollback 계획:

```text
profile RuntimeValidation check가 불안정하면 mutation 금지 상태의 위반 검출만 우선 적용한다.
RuntimeValidation BLOCKER가 있으면 RC PASS를 차단한다.
```

## 18. P7-13 Phase 7 Regression Test and Evidence

Patch ID: `P7-13`

목적:

Phase 7 전체의 회귀 테스트, static review, runtime evidence, release blocker 기준을 정리한다.

작업 범위:

```text
Phase 7 regression matrix 작성
profile gate test list 작성
fallback scenario test list 작성
Evidence completeness test list 작성
RuntimeValidation blocker test list 작성
ReleaseChecklist 반영 항목 정리
RC_Runbook 반영 항목 정리
```

수정 가능 파일:

```text
docs/validation/RegressionMatrix_v3.1.md
docs/release/ReleaseChecklist_v3.1.md
docs/release/RC_Runbook_v3.1.md
docs/evidence/EvidenceSpecification.md
validation scripts if existing
tests/*
```

수정 금지 파일:

```text
SchedulerController apply internals
특정 게임 Validated profile 선언
성능 개선 PASS 기준 완화
```

구현 규칙:

```text
Phase 7 regression은 mutation 허용보다 mutation 차단 검증을 우선한다.
Unknown Default Profile, MonitoringOnly, AntiCheatSensitive 위반은 필수 테스트로 둔다.
profile Evidence 누락은 release review에서 확인 가능해야 한다.
회귀 테스트는 성능 향상 수치를 성공 기준으로 삼지 않는다.
```

완료 기준:

```text
Phase 7 test matrix가 생성된다.
profile gate 위반 scenario가 필수 테스트에 포함된다.
ReleaseChecklist와 RC_Runbook에 Phase 7 blocker가 반영된다.
```

필수 테스트:

```text
Unknown Default Profile no mutation regression
MonitoringOnly no mutation regression
AntiCheatSensitive all mutation blocked regression
blocked overrides allowed regression
conditional policy missing evidence regression
profile Evidence completeness regression
```

필수 Evidence:

```text
phase7RegressionSummary
profileGateRegressionSummary
unknownFallbackRegressionSummary
monitoringOnlyRegressionSummary
antiCheatSafetyRegressionSummary
profileEvidenceCompletenessSummary
```

리스크:

```text
회귀 테스트가 positive apply path에만 집중하면 안전 fallback 회귀를 놓칠 수 있다.
ReleaseChecklist에 profile blocker가 없으면 RC에서 잘못된 PASS가 생성될 수 있다.
```

Rollback 계획:

```text
별도 regression matrix가 준비되지 않았으면 PatchPlan_Phase7.md의 완료 기준을 source of truth로 유지한다.
ReleaseChecklist와 RC_Runbook 수정은 profile blocker 항목만 최소 반영한다.
```

## 19. Phase 7 완료 기준

Phase 7 전체 완료 기준은 다음과 같다.

1. GameProfileSpecification과 runtime profile contract가 정렬된다.
2. profile 필수 필드와 validation boundary가 정의된다.
3. registry lookup 실패 시 `Unknown Default Profile`이 적용된다.
4. target identity가 불명확하면 mutation policy가 생성되지 않는다.
5. `Candidate` profile이 자동으로 `Validated`로 승격되지 않는다.
6. `MonitoringOnly` profile에서 mutation policy count가 0이다.
7. `AntiCheatSensitive` profile에서 mutation policy count가 0이다.
8. `blockedPolicies`가 `allowedPolicies`보다 우선한다.
9. `conditionalPolicies`는 required evidence와 runtime condition을 모두 만족해야 한다.
10. confidence, topology, rollback requirement gate가 profile gate와 함께 평가된다.
11. profile selection, fallback, gate result, policy rejection reason이 Evidence에 남는다.
12. RuntimeValidationMonitor가 profile 위반을 BLOCKER로 분류한다.
13. ReleaseChecklist와 RC_Runbook에서 Phase 7 blocker를 확인할 수 있다.

완료 기준은 성능 향상 수치가 아니라 안전한 허용·차단·fallback 판단의 재현성이다.

## 20. Phase 7 BLOCKER 조건

다음 조건 중 하나라도 발생하면 Phase 7 완료 또는 RC PASS를 차단한다.

```text
Candidate profile 자동 Validated 승격
Unknown Default Profile에서 mutation policy 생성 또는 dispatch
MonitoringOnly profile에서 mutation policy 생성 또는 dispatch
AntiCheatSensitive profile에서 mutation policy 생성 또는 dispatch
blockedPolicies 충돌 policy dispatch
conditionalPolicies required evidence 누락 상태에서 dispatch
targetProcessName TBD 상태에서 apply mode dispatch
target identity mismatch 상태에서 mutation dispatch
profile lookup 실패 후 aggressive default policy 적용
profile selection 실패를 group 0 fallback으로 복구
rollback requirement 미충족 policy dispatch
profile fallback reason 누락
profile RuntimeValidation BLOCKER 상태에서 PASS 생성
```

BLOCKER는 성능 수치와 무관하게 안전성 및 검증 가능성 기준으로 판단한다.

## 21. 패치 순서

권장 순서:

```text
1. P7-01 Game Profile Runtime Contract Alignment
2. P7-02 Profile Data Model and Validation Boundary
3. P7-03 Profile Registry and Lookup
4. P7-04 Target Identity and Profile Binding
5. P7-05 Unknown Default Profile Fallback
6. P7-06 Profile Status Enforcement
7. P7-07 Allowed / Blocked Policy Enforcement
8. P7-08 Conditional Policy Gate
9. P7-09 Confidence / Topology / Rollback Requirement Gate
10. P7-10 Monitoring-only and Anti-Cheat Safety Fallback
11. P7-11 Profile Selection Evidence Integration
12. P7-12 RuntimeValidation Profile Checks
13. P7-13 Phase 7 Regression Test and Evidence
```

순서 규칙:

1. contract alignment 없이 runtime gate를 구현하지 않는다.
2. data validation 없이 registry lookup 결과를 신뢰하지 않는다.
3. target binding 없이 game-specific policy를 허용하지 않는다.
4. Unknown Default Profile fallback 없이 profile lookup failure를 처리하지 않는다.
5. status enforcement 이후 allowed/blocked/conditional policy gate를 적용한다.
6. Evidence integration 이후 RuntimeValidation check를 RC input으로 연결한다.

## 22. 패치 작성 규칙

Phase 7 패치 작성 규칙:

1. 검증되지 않은 게임 정보는 `TBD` 또는 `Unknown`으로 둔다.
2. 특정 게임을 자동으로 `Validated`로 작성하지 않는다.
3. profile not found는 `Unknown Default Profile`로 처리한다.
4. `Unknown Default Profile`은 monitoring-only 또는 dry-run만 허용한다.
5. `blockedPolicies`는 항상 `allowedPolicies`보다 우선한다.
6. `conditionalPolicies`는 evidence와 runtime condition을 모두 요구한다.
7. `Candidate`는 `Validated`보다 보수적으로 처리한다.
8. `MonitoringOnly`는 mutation policy를 생성하거나 전달하지 않는다.
9. `AntiCheatSensitive`는 모든 mutation을 차단한다.
10. profile selection failure를 group 0 또는 aggressive default policy로 복구하지 않는다.
11. profile gate 결과는 Evidence와 RuntimeValidation에 남긴다.
12. 성능 향상 수치를 Phase 7 성공 기준으로 사용하지 않는다.

문서 작성 규칙:

```text
관련 문서 경로는 docs/... 기준으로 작성한다.
정량 기준은 확정 수치가 아니라 후보 기준으로 표시한다.
Open Questions는 Blocking / Non-blocking으로 분류한다.
Approval 판단에는 Evidence completeness와 RuntimeValidation 결과를 포함한다.
```

## 23. Non-Goals

Phase 7 Non-Goals:

1. 마비노기 또는 다른 특정 게임을 공식 지원 대상으로 선언하지 않는다.
2. 특정 게임의 process name을 검증 없이 확정하지 않는다.
3. 특정 게임의 Anti-Cheat risk를 검증 없이 확정하지 않는다.
4. 특정 게임의 성능 병목 구조를 검증 없이 확정하지 않는다.
5. profile 기반 aggressive policy를 기본값으로 추가하지 않는다.
6. registry 변경, system-wide tuning, protected process access 변경을 도입하지 않는다.
7. runtime safety gate를 profile status만으로 우회하지 않는다.
8. release PASS 기준을 성능 개선 주장 중심으로 변경하지 않는다.

Non-Goals를 위반하는 변경은 Phase 7 범위 밖이며 별도 승인 문서가 필요하다.

## 24. Acceptance Criteria

Phase 7 Acceptance Criteria:

1. 모든 Patch Unit이 `Patch ID`, `목적`, `작업 범위`, `수정 가능 파일`, `수정 금지 파일`, `구현 규칙`, `완료 기준`, `필수 테스트`, `필수 Evidence`, `리스크`, `Rollback 계획`을 포함한다.
2. `Unknown Default Profile` fallback이 profile lookup 실패의 기본 처리로 정의된다.
3. `MonitoringOnly`와 `AntiCheatSensitive`에서 mutation count 0 기준이 명시된다.
4. `blockedPolicies` 우선순위가 `allowedPolicies`보다 높다는 기준이 명시된다.
5. `conditionalPolicies`의 evidence 및 runtime condition 요구가 명시된다.
6. target identity 불명확, `TBD`, mismatch 상태에서 mutation 차단 기준이 명시된다.
7. profile selection, fallback, policy rejection, RuntimeValidation 결과가 Evidence 항목으로 정의된다.
8. Phase 7 BLOCKER 조건이 ReleaseChecklist와 RC_Runbook에 반영 가능한 형태로 정리된다.

Approval Criteria:

1. 문서 리뷰에서 특정 게임의 성능 특성, 실행 파일명, Anti-Cheat 위험도 단정이 없어야 한다.
2. profile gate가 performance claim이 아니라 safety restriction으로 설명되어야 한다.
3. 정량 기준 후보는 mutation count, dispatch count, Evidence completeness, BLOCKER 여부 중심이어야 한다.
4. Blocking Open Questions가 남아 있으면 Phase 7 구현 착수를 승인하지 않는다.
5. Non-blocking Open Questions는 Evidence에 `TBD` 또는 `Unknown`으로 남긴 상태에서 후속 Phase로 이월할 수 있다.

## 25. Open Questions

### Blocking

1. `GameProfileRegistry`의 실제 profile source는 파일 기반, embedded table, 또는 mixed source 중 무엇인가?
2. `Unknown Default Profile`의 defaultMode를 `monitoring-only`로 고정할지, `dry-run`을 허용할지 결정해야 하는가?
3. profile validation 실패의 severity mapping에서 `blockedPolicies` 누락은 항상 BLOCKER인가?
4. RuntimeValidationMonitor가 profile violation을 감지하는 기준 source는 policy timeline, runtime snapshot, dispatcher result 중 무엇인가?
5. ReleaseChecklist에서 profile RuntimeValidation BLOCKER를 RC PASS 차단 조건으로 어디에 연결할 것인가?

### Non-blocking

1. 특정 게임 후보 profile을 어느 문서에서 추가 관리할 것인가?
2. profile selection Evidence의 displayName은 사용자 표시용으로 필요한가?
3. conditional policy의 runtime condition freshness 후보 기준은 몇 cycle로 둘 것인가?
4. profile registry lookup 성능 지표를 별도 Evidence로 남길 것인가?
5. Phase 8에서 실제 게임 후보 validation matrix를 별도 문서로 분리할 것인가?

Blocking 질문은 Phase 7 구현 전 결정해야 한다. Non-blocking 질문은 안전 fallback을 유지하는 조건에서 `TBD` 또는 `Unknown`으로 이월할 수 있다.
