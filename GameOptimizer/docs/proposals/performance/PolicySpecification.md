> Status: Proposal  
> 이 문서는 제안 또는 미래 설계를 설명합니다. `docs/DOCUMENT_REGISTER.md`에서 승격되기 전까지 현재 구현 동작으로 간주하지 않습니다.

# GameOptimizer v3.1 Policy Specification

Status: Proposal
Authority: Proposed policy design, not current implementation contract
TargetVersion: v3.1 proposal
ImplementationStatus: Not implemented
VerificationStatus: Not verified/manual design review
Owner: Performance proposal

## 1. 문서 개요

문서명: GameOptimizer v3.1 Policy Specification

버전: v1.0

작성 목적: GameOptimizer v3.1의 성능 엔진(Performance Engine)이 생성하는 정책 후보(Policy Candidate)를 실제 정책 체계로 연결하기 위한 기준을 정의한다. 본 문서는 정책 후보의 분류, 활성 조건(Activation Condition), 비활성 조건(Deactivation Condition), 쿨다운(Cooldown), 위험도(Risk Level), 담당 Controller, 롤백 범위(Rollback Scope), 성능 증거(Performance Evidence), PolicyDispatcher handoff 규칙을 정의한다.

적용 범위:

- Policy Candidate 공통 구조
- v3.1 성능 정책 후보(Performance Policy Candidate)
- 정책별 활성/비활성 조건
- Controller handoff 조건
- Rollback Scope와 Evidence Field 요구사항
- Risk Level, Cooldown, Conflict 규칙

비적용 범위:

- C++ enum 최종 구현
- 구체 함수 시그니처 확정
- 구체 Win32 API 호출 구현
- UI 정책 설정 화면
- Evidence JSON schema 최종 확정
- RC Gate 구현 변경

상위 문서: `docs/proposals/performance/PerformanceEngineSpec.md`

후속 문서:

- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

기준 문서:

- `docs/vision/PVD_v1.0.md`
- `docs/proposals/performance/PerformanceEngineSpec.md`
- `docs/architecture/Architecture_Decision_Record.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Evidence_Schema.md`

## 2. Policy Layer 개요

Policy Layer는 Performance Engine의 판단을 실행 가능한 정책 후보로 정리하지만, 직접 Win32 제어 API를 호출하지 않는다.

Policy Layer는 Performance Engine이 만든 판단 결과를 Controller가 이해할 수 있는 정책 후보(Policy Candidate)로 정리한다. 실제 적용은 PolicyDispatcher를 거쳐 SchedulerController, BackgroundController, TimerResolutionController 등 기존 담당 Controller가 수행한다.

Policy Layer의 책임은 다음과 같다.

1. Policy Candidate의 종류 정의
2. Activation Condition 정의
3. Deactivation Condition 정의
4. Cooldown 정의
5. Risk Level 정의
6. Required Controller 정의
7. Rollback Scope 정의
8. Evidence Field 정의
9. PolicyDispatcher handoff 규칙 정의

Policy Layer의 금지사항은 다음과 같다.

1. 직접 SetThreadAffinityMask 호출 금지
2. 직접 SetThreadGroupAffinity 호출 금지
3. 직접 SetThreadPriority 호출 금지
4. 직접 SetPriorityClass 호출 금지
5. ApplyGuard 우회 금지
6. RollbackManager 우회 금지
7. Evidence 없이 성공 처리 금지
8. Confidence 미달 상태에서 강한 정책 생성 금지

Performance Engine은 정책 후보를 생성하고, Policy Layer는 그 후보가 안전 계약과 handoff 조건을 만족하는지 정리한다. Save, Arm, Apply, Verify, Commit, Rollback은 Controller와 기존 안전 계약의 책임이다.

## 3. Policy Candidate 공통 구조

Policy Candidate는 구현 구조체를 확정하지 않는 문서 레벨 개념이다.

```text
PolicyCandidate:
- policyType
- source
- confidenceLevel
- activationReason
- deactivationCondition
- requiredController
- requiredRollbackScope
- requiredEvidenceFields
- riskLevel
- cooldownHint
- fallbackBehavior
- monitoringOnlyReason
```

