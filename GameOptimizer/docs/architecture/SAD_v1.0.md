# GameOptimizer v3.1 Software Architecture Design

## 1. 문서 개요

문서명: GameOptimizer v3.1 Software Architecture Design

버전: v1.0

작성 목적: GameOptimizer v3.1에서 추가되는 성능 엔진(Performance Engine), 정책 계층(Policy Layer), 성능 증거(Performance Evidence), 롤백(Rollback), 안전 계약(Safety Contract)을 기존 GameOptimizer 아키텍처에 어떻게 배치할지 정의한다.

적용 범위:

- v3.1 전체 Layer 구조
- 기존 모듈과 신규 성능 모듈의 배치
- Runtime data flow와 control flow
- Mutation boundary
- Processor Group / Topology architecture
- Performance / Policy / Evidence architecture
- Runtime state 개요
- Error / fallback architecture
- Module status matrix

비적용 범위:

- 구체 C++ 클래스 시그니처 확정
- 구체 enum 최종 구현
- 상세 테스트 케이스 작성
- UI 설계
- 설치 프로그램 설계
- Release Gate 스크립트 구현 변경

상위 문서:

- `docs/vision/PVD_v1.0.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/architecture/Architecture_Decision_Record.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Evidence_Schema.md`
- `docs/release/Release_Gate_Spec.md`

후속 문서:

- `docs/architecture/RuntimeStateMachine.md`
- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-005_RollbackManager.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

본 문서는 코드 구현 문서가 아니다. 본 문서는 후속 Module Design Specification(MDS)이 따라야 할 상위 소프트웨어 아키텍처 설계(Software Architecture Design)다.

## 2. 아키텍처 목표

v3.1 아키텍처의 목표는 더 많은 시스템 영역을 건드리는 것이 아니라, 더 정확한 대상에 더 안전하게 제한된 정책을 적용하는 구조를 만드는 것이다.

v3.1 아키텍처의 필수 목표는 다음과 같다.

1. 싱글코어 병목 게임에 특화된 성능 판단 계층 추가
2. 기존 안정화/롤백/증거 체계를 유지한 상태에서 성능 기능 확장
3. 메인 스레드 신뢰도(Main Thread Confidence) 기반 정책 적용 구조 확립
4. 직접 mutation과 정책 판단 계층 분리
5. Runtime Evidence 기반의 성능 효과 판단
6. Processor Group, CCX, SMT 정보를 안전하게 반영
7. 실패 시 monitoring-only 또는 rollback으로 안전하게 전환

핵심 전제는 다음과 같다.

- GameOptimizer는 모든 게임을 빠르게 만드는 범용 FPS Booster가 아니다.
- v3.1의 대상은 싱글코어 병목 게임이다.
- Performance Engine은 직접 Win32 제어 API를 호출하지 않는다.
- 실제 적용은 PolicyDispatcher와 담당 Controller가 수행한다.
- ThreadTracker는 observation-only 모듈이다.
- SchedulerController는 thread-level scheduling mutation의 소유자이며, process-level background restriction은 BackgroundController가 소유한다.
- Rollback 없는 mutation은 허용하지 않는다.
- Evidence 없는 성능 성공 판정은 허용하지 않는다.
- Processor Group 정보가 없을 때 group 0으로 조용히 보정하지 않는다.
- Anti-Cheat 우회, DLL Injection, Kernel Driver, Game Memory Patch는 제품 범위가 아니다.

## 3. 전체 Layer 구조

### 3.1 Application Layer

목적: 사용자 입력, startup/shutdown orchestration, runtime 생명주기 진입점을 관리한다.

포함 모듈:

- CLI / CliOptions
- StartupPipeline
- ShutdownPipeline
- RuntimeOrchestrator
- RuntimeContext

입력:

- CLI arguments
- target process name
- scheduler mode
- background filter
- latency ping target
- runtime option flags

출력:

- StartupPlan
- RuntimeContext
- shutdown reason
- process exit code

소유 책임:

- 런타임 구성 요소 조립
- startup mutation 진입 전 안전 조건 확인
- shutdown rollback sequence 호출
- RuntimeContext 생명주기 관리

금지 책임:

- ThreadTracker나 Performance Engine 판단을 우회한 직접 정책 적용
- ApplyGuard/RollbackManager 없는 mutation
- Release evidence 직접 조작

### 3.2 Runtime Layer

목적: watchdog cycle, runtime 상태 관찰, validation summary, shutdown-safe point를 관리한다.

포함 모듈:

- TrackingWatchdog
- WatchdogCycleRunner
- RuntimeValidationMonitor
- RuntimeContext
- RuntimeSignalState

입력:

- RuntimeContext
- ThreadTracker samples
- PolicyDispatcher 결과
- Controller apply/rollback 결과
- runtime metrics

