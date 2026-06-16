# GameOptimizer v3.1 Patch Plan — Phase 3: Policy Candidate & Dispatch Layer

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 3

버전: v1.0

작성 목적: GameOptimizer v3.1의 세 번째 구현 단계인 Phase 3 — Policy Candidate & Dispatch Layer를 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, Phase 3에서 어떤 순서로 정책 후보(Policy Candidate), 정책 검증(Policy Validation), 쿨다운(Cooldown), 정책 충돌(Policy Conflict), 관찰 전용(Monitoring-only), 컨트롤러 전달(Controller Handoff)을 정리할지 정의하는 작업 계획 문서다.

적용 범위:

- PolicyCandidate 구조와 필수 필드 기준
- PolicyResolver / PolicyDispatcher 책임 경계
- Confidence gate와 Game Profile restriction 연결
- cooldown / conflict / blocked policy 처리
- monitoring-only dispatch 처리
- controller handoff result와 policy timeline Evidence 기록
- RuntimeValidation policy contract checks 기준

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 성능 개선 수치 보장
- SchedulerController mutation 구현 변경
- RollbackManager 변경
- ApplyGuard 변경
- BackgroundController mutation 구현 확장
- Aggressive Single-Core Mode 실제 활성화
- Release Gate 자동화

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/implementation/PatchPlan_Phase1.md`
- `docs/implementation/PatchPlan_Phase2.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/performance/GameProfileSpecification.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`

후속 문서:

- `docs/implementation/PatchPlan_Phase4.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 3의 목표는 정책을 적용하는 것이 아니라, 적용해도 되는 정책과 차단해야 하는 정책을 안전하게 구분하고 Controller Layer로 전달하는 경계를 만드는 것이다.

## 2. Phase 3 목표

Phase 3의 핵심 목표는 PerformanceEngine의 판단 결과를 PolicyCandidate로 표현하고, Controller Layer로 전달하기 전에 정책 안전 계약을 검증하는 것이다.

필수 목표:

1. PolicyCandidate 구조와 필수 필드 정리
2. policy type validation 정리
3. requiredController validation 정리
4. requiredRollbackScope validation 정리
5. requiredEvidenceFields validation 정리
6. confidence gate 연결
7. game profile allowed/blocked/conditional policy 반영
8. cooldown / conflict 처리 기준 정리
9. monitoring-only dispatch 처리
10. controller handoff result Evidence 기록

Phase 3의 산출물은 mutation 실행 결과가 아니다. Phase 3의 PASS는 policy candidate와 dispatch boundary가 안전하게 준비되었다는 의미다.

## 3. Phase 3 비목표

다음은 Phase 3에서 구현하지 않는다.

```text
SchedulerController mutation 경로 변경
RollbackManager state save 로직 변경
ApplyGuard transaction 변경
BackgroundController restriction 구현 확장
Aggressive Single-Core Mode 실제 활성화
Before/After 성능 분류 최종 구현
Release Gate 자동화
UI 변경
```

Phase 3에서 Win32 mutation을 수행하지 않는다.

Phase 3에서 Controller의 실제 apply 로직을 수정해야 한다면 Phase 4 또는 Phase 5로 분리한다.

## 4. 영향 모듈

Phase 3에서 영향을 받을 수 있는 모듈은 직접 영향, 간접 영향, 변경 금지 또는 최소 변경 대상으로 구분한다.

### 4.1 직접 영향 모듈

```text
PolicyCandidate
PolicyResolver
PolicyDispatcher
PerformanceEngine
PerformanceEvidencePlanner
RuntimeContext
PolicyTimelineRecorder
```

### 4.2 간접 영향 모듈

```text
MainThreadConfidenceAnalyzer
GameProfileRegistry
SchedulerController
BackgroundController
RuntimeValidationMonitor
Evidence Recorder
```

간접 영향 모듈은 Phase 3 결과를 읽거나 후속 Phase에서 사용할 수 있지만, Phase 3에서 mutation 책임을 새로 가져서는 안 된다.

