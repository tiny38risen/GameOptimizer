> Status: Proposal  
> 이 문서는 제안 또는 미래 설계를 설명합니다. `docs/DOCUMENT_REGISTER.md`에서 승격되기 전까지 현재 구현 동작으로 간주하지 않습니다.

# GameOptimizer v3.1 Game Profile Specification

Status: Proposal
Authority: Proposed game profile design, not current implementation contract
TargetVersion: v3.1 proposal
ImplementationStatus: Not implemented
VerificationStatus: Not verified/manual design review
Owner: Performance proposal

## 1. 문서 개요

문서명: GameOptimizer v3.1 Game Profile Specification

버전: v1.0

작성 목적: GameOptimizer v3.1에서 게임별 정책 적용 기준을 정의한다. 이 문서는 게임 프로파일(Game Profile)을 통해 정책 적용 가능성, 위험도(Risk Level), 근거(Evidence), 검증 상태(Profile Status)를 판단하기 위한 기준 문서다.

적용 범위:

- 게임별 기본 적용 모드(Default Apply Mode) 정의
- 게임별 허용 정책(Allowed Policies), 금지 정책(Blocked Policies), 조건부 정책(Conditional Policies) 정의
- 검증 대상 후보(Validation Candidate)와 관찰 전용(Monitoring-only) 판단 기준 정의
- 프로파일 검증(Profile Validation)과 fallback 기준 정의

비적용 범위:

- 코드 구현
- 구체 JSON/YAML/TOML schema 확정
- 프로파일 파서(Profile Parser) 구현
- UI profile 선택 화면 구현
- 특정 게임의 공식 지원 선언
- 특정 게임의 성능 향상 보장
- Anti-Cheat 우회 정책

상위 문서:

- `docs/vision/PVD_v1.0.md`
- `docs/proposals/performance/PerformanceEngineSpec.md`
- `docs/proposals/performance/PolicySpecification.md`
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

후속 문서:

- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/implementation/ImplementationPlan_v3.1.md`

문서 경로 기준: `GameOptimizer/docs/`

Game Profile은 게임별 성능 향상을 보장하는 문서가 아니라, 게임별 정책 적용 가능성과 위험도를 판단하기 위한 설정 기준 문서다.

## 2. Game Profile 목적

Game Profile의 목적은 게임별로 정책을 더 강하게 적용하기 위한 것이 아니라, 적용 가능한 정책과 적용하면 안 되는 정책을 안전하게 구분하는 데 있다.

필수 목적:

1. 게임별 정책 적용 범위 정의
2. 게임별 기본 apply mode 정의
3. 게임별 금지 정책 정의
4. 게임별 risk level 정의
5. 게임별 required evidence 정의
6. 게임별 validation priority 정의
7. monitoring-only 전환 조건 정의
8. profile 변경 기준 정의
9. 검증되지 않은 게임 정보의 TBD 처리 기준 정의

Game Profile의 목적은 더 많은 게임에 강한 정책을 적용하는 것이 아니라, 적용해도 되는 게임과 적용하면 안 되는 게임을 구분하는 것이다.

GameOptimizer v3.1은 모든 게임을 빠르게 만드는 범용 FPS Booster가 아니다. 싱글코어 병목(Single-core Bottleneck) 가능성이 있는 게임을 우선 관찰하되, 최신 멀티코어 구조, GPU 병목(GPU-bound), Anti-Cheat 위험, 증거 부족 상태에서는 강한 mutation보다 dry-run 또는 monitoring-only를 우선한다.

## 3. 검증되지 않은 게임 정보 처리 규칙

Codex는 실제 게임 정보를 임의로 확정하지 않는다. 웹 검색 또는 추측 기반으로 게임 내부 구조, target process, anti-cheat 위험도, 병목 구조, 성능 개선 가능성을 확정하지 않는다.

다음 값은 검증 전까지 반드시 `TBD`, `Unknown`, `Candidate`, 또는 `Validation Required` 상태로 둔다.

```text
targetProcessName
engineType
antiCheatRisk
singleCoreBottleneckConfirmed
gpuBoundConfirmed
networkSensitivityConfirmed
validatedPolicySet
aggressiveModeEligibility
```

금지 예시:

```text
profileStatus: Validated
targetProcessName: Mabinogi.exe
engineType: single-core legacy
antiCheatRisk: Low
```

허용 예시:

```text
profileStatus: Candidate
targetProcessName: TBD
engineType: TBD
antiCheatRisk: Unknown
singleCoreBottleneckConfirmed: false
validationRequired: true
```

검증되지 않은 게임 정보는 profile 문서에서 확정값처럼 작성하지 않는다.
Candidate profile은 검증 대상을 정의할 뿐, 성능 특성을 단정하지 않는다.

검증되지 않은 프로파일은 aggressive mode 또는 registry 변경 정책을 허용하지 않는다. 실제 apply 가능 여부는 `docs/validation/PerformanceValidationPlan.md`와 `docs/evidence/EvidenceSpecification.md` 기준을 통과한 이후에만 판단한다.

## 4. Profile 분류 체계

Game Profile은 다음 profile type으로 분류한다.

1. `SingleCoreLegacy`
2. `NetworkSensitiveLegacy`
3. `LegacyMMORPG`
4. `ModernMulticore`
5. `GpuBound`
6. `AntiCheatSensitive`
7. `Unknown`

각 profile type은 다음 양식으로 정의한다.

```text
목적
대상 특성
기본 apply mode
허용 정책
금지 정책
required evidence
fallback
risk level
validation requirement
```

Profile 분류는 영구 고정값이 아니다. Evidence와 Validation 결과에 따라 `Candidate`에서 `Restricted`, `MonitoringOnly`, 또는 `Validated`로 변경될 수 있으며, 본 문서는 특정 게임을 `Validated`로 분류하지 않는다.

## 5. Profile Type 상세 정의

### 5.1 SingleCoreLegacy

목적: 메인 스레드(Main Thread) 또는 싱글코어 병목이 의심되는 게임 후보에 대해 보수적인 CPU scheduling 정책 후보를 정의한다.

대상 특성:

- 싱글코어 또는 메인 스레드 병목이 강한 것으로 의심되거나 검증 대상인 게임
- 오래된 엔진 기반일 가능성이 있는 게임
- thread migration에 민감할 가능성이 있는 게임
- CPU 전체 사용률은 낮아도 main thread가 병목일 수 있는 게임

기본 apply mode:

```text
dry-run -> soft-apply -> apply
```

허용 정책 후보:

```text
MAIN_THREAD_PRIORITY_UP
MAIN_THREAD_AFFINITY_STABILIZE
CCX_STABILITY_PREFER
CORE_ISOLATION_PLAN
TIMER_RESOLUTION_MAINTAIN
```

조건부 허용:

```text
AGGRESSIVE_SINGLE_CORE_ENABLE
```

조건부 허용 조건:

```text
MainThreadConfidence >= VeryHigh
Rollback state 저장 가능
RuntimeValidation BLOCKER 없음
Before/After Evidence 확보 가능
Processor Group 정책 안전
```

금지 또는 제한:

```text
검증 없는 background restriction
Processor Group 1+ unsafe process-wide restriction
Anti-Cheat 의심 환경에서 mutation
검증 없는 registry 자동 변경
```

required evidence:

```text
Main Thread Confidence History
Main Thread Migration Count
TopologyResult
Processor Group state
Before/After Summary
Rollback Result
Runtime Validation Summary
```

fallback: required evidence 누락, rollback 불가, RuntimeValidation BLOCKER 발생, 또는 anti-cheat suspicion 발생 시 `MonitoringOnly`로 전환한다.

risk level: `Medium` 후보이며, 실제 risk level은 검증 전까지 `TBD`로 둔다.

validation requirement: `docs/validation/PerformanceValidationPlan.md` 기준의 baseline, soft-apply, rollback, before/after evidence를 충족해야 한다.

### 5.2 NetworkSensitiveLegacy

목적: 네트워크 지터(Network Jitter), 입력 반응성(Input Responsiveness), DPC spike 영향을 받을 수 있는 구형 온라인 게임 후보의 보수적 정책 기준을 정의한다.

대상 특성:

- 네트워크 지터와 입력 반응성에 민감한 구형 온라인 게임 후보
- RTT jitter 또는 DPC spike가 체감 지연에 영향을 줄 수 있는 게임 후보

기본 apply mode:

```text
dry-run -> soft-apply
```

허용 정책 후보:

```text
BACKGROUND_RESTRICT_UP
CORE_ISOLATION_PLAN
TIMER_RESOLUTION_MAINTAIN
MAIN_THREAD_PRIORITY_UP
```

조건부 허용:

```text
MAIN_THREAD_AFFINITY_STABILIZE
```

금지 또는 제한:

```text
네트워크 registry 자동 변경
검증 없는 TCP/UDP stack 변경
anti-cheat boundary 의심 환경에서 mutation
```

required evidence:

```text
RTT Jitter Summary
DPC Spike Count
Background Restriction Result
Policy Activation Count
Rollback Result
```

fallback: network sensitivity가 확인되지 않거나 지표가 악화되면 `MonitoringOnly` 또는 `Restricted`로 전환한다.

risk level: `Medium` 후보이며, network mutation이 필요한 경우 `High` 후보로 격상한다.

validation requirement: 장시간 관찰(Long Soak)과 before/after jitter 비교가 필요하며, 구체 수치는 `TBD`로 둔다.

주의사항:

네트워크 registry 자동 변경은 profile 기본 정책에 포함하지 않는다.
검증 없는 registry 변경은 금지한다.

### 5.3 LegacyMMORPG

목적: 구형 MMORPG 후보에 대해 메인 스레드 병목, 네트워크 민감성, 장시간 실행 영향을 함께 검증하기 위한 profile type을 정의한다.

대상 특성:

- 구형 MMORPG 후보
- 메인 스레드 병목과 네트워크 민감성이 함께 존재할 가능성이 있는 게임
- 장시간 실행과 background interference 영향을 받을 수 있는 게임 후보

기본 apply mode:

```text
dry-run 우선
soft-apply 후 evidence 확인
apply는 profile validation 이후 허용
```

허용 정책 후보:

```text
MAIN_THREAD_PRIORITY_UP
MAIN_THREAD_AFFINITY_STABILIZE
CORE_ISOLATION_PLAN
CCX_STABILITY_PREFER
BACKGROUND_RESTRICT_UP
```

조건부 허용:

```text
AGGRESSIVE_SINGLE_CORE_ENABLE
```

금지 또는 제한:

```text
검증 없는 registry 자동 변경
anti-cheat boundary 우회
게임 메모리 패치
DLL injection
kernel driver 기반 개입
```

required evidence:

```text
Main Thread Confidence History
Migration Summary
RTT Jitter Summary
DPC Spike Summary
Background Restriction Summary
Rollback Summary
Long Soak Summary
```

fallback: long soak 중 regression, rollback 실패, access denied 증가, 또는 evidence completeness 실패 시 `MonitoringOnly`로 전환한다.

risk level: `Medium` 또는 `High` 후보이며, 검증 전까지 `TBD`로 둔다.

validation requirement: baseline, soft-apply, apply 후보 run을 단계적으로 수집해야 하며, 최소 run 수는 `TBD`다.

예시 후보:

```text
마비노기
메이플스토리
던전앤파이터
라그나로크 온라인
```

주의:

위 목록은 공식 지원 게임이 아니라 우선 검증 대상 후보로만 작성한다.
각 게임의 targetProcessName, antiCheatRisk, 병목 구조는 검증 전까지 `TBD` 또는 `Unknown`으로 둔다.

### 5.4 ModernMulticore

목적: 최신 멀티코어 구조(Modern Multicore Architecture)를 가진 게임 후보에서 강한 CPU scheduling mutation을 기본적으로 차단한다.

대상 특성:

- DX12/Vulkan 기반일 가능성이 높은 게임
- Job System 또는 Task Graph 기반 멀티코어 활용이 이미 최적화된 게임

기본 apply mode:

```text
monitoring-only
```

허용 정책 후보:

```text
MONITORING_ONLY
Performance Evidence collection
```

금지 정책:

```text
MAIN_THREAD_AFFINITY_STABILIZE
AGGRESSIVE_SINGLE_CORE_ENABLE
강제 background restriction
검증 없는 process priority 변경
```

required evidence:

```text
monitoringOnlyReason
detectedMulticoreBehavior
policyRejectedReason
```

fallback: profile 정보가 불충분하거나 멀티코어 사용 특성이 확인되지 않으면 `Unknown Default Profile`로 전환한다.

risk level: `Low` for observation, `High` for mutation 후보로 본다.

validation requirement: mutation 허용 전 workload 분산, main thread confidence, rollback 가능성을 다시 검증해야 한다.

ModernMulticore profile은 GameOptimizer가 적용하지 않는 판단도 올바른 결과임을 명확히 한다.

### 5.5 GpuBound

목적: GPU 병목(GPU-bound)이 확인되었거나 의심되는 게임 후보에서 CPU scheduling 정책의 과도한 적용을 방지한다.

대상 특성:

- GPU 사용률이 병목의 대부분을 차지하는 것으로 확인되었거나 의심되는 게임
- CPU scheduling 조정으로 얻을 수 있는 이득이 제한적인 게임

기본 apply mode:

```text
monitoring-only
```

허용 정책:

```text
Evidence collection
Runtime observation
```

금지 정책:

```text
Aggressive Single-Core Mode
무조건적인 priority 상향
무조건적인 affinity 고정
```

required evidence:

```text
gpuBoundReason
cpuBottleneckInsufficientReason
monitoringOnlyReason
```

fallback: CPU 병목 증거가 충분해질 때까지 `MonitoringOnly`를 유지한다.

risk level: `Low` for observation, mutation은 `High` 후보로 본다.

validation requirement: CPU bottleneck evidence가 충분하지 않으면 profile 승격 대상이 아니다.

### 5.6 AntiCheatSensitive

목적: Anti-Cheat 경계가 확인되었거나 의심되는 게임 후보에서 mutation 정책을 차단하고 안전한 비적용 판단을 우선한다.

대상 특성:

- Anti-Cheat가 강하게 적용된 것으로 확인되었거나 의심되는 게임
- 외부 process/thread scheduling 제어가 위험할 수 있는 게임
- Access Denied 또는 protection boundary가 자주 발생하는 게임

기본 apply mode:

```text
monitoring-only
```

금지 정책:

```text
thread affinity mutation
thread priority mutation
process priority mutation
background restriction
aggressive mode
registry 변경
```

required evidence:

```text
antiCheatBoundarySuspicion
accessDeniedCount
protectedProcessDetection
monitoringOnlyReason
```

fallback: anti-cheat suspicion이 존재하면 `AntiCheatSensitive` 또는 `MonitoringOnly`를 유지한다.

risk level: `High`

validation requirement: 본 profile은 mutation 승격보다 차단 판단 검증을 우선한다.

Anti-Cheat 우회는 GameOptimizer의 제품 범위가 아니다.
AntiCheatSensitive profile은 성능보다 안전한 비적용 판단을 우선한다.

### 5.7 Unknown

목적: 정보가 부족한 게임 후보에 대해 기본적으로 관찰과 evidence 수집만 허용한다.

대상 특성:

- 아직 profile이 확정되지 않은 게임
- 정보 부족
- validation history 없음
- 게임 구조를 단정할 수 없는 대상

기본 apply mode:

```text
dry-run 또는 monitoring-only
```

허용 정책:

```text
Thread observation
Main Thread Confidence calculation
Topology observation
Evidence collection
```

금지 정책:

```text
Aggressive Single-Core Mode
강한 affinity/priority 적용
background restriction
registry 변경
```

required evidence:

```text
targetProcessIdentity
topologyState
confidenceHistory
monitoringOnlyReason
```

fallback: profile selection이 실패하면 `Unknown Default Profile`을 적용한다.

risk level: `High`

validation requirement:

```text
충분한 baseline evidence
rollback-safe validation
runtime validation clean
target process identity 안정
main thread confidence 안정
anti-cheat boundary 위험 없음
```

## 6. Game Profile 공통 필드

Game Profile은 다음 개념 필드를 가진다. 이 목록은 구현 schema가 아니라 문서 수준의 필드 계약(Field Contract)이다.

| 필드 | 의미 |
|---|---|
| `profileId` | profile을 식별하는 안정적인 ID |
| `profileName` | 사람이 읽을 수 있는 profile 이름 |
| `profileType` | `SingleCoreLegacy`, `NetworkSensitiveLegacy`, `LegacyMMORPG`, `ModernMulticore`, `GpuBound`, `AntiCheatSensitive`, `Unknown` 중 하나 |
| `targetProcessName` | 대상 process 이름. 검증 전에는 `TBD` |
| `displayName` | 사용자 표시용 게임 이름 |
| `defaultMode` | 기본 적용 모드(Default Apply Mode): `dry-run`, `soft-apply`, `apply`, `monitoring-only` 후보 |
| `allowedPolicies` | profile에서 허용되는 정책 후보 |
| `blockedPolicies` | profile에서 항상 차단되는 정책 |
| `conditionalPolicies` | evidence와 runtime condition 충족 시에만 허용되는 정책 |
| `riskLevel` | profile 적용 위험도. 검증 전 수치는 `TBD` 가능 |
| `requiredEvidence` | profile 선택, 적용, 검증에 필요한 evidence 목록 |
| `minimumObservationTime` | 최소 관찰 시간. 구체 수치는 `TBD` |
| `confidenceRequirement` | MainThreadConfidence 등 confidence 요구 수준 |
| `topologyRequirement` | Processor Group, CCX, core topology 관련 요구사항 |
| `rollbackRequirement` | mutation 전 rollback state 저장 요구사항 |
| `validationPriority` | validation 우선순위 |
| `monitoringOnlyReasons` | monitoring-only 전환 또는 유지 사유 |
| `fallbackBehavior` | profile 적용 실패 시 fallback 동작 |
| `lastValidatedVersion` | 마지막 검증 버전. 검증 전에는 `TBD` |
| `profileStatus` | `Draft`, `Candidate`, `Validated`, `Restricted`, `MonitoringOnly`, `Deprecated` 중 하나 |
| `validationRequired` | 추가 검증 필요 여부 |
| `engineType` | 엔진 유형. 검증 전에는 `TBD` |
| `antiCheatRisk` | Anti-Cheat 위험도. 검증 전에는 `Unknown` |
| `singleCoreBottleneckConfirmed` | 싱글코어 병목 확인 여부. 검증 전에는 `false` |
| `gpuBoundConfirmed` | GPU 병목 확인 여부. 검증 전에는 `false` |
| `networkSensitivityConfirmed` | 네트워크 민감성 확인 여부. 검증 전에는 `false` |

필드 누락은 `docs/evidence/EvidenceSpecification.md`의 completeness 기준으로 분류한다. 특히 `profileId`, `profileType`, `profileStatus`, `blockedPolicies`, `validationRequired`는 profile 판단의 핵심 evidence다.

## 7. Profile Status 정의

Profile 상태(Profile Status)는 다음 값으로 정의한다.

```text
Draft
Candidate
Validated
Restricted
MonitoringOnly
Deprecated
```

### Draft

작성 중인 profile이다. 실제 apply 대상이 아니다.

### Candidate

dry-run 또는 soft-apply 검증 후보이다. 성능 특성을 확정하지 않는다.

### Validated

정해진 validation 기준을 통과한 profile이다. Codex는 본 문서에서 특정 게임을 Validated로 작성하지 않는다.

### Restricted

일부 정책만 허용되는 제한 profile이다. rollback, evidence, risk level 조건에 따라 mutation 범위를 축소한다.

### MonitoringOnly

관찰만 허용되는 profile이다. mutation policy를 생성하거나 실행하지 않는다.

### Deprecated

더 이상 사용하지 않는 profile이다. historical evidence 참조 외에는 신규 적용 대상이 아니다.

## 8. 기본 Profile 예시

다음 예시는 문서 수준의 template이며, 실제 지원 선언이 아니다. 예시 profile은 구현 schema를 확정하지 않는다.

### 8.1 Mabinogi Candidate Profile

다음 profile은 공식 지원 선언이 아니다.
이 profile은 우선 검증 대상 후보 예시다.

```text
profileId: mabinogi_candidate
profileName: Mabinogi Candidate Profile
profileType: LegacyMMORPG
targetProcessName: TBD
displayName: 마비노기
defaultMode: dry-run -> soft-apply
riskLevel: TBD
profileStatus: Candidate
validationRequired: true