출력:

- runtime validation summary
- rollback request
- shutdown reason
- heartbeat/timeline evidence

소유 책임:

- Watchdog cycle boundary 유지
- RuntimeValidation BLOCKER 기록
- shutdown-safe point 제공
- runtime timeline monotonicity와 heartbeat progression 관찰

금지 책임:

- Controller 없이 thread/process scheduling mutation 수행
- RollbackManager 직접 우회
- Performance success 판정 단독 수행

### 3.3 Observation Layer

목적: target process, thread, latency, DPC, input 관련 신호를 관찰한다.

포함 모듈:

- ThreadTracker
- ProcessFinder
- LatencyMetricsCollector
- NetworkInterruptController의 monitoring path
- InputLatencyController의 query-only path
- Planned: LatencySensor
- Planned: NetworkJitterMonitor
- Planned: DpcMonitor

입력:

- target process handle 또는 process id
- thread enumeration result
- PDH / latency samples
- Raw Input query result

출력:

- thread sample
- EMA score
- CPU delta
- migration count
- RTT jitter
- DPC spike signal
- access-boundary telemetry

소유 책임:

- 관찰 데이터 수집
- query-only access failure 기록
- Performance Analysis Layer에 raw signal 제공

금지 책임:

- SetThreadAffinityMask 직접 호출
- SetThreadGroupAffinity 직접 호출
- SetThreadPriority 직접 호출
- SetPriorityClass 직접 호출
- SetProcessAffinityMask 직접 호출
- RollbackManager 상태 변경

### 3.4 Performance Analysis Layer

목적: 관찰 데이터를 분석하여 정책 후보(Policy Candidate)를 생성한다.

포함 모듈:

- Planned: PerformanceEngine
- Planned: MainThreadConfidenceAnalyzer
- Planned: CoreIsolationPlanner
- Planned: CCXStabilityPlanner
- Planned: AggressiveSingleCoreController
- Planned: PerformanceEvidencePlanner

입력:

- ThreadTracker sample
- TopologyResult
- RuntimeValidation state
- latency / DPC / migration telemetry
- current policy state

출력:

- Main Thread Confidence
- Core Isolation Plan
- CCX Stability 판단
- Aggressive Single-Core Mode 활성/비활성 후보
- Performance Evidence requirements
- Policy Candidate

소유 책임:

- 성능 판단
- policy candidate generation
- monitoring-only reason 생성
- Evidence requirement 생성

금지 책임:

- Win32 제어 API 직접 호출
- SchedulerController / BackgroundController 소유 상태 직접 변경
- RollbackManager 직접 호출
- ApplyGuard 직접 생성

### 3.5 Policy Layer

목적: Performance Engine의 판단을 검증 가능한 정책 후보로 정리하고 담당 Controller로 전달할 준비를 한다.

포함 모듈:

- PolicyDispatcher
- Planned: PolicyCandidate
- Planned: PolicyResolver
- `docs/performance/PolicySpecification.md`
- Existing: PolicyCommand

입력:

- Policy Candidate
- confidence level
- risk level
- cooldown state
- rollback scope
- evidence field requirement
- runtime validation state

출력:

- validated policy
- rejected policy
- monitoring-only policy
- rollback request
- Controller handoff

소유 책임:

- policyType 유효성 검증
- cooldown 검증
- conflict 검증
- riskLevel 검증
- rollbackScope/evidenceFields 존재 여부 검증
- PolicyDispatcher handoff

금지 책임:

- Win32 mutation 직접 수행
- ApplyGuard/RollbackManager 우회
- Evidence 없는 성공 판정
- confidence 미달 상태에서 강한 정책 생성

### 3.6 Control Layer

목적: 검증된 정책을 실제 Windows runtime state에 적용하거나 rollback 가능한 방식으로 검증한다.

포함 모듈:

- SchedulerController
- BackgroundController
- TimerResolutionController
- NetworkInterruptController
- InputLatencyController
- Planned: NetworkController abstraction

입력:

- validated policy
- SchedulerPolicy
- background restriction policy
- timer resolution policy
- rollback scope
- target identity

출력:

- apply result
- verify result
- rollback result
- fallback evidence
- access denied evidence

소유 책임:

- thread-level scheduling mutation
- background process restriction
- timer resolution request/release
- supported monitoring-only network path
- ApplyGuard/RollbackManager 계약 준수

금지 책임:

- rollback state 저장 전 mutation
- ApplyGuard arm 전 mutation
- post-apply verify 없이 commit
- unsupported Processor Group 1+ process-wide restriction

### 3.7 Hardware/Topology Layer

목적: CPU topology, processor group, SMT, L3/CCX 정보를 안전하게 제공한다.

포함 모듈:

- TopologyAnalyzer
- Planned: ProcessorGroupManager
- Planned: CoreMaskPlanner

입력:

- Win32 processor topology data
- process affinity data
- processor group data
- cache / core / logical processor records

출력:

- TopologyResult
- processorGroup
- KAFFINITY
- SMT sibling 정보
- L3/CCX candidate range
- topology completeness

소유 책임:

- group-aware topology parsing
- group-preserving mask provenance
- topology completeness 판단
- HEDT/hybrid/NUMA limitation 노출

금지 책임:

- group 0 hardcoding
- 불완전한 topology를 강제 적용 가능으로 표시
- Intel Hybrid를 AMD CCX 정책으로 단정

### 3.8 Safety/Rollback Layer

목적: runtime mutation의 transaction boundary와 원상 복구를 보장한다.

포함 모듈:

- ApplyGuard
- RollbackManager
- ShutdownPipeline rollback path
- Planned: StateSnapshot

입력:

- original thread/process state
- target identity
- save state disposition
- apply/verify result
- rollback request

출력:

- rollback state saved
- rollback result
- rollback failure transfer
- preserved state evidence

소유 책임:

- mutation 전 original state 저장
- ApplyGuard arm/commit/rollback boundary
- rollback failure state preservation
- shutdown retry responsibility 유지

금지 책임:

- rollback failure를 WARN/INFO로 낮춤
- unsafe rollback state discard
- identity mismatch를 무시한 restore

### 3.9 Evidence Layer

목적: 정책 적용 결과, 실패 사유, rollback 결과, runtime validation 결과를 기록한다.

포함 모듈:

- release_gate_evidence.py
- Evidence Schema documents
- RuntimeValidationMonitor summary
- Planned: RuntimeSnapshotRecorder
- Planned: PerformanceEvidenceRecorder
- Planned: PolicyTimelineRecorder

입력:

- Policy Candidate evidence requirements
- Controller apply result
- verify result
- rollback result
- runtime validation result
- Before/After samples

출력:

- Performance Evidence
- runtime validation summary
- release-facing evidence fields
- WARN/BLOCKER/INFO classification input

소유 책임:

- evidence field completeness 확인
- Before/After Summary 연결
- fallback reason 기록
- release evidence bundle 입력 제공

금지 책임:

- Evidence 없는 성능 성공 판정
- rollback failure severity 하향
- 정책 적용 결과를 홍보성 문구로 대체

### 3.10 Release/Validation Layer

목적: RC Gate, regression, runtime artifacts, evidence bundle을 검증한다.

포함 모듈:

- run_rc_gate.bat
- run_release_gate_static_checks.py
- run_release_gate_log_assertions.py
- verify_rc_candidate.py
- verify_real_game_validation.py
- create_rc_evidence_bundle.py
- Release documentation
- Planned: RuntimeArtifactValidator
- Planned: RCReportGenerator

입력:

- build output
- regression logs
- smoke/soak evidence
- real-game validation matrix
- RC evidence reports

출력:

- PASS/FAIL
- WARN/BLOCKER/INFO counts
- RC evidence bundle
- release decision evidence

소유 책임:

- release gate enforcement
- static contract verification
- evidence schema verification
- current-commit identity check

금지 책임:

- missing evidence를 release success로 처리
- rollback failure를 release non-blocking으로 처리
- stale architecture contracts를 무시

## 4. 권장 Layer 배치

기본 Layer 배치는 다음과 같다.

```text
GameOptimizer
│
├─ Application Layer
│  ├─ CLI (Existing)
│  ├─ StartupPipeline (Existing)
│  ├─ ShutdownPipeline (Existing)
│  └─ RuntimeOrchestrator (Existing)
│
├─ Runtime Layer
│  ├─ TrackingWatchdog (Existing)
│  ├─ WatchdogCycleRunner (Existing)
│  ├─ RuntimeValidationMonitor (Existing)
│  └─ RuntimeContext (Existing)
│
├─ Observation Layer
│  ├─ ThreadTracker (Existing)
│  ├─ LatencySensor (Planned)
│  ├─ NetworkJitterMonitor (Planned)
│  └─ DpcMonitor (Planned)
│
├─ Performance Analysis Layer
│  ├─ PerformanceEngine (Planned)
│  ├─ MainThreadConfidenceAnalyzer (Planned)
│  ├─ CoreIsolationPlanner (Planned)
│  ├─ CCXStabilityPlanner (Planned)
│  ├─ AggressiveSingleCoreController (Planned)
│  └─ PerformanceEvidencePlanner (Planned)
│
├─ Policy Layer
│  ├─ PolicyCandidate (Planned)
│  ├─ PolicySpecification (Existing document)
│  ├─ PolicyResolver (Planned)
│  └─ PolicyDispatcher (Existing)
│
├─ Control Layer
│  ├─ SchedulerController (Existing)
│  ├─ BackgroundController (Existing)
│  ├─ TimerResolutionController (Existing)
│  └─ NetworkController (Planned abstraction)
│
├─ Hardware/Topology Layer
│  ├─ TopologyAnalyzer (Existing)
│  ├─ ProcessorGroupManager (Planned)
│  └─ CoreMaskPlanner (Planned)
│
├─ Safety/Rollback Layer
│  ├─ ApplyGuard (Existing)
│  ├─ RollbackManager (Existing)
│  └─ StateSnapshot (Planned)
│
├─ Evidence Layer
│  ├─ RuntimeSnapshotRecorder (Planned)
│  ├─ PerformanceEvidenceRecorder (Planned)
│  ├─ PolicyTimelineRecorder (Planned)
│  └─ EvidenceSchema (Existing document)
│
└─ Release/Validation Layer
   ├─ ReleaseGate (Existing scripts)
   ├─ RegressionMatrix (Existing document)
   ├─ RuntimeArtifactValidator (Planned)
   └─ RCReportGenerator (Planned)
```