### 4.3 변경 금지 또는 최소 변경 모듈

```text
SchedulerController mutation implementation
BackgroundController mutation implementation
RollbackManager
ApplyGuard
ThreadTracker
TopologyAnalyzer
```

주의:

Phase 3에서 Controller의 실제 apply 로직을 수정해야 한다면 Phase 4 또는 Phase 5로 분리한다.

## 5. 패치 단위 개요

Phase 3은 다음 Patch ID 구조를 사용한다.

```text
P3-01 Policy Candidate Contract Alignment
P3-02 Required Field Validation
P3-03 Confidence Gate Integration
P3-04 Game Profile Policy Restriction
P3-05 Cooldown Handling
P3-06 Policy Conflict Handling
P3-07 Monitoring-only Dispatch
P3-08 Controller Handoff Boundary
P3-09 Policy Evidence & Timeline Integration
P3-10 RuntimeValidation Policy Checks
P3-11 Phase 3 Regression Test & Evidence
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

패치 하나가 policy contract, controller internals, rollback behavior, release gate를 동시에 변경해서는 안 된다.

## 6. P3-01 Policy Candidate Contract Alignment

Patch ID: `P3-01`

목적:

PolicyCandidate의 개념 필드와 책임 범위를 PolicySpecification 기준으로 정렬한다.

작업 범위:

```text
policyType 정의
source 정의
confidenceLevel 연결
activationReason 정의
deactivationCondition 정의
requiredController 정의
requiredRollbackScope 정의
requiredEvidenceFields 정의
riskLevel 정의
cooldownHint 정의
fallbackBehavior 정의
monitoringOnlyReason 정의
```

수정 가능 파일:

```text
PolicyCandidate 관련 소스/헤더
PolicySpecification 참조 문서
PerformanceEngine 관련 소스/헤더
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
PolicyCandidate는 정책 적용 결과가 아니다.
PolicyCandidate는 Controller에 넘기기 전 검증 대상이다.
PolicyCandidate는 requiredController, requiredRollbackScope, requiredEvidenceFields를 표현할 수 있어야 한다.
```

완료 기준:

```text
PolicyCandidate가 정책 타입, controller, rollback scope, evidence 요구사항을 표현할 수 있다.
정책 적용 결과와 정책 후보가 명확히 분리된다.
```

필수 테스트:

```text
policy candidate required field construction test
policy candidate monitoring-only construction test
policy candidate mutation metadata test
policy candidate evidence field list test
```

필수 Evidence:

```text
policyCandidateContractSummary
policyCandidateRequiredFieldList
policyCandidateValidationState
```

리스크:

```text
PolicyCandidate가 apply result와 섞이면 Controller handoff 이전 검증 경계가 무너질 수 있다.
requiredRollbackScope를 optional처럼 다루면 mutation policy 안전 계약이 약해질 수 있다.
```

Rollback 계획:

```text
필드 확장이 과도하면 required 필드와 diagnostic 필드를 분리한다.
PolicyCandidate 구조 변경이 위험하면 adapter를 두고 기존 policy 타입과 병행한다.
```

## 7. P3-02 Required Field Validation

Patch ID: `P3-02`

목적:

PolicyCandidate의 필수 필드 누락을 Controller 전달 전에 차단한다.

작업 범위:

```text
requiredController 누락 검증
requiredRollbackScope 누락 검증
requiredEvidenceFields 누락 검증
policyType 누락 검증
riskLevel 누락 검증
mutation policy와 monitoring-only policy 구분
```

수정 가능 파일:

```text
PolicyDispatcher
PolicyResolver
PolicyCandidate validator
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
mutation policy는 requiredRollbackScope 없이 Controller로 전달될 수 없다.
requiredController가 없는 policy는 reject 또는 monitoring-only로 전환한다.
requiredEvidenceFields가 없는 policy는 success 판정 대상이 아니다.
검증 실패 policy는 Controller로 전달하지 않는다.
```

완료 기준:

```text
필수 필드가 누락된 policy는 Controller로 전달되지 않는다.
reject reason이 Evidence에 기록된다.
monitoring-only policy와 mutation policy validation이 분리된다.
```

필수 테스트:

```text
missing policyType reject test
missing requiredController reject test
missing rollbackScope mutation reject test
missing evidenceFields warning or reject test
monitoring-only policy validation test
```

필수 Evidence:

```text
policyValidationResult
missingRequiredField
rejectionReason
monitoringOnlyFallbackReason
```

리스크:

```text
필수 필드 검증이 너무 늦으면 Controller에 unsafe policy가 도달할 수 있다.
requiredEvidenceFields 누락을 success로 처리하면 release evidence가 왜곡된다.
```

Rollback 계획:

```text
검증 rule이 과도하면 monitoring-only policy와 mutation policy rule을 분리해 완화한다.
mutation policy의 rollback scope 누락 차단은 유지한다.
```

BLOCKER 기준:

```text
requiredRollbackScope 없는 mutation policy가 Controller로 전달됨
requiredController 없는 policy가 apply 경로로 전달됨
```

## 8. P3-03 Confidence Gate Integration

Patch ID: `P3-03`

목적:

MainThreadConfidenceAnalyzer 결과를 PolicyCandidate 생성/전달 조건에 연결한다.

작업 범위:

```text
Low confidence policy 제한
Medium confidence policy 제한
High confidence limited mutation eligibility
VeryHigh aggressive candidate eligibility marker
insufficient evidence block
confidence downgrade 반영
```

수정 가능 파일:

```text
PerformanceEngine
PolicyResolver
PolicyDispatcher
MainThreadConfidenceAnalyzer consumer
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
Confidence < High이면 affinity/priority policy 전달 금지.
VeryHigh는 aggressive mode 즉시 활성화가 아니라 candidate marker만 허용.
insufficient evidence 상태에서는 mutation policy 생성 또는 전달 금지.
confidence downgrade는 기존 candidate의 eligibility를 낮출 수 있어야 한다.
```

완료 기준:

```text
Confidence level에 따라 policy eligibility가 제한된다.
차단된 policy의 reason이 Evidence에 기록된다.
VeryHigh는 aggressive candidate marker만 제공한다.
```

필수 테스트:

```text
low confidence blocks mutation policy test
medium confidence blocks affinity priority test
high confidence allows limited policy test
very high creates aggressive candidate marker only test
insufficient evidence blocks policy test
```

필수 Evidence:

```text
confidenceGateResult
confidenceLevel
policyEligibility
policyBlockedByConfidenceReason
```

리스크:

```text
Confidence gate가 policy generation 후에만 적용되면 unsafe policy가 timeline에 success처럼 남을 수 있다.
VeryHigh candidate marker를 enable로 오해하면 aggressive mode가 너무 일찍 열린다.
```

Rollback 계획:

```text
Confidence gate 연결이 불안정하면 all mutation eligibility를 false로 fallback한다.
VeryHigh marker는 유지하되 Controller handoff는 차단한다.
```

## 9. P3-04 Game Profile Policy Restriction

Patch ID: `P3-04`

목적:

GameProfileSpecification의 allowedPolicies, blockedPolicies, conditionalPolicies를 Policy Layer에 반영한다.

작업 범위:

```text
selectedProfile 확인
profileStatus 확인
allowedPolicies 적용
blockedPolicies 우선 적용
conditionalPolicies 조건 확인
Unknown Default Profile restriction
AntiCheatSensitive restriction
Candidate vs Validated 차이 반영
```

수정 가능 파일:

```text
GameProfileRegistry 또는 profile consumer
PolicyResolver
PolicyDispatcher
RuntimeContext
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
blockedPolicies는 allowedPolicies보다 항상 우선한다.
Unknown profile은 aggressive mode를 허용하지 않는다.
AntiCheatSensitive profile은 mutation policy를 허용하지 않는다.
Candidate profile은 Validated profile보다 보수적으로 처리한다.
```

완료 기준:

```text
profile restriction이 policy 전달 전에 적용된다.
profile selection과 restriction result가 Evidence에 기록된다.
conditional policy는 required evidence 없이는 통과하지 않는다.
```

필수 테스트:

```text
blocked policy precedence test
unknown profile blocks aggressive mode test
anti-cheat sensitive blocks mutation test
candidate profile conservative restriction test
conditional policy requires evidence test
```

필수 Evidence:

```text
selectedProfile
profileStatus
allowedPolicies
blockedPolicies
conditionalPolicies
profileRestrictionResult
profileBlockedReason
```

리스크:

```text
GameProfileRegistry가 아직 placeholder이면 실제 runtime integration 범위가 불명확할 수 있다.
blockedPolicies 우선순위가 깨지면 위험 profile에서 mutation policy가 통과할 수 있다.
```

Rollback 계획:

```text
Profile integration이 불안정하면 Unknown Default Profile과 MonitoringOnly restriction만 우선 적용한다.
conditionalPolicies는 required evidence가 준비될 때까지 reject로 처리한다.
```

## 10. P3-05 Cooldown Handling

Patch ID: `P3-05`

목적:

정책 반복 적용과 thrashing을 방지하기 위한 cooldown 처리를 정의한다.

작업 범위:

```text
policy cooldown state 정의
policy type별 cooldown hint 소비
active cooldown 판단
cooldown block reason 기록
cooldown bypass 금지
```

수정 가능 파일:

```text
PolicyDispatcher
PolicyResolver
RuntimeContext
PolicyTimelineRecorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
cooldown 중인 policy는 Controller로 전달하지 않는다.
cooldown block은 실패가 아니라 안전한 차단 상태다.
cooldown bypass는 Release Evidence에 blocker 후보로 남겨야 한다.
```

완료 기준:

```text
동일 또는 충돌 policy가 cooldown 동안 반복 전달되지 않는다.
cooldown block result가 Evidence에 기록된다.
policy timeline에 cooldown block event가 남는다.
```

필수 테스트:

```text
same policy cooldown block test
cooldown expiration test
cooldown evidence test
cooldown does not call controller test
```

필수 Evidence:

```text
cooldownState
cooldownPolicyType
cooldownRemaining
cooldownBlockReason
```

리스크:

```text
cooldown state ownership이 불명확하면 RuntimeContext와 PolicyDispatcher가 중복 상태를 가질 수 있다.
cooldown 누락은 policy thrashing으로 이어질 수 있다.
```

Rollback 계획:

```text
정교한 cooldown state가 불안정하면 policy type별 coarse cooldown만 유지한다.
cooldown 판단 실패 시 Controller handoff를 차단하고 monitoring-only reason을 남긴다.
```

## 11. P3-06 Policy Conflict Handling

Patch ID: `P3-06`

목적:

동시에 충돌할 수 있는 policy 간 우선순위를 정리한다.

작업 범위:

```text
affinity policy conflict 처리
background restriction conflict 처리
aggressive mode conflict 처리
rollback request 우선순위 처리
monitoring-only fallback 처리
Safety Contract 우선 원칙 적용
```

수정 가능 파일:

```text
PolicyResolver
PolicyDispatcher
PolicyConflictResolver if existing
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
Rollback request는 성능 policy보다 우선한다.
Safety policy는 aggressive policy보다 우선한다.
conflict가 해결되지 않으면 monitoring-only 또는 reject로 처리한다.
충돌 policy가 동시에 Controller로 전달되면 안 된다.
```

완료 기준:

```text
충돌 policy가 동시에 Controller로 전달되지 않는다.
conflict result가 Evidence에 기록된다.
unresolved conflict는 monitoring-only 또는 reject로 처리된다.
```

필수 테스트:

```text
rollback request overrides performance policy test
safety conflict blocks aggressive policy test
affinity conflict resolution test
unresolved conflict monitoring-only fallback test
```

필수 Evidence:

```text
policyConflictResult
conflictingPolicyTypes
conflictResolution
conflictFallbackReason
```

리스크:

```text
conflict 우선순위가 모호하면 안전 정책보다 성능 정책이 먼저 통과할 수 있다.
rollback request가 낮은 우선순위로 처리되면 rollback safety가 깨진다.
```

Rollback 계획:

```text
conflict resolver가 불안정하면 rollback/safety policy 우선, 나머지는 reject로 단순화한다.
unresolved conflict는 monitoring-only fallback으로 처리한다.
```

## 12. P3-07 Monitoring-only Dispatch

Patch ID: `P3-07`

목적:

정책 적용이 불가능하거나 안전하지 않을 때 monitoring-only 상태를 명확히 전달한다.

작업 범위:

```text
monitoring-only policy type 처리
monitoringOnlyReason 전달
Controller handoff 없이 Evidence 기록
RuntimeStateMachine MonitoringOnly 상태 연계
```

수정 가능 파일:

```text
PolicyDispatcher
RuntimeContext
RuntimeValidationMonitor
Evidence recorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
Monitoring-only는 실패가 아니다.
Monitoring-only 상태에서는 mutation Controller로 전달하지 않는다.
Monitoring-only reason은 반드시 Evidence에 남긴다.
```

완료 기준:

```text
monitoring-only dispatch가 mutation 없이 종료된다.
monitoringOnlyReason이 Runtime/Evidence에 기록된다.
RuntimeStateMachine 상태와 policy dispatch 결과가 모순되지 않는다.
```

필수 테스트:

```text
monitoring-only no controller call test
monitoring-only reason recorded test
unknown profile monitoring-only test
topology incomplete monitoring-only test
```

필수 Evidence:

```text
monitoringOnlyReason
monitoringOnlySource
monitoringOnlyPolicyType
dispatchResult
```

리스크:

```text
monitoring-only가 failure로 기록되면 RC Report가 잘못 해석될 수 있다.
reason 누락 시 안전한 비적용 판단을 설명할 수 없다.
```

Rollback 계획:

```text
RuntimeStateMachine 연계가 불안정하면 reason evidence만 우선 남기고 state transition 연결은 후속 패치로 분리한다.
Controller handoff는 계속 차단한다.
```

## 13. P3-08 Controller Handoff Boundary

Patch ID: `P3-08`

목적:

PolicyDispatcher가 검증된 policy만 Controller로 전달하고, 직접 mutation하지 않는 경계를 고정한다.

작업 범위:

```text
SchedulerController handoff 조건 정의
BackgroundController handoff 조건 정의
Timer/Network controller handoff 조건 정의 if applicable
handoff result 기록
direct Win32 mutation static check
```

수정 가능 파일:

```text
PolicyDispatcher
Controller registry or interface
관련 테스트
정적 검사 스크립트
```

수정 금지 파일:

```text
SchedulerController apply internals
BackgroundController apply internals
RollbackManager
ApplyGuard
```

구현 규칙:

```text
PolicyDispatcher는 Win32 API를 호출하지 않는다.
PolicyDispatcher는 ApplyGuard를 직접 생성하지 않는다.
PolicyDispatcher는 RollbackManager state를 직접 조작하지 않는다.
PolicyDispatcher는 검증된 policy의 handoff만 수행한다.
```

완료 기준:

```text
PolicyDispatcher는 controller handoff만 수행한다.
direct mutation API 호출이 정적 검사로 차단된다.
invalid policy는 controller로 전달되지 않는다.
```

필수 테스트:

```text
policy dispatcher no Win32 mutation static check
policy dispatcher no ApplyGuard creation test
policy dispatcher no RollbackManager state manipulation test
valid policy calls correct controller test
invalid policy does not call controller test
```

필수 Evidence:

```text
controllerHandoffResult
selectedController
handoffRejectedReason
mutationBoundaryCheckResult
```

리스크:

```text
handoff interface가 Controller internals를 노출하면 Dispatcher가 mutation owner처럼 변할 수 있다.
정적 검사가 누락되면 direct Win32 mutation이 침투할 수 있다.
```

Rollback 계획:

```text
controller handoff가 불안정하면 dispatcher는 reject/monitoring-only만 수행하도록 제한한다.
controller registry 변경은 adapter로 격리한다.
```

BLOCKER 기준:

```text
PolicyDispatcher에서 Win32 mutation API 호출
PolicyDispatcher가 ApplyGuard 직접 생성
PolicyDispatcher가 RollbackManager state 직접 변경
```

## 14. P3-09 Policy Evidence & Timeline Integration

Patch ID: `P3-09`

목적:

PolicyCandidate 생성, 검증, 차단, 전달 결과를 Evidence와 timeline에 기록한다.

작업 범위:

```text
policy candidate created event
policy rejected event
policy cooldown blocked event
policy conflict event
monitoring-only event
controller handoff event
policy timeline monotonicity 유지
```

수정 가능 파일:

```text
PolicyTimelineRecorder
PerformanceEvidenceRecorder
RuntimeSnapshotRecorder
PolicyDispatcher
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
정책 성공/실패/차단은 Evidence에 남아야 한다.
Evidence 없는 policy success 판정은 금지한다.
timeline event는 monotonicity를 유지해야 한다.
```

완료 기준:

```text
policy lifecycle이 timeline에 기록된다.
timeline monotonicity를 검증할 수 있다.
reject, cooldown, conflict, handoff event가 구분된다.
```

필수 테스트:

```text
policy candidate timeline event test
policy reject timeline event test
cooldown event test
conflict event test
controller handoff event test
timeline monotonicity test
```

필수 Evidence:

```text
policyTimelineEventCount
policyCreatedCount
policyRejectedCount
policyDispatchedCount
policyBlockedCount
timelineMonotonicity
```

리스크:

```text
timeline event가 과도하게 많으면 evidence volume이 커질 수 있다.
success와 handoff를 혼동하면 실제 mutation 전 상태가 success처럼 기록될 수 있다.
```

Rollback 계획:

```text
timeline이 불안정하면 summary evidence를 우선 유지하고 detailed timeline은 후속 패치로 분리한다.
handoff event와 apply success event는 반드시 분리한다.
```

## 15. P3-10 RuntimeValidation Policy Checks

Patch ID: `P3-10`

목적:

Policy Layer 계약 위반을 RuntimeValidation에서 감지하게 한다.

작업 범위:

```text
missing requiredController validation
missing rollbackScope validation
confidence gate consistency validation
profile restriction consistency validation
controller handoff consistency validation
cooldown/conflict state validation
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
PolicyDispatcher
Evidence recorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation path
RollbackManager
ApplyGuard
BackgroundController mutation path
```

구현 규칙:

```text
RuntimeValidation은 mutation을 수행하지 않는다.
RuntimeValidation은 policy contract violation을 WARN 또는 BLOCKER로 분류한다.
requiredRollbackScope 없는 mutation policy handoff는 BLOCKER다.
```

완료 기준:

```text
Policy Layer 이상이 RuntimeValidation Summary에 반영된다.
policy contract violation이 release-facing evidence로 기록된다.
```

필수 테스트:

```text
missing controller runtime validation test
missing rollback scope blocker test
confidence gate violation blocker test
profile restriction violation blocker test
dispatcher mutation boundary blocker test
```

필수 Evidence:

```text
policyValidationState
policyContractWarnCount
policyContractBlockerCount
policyRuntimeValidationSummary
```

리스크:

```text
RuntimeValidation rule이 과도하면 정상 monitoring-only dispatch가 BLOCKER로 오분류될 수 있다.
반대로 required field violation을 WARN으로 낮추면 unsafe handoff를 놓칠 수 있다.
```

Rollback 계획:

```text
초기 rule은 requiredController, requiredRollbackScope, dispatcher boundary에 집중한다.
과도한 warning rule은 조정하되 unsafe handoff는 BLOCKER로 유지한다.
```

## 16. P3-11 Phase 3 Regression Test & Evidence

Patch ID: `P3-11`

목적:

Phase 3 패치 묶음의 회귀 테스트와 Evidence 기준을 정리한다.

작업 범위:

```text
Phase 3 test list 작성
Phase 3 static check list 작성
Phase 3 runtime evidence list 작성
Phase 3 release blocker list 작성
RegressionMatrix_v3.1 연결
ReleaseChecklist_v3.1 연결
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
Phase 3 PASS는 정책 적용 성공이 아니다.
Phase 3 PASS는 policy candidate와 dispatch boundary가 안전하게 준비되었다는 의미다.
Regression test는 candidate validation, dispatcher boundary, profile restriction, cooldown/conflict, evidence, RuntimeValidation을 분리해 기록한다.
```

완료 기준:

```text
Phase 3 regression checklist가 존재한다.
Phase 3 blocker 조건이 ReleaseChecklist와 연결된다.
Phase 3 Evidence Bundle 최소 항목이 정의된다.
```

필수 테스트:

```text
PolicyCandidate validation suite
PolicyDispatcher boundary suite
confidence gate policy suite
game profile restriction suite
cooldown conflict suite
policy evidence suite
RuntimeValidation policy suite
```

필수 Evidence:

```text
phase3RegressionSummary
policyCandidateValidationSummary
policyDispatcherBoundarySummary
policyEvidenceSummary
runtimeValidationSummary
```

리스크:

```text
RegressionMatrix_v3.1.md가 아직 없으면 테스트 목록이 문서상 계획에 머물 수 있다.
ReleaseChecklist_v3.1.md 변경이 과도하면 릴리즈 기준과 Phase 3 기준이 섞일 수 있다.
```

Rollback 계획:

```text
신규 regression 문서가 준비되지 않았으면 PatchPlan_Phase3.md의 Evidence 기준을 우선 source of truth로 유지한다.
ReleaseChecklist 수정은 Phase 3 blocker 항목만 최소 반영한다.
```

## 17. Phase 3 완료 기준

Phase 3 전체 완료 기준은 다음과 같다.

1. PolicyCandidate의 필수 필드가 정의되어 있다.
2. requiredController 누락 policy가 Controller로 전달되지 않는다.
3. requiredRollbackScope 없는 mutation policy가 Controller로 전달되지 않는다.
4. requiredEvidenceFields 없는 policy는 success 판정 대상이 아니다.
5. Confidence gate가 policy 전달 전에 적용된다.
6. Game Profile restriction이 policy 전달 전에 적용된다.
7. cooldown과 conflict 처리가 정의되어 있다.
8. monitoring-only dispatch가 mutation 없이 동작한다.
9. PolicyDispatcher가 직접 Win32 mutation을 수행하지 않는다.
10. policy lifecycle이 Evidence와 timeline에 기록된다.
11. RuntimeValidation이 policy contract violation을 감지할 수 있다.
12. Phase 3 regression test가 정의되어 있다.

완료 판단은 policy apply 성공이 아니라 policy validation과 handoff boundary의 안전성을 기준으로 한다.

## 18. Phase 3 BLOCKER 조건

다음 조건은 Phase 3 BLOCKER로 분류한다.

```text
PolicyDispatcher에서 Win32 mutation API 호출
PolicyDispatcher가 ApplyGuard 직접 생성
PolicyDispatcher가 RollbackManager state 직접 변경
requiredRollbackScope 없는 mutation policy가 Controller로 전달됨
requiredController 없는 policy가 apply 경로로 전달됨
Confidence < High인데 affinity/priority policy 전달
Unknown profile에서 aggressive mode 전달
AntiCheatSensitive profile에서 mutation policy 전달
blockedPolicy가 allowedPolicy보다 낮은 우선순위로 처리됨
policy success Evidence 누락
RuntimeValidation이 policy contract violation을 무시
```

BLOCKER가 발생하면 후속 patch보다 해당 blocker 제거를 우선한다. 특히 P3-02와 P3-08의 BLOCKER는 Phase 3 전체 진행을 차단한다.

## 19. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P3-01 Policy Candidate Contract Alignment
2. P3-02 Required Field Validation
3. P3-03 Confidence Gate Integration
4. P3-04 Game Profile Policy Restriction
5. P3-05 Cooldown Handling
6. P3-06 Policy Conflict Handling
7. P3-07 Monitoring-only Dispatch
8. P3-08 Controller Handoff Boundary
9. P3-09 Policy Evidence & Timeline Integration
10. P3-10 RuntimeValidation Policy Checks
11. P3-11 Phase 3 Regression Test & Evidence
```