engineType: TBD
antiCheatRisk: Unknown
singleCoreBottleneckConfirmed: false
gpuBoundConfirmed: false
networkSensitivityConfirmed: false

allowedPolicies:
- MAIN_THREAD_PRIORITY_UP
- MAIN_THREAD_AFFINITY_STABILIZE
- CORE_ISOLATION_PLAN
- CCX_STABILITY_PREFER

conditionalPolicies:
- BACKGROUND_RESTRICT_UP
- AGGRESSIVE_SINGLE_CORE_ENABLE
- TIMER_RESOLUTION_MAINTAIN

blockedPolicies:
- registry auto-change
- unsafe Processor Group 1+ restriction
- Raw Input 추정 기반 강제 pinning
- anti-cheat boundary 우회
- DLL Injection
- Kernel Driver
- Game Memory Patch

requiredEvidence:
- Main Thread Confidence History
- Migration Summary
- RTT Jitter Summary
- DPC Spike Summary
- Rollback Summary
- Runtime Validation Summary
- Before/After Summary
```

Mabinogi Candidate Profile은 검증 대상 후보일 뿐이며, targetProcessName, 병목 구조, antiCheatRisk, 성능 개선 가능성을 확정하지 않는다.

### 8.2 Unknown Default Profile

```text
profileId: unknown_default
profileName: Unknown Default Profile
profileType: Unknown
targetProcessName: TBD
defaultMode: monitoring-only 또는 dry-run
riskLevel: High
profileStatus: MonitoringOnly
validationRequired: true