Planned 모듈은 설계 위치를 나타내며 현재 구현 존재를 의미하지 않는다.

## 5. 모듈 소유권 원칙

ThreadTracker:

- thread 관찰만 수행한다.
- affinity, priority, rollback state를 직접 수정하지 않는다.
- observation-only 계약을 유지한다.

PerformanceEngine:

- 관찰 데이터를 분석하고 Policy Candidate를 생성한다.
- Win32 제어 API를 직접 호출하지 않는다.
- ApplyGuard와 RollbackManager를 직접 호출하지 않는다.

PolicyDispatcher:

- 정책 후보를 담당 Controller로 전달한다.
- 정책 충돌과 cooldown 조건을 검증한다.
- 검증되지 않은 policy candidate를 controller로 넘기지 않는다.

SchedulerController:

- thread-level priority와 affinity mutation의 소유자다.
- ApplyGuard와 RollbackManager 계약을 따라야 한다.
- process-level background restriction은 BackgroundController의 소유권을 침범하지 않는다.

BackgroundController:

- background process restriction의 소유자다.
- protected process와 anti-cheat 의심 process를 제한 대상에서 제외해야 한다.
- Processor Group 1+ process-wide restriction 안전 경로가 없으면 monitoring-only로 남긴다.

TopologyAnalyzer:

- processor group, SMT, L3/CCX 정보를 제공한다.
- group 0 hardcoding을 허용하지 않는다.
- topology completeness를 명시적으로 제공해야 한다.

RollbackManager:

- mutation 전 원상 상태 저장과 rollback 실행 책임을 가진다.
- identity mismatch와 stale target을 안전하게 처리해야 한다.
- rollback state 보존이 필요한 실패를 숨기지 않는다.

ApplyGuard:

- Save/Arm/Apply/Verify/Commit 흐름의 트랜잭션 경계를 보장한다.
- verify 실패 또는 partial apply 실패 시 rollback 경로를 보장한다.
- rollback failure responsibility transfer를 evidence-visible하게 유지한다.

Evidence Layer:

- 정책 적용 결과, 실패 사유, rollback 결과, runtime validation 결과를 기록한다.
- Evidence 없는 성능 성공 판정을 허용하지 않는다.

## 6. Runtime Data Flow

기본 흐름은 다음과 같다.

```text
StartupPipeline
↓
TopologyAnalyzer
↓
RuntimeContext 초기화
↓
TrackingWatchdog 시작
↓
ThreadTracker Sample
↓
PerformanceEngine 분석
↓
PolicyCandidate 생성
↓
PolicyResolver 검증
↓
PolicyDispatcher 전달
↓
담당 Controller Save/Arm/Apply/Verify/Commit
↓
Evidence 기록
↓
RuntimeValidationMonitor 검증
```

Policy validation 실패 흐름:

```text
Policy validation 실패
↓
MONITORING_ONLY
↓
Evidence 기록
```

적용 실패 흐름:

```text
Apply 또는 Verify 실패
↓
Rollback
↓
Evidence 기록
↓
Monitoring-only fallback 또는 Safe Shutdown
```

Runtime Data Flow의 원칙은 observation, analysis, policy, control, evidence를 분리하는 것이다. Performance Analysis Layer와 Policy Layer는 판단과 검증을 수행하고, Control Layer와 Safety/Rollback Layer가 실제 mutation boundary를 가진다.

## 7. Control Flow와 Mutation Boundary

### Mutation 가능 Layer

다음 Layer만 시스템 상태 변경을 수행할 수 있다.

```text
Control Layer
Safety/Rollback Layer
```

단, Control Layer도 ApplyGuard와 RollbackManager 계약 없이 직접 mutation할 수 없다.