주의:

```text
P3-02와 P3-08은 Phase 3의 핵심 안전 패치다.
P3-02에서 requiredRollbackScope 없는 mutation policy가 통과하면 즉시 수정한다.
P3-08에서 PolicyDispatcher가 mutation owner를 침범하면 Phase 3를 중단한다.
```

## 20. 패치 작성 규칙

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

패치 작성자는 Phase 3 범위를 벗어난 Controller apply internals, rollback behavior 변경, aggressive mode 구현을 같은 패치에 섞지 않는다.

## 21. Non-Goals

다음은 PatchPlan_Phase3.md의 비목표(Non-Goals)다.

```text
구체 C++ 코드 작성
패치 파일 직접 생성
테스트 코드 직접 작성
성능 개선 수치 보장
SchedulerController mutation 구현 변경
RollbackManager 변경
ApplyGuard 변경
BackgroundController mutation 구현 확장
Aggressive Single-Core Mode 실제 활성화
Release Gate 자동화
```

Phase 3은 PolicyCandidate와 PolicyDispatcher 경계를 고정하는 문서다.

## 22. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 3의 목적과 비목표가 명확하다.
2. Phase 3 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P3-01부터 P3-11까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 완료 기준, 테스트, Evidence가 정의되어 있다.
5. PolicyCandidate 필수 필드 검증 기준이 명확하다.
6. PolicyDispatcher가 mutation owner가 아니라 handoff boundary임이 명확하다.
7. Confidence gate와 Game Profile restriction이 policy 전달 전에 적용된다는 원칙이 명확하다.
8. Phase 3 BLOCKER 조건이 정의되어 있다.
9. 패치 순서가 정의되어 있다.
10. 후속 `docs/implementation/PatchPlan_Phase4.md` 작성에 필요한 기준이 충분하다.