### 필드 설명

`policyType`:
정책 후보의 종류다. 예: `MAIN_THREAD_PRIORITY_UP`, `MAIN_THREAD_AFFINITY_STABILIZE`, `MONITORING_ONLY`.

`source`:
정책 후보를 생성한 판단 계층이다. 예: PerformanceEngine, LatencyDecisionLayer, RuntimeValidation.

`confidenceLevel`:
메인 스레드 신뢰도(MainThreadConfidenceLevel) 또는 정책 신뢰도다. 성능 정책은 최소 요구 confidence를 만족해야 한다.

`activationReason`:
정책 후보가 생성된 이유다. 예: migration 증가, RTT jitter 악화, confidence 상승, aggressive mode 활성 조건 충족.

`deactivationCondition`:
정책을 해제하거나 monitoring-only로 낮춰야 하는 조건이다. 예: confidence 하락, main thread 후보 교체, runtime validation BLOCKER.

`requiredController`:
실제 적용 책임을 가진 Controller다. Policy Layer가 Controller 책임을 대신하지 않는다.

`requiredRollbackScope`:
적용 전 반드시 저장해야 하는 rollback 범위다. rollback scope가 정의되지 않은 정책은 handoff될 수 없다.

`requiredEvidenceFields`:
정책 결과를 판단하기 위해 필요한 Evidence 항목이다. Evidence가 누락되면 성공으로 분류하지 않는다.

`riskLevel`:
정책 위험도다. Low, Medium, High, Blocked 중 하나로 분류한다.

`cooldownHint`:
반복 적용과 thrashing을 막기 위한 최소 대기 조건이다. 구체 수치는 후속 문서에서 확정한다.

`fallbackBehavior`:
정책 적용 실패, 조건 미충족, access boundary 발생 시 동작이다. 예: monitoring-only, rollback request, safe shutdown.

`monitoringOnlyReason`:
적용하지 않고 관찰만 하는 이유다. 안전 조건 미충족 사유는 Evidence에 남아야 한다.

## 4. Policy Type 정의

v3.1에서 사용할 정책 후보는 다음을 최소 집합으로 둔다.

```text
MAIN_THREAD_PRIORITY_UP
MAIN_THREAD_AFFINITY_STABILIZE
CORE_ISOLATION_PLAN
BACKGROUND_RESTRICT_UP
CCX_STABILITY_PREFER
AGGRESSIVE_SINGLE_CORE_ENABLE
AGGRESSIVE_SINGLE_CORE_DISABLE
TIMER_RESOLUTION_MAINTAIN
MONITORING_ONLY
ROLLBACK_REQUEST
```

각 정책은 다음 항목을 정의해야 한다.

```text
정책명
목적
생성 조건
금지 조건
필요 Confidence
필요 Controller
Rollback Scope
Evidence Fields
Cooldown
Fallback
주의사항
```

Policy Type은 문서 레벨 정의다. 기존 `PolicyCommand` enum을 확장할지, `PolicyCandidate`를 별도 계층으로 둘지는 Open Question으로 남긴다.

## 5. 정책별 상세 명세

### 5.1 MAIN_THREAD_PRIORITY_UP

정책명: `MAIN_THREAD_PRIORITY_UP`

목적:

```text
신뢰도가 높은 메인 스레드의 실행 우선순위를 제한적으로 상향한다.
```

생성 조건:

```text
MainThreadConfidence >= High
target thread identity 확인 가능
Rollback state 저장 가능
ApplyGuard Verify 가능
SchedulerMode가 apply 허용 상태
```

금지 조건:

```text
Confidence < High
thread identity 불명확
Access Denied 반복
Anti-Cheat 위험
Rollback state 저장 실패
ApplyGuard Verify 불가능
```

필요 Confidence: `High` 이상

필요 Controller:

```text
SchedulerController
```

Rollback Scope:

```text
Thread priority original state
Thread identity
Process identity
```

Evidence Fields:

```text
mainThreadId
confidenceLevel
priorityBefore
priorityAfter
applyResult
verifyResult
rollbackStateSaved
```

Cooldown: `Policy cooldown duration: TBD`