allowedPolicies:
- Thread observation
- Main Thread Confidence calculation
- Topology observation
- Evidence collection

blockedPolicies:
- AGGRESSIVE_SINGLE_CORE_ENABLE
- BACKGROUND_RESTRICT_UP
- registry change
- unsafe affinity apply
- process priority mutation

requiredEvidence:
- monitoringOnlyReason
- targetProcessIdentity
- topologyState
- confidenceHistory
```

Unknown Default Profile은 profile registry lookup 실패, target process identity 불명확, evidence 부족, 또는 profile corruption 상태에서 적용한다.

## 9. Profile Selection Flow

Profile 선택 흐름(Profile Selection Flow)은 다음 순서를 따른다.

```text
CLI target process
-> profile registry lookup
-> target process identity validation
-> profile type 확인
-> risk level 확인
-> defaultMode 결정
-> allowed/blocked/conditional policy 적용
-> Performance Engine / Policy Layer로 전달
```

Profile을 찾지 못한 경우:

```text
Unknown Default Profile 적용
```

Selection Flow의 기본 원칙:

1. target process identity가 불안정하면 mutation을 허용하지 않는다.
2. profile registry lookup에 실패하면 `Unknown`으로 처리한다.
3. `blockedPolicies`가 하나라도 충돌하면 해당 정책은 생성하지 않는다.
4. `conditionalPolicies`는 evidence와 runtime condition을 모두 만족해야 한다.
5. profileStatus가 `Candidate`이면 apply보다 dry-run 또는 soft-apply를 우선한다.
6. profileStatus가 `MonitoringOnly`이면 mutation policy를 생성하지 않는다.

## 10. Policy 적용 규칙

Profile은 `docs/proposals/performance/PolicySpecification.md`의 policy type을 제한할 수 있어야 한다.

규칙:

1. `blockedPolicies`는 항상 `allowedPolicies`보다 우선한다.
2. `conditionalPolicies`는 required evidence와 runtime condition을 만족해야 한다.
3. profile `riskLevel`이 `High`이면 apply보다 soft-apply 또는 monitoring-only를 우선한다.
4. `MonitoringOnly` profile은 mutation policy를 생성할 수 없다.
5. `AntiCheatSensitive` profile은 모든 mutation을 차단한다.
6. `Candidate` profile은 `Validated` profile보다 보수적으로 동작한다.
7. `Unknown` profile은 aggressive mode를 허용하지 않는다.
8. rollback state를 저장할 수 없는 정책은 apply 대상이 아니다.
9. RuntimeValidation BLOCKER가 존재하면 profile 정책은 `MonitoringOnly`로 fallback한다.
10. Processor Group 1+ 환경에서 안전하지 않은 process-wide restriction은 차단한다.

정책 handoff는 `docs/architecture/RuntimeStateMachine.md`와 `docs/modules/MDS-008_PolicyDispatcher.md`의 상태 전이 규칙을 따른다.

## 11. Evidence 요구사항

Profile별 required evidence는 profile 선택, 정책 제한, validation, fallback 판단에 사용된다.

공통 required evidence:

```text
profileId
profileType
profileStatus
selectedProfile
profileSelectionReason
allowedPolicies
blockedPolicies
conditionalPolicies
profileRiskLevel
profileFallbackReason
validationRequired
```

profile validation evidence:

```text
baselineRunCount
softApplyRunCount
applyRunCount
rollbackSuccessCount
runtimeValidationBlockerCount
lastValidationResult
lastValidatedAt
```

누락 시 처리:

```text
profileId 없음: WARN 또는 BLOCKER 후보
profileType 없음: BLOCKER 후보
blockedPolicies 없음: BLOCKER 후보
profileSelectionReason 없음: WARN
validationRequired 누락: WARN
```

Evidence는 `docs/evidence/EvidenceSpecification.md`의 severity, completeness, classification 기준에 맞게 기록한다. Game Profile 관련 evidence가 불완전하면 profile 승격은 보류한다.

## 12. Profile Validation 기준

Profile이 `Candidate`에서 `Validated`로 승격되기 위한 조건은 다음과 같다.

기본 후보:

1. 최소 baseline run 수 충족
2. soft-apply run에서 RuntimeValidation BLOCKER 없음
3. rollback 성공률 100%
4. target process identity 안정
5. MainThreadConfidence가 일정 구간 이상 `High` 이상 유지
6. Evidence completeness가 `PASS` 또는 `PASS_WITH_WARNINGS`
7. 성능 지표가 `REGRESSION` 또는 `BLOCKER`가 아님
8. anti-cheat boundary 위험 없음

구체 수치는 `TBD`로 둔다.

```text
Minimum baseline runs: TBD
Minimum soft-apply runs: TBD
Minimum apply runs: TBD
Confidence stability duration: TBD
Minimum long soak duration: TBD
Maximum acceptable migration regression: TBD
Maximum acceptable RTT jitter regression: TBD
```

Codex는 본 문서에서 어떤 특정 게임도 Validated로 승격하지 않는다.
Validated 승격은 실제 PerformanceValidationPlan과 EvidenceSpecification 기준을 통과한 이후에만 가능하다.

Validation 결과가 불충분하면 `Candidate`를 유지하거나 `Restricted` 또는 `MonitoringOnly`로 낮춘다. 성능 개선 가능성이 관찰되더라도 rollback과 runtime validation이 실패하면 승격하지 않는다.

## 13. Fallback 규칙

Profile 적용 실패 시 fallback 규칙은 다음과 같다.

```text
profile not found -> Unknown Default Profile
profile corrupted -> MonitoringOnly
profile risk high -> soft-apply 또는 monitoring-only
required evidence missing -> MonitoringOnly
anti-cheat suspicion -> AntiCheatSensitive 또는 MonitoringOnly
validation regression -> Restricted 또는 MonitoringOnly
targetProcessName unknown -> dry-run 또는 monitoring-only
rollback unavailable -> MonitoringOnly
runtime validation blocker -> MonitoringOnly
processor group unsafe -> Restricted 또는 MonitoringOnly
```

Fallback은 실패를 숨기기 위한 동작이 아니다. Fallback이 발생하면 `profileFallbackReason`, `policyRejectedReason`, `monitoringOnlyReason`을 evidence로 남겨야 한다.

## 14. Non-Goals

다음은 GameProfileSpecification의 비목표(Non-Goals)다.

```text
구체 JSON/YAML/TOML schema 구현
profile parser 구현
UI profile 선택 화면 구현
공식 지원 게임 목록 확정
성능 향상 보장
Anti-Cheat 우회 정책
자동 게임 탐지 엔진 구현
AI/ML 기반 profile 추천
웹 조사 기반 게임 구조 단정
검증 없는 aggressive mode 허용
검증 없는 registry 변경 정책 허용
```

이 문서는 GameOptimizer가 무엇을 적용할 수 있는지뿐 아니라, 무엇을 적용하지 않아야 하는지를 명확히 하기 위한 기준 문서다.

## 15. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Game Profile의 목적이 정의되어 있다.
2. 검증되지 않은 게임 정보 처리 규칙이 정의되어 있다.
3. Profile Type 분류가 정의되어 있다.
4. `SingleCoreLegacy`, `NetworkSensitiveLegacy`, `LegacyMMORPG`, `ModernMulticore`, `GpuBound`, `AntiCheatSensitive`, `Unknown` profile이 정의되어 있다.
5. Profile 공통 필드가 정의되어 있다.
6. Profile Status가 정의되어 있다.
7. Unknown Default Profile이 정의되어 있다.
8. Mabinogi Candidate Profile이 `Candidate`/`TBD` 기준으로 작성되어 있다.
9. Policy allowed/blocked/conditional 규칙이 정의되어 있다.
10. Profile Selection Flow가 정의되어 있다.
11. Profile Evidence 요구사항이 정의되어 있다.
12. Profile Validation 기준이 정의되어 있다.
13. 특정 게임을 공식 지원한다고 단정하지 않는다.
14. 특정 게임의 targetProcessName, antiCheatRisk, 병목 구조를 확정하지 않는다.
15. 어떤 특정 게임도 `Validated`로 분류하지 않는다.
16. 후속 문서 `docs/release/ReleaseChecklist_v3.1.md`와 연결되어 있다.

## 16. Open Questions

다음 질문은 후속 문서 또는 구현 계획에서 결정한다.

1. profile 저장 포맷은 JSON, YAML, TOML 중 무엇으로 둘 것인가?
2. profile 파일은 사용자 수정 가능하게 둘 것인가?
3. 첫 번째 Candidate Profile은 마비노기로 둘 것인가?
4. profile validation에 필요한 최소 run 수는 몇 회로 둘 것인가?
5. profileStatus를 Runtime에서 강제할 것인가, 문서/설정 기준으로만 둘 것인가?
6. AntiCheatSensitive 판정은 수동 profile로만 둘 것인가, access denied telemetry로 자동 전환할 것인가?
7. Unknown profile의 기본값은 dry-run인가, monitoring-only인가?
8. Game Profile은 Release Gate에서 필수 Evidence로 포함할 것인가?
9. targetProcessName은 사용자가 직접 입력하게 할 것인가, profile registry에서 관리할 것인가?
10. Mabinogi Candidate Profile의 실제 targetProcessName은 어떤 검증 절차로 확정할 것인가?

Open Questions가 해결되기 전까지 본 문서는 profile 구조와 candidate 기준만 정의하며, 특정 게임을 validated profile로 취급하지 않는다.
