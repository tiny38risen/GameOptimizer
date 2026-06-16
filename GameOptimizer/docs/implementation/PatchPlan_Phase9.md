# GameOptimizer v3.1 Patch Plan — Phase 9: Aggressive Single-Core Mode

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 9

버전: v1.0

작성 목적: GameOptimizer v3.1의 아홉 번째 구현 단계인 Phase 9 — Aggressive Single-Core Mode를 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, 공격적 싱글코어 모드(Aggressive Single-Core Mode)의 활성화 후보(Activation Candidate), 활성화 게이트(Activation Gate), 정책 묶음(Policy Bundle), 안전 적용, 검증, 자동 해제(Automatic Deactivation), 히스테리시스(Hysteresis), 쿨다운(Cooldown), 부분 적용(Partial Application), rollback, Evidence, RuntimeValidation 연결 순서를 정의하는 작업 계획 문서다.

적용 범위:

- Aggressive Mode 책임 경계 정의
- Aggressive Mode 상태 모델 정의
- Activation Candidate 생성 기준 정의
- 복합 Activation Gate 정의
- Aggressive Policy Bundle 구성 기준 정의
- Controller handoff와 transaction boundary 정의
- 부분 적용 실패 처리 정의
- Active 상태 검증 기준 정의
- 자동 해제 조건 정의
- cooldown / hysteresis 기준 정의
- 성능 회귀 대응 기준 정의
- Aggressive Mode Evidence 정의
- RuntimeValidation 검사 정의
- Long Soak 및 Failure Injection 검증 기준 정의

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 성능 향상 수치 보장
- 공식 지원 게임 목록 확정
- 특정 게임의 실행 파일명 확정
- AI/ML 기반 self-tuning
- 자동 threshold 학습
- Anti-Cheat 우회
- DLL Injection
- Kernel Driver
- Game Memory Patch
- SchedulerController를 우회하는 affinity 변경
- RollbackManager를 우회하는 상태 복구
- 검증 없는 registry 자동 변경
- Interrupt Affinity 강제 적용
- 외부 FPS/frame time 측정 도구 구현
- UI 구현
- 자동 배포 구현

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/implementation/PatchPlan_Phase1.md`
- `docs/implementation/PatchPlan_Phase2.md`
- `docs/implementation/PatchPlan_Phase3.md`
- `docs/implementation/PatchPlan_Phase4.md`
- `docs/implementation/PatchPlan_Phase5.md`
- `docs/implementation/PatchPlan_Phase6.md`
- `docs/implementation/PatchPlan_Phase7.md`
- `docs/implementation/PatchPlan_Phase8.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/performance/GameProfileSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
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
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`

후속 문서:

- `docs/implementation/PatchPlan_Phase10.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 9의 목표는 가장 강한 정책을 가능한 한 자주 적용하는 것이 아니라, 강한 정책이 안전하다고 입증된 제한된 상황에서만 짧고 추적 가능한 transaction으로 활성화되도록 만드는 것이다.

## 2. Phase 9 목표

Aggressive Single-Core Mode는 기본 실행 모드가 아니다. 이 모드는 메인 스레드 병목이 높은 신뢰도로 확인되고, profile, topology, processor group, rollback, evidence, RuntimeValidation 조건이 모두 통과한 제한된 상황에서만 허용되는 고위험 정책 모드다.

필수 목표:

1. Aggressive Mode 책임 경계 정의
2. Aggressive Mode 상태 모델 정의
3. Activation Candidate 생성 기준 정의
4. 복합 Activation Gate 정의
5. Policy Bundle 구성 기준 정의
6. Controller handoff 기준 정의
7. 부분 적용 실패 처리 정의
8. Active 상태 진입 조건 정의
9. 자동 해제 조건 정의
10. cooldown / hysteresis 기준 정의
11. 성능 회귀 대응 기준 정의
12. Aggressive Mode Evidence 정의
13. RuntimeValidation 검사 정의
14. Long Soak 검증 기준 정의

핵심 원칙:

1. VeryHigh Confidence는 필요조건이지 충분조건이 아니다.
2. Aggressive Mode는 Unknown Profile에서 금지한다.
3. Aggressive Mode는 MonitoringOnly Profile에서 금지한다.
4. Aggressive Mode는 AntiCheatSensitive Profile에서 금지한다.
5. Processor Group 또는 Topology 정보가 불완전하면 금지한다.
6. rollback state를 확보할 수 없으면 금지한다.
7. RuntimeValidation BLOCKER가 있으면 금지한다.
8. Evidence writer가 준비되지 않았으면 금지한다.
9. PolicyDispatcher와 Controller Layer를 우회하지 않는다.
10. AggressiveSingleCoreController는 Win32 mutation을 직접 수행하지 않는다.
11. 필요한 정책이 모두 Verify되기 전 Active로 기록하지 않는다.
12. 부분 적용 실패 시 적용된 정책을 모두 rollback한다.
13. 성능 회귀가 감지되면 자동 해제 또는 rollback한다.
14. shutdown 요청 이후 신규 활성화를 금지한다.
15. AI/ML 기반 자동 self-tuning은 Phase 9 범위가 아니다.

정량 기준 후보:

- VeryHigh Confidence 최소 유지 시간은 `TBD`로 둔다.
- 최소 observation time은 `TBD`로 둔다.
- activation gate 실패 개수가 1개 이상이면 activation count는 0이어야 한다.
- required policy verify 실패 개수가 1개 이상이면 Active 전환 count는 0이어야 한다.
- partial application 상태가 감지되면 rollback request count는 1 이상이어야 한다.
- RuntimeValidation BLOCKER가 존재하는 동안 Active 유지 시간은 0이어야 한다.
- cooldown duration, maximum active duration, regression-required-cycles는 확정 전까지 `TBD`로 둔다.

## 3. Phase 9 비목표

Phase 9는 자기 학습형 자동 튜닝 시스템을 구현하는 단계가 아니다.

비목표:

1. AI/ML 기반 self-tuning
2. 게임별 threshold 자동 학습
3. 웹 검색 기반 게임 profile 생성
4. 특정 게임의 공식 지원 확정
5. 특정 게임의 실행 파일명 자동 확정
6. Anti-Cheat 우회
7. DLL Injection
8. Kernel Driver
9. Game Memory Patch
10. SchedulerController를 우회하는 affinity 변경
11. RollbackManager를 우회하는 상태 복구
12. 검증 없는 registry 자동 변경
13. Interrupt Affinity 강제 적용
14. 외부 FPS/frame time 도구 구현

Aggressive Mode는 성능 향상을 보장하는 기능이 아니다. Phase 9는 강한 정책을 언제 금지하고, 언제 짧은 transaction으로만 허용하며, 실패 시 어떻게 되돌릴지 정의하는 단계다.

## 4. 영향 모듈

### 4.1 직접 영향 모듈

```text
AggressiveSingleCoreController
PerformanceEngine
MainThreadConfidenceAnalyzer consumer
GameProfile consumer
PolicyResolver
PolicyDispatcher
RuntimeContext
RuntimeStateMachine integration point
PerformanceEvidenceRecorder
RuntimeValidationMonitor
```