## 23. Open Questions

다음 질문은 Phase 3 실제 패치 착수 전 또는 P3-01에서 분류한다.

```text
1. PolicyCandidate는 기존 policy 구조를 확장할 것인가, 신규 타입으로 분리할 것인가?
2. PolicyResolver와 PolicyDispatcher를 분리할 것인가, 하나의 모듈로 유지할 것인가?
3. cooldown state는 RuntimeContext가 소유할 것인가, PolicyDispatcher가 소유할 것인가?
4. conflict resolution 우선순위는 PolicySpecification에서 확정할 것인가, 별도 ADR로 분리할 것인가?
5. monitoring-only dispatch는 RuntimeStateMachine 상태 전이까지 직접 유도할 것인가, reason만 제공할 것인가?
6. GameProfileRegistry는 Phase 3에서 실제 구현할 것인가, Phase 7까지 placeholder로 둘 것인가?
7. policy timeline은 log 유지인가, JSONL 전환인가?
8. Phase 3 완료 전 soft-apply validation을 수행할 것인가?
```

Open Questions가 해결되기 전까지 관련 구현 범위는 `TBD`로 유지한다. 단, PolicyDispatcher에 mutation 책임을 부여하지 않는 것과 requiredRollbackScope 없는 mutation policy를 허용하지 않는 것은 Open Question이 아니라 Phase 3 안전 계약이다.