### Mutation 금지 Layer

다음 Layer는 직접 mutation 금지다.

```text
Observation Layer
Performance Analysis Layer
Policy Layer
Evidence Layer
Release/Validation Layer
```

Performance Engine과 Policy Layer는 판단 계층이며, Win32 mutation 소유 계층이 아니다.

Mutation boundary는 다음 규칙을 따른다.

- mutation owner는 SchedulerController, BackgroundController, TimerResolutionController 등 명시된 Control Layer 모듈이다.
- RollbackManager는 rollback state 저장과 복구 책임을 가진다.
- ApplyGuard는 mutation transaction boundary를 가진다.
- ThreadTracker, PerformanceEngine, PolicyResolver, Evidence Recorder는 직접 mutation하지 않는다.

## 8. Safety Architecture

모든 mutation은 다음 흐름을 따른다.

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

필수 규칙:

```text
1. Rollback state 저장 전 mutation 금지
2. ApplyGuard arm 전 mutation 금지
3. Verify 실패 시 rollback 또는 failed-state preservation 필요
4. Rollback 실패를 WARN/INFO로 낮추지 않음
5. RuntimeValidation BLOCKER는 aggressive mode보다 우선
6. Evidence 누락 시 성능 성공 판정 금지
```

Safety Architecture는 성능 기능보다 우선한다. Aggressive Single-Core Mode도 이 흐름을 우회할 수 없다.

## 9. Processor Group / Topology Architecture

Processor Group, SMT, CCX/L3 정책은 Hardware/Topology Layer에서 시작해 Performance Analysis Layer와 Policy Layer를 거쳐 Control Layer로 handoff된다.

필수 원칙:

```text
1. TopologyAnalyzer는 실제 processor group 정보를 제공해야 한다.
2. group 0 hardcoding 금지.
3. KAFFINITY와 processorGroup을 함께 다룬다.
4. Processor Group 1+에서 안전하지 않은 process-wide restriction 금지.
5. topology 정보가 불완전하면 monitoring-only로 전환한다.
6. AMD CCX 정책과 Intel Hybrid 정책을 동일하게 취급하지 않는다.
```

Topology 정보 사용 흐름:

```text
TopologyAnalyzer
↓
PerformanceEngine / CoreIsolationPlanner / CCXStabilityPlanner
↓
PolicyCandidate
↓
SchedulerController 또는 BackgroundController
```

Thread-level affinity는 processorGroup과 KAFFINITY를 함께 다뤄야 한다. Processor Group 정보가 누락된 경우 group 0으로 조용히 보정하지 않고 정책 후보를 차단하거나 monitoring-only로 전환한다.

## 10. Performance Architecture

### 10.1 Main Thread Confidence

목적: 메인 스레드 후보가 실제 최적화 대상인지 신뢰도를 계산한다.

입력:

- ThreadTracker EMA score
- CPU delta
- 후보 유지 시간
- 관찰 지속 시간
- migration count
- thread lifetime stability

출력:

- MainThreadConfidenceLevel
- confidence history
- policy eligibility
- monitoring-only reason

의존 모듈:

- ThreadTracker
- RuntimeValidationMonitor
- Planned: MainThreadConfidenceAnalyzer

정책 handoff 대상:

- `MAIN_THREAD_PRIORITY_UP`
- `MAIN_THREAD_AFFINITY_STABILIZE`
- `AGGRESSIVE_SINGLE_CORE_ENABLE`
- `MONITORING_ONLY`

Evidence 요구사항:

- Main Thread Confidence History
- Main Thread Switch Count
- access failure count
- confidence transition reason

Fallback 조건:

- Confidence < High
- candidate switch frequency 높음
- access failure 반복
- observation window 부족

### 10.2 Core Isolation

목적: 메인 스레드가 사용하는 core 주변의 background interference를 줄인다.

입력:

- MainThreadConfidence
- TopologyResult
- processorGroup
- KAFFINITY
- background process 후보
- SMT sibling 정보

출력:

- CoreIsolationPlan
- backgroundExclusionMask
- isolationStrength
- fallbackReason

의존 모듈:

- TopologyAnalyzer
- BackgroundController
- SchedulerController
- Planned: CoreIsolationPlanner

정책 handoff 대상:

- `CORE_ISOLATION_PLAN`
- `BACKGROUND_RESTRICT_UP`
- `MONITORING_ONLY`

Evidence 요구사항:

- coreIsolationPlan
- backgroundProcessCount
- excludedCoreMask
- applyResult
- fallbackReason

Fallback 조건:

- Processor Group 1+ process-wide restriction 안전 경로 없음
- background process identity 불명확
- protected process 또는 anti-cheat 의심 process 포함

### 10.3 CCX Stability