Fallback: 조건 미충족 시 `MONITORING_ONLY`로 전환한다. 적용 중 verify 실패가 발생하면 담당 Controller가 rollback 경로를 수행해야 한다.

주의사항: priority 상향은 단독으로 성능 개선을 보증하지 않는다. Before/After Evidence 없이 성공으로 분류하지 않는다.

### 5.2 MAIN_THREAD_AFFINITY_STABILIZE

정책명: `MAIN_THREAD_AFFINITY_STABILIZE`

목적:

```text
신뢰도가 높은 메인 스레드의 affinity를 안정화하여 migration을 줄인다.
```

생성 조건:

```text
MainThreadConfidence >= High
TopologyResult 사용 가능
processorGroup 확인 가능
KAFFINITY 계산 가능
migration이 임계값 이상 발생
rollback state 저장 가능
```

금지 조건:

```text
processorGroup 불명확
group 0 hardcoding 필요
Topology 정보 불완전
Confidence < High
ApplyGuard Verify 불가능
```

필요 Confidence: `High` 이상

필요 Controller:

```text
SchedulerController
TopologyAnalyzer
```

Rollback Scope:

```text
Thread group affinity original state
Thread priority original state if changed
Thread identity
```

Evidence Fields:

```text
processorGroup
affinityMaskBefore
affinityMaskAfter
migrationBefore
migrationAfter
verifyResult
rollbackStateSaved
```

Cooldown: `Policy cooldown duration: TBD`

Fallback: topology 또는 processor group 판단이 불완전하면 `MONITORING_ONLY`로 전환한다.

주의사항:

```text
Processor Group 정보가 없으면 group 0으로 조용히 보정하지 않는다.
```

### 5.3 CORE_ISOLATION_PLAN

정책명: `CORE_ISOLATION_PLAN`

목적:

```text
메인 스레드가 사용하는 core에 대한 background interference를 줄인다.
```

생성 조건:

```text
MainThreadConfidence >= High
CoreIsolationPlan 생성 가능
background process 후보 확인 가능
Processor Group 정책이 안전함
```

금지 조건:

```text
Processor Group 1+ process-wide restriction 안전 경로 없음
protected process 또는 anti-cheat 의심 process 포함
background process identity 불명확
Rollback scope 불충분
```

필요 Confidence: `High` 이상

필요 Controller:

```text
BackgroundController
SchedulerController
```

Rollback Scope:

```text
Background process affinity state
Background process priority state
Target process identity
```

Evidence Fields:

```text
coreIsolationPlan
backgroundProcessCount
excludedCoreMask
isolationStrength
applyResult
fallbackReason
```

Cooldown: `Policy cooldown duration: TBD`

Fallback: process-wide restriction이 안전하지 않으면 background restriction 후보를 생성하지 않고 monitoring-only Evidence를 남긴다.

주의사항: Core Isolation은 성능 보장이 아니라 간섭 감소 정책이다. BackgroundController가 안전하게 처리할 수 없는 범위는 handoff하지 않는다.

### 5.4 BACKGROUND_RESTRICT_UP

정책명: `BACKGROUND_RESTRICT_UP`

목적:

```text
게임 실행 코어 주변의 백그라운드 간섭을 줄이기 위해 제한적으로 background restriction을 강화한다.
```

생성 조건:

```text
RTT jitter 악화
DPC spike 관찰
background interference 의심
CoreIsolationPlan 존재
rollback state 저장 가능
```

금지 조건:

```text
protected process
system critical process
anti-cheat 의심 process
identity revalidation 실패
Processor Group 1+ 안전 경로 없음
```

필요 Confidence: `High` 이상 또는 CoreIsolationPlan이 안전 조건을 만족한 상태

필요 Controller:

```text
BackgroundController
```

Rollback Scope:

```text
Process affinity original state
Process priority original state
Process identity
```

Evidence Fields:

```text
targetProcessList
restrictionLevel
rttJitterBefore
rttJitterAfter
dpcSpikeBefore
dpcSpikeAfter
rollbackStateSaved
```

Cooldown: `Policy cooldown duration: TBD`, `Access Denied threshold: TBD`