### 4.2 간접 영향 모듈

```text
TopologyAnalyzer
SchedulerController
BackgroundController
RollbackManager
ApplyGuard
PerformanceEvidencePlanner
PolicyTimelineRecorder
RuntimeSnapshotRecorder
RuntimeOrchestrator
ShutdownPipeline
RCReportGenerator
```

### 4.3 변경 금지 또는 최소 변경 모듈

```text
ThreadTracker core sampling logic
TopologyAnalyzer detection internals
SchedulerController transaction internals
RollbackManager rollback internals
ApplyGuard state machine internals
GameProfileRegistry lookup internals
```

주의:

```text
AggressiveSingleCoreController는 mode state와 policy plan을 소유할 수 있지만 실제 Win32 mutation을 소유할 수 없다.
실제 affinity / priority 적용은 SchedulerController에 위임한다.
백그라운드 제한은 BackgroundController에 위임한다.
rollback execution은 RollbackManager에 위임한다.
```

## 5. 패치 단위 개요

Phase 9은 다음 Patch ID 구조를 사용한다.

```text
P9-01 Aggressive Mode Contract Alignment
P9-02 Aggressive Mode State Model
P9-03 Activation Candidate Generation
P9-04 Composite Activation Gate
P9-05 Aggressive Policy Bundle Planning
P9-06 Controller Handoff and Transaction Boundary
P9-07 Partial Application Failure Handling
P9-08 Active State Verification
P9-09 Automatic Deactivation Triggers
P9-10 Cooldown and Hysteresis
P9-11 Performance Regression Response
P9-12 Aggressive Mode Evidence Integration
P9-13 RuntimeValidation Aggressive Mode Checks
P9-14 Long Soak and Failure Injection Validation
P9-15 Phase 9 Regression Test and Evidence
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

패치 하나가 activation gate, controller mutation path, rollback execution, release classification을 동시에 변경해서는 안 된다.

## 6. P9-01 Aggressive Mode Contract Alignment

Patch ID: `P9-01`

목적:

AggressiveSingleCoreController와 관련 모듈의 책임과 비책임을 정렬한다.

작업 범위:

```text
AggressiveSingleCoreController 책임 정의
PerformanceEngine과의 입력 경계 정의
PolicyResolver와의 연결 경계 정의
PolicyDispatcher와의 handoff 경계 정의
SchedulerController와의 mutation 경계 정의
RollbackManager와의 rollback 경계 정의
Evidence ownership 정의
RuntimeValidation ownership 정의
```

수정 가능 파일:

```text
docs/implementation/PatchPlan_Phase9.md
docs/implementation/MIGRATION_NOTES_Phase9.md
AggressiveSingleCoreController 관련 placeholder 또는 계약 파일
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback internals
ApplyGuard internals
ThreadTracker internals
```

구현 규칙:

```text
AggressiveSingleCoreController는 mode state와 policy plan을 소유한다.
AggressiveSingleCoreController는 SetThreadPriority, SetThreadGroupAffinity, SetPriorityClass를 직접 호출하지 않는다.
AggressiveSingleCoreController는 ApplyGuard를 직접 우회하지 않는다.
AggressiveSingleCoreController는 RollbackManager를 우회하지 않는다.
```

완료 기준:

```text
Aggressive Mode의 책임과 mutation ownership이 구분된다.
Controller Layer 우회 경로가 없다.
rollback ownership과 evidence ownership이 분리된다.
```

필수 테스트:

```text
aggressive ownership map review test
no direct Win32 mutation static check
no PolicyDispatcher bypass review test
no RollbackManager bypass review test
evidence ownership review test
```

필수 Evidence:

```text
aggressiveModeContractSummary
aggressiveModeOwnershipMap
mutationBoundarySummary
aggressiveEvidenceOwnershipMap
blockingQuestionList
```

리스크:

```text
AggressiveSingleCoreController가 mutation을 직접 소유하면 Phase 4/5의 transaction과 rollback 계약을 우회할 수 있다.
책임 경계가 불명확하면 failure injection에서 부분 적용 상태가 남을 수 있다.
```

Rollback 계획:

```text
책임 경계가 불명확하면 AggressiveSingleCoreController를 planning-only component로 제한한다.
mutation ownership 충돌이 있으면 기존 Controller ownership을 우선한다.
```

BLOCKER 기준:

```text
AggressiveSingleCoreController에 직접 Win32 mutation 책임 부여
RollbackManager 우회 경로 정의
PolicyDispatcher 우회 경로 정의
```

## 7. P9-02 Aggressive Mode State Model

Patch ID: `P9-02`

목적:

Aggressive Mode의 상태와 허용 전이를 명확히 정의한다.

권장 상태:

```text
Disabled
Candidate
GateRejected
Arming
Applying
Verifying
Active
Deactivating
RollbackPending
Cooldown
Blocked
```

작업 범위:

```text
각 상태 의미 정의
정상 상태 전이 정의
실패 상태 전이 정의
shutdown 상태 전이 정의
illegal transition 정의
RuntimeStateMachine 연결 기준 정의
state evidence field 정의
```

수정 가능 파일:

```text
AggressiveSingleCoreController
RuntimeContext
RuntimeStateMachine integration point
RuntimeSnapshotRecorder
관련 테스트
```

수정 금지 파일:

```text
RuntimeStateMachine core transition semantics
SchedulerController transaction internals
RollbackManager rollback internals
ApplyGuard state machine internals
```

구현 규칙:

```text
Candidate에서 Active로 직접 전이 금지.
Applying에서 Active로 직접 전이 금지.
Verifying 성공 후에만 Active 전이 허용.
RollbackPending 상태에서는 신규 activation 금지.
Cooldown 중 Candidate 재생성 또는 Active 재진입 금지.
ShutdownRequested 이후 신규 activation 금지.
```

완료 기준:

```text
Aggressive Mode 상태 전이가 명확하다.
illegal transition을 감지할 수 있다.
상태 전이가 runtime snapshot과 policy timeline에 기록된다.
```

필수 테스트:

```text
disabled to candidate transition test
candidate to arming transition test
candidate to active forbidden test
applying to active forbidden test
verifying success to active test
rollback pending blocks activation test
cooldown blocks reactivation test
illegal transition test
shutdown blocks activation test
```

필수 Evidence:

```text
aggressiveModeStateBefore
aggressiveModeStateAfter
aggressiveModeTransitionReason
aggressiveModeIllegalTransition
aggressiveStateTimeline
```

리스크:

```text
상태 모델이 느슨하면 verify 전 Active 전환이나 cooldown 중 재활성화가 발생할 수 있다.
illegal transition이 Evidence에 남지 않으면 RC에서 상태 회귀를 검증할 수 없다.
```

Rollback 계획:

```text
state transition 판단이 불안정하면 Aggressive Mode를 Disabled 또는 Blocked로 제한한다.
illegal transition 발견 시 Active 전환을 차단하고 Cooldown 또는 Blocked로 이동한다.
```

BLOCKER 기준:

```text
Verify 없이 Active 전환
RollbackPending 중 Active 전환
Cooldown 중 재활성화
illegal transition 무시
```

## 8. P9-03 Activation Candidate Generation

Patch ID: `P9-03`

목적:

Aggressive Mode 활성화 후보를 안전한 판단 계층에서 생성한다.

작업 범위:

```text
VeryHigh Confidence 확인
minimum observation time 확인
confidence stability duration 확인
migration stability 확인
profile permission 확인
topology eligibility 확인
rollback readiness 후보 확인
RuntimeValidation state 확인
target identity stability 확인
```

수정 가능 파일:

```text
PerformanceEngine
MainThreadConfidenceAnalyzer consumer
AggressiveSingleCoreController
RuntimeContext
관련 테스트
```

수정 금지 파일:

```text
MainThreadConfidenceAnalyzer scoring internals
ThreadTracker sampling internals
TopologyAnalyzer detection internals
SchedulerController apply internals
```

구현 규칙:

```text
VeryHigh Confidence만으로 Candidate를 Active로 전환하지 않는다.
Candidate 생성은 mutation이 아니다.
Confidence history가 없는 단일 cycle VeryHigh는 후보 생성 근거로 사용하지 않는다.
Candidate 생성은 profile permission과 target identity stability를 함께 확인한다.
```

완료 기준:

```text
모든 기본 입력 조건을 만족한 경우에만 Activation Candidate가 생성된다.
Candidate 생성 또는 거부 사유가 기록된다.
단일 cycle VeryHigh는 candidate 생성에서 차단된다.
```

필수 테스트:

```text
very high confidence candidate test
high confidence blocks candidate test
single cycle very high blocks candidate test
insufficient observation blocks candidate test
migration instability blocks candidate test
profile restriction blocks candidate test
target identity unstable blocks candidate test
```

필수 Evidence:

```text
aggressiveCandidateCreated
aggressiveCandidateReason
confidenceLevel
confidenceStableDuration
observationDuration
migrationStability
candidateRejectedReason
targetIdentityStability
```

리스크:

```text
단일 cycle의 VeryHigh를 후보 근거로 사용하면 일시적 측정 변동으로 강한 정책이 활성화될 수 있다.
Candidate 생성과 activation을 혼동하면 gate와 transaction boundary가 약해진다.
```

Rollback 계획:

```text
candidate input이 불완전하면 Candidate를 생성하지 않는다.
confidence history가 충분하지 않으면 Disabled 상태를 유지한다.
```

## 9. P9-04 Composite Activation Gate

Patch ID: `P9-04`

목적:

Aggressive Mode 활성화 전에 모든 안전 조건을 하나의 복합 Gate로 검증한다.

작업 범위:

```text
Activation Candidate 입력 수집
필수 Gate 목록 평가
Gate별 pass/fail reason 기록
Gate 실패 시 activation 차단
Gate 결과와 state transition 연결
Gate 결과 Evidence 기록
```

필수 Gate:

```text
Confidence Gate
Observation Duration Gate
Migration Stability Gate
Profile Permission Gate
Profile Status Gate
Topology Completeness Gate
Processor Group Safety Gate
Rollback Availability Gate
ApplyGuard Compatibility Gate
RuntimeValidation Clean Gate
Evidence Writer Readiness Gate
Target Identity Gate
Anti-Cheat Safety Gate
Shutdown State Gate
Conflict Policy Gate
Cooldown Gate
```

수정 가능 파일:

```text
AggressiveSingleCoreController
PolicyResolver
RuntimeContext
RuntimeValidationMonitor consumer
PerformanceEvidenceRecorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback internals
ApplyGuard internals
GameProfileRegistry lookup internals
```

구현 규칙:

```text
Gate 하나라도 실패하면 activation 금지.
실패한 Gate를 다른 fallback 값으로 강제 통과시키지 않는다.
Processor Group 누락을 group 0으로 보정하지 않는다.
Anti-Cheat 위험이 Unknown 또는 높음이면 보수적으로 차단한다.
RuntimeValidation BLOCKER가 있으면 activation 금지.
```

완료 기준:

```text
모든 Gate가 통과한 경우에만 Arming 상태로 전환된다.
실패 Gate 목록이 Evidence에 기록된다.
Gate 실패 상태에서 Applying 또는 Active로 이동할 수 없다.
```

필수 테스트:

```text
all gates pass test
confidence gate failure test
profile gate failure test
topology gate failure test
processor group gate failure test
rollback gate failure test
runtime blocker gate failure test
evidence writer gate failure test
identity gate failure test
anti-cheat gate failure test
shutdown gate failure test
cooldown gate failure test
```

필수 Evidence:

```text
aggressiveActivationGateResult
aggressiveActivationGateList
failedActivationGates
activationBlockedReason
allRequiredGatesPassed
activationGateEvaluationTime
```

리스크:

```text
복합 Gate가 일부 조건을 WARN으로만 처리하면 Unknown 또는 unsafe 상태에서 activation이 가능해진다.
Processor Group 누락을 group 0으로 보정하면 Phase 3/4의 topology safety 원칙을 훼손한다.
```

Rollback 계획:

```text
Gate 평가가 불완전하면 activation을 차단하고 GateRejected로 전환한다.
Gate owner가 불명확한 조건은 fail-closed로 처리한다.
```

BLOCKER 기준:

```text
Gate 실패 상태에서 Arming 또는 Applying 진입
Processor Group 누락 상태에서 activation
RuntimeValidation BLOCKER 상태에서 activation
rollback unavailable 상태에서 activation
```

## 10. P9-05 Aggressive Policy Bundle Planning

Patch ID: `P9-05`

목적:

Aggressive Mode에서 사용할 정책 묶음을 직접 mutation이 아닌 PolicyCandidate 집합으로 구성한다.

정책 후보 예시:

```text
MAIN_THREAD_PRIORITY_UP
MAIN_THREAD_AFFINITY_STABILIZE
CORE_ISOLATION_PLAN
CCX_STABILITY_PREFER
조건부 BACKGROUND_RESTRICT_UP
```

작업 범위:

```text
required policy 정의
optional policy 정의
blocked policy 제거
policy dependency 정의
policy apply order 정의
policy rollback scope 정의
required evidence 정의
profile condition 연결
```

수정 가능 파일:

```text
AggressiveSingleCoreController
PerformanceEngine
PolicyCandidate
PolicyResolver
관련 테스트
```

수정 금지 파일:

```text
SchedulerController mutation internals
BackgroundController internals
RollbackManager internals
registry mutation policy
Interrupt Affinity 강제 적용 로직
```

구현 규칙:

```text
Policy Bundle은 Win32 mutation 목록이 아니라 PolicyCandidate 목록이다.
BACKGROUND_RESTRICT_UP은 Game Profile이 명시적으로 허용한 경우에만 포함한다.
registry mutation을 Aggressive Policy Bundle에 포함하지 않는다.
Interrupt Affinity 강제 변경을 기본 Bundle에 포함하지 않는다.
blocked policy는 bundle에서 제거한다.
```

완료 기준:

```text
Aggressive Policy Bundle의 required / optional / blocked policy가 구분된다.
각 policy가 controller와 rollback scope를 가진다.
profile 조건부 policy가 명확히 구분된다.
```

필수 테스트:

```text
required policy bundle generated test
optional policy profile permission test
blocked policy removed from bundle test
missing rollback scope rejects bundle test
registry mutation not included test
unsafe interrupt affinity not included test
background restrict profile condition test
```

필수 Evidence:

```text
aggressivePolicyBundle
requiredAggressivePolicies
optionalAggressivePolicies
blockedAggressivePolicies
policyBundleValidationResult
policyBundleRejectedReason
policyRollbackScopeMap
```

리스크:

```text
Policy Bundle이 직접 Win32 action 목록이 되면 Controller transaction과 rollback 경계를 우회할 수 있다.
optional policy 실패 기준이 불명확하면 degraded active mode가 암묵적으로 생길 수 있다.
```

Rollback 계획:

```text
bundle validation이 불완전하면 activation을 차단한다.
rollback scope가 없는 policy는 bundle에서 제외하거나 전체 bundle을 reject한다.
```

## 11. P9-06 Controller Handoff and Transaction Boundary

Patch ID: `P9-06`

목적:

Aggressive Policy Bundle을 기존 PolicyDispatcher와 Controller 경로로 전달한다.

작업 범위:

```text
PolicyDispatcher handoff
SchedulerController handoff
BackgroundController conditional handoff
RollbackManager state save 연결
ApplyGuard transaction 연결
policy별 apply/verify result 수집
controller별 transaction result 수집
```

수정 가능 파일:

```text
AggressiveSingleCoreController
PolicyDispatcher
Controller interface boundary
RuntimeContext
관련 테스트
```

수정 금지 파일:

```text
SchedulerController의 Save → Arm → Apply → Verify → Commit 계약
RollbackManager의 state ownership
ApplyGuard의 transaction ownership
Controller 내부 Win32 mutation 세부 구현
```

구현 규칙:

```text
AggressiveSingleCoreController가 Controller를 직접 우회 호출하지 않는다.
모든 mutation은 기존 Controller의 transaction 경계를 따른다.
필수 정책이 모두 Verify되기 전 Active로 전환하지 않는다.
RollbackManager state save 실패 시 activation을 중단한다.
```

완료 기준:

```text
Aggressive Policy Bundle이 PolicyDispatcher를 통해 올바른 Controller로 전달된다.
각 mutation 결과가 transaction 단위로 기록된다.
필수 policy verify 결과가 Active 전환 입력으로 연결된다.
```

필수 테스트:

```text
aggressive bundle dispatcher handoff test
correct controller selection test
no direct Win32 mutation static check
rollback state save required test
ApplyGuard arm required test
all required policies verified before active test
controller transaction result evidence test
```

필수 Evidence:

```text
aggressiveControllerHandoffResult
aggressiveSelectedControllers
aggressivePolicyApplyResults
aggressivePolicyVerifyResults
transactionBoundaryCheckResult
rollbackStateSaveResult
```

리스크:

```text
handoff 경계가 불명확하면 AggressiveSingleCoreController가 mutation executor처럼 변할 수 있다.
ApplyGuard 또는 RollbackManager가 빠지면 partial failure 복구가 불가능해진다.
```

Rollback 계획:

```text
handoff 실패 시 Applying으로 진입하지 않고 GateRejected 또는 RollbackPending으로 전환한다.
transaction boundary check가 실패하면 activation을 중단하고 적용된 policy를 rollback 대상으로 등록한다.
```

BLOCKER 기준:

```text
PolicyDispatcher 우회
Controller 우회
RollbackManager 우회
ApplyGuard 우회
필수 policy Verify 전 Active 전환
```

## 12. P9-07 Partial Application Failure Handling

Patch ID: `P9-07`

목적:

Aggressive Policy Bundle 일부만 적용된 상태를 허용하지 않는다.

작업 범위:

```text
required policy apply failure
required policy verify failure
optional policy failure
partial success detection
rollback request 생성
applied component rollback
Active 전환 차단
failure severity 분류
```

수정 가능 파일:

```text
AggressiveSingleCoreController
PolicyDispatcher result consumer
RuntimeStateMachine integration point
RuntimeValidationMonitor
PerformanceEvidenceRecorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback execution internals
ApplyGuard internals
failure severity 하향 조정 rule
```

구현 규칙:

```text
required policy 하나라도 실패하면 전체 Aggressive Mode 활성화 실패.
이미 적용된 required policy는 rollback 대상으로 등록한다.
optional policy 실패가 전체 실패인지 degraded mode인지 문서에서 명확히 정의한다.
degraded mode를 정의하지 않았다면 전체 activation을 실패 처리한다.
부분 적용 상태를 success로 처리하지 않는다.
```

완료 기준:

```text
부분 적용 실패가 감지된다.
적용된 정책에 대한 rollback request가 생성된다.
Active 상태가 기록되지 않는다.
partial failure evidence가 생성된다.
```

필수 테스트:

```text
first required policy failure test
middle required policy failure partial rollback test
required verify failure rollback test
optional policy failure handling test
partial application blocks active state test
partial rollback evidence test
partial failure severity test
```

필수 Evidence:

```text
partialApplicationDetected
failedAggressivePolicy
successfullyAppliedBeforeFailure
aggressiveRollbackRequested
partialApplicationRollbackResult
aggressiveActivationFailed
partialFailureSeverity
```

리스크:

```text
부분 적용 상태를 Active로 기록하면 실제 system state와 Evidence가 어긋난다.
optional policy 실패 기준이 불명확하면 degraded mode가 검증 없이 발생할 수 있다.
```

Rollback 계획:

```text
partial application이 감지되면 적용된 policy 전체를 rollback 대상으로 등록한다.
rollback 결과가 불완전하면 RuntimeValidation BLOCKER와 RC blocker로 연결한다.
```

BLOCKER 기준:

```text
부분 적용 상태 유지
부분 실패 후 Active 기록
적용된 정책 rollback 누락
실패 policy Evidence 누락
```

## 13. P9-08 Active State Verification

Patch ID: `P9-08`

목적:

Aggressive Mode가 실제 활성 상태인지 독립적으로 검증한다.

작업 범위:

```text
required policy active 확인
thread priority verify
thread group affinity verify
processor group verify
core isolation state 확인
rollback state 존재 확인
policy timeline 일치 확인
runtime snapshot 반영
active evidence consistency 확인
```

수정 가능 파일:

```text
AggressiveSingleCoreController
RuntimeValidationMonitor
RuntimeSnapshotRecorder
PolicyTimelineRecorder consumer
관련 테스트
```

수정 금지 파일:

```text
SchedulerController verify internals
RollbackManager state ownership
ApplyGuard transaction internals
ThreadTracker sampling internals
```

구현 규칙:

```text
apply API success만으로 Active 판정 금지.
required policy의 실제 상태와 Evidence가 일치해야 한다.
active policy와 rollback state가 불일치하면 BLOCKER 후보.
모든 required policy가 Verify된 뒤에만 Active를 기록한다.
```

완료 기준:

```text
모든 required policy가 Verify된 경우에만 Active 상태가 된다.
Active 상태가 runtime snapshot과 policy timeline에 반영된다.
rollback state와 active policy 일치 여부가 기록된다.
```

필수 테스트:

```text
all required policy verified active test
missing priority verification blocks active test
missing affinity verification blocks active test
processor group mismatch blocks active test
rollback state missing blocks active test
timeline mismatch blocks active test
active evidence consistency test
```

필수 Evidence:

```text
aggressiveActiveVerificationResult
verifiedAggressivePolicies
activeProcessorGroup
activeAffinityMask
activeThreadPriority
activeRollbackStatePresent
activeStateEvidenceConsistent
```

리스크:

```text
apply success를 Active로 간주하면 verify 실패 또는 rollback state 누락을 놓칠 수 있다.
timeline과 snapshot이 불일치하면 RC에서 실제 활성 시간을 추적할 수 없다.
```

Rollback 계획:

```text
Active verification이 실패하면 Active를 기록하지 않고 RollbackPending으로 전환한다.
active policy와 rollback state가 불일치하면 rollback을 요청하고 RuntimeValidation BLOCKER로 연결한다.
```

BLOCKER 기준:

```text
Verify 전 Active 상태 전환
required policy verification 누락
rollback state 없이 Active 기록
active state Evidence 불일치
```

## 14. P9-09 Automatic Deactivation Triggers

Patch ID: `P9-09`

목적:

위험 또는 비효율 조건 발생 시 Aggressive Mode를 자동 해제한다.

작업 범위:

```text
필수 해제 조건 정의
trigger source 수집
safety trigger 우선순위 적용
deactivation state transition 정의
rollback 필요 여부 판단
shutdown state 연결
deactivation Evidence 기록
```

필수 해제 조건:

```text
Confidence가 기준 이하로 하락
Main thread candidate 변경
Migration spike 발생
Thread switch 증가
Topology 또는 Processor Group 변경
target identity mismatch
Game Profile 변경 또는 제한 강화
RuntimeValidation BLOCKER
Apply/Verify inconsistency
Rollback state inconsistency
Performance regression
Anti-Cheat 또는 protected process 의심
사용자 종료 요청
ShutdownRequested
```

수정 가능 파일:

```text
AggressiveSingleCoreController
RuntimeContext
RuntimeValidationMonitor consumer
PerformanceEvidenceRecorder consumer
ShutdownPipeline integration point
관련 테스트
```

수정 금지 파일:

```text
RuntimeValidation BLOCKER semantics
ShutdownPipeline core ordering
RollbackManager rollback execution internals
SchedulerController apply internals
```

구현 규칙:

```text
ShutdownRequested 시 신규 활성화와 유지 연장을 금지한다.
Safety trigger는 성능 trigger보다 우선한다.
Deactivation은 필요 시 RollbackPending으로 이어져야 한다.
RuntimeValidation BLOCKER 상태에서 Active를 유지하지 않는다.
```

완료 기준:

```text
필수 해제 조건이 발생하면 Deactivating 또는 RollbackPending 상태로 전환된다.
해제 원인이 Evidence에 기록된다.
해제 후 cooldown 또는 blocked state 진입 기준이 정의된다.
```

필수 테스트:

```text
confidence drop deactivation test
main thread change deactivation test
migration spike deactivation test
topology change deactivation test
identity mismatch deactivation test
runtime blocker deactivation test
performance regression deactivation test
shutdown deactivation test
anti-cheat suspicion deactivation test
```

필수 Evidence:

```text
aggressiveDeactivationTriggered
aggressiveDeactivationReason
deactivationTriggerSource
rollbackRequiredOnDeactivation
aggressiveActiveDuration
deactivationResultState
```

리스크:

```text
필수 해제 조건이 늦게 반영되면 unsafe 상태에서 강한 정책이 유지될 수 있다.
shutdown 이후 Active 유지 연장이 가능하면 final evidence flush와 충돌할 수 있다.
```

Rollback 계획:

```text
해제 trigger 판단이 불명확하면 safety trigger로 간주하고 즉시 Deactivating으로 전환한다.
deactivation 중 rollback 필요성이 있으면 RollbackPending으로 전환한다.
```

BLOCKER 기준:

```text
RuntimeValidation BLOCKER 상태에서 Active 유지
identity mismatch 상태에서 Active 유지
ShutdownRequested 이후 Active 유지 연장
필수 해제 조건 무시
```

## 15. P9-10 Cooldown and Hysteresis

Patch ID: `P9-10`

목적:

Aggressive Mode 반복 활성화와 thrashing을 방지한다.

작업 범위:

```text
activation hysteresis
deactivation hysteresis
minimum active duration
maximum active duration
cooldown duration
reactivation condition
failure cooldown
regression cooldown
cooldown evidence
```

수정 가능 파일:

```text
AggressiveSingleCoreController
RuntimeContext
PolicyTimelineRecorder
PerformanceEvidenceRecorder
관련 테스트
```

수정 금지 파일:

```text
RuntimeValidation safety trigger semantics
SchedulerController apply internals
RollbackManager rollback internals
PolicyDispatcher dispatch semantics
```

구현 규칙:

```text
Safety trigger에는 deactivation hysteresis를 적용하지 않는다.
성능 변동에 의한 해제는 제한적 hysteresis를 적용할 수 있다.
rollback failure 이후 자동 재활성화를 금지한다.
cooldown 중 Candidate 재생성 또는 Active 재진입을 금지한다.
```

완료 기준:

```text
Aggressive Mode가 짧은 지표 변동으로 반복 활성화·해제되지 않는다.
cooldown 상태와 잔여 시간이 Evidence에 기록된다.
failure cooldown과 regression cooldown이 구분된다.
```

필수 테스트:

```text
activation hysteresis test
performance deactivation hysteresis test
safety trigger immediate deactivation test
cooldown blocks reactivation test
failure cooldown test
regression cooldown test
maximum active duration test
```

필수 Evidence:

```text
aggressiveHysteresisState
aggressiveCooldownState
aggressiveCooldownReason
aggressiveCooldownRemaining
aggressiveActivationCount
aggressiveDeactivationCount
```

리스크:

```text
cooldown이 없으면 policy thrashing으로 성능과 안정성이 모두 악화될 수 있다.
safety trigger에 hysteresis를 적용하면 위험 상태 해제가 지연될 수 있다.
```

Rollback 계획:

```text
cooldown 기준이 확정되지 않았으면 failure와 safety trigger 후 재활성화를 금지한다.
hysteresis 판단이 불명확하면 safety 우선으로 즉시 해제하고 Cooldown으로 전환한다.
```

## 16. P9-11 Performance Regression Response

Patch ID: `P9-11`

목적:

Aggressive Mode 활성 후 성능 회귀가 감지되면 안전하게 해제한다.

작업 범위:

```text
Baseline / Applied Window 비교
migration regression
RTT jitter regression
DPC spike regression
confidence regression
thread switch regression
external FPS/frame time regression if available
regression-required-cycles
rollback or deactivate 결정
classification result 연결
```

수정 가능 파일:

```text
AggressiveSingleCoreController
PerformanceEvidenceRecorder consumer
Classification module consumer
RuntimeValidationMonitor
관련 테스트
```

수정 금지 파일:

```text
PerformanceEvidenceRecorder metric source semantics
Classification safety override semantics
SchedulerController apply internals
RollbackManager rollback internals
```

구현 규칙:

```text
성능 회귀가 지속 기준을 충족하면 Aggressive Mode를 해제한다.
Safety BLOCKER는 지속 기준을 기다리지 않고 즉시 해제한다.
REGRESSION 상태에서 Active 유지 또는 성능 향상 주장을 금지한다.
regression-required-cycles는 확정 전까지 TBD로 둔다.
```

완료 기준:

```text
regressionDetectionResult가 Aggressive Mode 해제 조건에 연결된다.
회귀 사유와 해제 결과가 Evidence에 기록된다.
REGRESSION 상태에서 improvement claim이 차단된다.
```

필수 테스트:

```text
migration regression deactivation test
rtt jitter regression deactivation test
dpc regression deactivation test
confidence regression deactivation test
single transient regression hold test
sustained regression deactivation test
safety blocker immediate override test
regression no improvement claim test
```

필수 Evidence:

```text
aggressiveRegressionDetected
aggressiveRegressedMetrics
aggressiveRegressionRequiredCycles
aggressiveRegressionObservedCycles
aggressiveRegressionResponse
aggressiveRegressionDeactivationResult
```

리스크:

```text
회귀 대응이 없으면 강한 정책이 성능 악화를 만든 상태로 유지될 수 있다.
transient와 sustained regression 기준이 불명확하면 과도한 해제 또는 늦은 해제가 발생할 수 있다.
```

Rollback 계획:

```text
regression threshold가 TBD이면 regression 신호를 improvement 근거로 사용하지 않는다.
명확한 REGRESSION이 감지되면 Deactivating 또는 RollbackPending으로 전환한다.
```

## 17. P9-12 Aggressive Mode Evidence Integration

Patch ID: `P9-12`

목적:

Aggressive Mode의 후보 생성부터 rollback까지 전체 lifecycle을 Evidence로 기록한다.

작업 범위:

```text
candidate created
gate passed/rejected
policy bundle planned
arming started
apply result
verify result
active started
active duration
deactivation triggered
rollback requested
rollback result
cooldown started
final classification
```

수정 가능 파일:

```text
RuntimeSnapshotRecorder
PolicyTimelineRecorder
PerformanceEvidenceRecorder
Rollback summary recorder
RCReportGenerator input
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback execution internals
ApplyGuard internals
RuntimeValidation BLOCKER semantics
```

구현 규칙:

```text
Aggressive Mode success는 Evidence 없이 인정하지 않는다.
Active 시작과 종료 시점을 모두 기록한다.
부분 적용과 rollback 결과를 숨기지 않는다.
activation / deactivation / cooldown은 timeline 순서가 보존되어야 한다.
```

완료 기준:

```text
Aggressive Mode lifecycle이 policy timeline과 runtime snapshot에 기록된다.
RC Report에서 활성화 조건, 지속 시간, 해제 사유, rollback 결과를 추적할 수 있다.
Evidence completeness가 RuntimeValidation과 RC input으로 연결된다.
```

필수 테스트:

```text
candidate evidence test
gate rejection evidence test
activation evidence test
active duration evidence test
deactivation evidence test
partial failure evidence test
rollback evidence test
cooldown evidence test
lifecycle timeline monotonicity test
```

필수 Evidence:

```text
aggressiveModeSummary
aggressiveCandidateCount
aggressiveActivationCount
aggressiveActivationGateSummary
aggressivePolicyBundleSummary
aggressiveActiveDuration
aggressiveDeactivationSummary
aggressiveRollbackSummary
aggressiveCooldownSummary
```

리스크:

```text
Evidence 누락은 강한 정책이 왜 켜졌고 왜 꺼졌는지 검증할 수 없게 만든다.
부분 적용 Evidence가 없으면 rollback completeness를 확인할 수 없다.
```

Rollback 계획:

```text
상세 lifecycle Evidence가 불안정하면 candidate, gate result, active duration, deactivation reason, rollback result 최소 필드를 우선 기록한다.
필수 Evidence가 누락되면 Active success로 인정하지 않는다.
```

BLOCKER 기준:

```text
Aggressive Mode Active Evidence 누락
해제 사유 누락
부분 적용 Evidence 누락
rollback result 누락
```

## 18. P9-13 RuntimeValidation Aggressive Mode Checks

Patch ID: `P9-13`

목적:

Aggressive Mode 계약 위반을 RuntimeValidation에서 감지하게 한다.

작업 범위:

```text
VeryHigh Confidence requirement validation
activation gate completeness validation
profile permission validation
topology safety validation
Processor Group safety validation
rollback state consistency validation
ApplyGuard consistency validation
active policy consistency validation
cooldown consistency validation
deactivation trigger response validation
Evidence completeness validation
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
AggressiveSingleCoreController
PolicyTimelineRecorder consumer
관련 테스트
```

수정 금지 파일:

```text
RuntimeValidationMonitor가 직접 mutation 수행
SchedulerController apply internals
RollbackManager rollback internals
ApplyGuard transaction internals
```

구현 규칙:

```text
RuntimeValidation은 Aggressive Mode를 직접 적용하거나 해제하지 않는다.
RuntimeValidation은 계약 위반을 WARN 또는 BLOCKER로 분류한다.
mutation safety 위반은 BLOCKER로 분류한다.
RuntimeValidation BLOCKER는 RC PASS를 차단한다.
```

완료 기준:

```text
Aggressive Mode 계약 위반이 RuntimeValidation Summary에 반영된다.
BLOCKER가 RC PASS를 차단한다.
계약 위반 목록이 Evidence에 기록된다.
```

필수 테스트:

```text
active without very high confidence blocker test
active without complete gates blocker test
unknown profile active blocker test
topology unsafe active blocker test
rollback state missing blocker test
ApplyGuard inconsistency blocker test
partial apply active blocker test
missing aggressive evidence blocker test
cooldown consistency blocker test
```

필수 Evidence:

```text
aggressiveRuntimeValidationState
aggressiveValidationWarnCount
aggressiveValidationBlockerCount
aggressiveContractViolationList
aggressiveEvidenceCompleteness
```

리스크:

```text
RuntimeValidation이 aggressive violation을 놓치면 RC PASS가 잘못 생성될 수 있다.
RuntimeValidation이 직접 mode를 조작하면 관찰자 역할과 executor 역할이 섞인다.
```

Rollback 계획:

```text
RuntimeValidation check가 불안정하면 mutation safety 위반과 missing evidence check를 우선 적용한다.
BLOCKER가 하나라도 있으면 PASS를 차단한다.
```

## 19. P9-14 Long Soak and Failure Injection Validation

Patch ID: `P9-14`

목적:

장시간 실행과 실패 주입 환경에서 Aggressive Mode의 안정성을 검증한다.

작업 범위:

```text
long soak activation count
long soak deactivation count
policy thrashing
cooldown effectiveness
memory growth
CPU overhead
snapshot freshness
timeline monotonicity
save failure injection
apply failure injection
verify failure injection
rollback failure injection
identity mismatch injection
shutdown during activation
```

수정 가능 파일:

```text
validation scripts if existing
AggressiveSingleCoreController tests
RuntimeValidation tests
Rollback / ApplyGuard integration tests
Evidence validation tests
```

수정 금지 파일:

```text
SchedulerController apply internals
RollbackManager rollback execution internals
ApplyGuard internals
ThreadTracker sampling internals
```

구현 규칙:

```text
Save, Apply, Verify 실패 시 부분 적용 상태가 남지 않아야 한다.
rollback failure는 BLOCKER로 기록한다.
Long Soak 중 policy thrashing을 허용하지 않는다.
shutdown 중 신규 activation을 허용하지 않는다.
```

완료 기준:

```text
Aggressive Mode failure injection 결과가 모두 분류된다.
Long Soak에서 state leak, rollback state loss, evidence 누락이 없다.
soak 결과가 RC blocker 목록과 연결된다.
```

필수 테스트:

```text
save failure injection test
apply failure injection test
verify failure injection test
rollback failure injection test
identity mismatch injection test
shutdown during arming test
shutdown during applying test
shutdown during active test
long soak thrashing test
long soak evidence completeness test
```

필수 Evidence:

```text
aggressiveSoakSummary
aggressiveFailureInjectionSummary
saveFailureResult
applyFailureResult
verifyFailureResult
rollbackFailureResult
identityMismatchResult
shutdownInterruptionResult
policyThrashingDetected
```

리스크:

```text
happy path만 검증하면 partial failure, rollback failure, shutdown interruption에서 상태가 새어 나갈 수 있다.
long soak가 없으면 cooldown과 hysteresis가 실제 thrashing을 막는지 확인할 수 없다.
```

Rollback 계획:

```text
failure injection에서 부분 적용 상태가 남으면 Phase 9 구현을 중단하고 P9-07로 되돌린다.
long soak에서 policy thrashing이 감지되면 cooldown/hysteresis 기준을 강화하고 Active success를 인정하지 않는다.
```

BLOCKER 기준:

```text
실패 주입 후 부분 적용 상태 잔존
rollback state 손실
rollback failure severity 하향
shutdown 중 신규 activation
long soak policy thrashing
Evidence 누락
```

## 20. P9-15 Phase 9 Regression Test and Evidence

Patch ID: `P9-15`

목적:

Phase 9 패치 묶음의 회귀 테스트와 Release Evidence 기준을 정리한다.

작업 범위:

```text
Phase 9 test list 작성
Phase 9 static check list 작성
Phase 9 runtime evidence list 작성
Phase 9 release blocker list 작성
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
AggressiveSingleCoreController 직접 Win32 mutation
성능 향상 수치 보장 문구
```

구현 규칙:

```text
Phase 9 PASS는 실제 게임 성능 향상 보장이 아니다.
Phase 9 PASS는 Aggressive Mode가 복합 Gate, transaction, rollback, evidence 계약을 만족한다는 의미다.
RegressionMatrix는 activation blocker와 partial failure blocker를 포함해야 한다.
```

완료 기준:

```text
Phase 9 regression checklist가 존재한다.
Phase 9 BLOCKER 조건이 ReleaseChecklist와 RC Runbook에 연결된다.
후속 Phase 10 작성에 필요한 기준이 남는다.
```

필수 테스트:

```text
Aggressive Mode state model suite
Activation Candidate suite
Composite Activation Gate suite
Policy Bundle suite
Controller handoff suite
Partial failure rollback suite
Automatic deactivation suite
Cooldown and hysteresis suite
Regression response suite
Aggressive Evidence suite
RuntimeValidation aggressive suite
Long Soak and failure injection suite
```

필수 Evidence:

```text
phase9RegressionSummary
aggressiveModeStateSummary
aggressiveActivationGateSummary
aggressivePolicyBundleSummary
aggressiveTransactionSummary
aggressiveDeactivationSummary
aggressiveRollbackSummary
aggressiveSoakSummary
runtimeValidationSummary
```

리스크:

```text
회귀 테스트가 activation happy path에만 집중하면 gate failure, partial failure, rollback failure를 놓칠 수 있다.
ReleaseChecklist에 Phase 9 blocker가 없으면 고위험 모드 실패가 RC PASS를 막지 못한다.
```

Rollback 계획:

```text
별도 regression matrix가 준비되지 않았으면 PatchPlan_Phase9.md의 완료 기준을 source of truth로 유지한다.
ReleaseChecklist와 RC_Runbook 수정은 Phase 9 blocker 항목만 최소 반영한다.
```

## 21. Phase 9 완료 기준

Phase 9 전체 완료 기준은 다음과 같다.

1. Aggressive Mode의 책임과 mutation ownership이 구분된다.
2. Aggressive Mode 상태 모델이 정의되어 있다.
3. Activation Candidate 생성 기준이 정의되어 있다.
4. 복합 Activation Gate가 정의되어 있다.
5. VeryHigh Confidence만으로 즉시 활성화되지 않는다.
6. Unknown / MonitoringOnly / AntiCheatSensitive Profile에서는 활성화되지 않는다.
7. Topology 또는 Processor Group 정보가 불완전하면 활성화되지 않는다.
8. rollback state가 없으면 활성화되지 않는다.
9. RuntimeValidation BLOCKER 상태에서는 활성화되지 않는다.
10. Aggressive Policy Bundle은 PolicyCandidate 집합으로 구성된다.
11. 실제 mutation은 기존 Controller에만 위임된다.
12. required policy가 모두 Verify된 후에만 Active가 된다.
13. 부분 적용 실패 시 rollback되고 Active로 기록되지 않는다.
14. 자동 해제 조건이 정의되어 있다.
15. cooldown과 hysteresis가 정의되어 있다.
16. 성능 회귀가 해제 조건에 연결된다.
17. Aggressive Mode lifecycle이 Evidence에 기록된다.
18. RuntimeValidation이 Aggressive Mode 계약 위반을 감지한다.
19. Long Soak 및 Failure Injection 검증이 정의되어 있다.
20. Phase 9 regression test가 정의되어 있다.

완료 기준은 성능 향상 보장이 아니라 강한 정책이 제한된 조건에서만 활성화되고, 실패하면 되돌릴 수 있으며, 전 과정이 Evidence로 추적 가능하다는 것이다.

## 22. Phase 9 BLOCKER 조건

다음 조건은 Phase 9 BLOCKER로 분류한다.

```text
VeryHigh Confidence 미만에서 activation
단일 cycle VeryHigh만으로 activation
Unknown Profile에서 activation
MonitoringOnly Profile에서 activation
AntiCheatSensitive Profile에서 activation
Topology incomplete 상태에서 activation
Processor Group 정보 누락을 group 0으로 보정
rollback state 없이 activation
RuntimeValidation BLOCKER 상태에서 activation
AggressiveSingleCoreController 직접 Win32 mutation
PolicyDispatcher 또는 Controller 우회
ApplyGuard 우회
Verify 전 Active 상태 전환
부분 적용 상태 유지
부분 실패 후 Active 기록
필수 deactivation trigger 무시
REGRESSION 상태에서 Active 유지
ShutdownRequested 이후 신규 activation
Aggressive Mode Evidence 누락
rollback failure severity 하향
```

BLOCKER는 성능 수치와 무관하게 Aggressive Mode activation 또는 RC PASS를 차단한다.

## 23. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P9-01 Aggressive Mode Contract Alignment
2. P9-02 Aggressive Mode State Model
3. P9-03 Activation Candidate Generation
4. P9-04 Composite Activation Gate
5. P9-05 Aggressive Policy Bundle Planning
6. P9-06 Controller Handoff and Transaction Boundary
7. P9-07 Partial Application Failure Handling
8. P9-08 Active State Verification
9. P9-09 Automatic Deactivation Triggers
10. P9-10 Cooldown and Hysteresis
11. P9-11 Performance Regression Response
12. P9-12 Aggressive Mode Evidence Integration
13. P9-13 RuntimeValidation Aggressive Mode Checks
14. P9-14 Long Soak and Failure Injection Validation
15. P9-15 Phase 9 Regression Test and Evidence
```