목적: AMD Ryzen 계열 등 L3/CCX 구조가 의미 있는 환경에서 불필요한 이동을 줄인다.

입력:

- TopologyResult
- L3 cache group
- CCX/CCD 추정 정보
- processorGroup
- migration history

출력:

- preferredCoreRange
- topologyCompleteness
- monitoringOnlyReason

의존 모듈:

- TopologyAnalyzer
- SchedulerController
- Planned: CCXStabilityPlanner

정책 handoff 대상:

- `CCX_STABILITY_PREFER`
- `MAIN_THREAD_AFFINITY_STABILIZE`
- `MONITORING_ONLY`

Evidence 요구사항:

- l3Group
- ccxOrCacheGroup
- migrationHistory
- topologyCompleteness

Fallback 조건:

- Topology 정보 불완전
- Processor Group 누락
- Intel Hybrid를 AMD CCX로 단정해야 하는 경우

### 10.4 Aggressive Single-Core Mode

목적: 충분한 confidence와 안전 조건이 확보된 경우 제한적인 강한 정책 묶음을 활성화한다.

입력:

- MainThreadConfidence
- observation duration
- migration stability
- rollback readiness
- ApplyGuard verify 가능성
- RuntimeValidation state

출력:

- aggressive mode enable/disable candidate
- enabledPolicies
- cooldown hint
- deactivation condition

의존 모듈:

- PerformanceEngine
- PolicyResolver
- PolicyDispatcher
- SchedulerController
- BackgroundController
- TimerResolutionController
- Planned: AggressiveSingleCoreController

정책 handoff 대상:

- `AGGRESSIVE_SINGLE_CORE_ENABLE`
- `AGGRESSIVE_SINGLE_CORE_DISABLE`
- `ROLLBACK_REQUEST`

Evidence 요구사항:

- aggressiveModeStart
- activeDuration
- enabledPolicies
- rollbackScopesSaved
- runtimeValidationState

Fallback 조건:

- Confidence < VeryHigh
- observation time 부족
- RuntimeValidation BLOCKER
- rollback state 저장 실패
- topology 불확실

### 10.5 Performance Evidence

목적: 성능 효과를 감각이 아니라 기록과 수치로 판단한다.

입력:

- policy candidate evidence requirements
- controller apply/verify result
- rollback result
- runtime validation summary
- Before/After samples

출력:

- Before/After Summary
- policy timeline
- missing evidence field
- release-facing evidence input

의존 모듈:

- RuntimeValidationMonitor
- release_gate_evidence.py
- `docs/release/Evidence_Schema.md`
- Planned: PerformanceEvidencePlanner
- Planned: PerformanceEvidenceRecorder

정책 handoff 대상:

- all performance policy types

Evidence 요구사항:

- policyType
- activationReason
- confidenceLevel
- riskLevel
- rollbackScope
- applyResult
- verifyResult
- rollbackStateSaved
- runtimeValidationState
- Before/After Summary

Fallback 조건:

- 필수 Evidence 누락
- Before/After window 비교 불가
- rollback failure
- runtime validation BLOCKER

## 11. Policy Architecture

Policy Architecture는 `docs/performance/PolicySpecification.md`를 기준으로 한다.

포함 개념:

```text
PolicyCandidate
PolicyResolver
PolicyDispatcher
PolicyCommand
Cooldown
RiskLevel
PolicyConflict
MonitoringOnly
RollbackRequest
```

Policy 흐름:

```text
PerformanceEngine
↓
PolicyCandidate
↓
PolicyResolver
↓
PolicyDispatcher
↓
Controller
```

필수 규칙:

```text
1. PolicyCandidate는 실행 명령이 아니다.
2. PolicyResolver는 충돌, cooldown, riskLevel, rollbackScope, evidenceFields를 검증한다.
3. PolicyDispatcher는 검증된 정책만 담당 Controller로 넘긴다.
4. Controller만 실제 적용 책임을 가진다.
```

PolicyResolver는 Planned 모듈이다. v3.1 초기 구현에서는 PolicyDispatcher 내부 책임으로 시작할 수 있으나, 역할은 문서상 분리되어야 한다.

## 12. Evidence Architecture

Evidence Layer는 정책 판단과 릴리즈 판단의 근거를 만든다.