Fallback: Access Denied 또는 identity revalidation 실패 시 monitoring-only로 전환하고 fallback reason을 Evidence에 기록한다.

주의사항: 명시적으로 허용된 background process 후보 외에는 제한 대상으로 삼지 않는다.

### 5.5 CCX_STABILITY_PREFER

정책명: `CCX_STABILITY_PREFER`

목적:

```text
AMD Ryzen 계열 등 L3/CCX 구조가 의미 있는 환경에서 main thread 이동 범위를 제한적으로 선호한다.
```

생성 조건:

```text
TopologyResult complete
L3/CCX 판단 가능
MainThreadConfidence >= High
migration history에서 CCX/L3 경계 이동 관찰
```

금지 조건:

```text
Topology 정보 불완전
Processor Group 누락
Intel Hybrid 구조를 AMD CCX로 단정해야 하는 경우
Confidence < High
```

필요 Confidence: `High` 이상

필요 Controller:

```text
TopologyAnalyzer
SchedulerController
```

Rollback Scope:

```text
Thread affinity original state
Thread group affinity original state
```

Evidence Fields:

```text
l3Group
ccxOrCacheGroup
preferredCoreRange
migrationHistory
topologyCompleteness
monitoringOnlyReason
```

Cooldown: `Policy cooldown duration: TBD`

Fallback:

```text
Topology 판단이 불완전하면 monitoring-only
```

주의사항: CCX 정책은 topology 선호 판단이며, 불확실한 추정값으로 강제 affinity 후보를 만들지 않는다.

### 5.6 AGGRESSIVE_SINGLE_CORE_ENABLE

정책명: `AGGRESSIVE_SINGLE_CORE_ENABLE`

목적:

```text
충분한 신뢰도와 안전 조건이 확보된 경우 제한적으로 강한 싱글코어 최적화 정책 묶음을 활성화한다.
```

생성 조건:

```text
MainThreadConfidence >= VeryHigh
Minimum observation time 충족
Migration 안정 상태
Rollback state 저장 가능
ApplyGuard Verify 가능
RuntimeValidation BLOCKER 없음
Access Denied 반복 없음
Processor Group 정책 안전
```

금지 조건:

```text
Confidence < VeryHigh
관찰 시간 부족
main thread 후보 교체 발생
rollback state 저장 실패
runtime validation BLOCKER
processor topology 불확실
Anti-Cheat boundary 위험
```

필요 Confidence: `VeryHigh`

필요 Controller:

```text
PolicyDispatcher
SchedulerController
BackgroundController
TimerResolutionController
PerformanceEvidenceRecorder
```

Rollback Scope:

```text
Thread priority
Thread group affinity
Process priority
Background process restriction state
Timer resolution original state if modified
```

Evidence Fields:

```text
aggressiveModeStart
confidenceLevel
observationDuration
migrationStability
enabledPolicies
rollbackScopesSaved
runtimeValidationState
```

Cooldown: `Policy cooldown duration: TBD`, `Rollback cooldown duration: TBD`

Fallback: 활성 조건 중 하나라도 깨지면 `AGGRESSIVE_SINGLE_CORE_DISABLE` 또는 `MONITORING_ONLY` 후보를 생성한다.

주의사항:

```text
Aggressive Single-Core Mode는 하나의 직접 적용 명령이 아니라 여러 제한 정책의 활성 조건이다.
```

### 5.7 AGGRESSIVE_SINGLE_CORE_DISABLE

정책명: `AGGRESSIVE_SINGLE_CORE_DISABLE`

목적:

```text
Aggressive Single-Core Mode를 안전하게 해제하거나 monitoring-only로 낮춘다.
```

생성 조건:

```text
Confidence 하락
main thread 후보 교체
Access Denied 반복
RuntimeValidation BLOCKER
Rollback 위험 발생
target process 종료
identity mismatch
```

금지 조건: 없음. 안전 해제 요청은 성능 정책보다 우선한다.

필요 Confidence: 제한 없음

필요 Controller:

```text
PolicyDispatcher
SchedulerController
BackgroundController
TimerResolutionController
```

Rollback Scope:

```text
Aggressive mode에서 저장한 모든 rollback state
```

Evidence Fields:

```text
disableReason
activeDuration
rollbackResult
preservedStateCount
runtimeValidationState
```

Cooldown: disable 후 강한 정책 재생성은 `Rollback cooldown duration: TBD` 동안 차단한다.

Fallback:

```text
Rollback 실패 시 release-facing evidence에서 BLOCKER 후보로 기록
```

주의사항: disable은 policy success가 아니라 안전 상태 전환이다. rollback failure는 낮은 severity로 낮추지 않는다.

### 5.8 TIMER_RESOLUTION_MAINTAIN

정책명: `TIMER_RESOLUTION_MAINTAIN`

목적:

```text
기존 timer resolution 정책을 성능 모드 중 유지할지 판단한다.
```

생성 조건:

```text
Timer resolution original state 저장 완료
RuntimeValidation 정상
Aggressive Mode 또는 High confidence 성능 정책 활성
```

금지 조건:

```text
original timer state 불명확
rollback 경로 불명확
system-wide 영향이 허용되지 않는 상태
```

필요 Confidence: 성능 정책이 의존하는 confidence를 따른다.

필요 Controller:

```text
TimerResolutionController
```

Rollback Scope:

```text
Original timer resolution state
```

Evidence Fields:

```text
timerResolutionBefore
timerResolutionAfter
rollbackStateSaved
restoreResult
```

Cooldown: `Policy cooldown duration: TBD`

Fallback: original state 또는 restore 경로가 불명확하면 유지 후보를 생성하지 않는다.

주의사항: timer resolution은 system-wide 영향을 가질 수 있으므로 적용 유지 판단에는 rollback 가능성과 shutdown restore Evidence가 필요하다.

### 5.9 MONITORING_ONLY

정책명: `MONITORING_ONLY`

목적:

```text
정책 적용 조건이 부족하거나 안전 조건이 불충분할 때 관찰만 수행한다.
```

생성 조건:

```text
Confidence 부족
Topology 불완전
Processor Group 제한
Access Denied
Anti-Cheat 의심
Rollback state 저장 불가능
Evidence field 부족
```

금지 조건: 없음. Monitoring-only는 안전 fallback이다.

필요 Confidence: 제한 없음

필요 Controller:

```text
없음 또는 RuntimeMonitor
```

Rollback Scope:

```text
None
```

Evidence Fields:

```text
monitoringOnlyReason
blockedPolicyType
confidenceLevel
topologyState
accessDeniedCount
```

Cooldown: monitoring-only 상태에서 강한 정책으로 복귀하려면 cooldown과 재관찰 조건을 만족해야 한다. 구체 수치는 `TBD`.

Fallback: 관찰 자체가 불가능하면 RuntimeValidation 또는 shutdown 경로에서 처리한다.

주의사항: monitoring-only는 제품 실패가 아니라 안전한 제한 동작이다.

### 5.10 ROLLBACK_REQUEST

정책명: `ROLLBACK_REQUEST`

목적:

```text
정책 적용 실패, runtime validation failure, shutdown, target process 종료 시 안전 복구를 요청한다.
```

생성 조건:

```text
shutdown
runtime validation BLOCKER
apply verify 실패
target process 종료
identity mismatch
aggressive mode 비활성
```

금지 조건: 없음. 안전 복구 요청은 성능 정책보다 우선한다.

필요 Confidence: 제한 없음

필요 Controller:

```text
RollbackManager
SchedulerController
BackgroundController
TimerResolutionController
```

Rollback Scope:

```text
Saved states affected by active policies
```

Evidence Fields:

```text
rollbackReason
rollbackScope
rollbackResult
failedStatePreserved
shutdownReason
```

Cooldown: rollback 직후 강한 정책 재생성은 `Rollback cooldown duration: TBD` 동안 차단한다.

Fallback: rollback 실패 시 rollback state를 보존하고 shutdown retry 책임을 기존 안전 계약에 따라 넘긴다.

주의사항:

```text
Rollback failure는 WARN/INFO로 낮추지 않는다.
```

## 6. Risk Level 정의

정책 위험도(Risk Level)는 다음과 같이 정의한다.