주의:

```text
P9-04, P9-06, P9-07, P9-08, P9-09는 Phase 9의 핵심 안전 패치다.

P9-04에서 Gate 실패 상태로 activation이 가능하면 Phase 9을 중단한다.
P9-06에서 Controller 또는 ApplyGuard 우회가 발생하면 Phase 9을 중단한다.
P9-07에서 부분 적용 상태가 남으면 Phase 9을 중단한다.
P9-08에서 Verify 전 Active 전환이 가능하면 Phase 9을 중단한다.
```

## 24. 패치 작성 규칙

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
11. AggressiveSingleCoreController에서 직접 Win32 mutation을 수행하지 않는다.
12. Save → Arm → Apply → Verify → Commit/Rollback 순서를 유지한다.
13. Processor Group 정보 누락을 group 0으로 보정하지 않는다.
14. 부분 적용 상태를 success로 처리하지 않는다.
15. activation과 deactivation 결과를 Evidence에 기록한다.
```

문서 작성 규칙:

```text
1. 구현 코드를 작성하지 않는다.
2. 구체 함수 시그니처를 확정하지 않는다.
3. 확정되지 않은 시간과 threshold는 TBD로 둔다.
4. 성능 향상을 보장하지 않는다.
5. 특정 게임을 공식 지원한다고 쓰지 않는다.
6. Anti-Cheat 우회 정책을 작성하지 않는다.
7. AggressiveSingleCoreController에 직접 mutation 책임을 부여하지 않는다.
8. VeryHigh Confidence만으로 즉시 activation을 허용하지 않는다.
9. Unknown / MonitoringOnly / AntiCheatSensitive Profile에서 activation을 허용하지 않는다.
10. Rollback state 없는 activation을 허용하지 않는다.
11. RuntimeValidation BLOCKER 상태의 activation을 허용하지 않는다.
12. Verify 전 Active 상태를 허용하지 않는다.
13. 부분 적용 상태를 허용하지 않는다.
14. Processor Group group 0 hardcoding을 허용하지 않는다.
15. Markdown 형식으로 작성한다.
```

## 25. Non-Goals

다음은 PatchPlan_Phase9.md의 비목표다.

```text
구체 C++ 코드 작성
패치 파일 직접 생성
테스트 코드 직접 작성
성능 향상 수치 보장
공식 지원 게임 목록 확정
특정 게임의 실행 파일명 확정
AI/ML 기반 self-tuning
자동 threshold 학습
Anti-Cheat 우회
DLL Injection
Kernel Driver
Game Memory Patch
registry 자동 변경
Interrupt Affinity 강제 적용
외부 FPS/frame time 측정 도구 구현
UI 구현
자동 배포 구현
```

Non-Goals를 위반하는 변경은 Phase 9 범위 밖이며 별도 승인 문서가 필요하다.

## 26. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 9의 목적과 비목표가 명확하다.
2. Phase 9 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P9-01부터 P9-15까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 완료 기준, 테스트, Evidence, 리스크, Rollback 계획이 정의되어 있다.
5. Aggressive Mode 상태 모델이 정의되어 있다.
6. 복합 Activation Gate가 정의되어 있다.
7. Aggressive Policy Bundle과 Controller handoff 경계가 정의되어 있다.
8. 부분 적용 실패 처리 기준이 정의되어 있다.
9. Active State Verification 기준이 정의되어 있다.
10. 자동 해제, cooldown, hysteresis 기준이 정의되어 있다.
11. 성능 회귀 대응 기준이 정의되어 있다.
12. Aggressive Mode Evidence 기준이 정의되어 있다.
13. RuntimeValidation 검사 기준이 정의되어 있다.
14. Long Soak 및 Failure Injection 기준이 정의되어 있다.
15. Phase 9 BLOCKER 조건이 정의되어 있다.
16. 패치 순서가 정의되어 있다.
17. 후속 PatchPlan_Phase10.md 작성에 필요한 기준이 충분하다.

Approval Criteria:

1. VeryHigh Confidence만으로 activation을 허용하는 문장이 없어야 한다.
2. Unknown / MonitoringOnly / AntiCheatSensitive Profile activation 금지가 명확해야 한다.
3. AggressiveSingleCoreController가 직접 Win32 mutation을 수행하지 않는다는 기준이 명확해야 한다.
4. Verify 전 Active 전환, 부분 적용 상태 유지, rollback state 없는 activation이 모두 BLOCKER로 정의되어야 한다.
5. 확정되지 않은 시간, threshold, duration, cycle 수는 `TBD`로 남아 있어야 한다.

## 27. Open Questions

### Blocking

1. Aggressive Mode를 v3.1 정식 기능으로 둘 것인가, Experimental 기능으로 둘 것인가?
2. Aggressive Mode는 CLI에서 사용자가 명시적으로 활성화해야 하는가?
3. VeryHigh Confidence 최소 유지 시간은 얼마로 둘 것인가?
4. 최소 observation time은 얼마로 둘 것인가?
5. maximum active duration을 둘 것인가?
6. Aggressive Policy Bundle의 required policy는 무엇으로 확정할 것인가?
7. BACKGROUND_RESTRICT_UP은 기본 Bundle에서 제외하고 profile 조건부로만 둘 것인가?
8. optional policy 실패 시 degraded active mode를 허용할 것인가?
9. 부분 실패 시 Controller별 rollback을 어떻게 하나의 activation failure로 집계할 것인가?
10. Safety trigger와 performance trigger의 deactivation 절차를 동일하게 둘 것인가?
11. cooldown duration은 고정값인가 profile별 값인가?
12. regression-required-cycles는 몇 cycle로 둘 것인가?
13. RuntimeValidation BLOCKER 발생 시 즉시 rollback할 것인가, RollbackPending state로 넘길 것인가?

### Non-blocking

1. 실제 게임 Candidate Profile에서 Aggressive Mode 검증을 RC 필수로 둘 것인가?
2. v3.1에서 Aggressive Mode를 구현하지 못할 경우 v3.2로 이관할 것인가?

Blocking 질문은 Phase 9 구현 전 결정해야 한다. Non-blocking 질문은 Aggressive Mode를 disabled 또는 experimental로 제한하는 조건에서 후속 릴리즈 판단으로 이월할 수 있다.