필수 Evidence:

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
Before/After Summary
```

Evidence 흐름:

```text
PerformanceEngine
↓
PolicyCandidate Evidence Requirements
↓
Controller Apply Result
↓
Rollback Result
↓
RuntimeValidation Result
↓
PerformanceEvidenceRecorder
↓
Release Evidence Bundle
```

Evidence는 홍보 자료가 아니라 정책 판단과 릴리즈 판단의 근거다.

Evidence Architecture의 규칙은 다음과 같다.

- Evidence가 누락된 정책 결과는 성능 성공으로 분류하지 않는다.
- Rollback failure는 release-facing BLOCKER 후보가 된다.
- RuntimeValidation BLOCKER는 aggressive mode보다 우선한다.
- Before/After Summary는 성능 정책에 필요한 비교 가능성을 제공해야 한다.
- Existing release evidence schema와 충돌하는 field 추가는 후속 Evidence Schema 변경으로 다룬다.

## 13. Runtime State 개요

상세 상태 머신은 별도 `docs/architecture/RuntimeStateMachine.md`에서 작성한다. SAD에서는 상태 개요만 정의한다.

`Uninitialized`:
RuntimeContext와 주요 component가 아직 준비되지 않은 상태다.

`Initializing`:
StartupPipeline이 CLI, target process, topology, controller, rollback manager를 준비하는 상태다.

`Observing`:
ThreadTracker와 runtime sensors가 target process를 관찰하는 상태다.

`Evaluating`:
Performance Engine이 observation data를 분석하고 confidence와 policy eligibility를 계산하는 상태다.

`PolicyPending`:
Policy Candidate가 생성되었고 PolicyResolver 또는 PolicyDispatcher 검증을 기다리는 상태다.

`Applying`:
담당 Controller가 Save/Arm/Apply 흐름을 수행하는 상태다.

`Verifying`:
Controller가 post-apply state를 검증하고 commit 또는 rollback을 결정하는 상태다.

`Running`:
정책 적용이 검증되어 runtime monitoring과 Evidence 수집이 지속되는 상태다.

`MonitoringOnly`:
정책 적용 없이 관찰과 Evidence 기록만 수행하는 안전 fallback 상태다.

`RollbackPending`:
apply failure, verify failure, runtime validation failure, shutdown 등으로 rollback이 요청된 상태다.

`ShuttingDown`:
ShutdownPipeline이 shutdown reason에 따라 rollbackAll과 final evidence를 처리하는 상태다.

`Terminated`:
target process 또는 GameOptimizer runtime이 종료된 상태다.

## 14. Error / Fallback Architecture

필수 fallback 조건은 다음과 같다.

```text
Access Denied
Anti-Cheat boundary suspicion
Processor Group unsupported path
Topology incomplete
MainThreadConfidence Low/Medium
Rollback state save failure
ApplyGuard verify failure
RuntimeValidation BLOCKER
Target process exited
Identity mismatch
```

Fallback 결과:

```text
Monitoring-only
Policy candidate rejected
Rollback request
Safe shutdown
Release-facing BLOCKER
```

Fallback 원칙:

- Access Denied와 anti-cheat boundary는 우회 대상이 아니라 안전 제한 신호다.
- Topology incomplete와 Processor Group unsupported path는 monitoring-only로 전환할 수 있다.
- Rollback state save failure는 mutation 차단 사유다.
- ApplyGuard verify failure는 rollback 또는 failed-state preservation으로 연결되어야 한다.
- RuntimeValidation BLOCKER는 aggressive mode와 성능 정책보다 우선한다.
- Identity mismatch는 rollback 대상 검증 실패로 처리한다.

## 15. Module Status Matrix

| Module | Layer | Status | Responsibility | Mutation Allowed |
|---|---|---|---|---|
| CLI / CliOptions | Application | Existing | Runtime option parsing | No |
| StartupPipeline | Application | Existing | Runtime component assembly | Limited, guarded startup path |
| ShutdownPipeline | Application | Existing | Shutdown rollback orchestration | Yes, rollback path |
| RuntimeOrchestrator | Application | Existing | Runtime lifecycle orchestration | No |
| RuntimeContext | Runtime | Existing | Shared runtime state | No |
| TrackingWatchdog | Runtime | Existing | Watchdog execution | No |
| WatchdogCycleRunner | Runtime | Existing | Watchdog cycle policy flow | No |
| RuntimeValidationMonitor | Runtime | Existing | Runtime validation samples | No |
| ThreadTracker | Observation | Existing | Thread sampling | No |
| ProcessFinder | Observation | Existing | Target process discovery | No |
| LatencyMetricsCollector | Observation | Existing | RTT/DPC metrics collection | No |
| InputLatencyController | Observation/Control | Existing | Raw Input query, pinning guarded off unless eligible | Limited, guarded |
| NetworkInterruptController | Observation/Control | Existing | DPC monitoring and unsupported IRQ fallback | Limited, guarded/monitoring-only |
| PerformanceEngine | Performance Analysis | Planned | Policy candidate generation | No |
| MainThreadConfidenceAnalyzer | Performance Analysis | Planned | Main thread confidence | No |
| CoreIsolationPlanner | Performance Analysis | Planned | Core isolation planning | No |
| CCXStabilityPlanner | Performance Analysis | Planned | CCX/L3 stability planning | No |
| AggressiveSingleCoreController | Performance Analysis | Planned | Aggressive mode condition tracking | No |
| PerformanceEvidencePlanner | Performance Analysis | Planned | Evidence requirement planning | No |
| PolicyCandidate | Policy | Planned | Policy candidate data model | No |
| PolicyResolver | Policy | Planned | Conflict/cooldown/risk validation | No |
| PolicyDispatcher | Policy | Existing | Dispatch validated policy commands | No direct mutation |
| PolicyCommand | Policy | Existing | Existing command vocabulary | No |
| SchedulerController | Control | Existing | Thread scheduling mutation | Yes, guarded |
| BackgroundController | Control | Existing | Background process restriction | Yes, guarded |
| TimerResolutionController | Control | Existing | Timer resolution request/release | Yes, guarded |
| TopologyAnalyzer | Hardware/Topology | Existing | Group-aware topology analysis | No |
| ProcessorGroupManager | Hardware/Topology | Planned | Processor group policy helper | No |
| CoreMaskPlanner | Hardware/Topology | Planned | Group-aware mask planning | No |
| ApplyGuard | Safety/Rollback | Existing | Transaction boundary | Yes, guard/rollback |
| RollbackManager | Safety/Rollback | Existing | State save and rollback | Yes, rollback |
| StateSnapshot | Safety/Rollback | Planned | Snapshot abstraction | No direct mutation |
| RuntimeSnapshotRecorder | Evidence | Planned | Runtime evidence snapshot | No |
| PerformanceEvidenceRecorder | Evidence | Planned | Performance evidence record | No |
| PolicyTimelineRecorder | Evidence | Planned | Policy timeline evidence | No |
| EvidenceSchema | Evidence | Existing document | Evidence field contract | No |
| ReleaseGate | Release/Validation | Existing scripts | RC gate execution | No runtime mutation |
| RegressionMatrix | Release/Validation | Existing document | Regression requirements | No |
| RuntimeArtifactValidator | Release/Validation | Planned | Artifact validation | No |
| RCReportGenerator | Release/Validation | Planned | RC report generation | No |

## 16. Non-Goals

SAD_v1.0.md에서 제외할 항목은 다음과 같다.

```text
구체 C++ 클래스 시그니처 확정
구체 enum 최종 구현
상세 테스트 케이스 작성
UI 설계
설치 프로그램 설계
AI/ML 자동 튜닝 구조
Kernel Driver 구조
DLL Injection 구조
Game Memory Patch 구조
Anti-Cheat 우회 구조
FPS 향상 보장
```

본 문서는 구조와 책임 경계를 정의한다. 구현 순서, 구체 API, 테스트 fixture, release script 변경은 후속 문서와 구현 단계에서 다룬다.

## 17. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

```text
1. v3.1 전체 Layer 구조가 정의되어 있다.
2. 기존 모듈과 신규 성능 모듈의 배치가 정의되어 있다.
3. Performance Engine이 직접 mutation하지 않는 구조가 명시되어 있다.
4. PolicyCandidate → PolicyResolver → PolicyDispatcher → Controller 흐름이 정의되어 있다.
5. SchedulerController, BackgroundController, RollbackManager, ApplyGuard의 소유권이 명확하다.
6. Processor Group / Topology 안전 원칙이 포함되어 있다.
7. Evidence 흐름이 정의되어 있다.
8. Runtime 상태 개요가 포함되어 있다.
9. Fallback 조건과 결과가 정의되어 있다.
10. 후속 MDS 문서 작성에 필요한 모듈 경계가 충분히 정의되어 있다.
```

Acceptance Criteria는 SAD 문서 승인 기준이다. 구현 완료나 RC 승인 기준으로 해석하지 않는다.

## 18. Open Questions

1. PolicyResolver를 별도 모듈로 둘 것인가, PolicyDispatcher 내부 책임으로 둘 것인가?
2. PerformanceEvidencePlanner와 PerformanceEvidenceRecorder를 분리할 것인가?
3. RuntimeStateMachine은 RuntimeOrchestrator가 소유할 것인가, 별도 RuntimeStateController를 둘 것인가?
4. TimerResolutionController는 Control Layer에 독립 모듈로 둘 것인가, 기존 Runtime 관리에 포함할 것인가?
5. ProcessorGroupManager를 TopologyAnalyzer 내부로 둘 것인가, 별도 모듈로 분리할 것인가?
6. PolicyCandidate는 기존 PolicyCommand와 병합할 것인가, 별도 중간 계층으로 둘 것인가?
7. Evidence 누락 시 release-facing WARN/BLOCKER 기준은 EvidenceSpecification에서 확정할 것인가?
8. Performance Engine의 최초 구현 범위는 MainThreadConfidenceAnalyzer만으로 제한할 것인가?