```text
Low:
관찰 또는 Evidence 기록 중심. 시스템 상태 변경 없음.

Medium:
제한적이고 즉시 복구 가능한 thread-level 정책 후보.

High:
Process-level restriction, background restriction, timer resolution 등 시스템 영향 가능성이 있는 정책.

Blocked:
안전 조건 미충족, anti-cheat 위험, rollback 불가능, evidence 부족.
```

각 정책은 Risk Level을 가져야 한다. Risk Level은 정책 적용 권한이 아니라 검증과 fallback 강도를 정하기 위한 분류다.

기본 후보 분류:

- `MONITORING_ONLY`: Low
- `MAIN_THREAD_PRIORITY_UP`: Medium
- `MAIN_THREAD_AFFINITY_STABILIZE`: Medium
- `CCX_STABILITY_PREFER`: Medium 또는 Blocked
- `CORE_ISOLATION_PLAN`: Medium 또는 High
- `BACKGROUND_RESTRICT_UP`: High
- `TIMER_RESOLUTION_MAINTAIN`: High
- `AGGRESSIVE_SINGLE_CORE_ENABLE`: High
- `AGGRESSIVE_SINGLE_CORE_DISABLE`: Low 또는 Medium
- `ROLLBACK_REQUEST`: Safety-critical

`Safety-critical`은 성능 정책 위험도보다 우선하는 안전 복구 요청을 의미한다. 구현상 enum 값으로 확정하지 않는다.

## 7. Cooldown 규칙

반복 적용과 thrashing을 막기 위해 쿨다운(Cooldown) 규칙을 둔다.

공통 규칙:

```text
동일 policyType은 짧은 시간 안에 반복 생성하지 않는다.
main thread 후보가 바뀌면 기존 aggressive 후보는 무효화한다.
rollback 직후에는 일정 시간 동안 강한 정책 생성을 차단한다.
Access Denied 반복 시 monitoring-only로 전환한다.
```

구체 수치는 아직 확정하지 않는다.

```text
Policy cooldown duration: TBD
Rollback cooldown duration: TBD
Access Denied threshold: TBD
```

Cooldown은 성능 개선 기회를 줄이기 위한 장치가 아니라 불안정한 반복 적용을 막기 위한 안전 장치다.

## 8. Policy Conflict 규칙

정책 간 충돌 사례는 다음과 같다.

```text
Core Isolation vs Background Restriction
Aggressive Single-Core Mode vs RuntimeValidation BLOCKER
CCX Stability vs Processor Group 불확실성
Main Thread Affinity vs Main Thread 후보 교체
Timer Resolution Maintain vs 원상 복구 상태 불명확
```

충돌 해결 원칙:

```text
1. Safety Contract가 항상 우선한다.
2. Rollback 불가능한 정책은 생성하지 않는다.
3. Evidence 부족 시 성능 성공으로 분류하지 않는다.
4. Processor Group 정보가 불완전하면 group 0으로 보정하지 않는다.
5. RuntimeValidation BLOCKER는 aggressive mode보다 우선한다.
```

Policy Conflict는 PolicyDispatcher 이전에 가능한 한 식별되어야 한다. 단, 실제 적용 중 발생하는 충돌은 담당 Controller의 verify/rollback 경로와 RuntimeValidation Evidence를 통해 처리한다.

## 9. Policy Handoff Flow

정책 handoff 흐름은 다음과 같다.

```text
Performance Engine
↓
Policy Candidate
↓
Policy Layer validation
↓
PolicyDispatcher
↓
담당 Controller
↓
ApplyGuard / RollbackManager
↓
Evidence Recorder
```

PolicyDispatcher로 넘기기 전 확인 항목:

```text
policyType 유효성
requiredController 지정 여부
rollbackScope 지정 여부
evidenceFields 지정 여부
riskLevel 지정 여부
deactivationCondition 지정 여부
cooldown 위반 여부
```

handoff가 실패하면 해당 정책은 적용하지 않는다. 실패 사유는 monitoring-only 또는 blocked policy Evidence로 남긴다.

## 10. Evidence 요구사항

각 정책은 Evidence Field를 가져야 한다.

공통 필수 Evidence:

```text
policyType
activationReason
confidenceLevel
riskLevel
requiredController
rollbackScope
applyResult
verifyResult
rollbackStateSaved
fallbackReason
runtimeValidationState
```

성능 정책은 Before/After 비교 가능성을 가져야 한다.

```text
Baseline Window
Applied Window
Comparison Summary
```

Evidence가 누락된 경우:

```text
정책 성공으로 분류하지 않는다.
릴리즈 판단에서 WARN 또는 BLOCKER 후보가 된다.
```

Rollback failure, runtime validation BLOCKER, identity mismatch, unsafe rollback-state discard는 낮은 severity로 처리하지 않는다.

## 11. Safety Contract

모든 정책은 다음 흐름을 따라야 한다.

```text
Observe
↓
Analyze
↓
Decide
↓
Validate Policy Candidate
↓
Save
↓
Arm
↓
Apply
↓
Verify
↓
Commit
↓
Evidence
```

실패 시:

```text
Rollback
↓
Evidence
↓
Monitoring-only fallback 또는 Safe Shutdown
```

Rollback 없는 정책은 Policy Layer에서 승인될 수 없다.

Evidence 없는 정책은 Policy Layer에서 성공으로 분류될 수 없다.

Policy Layer는 Save/Arm/Apply/Verify/Commit을 직접 수행하지 않는다. 이 단계는 PolicyDispatcher 이후 담당 Controller와 기존 ApplyGuard/RollbackManager 계약이 수행한다.

## 12. Non-Goals

다음은 Policy Specification의 비목표다.

```text
C++ enum 최종 구현
구체 함수 시그니처 확정
구체 Win32 API 호출 구현
UI 정책 설정 화면
AI/ML 자동 튜닝
Self-Learning 정책
Anti-Cheat 우회 정책
Kernel Driver 정책
DLL Injection 정책
Game Memory Patch 정책
검증 없는 registry 자동 변경 정책
FPS 또는 frame time 개선 보증 정책
```

본 문서는 정책 판단 기준을 정의하지만 구현 방식과 최종 API 표면을 확정하지 않는다.

## 13. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

```text
1. Policy Candidate 공통 구조가 정의되어 있다.
2. v3.1의 주요 Policy Type이 정의되어 있다.
3. 각 정책의 생성 조건과 금지 조건이 정의되어 있다.
4. 각 정책의 requiredController가 정의되어 있다.
5. 각 정책의 Rollback Scope가 정의되어 있다.
6. 각 정책의 Evidence Fields가 정의되어 있다.
7. Risk Level 기준이 정의되어 있다.
8. Cooldown 기본 원칙이 정의되어 있다.
9. Policy Conflict 해결 원칙이 정의되어 있다.
10. PolicyDispatcher handoff 흐름이 정의되어 있다.
11. Rollback/Evidence 없는 정책 금지 원칙이 포함되어 있다.
12. 후속 SAD/MDS 문서 작성에 필요한 정책 기준이 충분하다.
```

Acceptance Criteria는 이 문서의 승인 기준이다. 구현 완료, RC 승인, 성능 검증 완료를 의미하지 않는다.

## 14. Open Questions

1. 기존 PolicyCommand enum을 확장할 것인가, PolicyCandidate를 별도 계층으로 둘 것인가?
2. MAIN_THREAD_PRIORITY_UP과 MAIN_THREAD_AFFINITY_STABILIZE를 하나의 정책으로 묶을 것인가?
3. Aggressive Single-Core Mode는 meta policy로 둘 것인가, 별도 command로 둘 것인가?
4. Background Restriction의 단계는 몇 단계로 나눌 것인가?
5. Rollback Scope를 정책별로 정적으로 정의할 것인가, 런타임에서 동적으로 계산할 것인가?
6. Cooldown 기본값은 어느 문서에서 확정할 것인가?
7. Evidence 누락 시 WARN과 BLOCKER 기준은 어디서 확정할 것인가?
8. Timer Resolution 정책은 Performance Policy에 포함할 것인가, Runtime Policy로 분리할 것인가?
9. PolicyDispatcher가 conflict resolution을 담당할 것인가, 별도 PolicyResolver를 둘 것인가?
