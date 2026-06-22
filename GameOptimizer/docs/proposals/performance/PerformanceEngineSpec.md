# GameOptimizer v3.1 Performance Engine Specification

Status: Proposal
Authority: Proposed performance design, not current implementation contract
TargetVersion: v3.1 proposal
ImplementationStatus: Not implemented
VerificationStatus: Not verified/manual design review
Owner: Performance proposal

## 1. 문서 개요

문서명: GameOptimizer v3.1 Performance Engine Specification

버전: v1.0

작성 목적: GameOptimizer v3.1에서 추가할 싱글코어 병목 게임 전용 성능 엔진(Performance Engine)의 설계 범위, 정책 조건, 안전 제약, 입력/출력, 적용 흐름, 성능 증거(Performance Evidence) 요구사항을 정의한다.

적용 범위:

- 메인 스레드 신뢰도(Main Thread Confidence) 계산
- 코어 격리(Core Isolation) 후보 계획
- CCX 안정성(CCX Stability) 판단
- 공격적 싱글코어 모드(Aggressive Single-Core Mode) 활성 조건 판단
- 정책 후보(Policy Candidate) handoff 요구사항
- Before/After 성능 증거(Performance Evidence) 요구사항

비적용 범위:

- Win32 제어 API 직접 호출 구현
- 구체적인 `PolicyCommand` enum 확장
- 구체적인 High/VeryHigh threshold 확정
- FPS/frame time 직접 계측 방식 확정
- UI, CLI, 저장 포맷, RC Gate 구현 변경

상위 문서: `docs/vision/PVD_v1.0.md`

후속 문서: `docs/proposals/performance/PolicySpecification.md`

기준 문서 경로 기준: 문서 루트. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

- `docs/vision/PVD_v1.0.md`
- 기존 GameOptimizer v3.0 SRS Rev 1.3
- `docs/architecture/Architecture_Decision_Record.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/release/Evidence_Schema.md`

본 문서는 코드 구현 문서가 아니다. 본 문서는 v3.1 성능 엔진이 어떤 판단을 만들고, 어떤 조건에서 정책 후보를 생성하며, 어떤 안전 계약을 따라야 하는지 정의한다.

## 2. Performance Engine 개요

Performance Engine은 GameOptimizer v3.1에서 싱글코어 병목 게임의 메인 스레드(Main Thread)를 더 정확히 찾고, 더 안정적으로 유지하며, 간섭이 적은 코어 환경에 배치하기 위한 성능 판단 계층이다.

Performance Engine은 직접 Win32 제어 API를 호출하는 모듈이 아니다.
Performance Engine은 관찰 결과를 분석하고, 정책 적용 가능성을 판단하며, SchedulerController와 PolicyDispatcher가 사용할 수 있는 성능 정책 결정을 생성한다.

Performance Engine의 핵심 책임은 다음과 같다.

1. Main Thread Confidence 계산
2. Core Isolation 후보 계획
3. CCX Stability 판단
4. Aggressive Single-Core Mode 활성 조건 판단
5. Before/After Performance Evidence 요구사항 정의

Performance Engine의 금지 책임은 다음과 같다.

1. SetThreadAffinityMask 직접 호출
2. SetThreadGroupAffinity 직접 호출
3. SetThreadPriority 직접 호출
4. SetPriorityClass 직접 호출
5. RollbackManager 우회
6. ApplyGuard 우회
7. Registry 직접 변경
8. Anti-Cheat 우회 시도

v3.1 Performance Engine은 다음 게임군을 대상으로 한다.

```text
싱글코어 병목 게임
레거시 엔진 기반 게임
구형 MMORPG
메인 스레드 병목이 강한 온라인 게임
Windows Scheduler의 thread migration에 민감한 게임
```

비대상은 다음과 같다.

```text
최신 DX12/Vulkan 기반 게임
Job System 기반 멀티코어 게임
GPU 병목 게임
Anti-Cheat 정책상 외부 스케줄링 제어가 위험한 게임
```

Performance Engine의 최상위 목표는 다음이다.

```text
메인 스레드를 더 정확히 찾고,
더 안정적으로 유지하며,
간섭이 적은 코어 환경에 배치하고,
정책 효과를 Evidence로 증명한다.
```

단, 모든 성능 기능은 기존 안정성 체계를 우회할 수 없다.

## 3. Performance Engine 구성 요소

Performance Engine은 다음 하위 구성 요소로 정의한다.

```text
PerformanceEngine
├─ MainThreadConfidenceAnalyzer
├─ CoreIsolationPlanner
├─ CCXStabilityPlanner
├─ AggressiveSingleCoreController
└─ PerformanceEvidencePlanner
```

### 3.1 PerformanceEngine

목적: 하위 분석기의 결과를 종합하여 정책 후보(Policy Candidate)를 생성하고, PolicyDispatcher로 넘길 수 있는 형태의 판단 결과를 만든다.

입력:

- ThreadTracker sample
- TopologyResult
- SchedulerMode
- RuntimeValidation 상태
- Access Denied 및 fallback telemetry
- 기존 policy activation 상태

출력:

- Main Thread Confidence 요약
- Core Isolation Plan 후보
- CCX Stability 판단
- Aggressive Single-Core Mode 활성/비활성 판단
- Policy Candidate 목록
- Evidence 요구사항 목록

상태:

- 현재 main thread 후보와 이전 후보
- confidence history
- policy candidate history
- cooldown 상태
- monitoring-only 전환 사유

금지사항:

- Win32 제어 API 직접 호출 금지
- RollbackManager 직접 호출 금지
- ApplyGuard 직접 생성 또는 우회 금지
- Controller 소유 상태 직접 변경 금지

실패 시 동작:

- 정책 후보 생성을 중단한다.
- 현재 cycle은 monitoring-only로 기록한다.
- 실패 사유를 Performance Evidence 요구사항에 포함한다.

Evidence 요구사항:

- policy candidate 생성 여부
- candidate 거부 사유
- monitoring-only 전환 사유
- confidence level 변화
- runtime validation 상태

### 3.2 MainThreadConfidenceAnalyzer

목적: 메인 스레드 후보가 실제 최적화 대상인지 신뢰도를 계산한다.

입력:

- ThreadTracker EMA score
- CPU delta
- 후보 유지 시간
- 관찰 지속 시간
- thread migration count
- thread lifetime stability
- thread access failure count
- main thread switch frequency

출력:

- MainThreadConfidenceLevel
- confidence score 또는 score bucket
- confidence 상승/하락 사유
- policy eligibility 판단

상태:

- 후보별 confidence history
- 후보별 유지 시간
- 후보 교체 횟수
- access failure 누적 상태

금지사항:

- thread affinity/priority 직접 변경 금지
- thread handle set 권한 요구 금지
- confidence가 낮은 후보에 정책 적용 가능 판정 금지

실패 시 동작:

- confidence를 Low 또는 monitoring-only 상태로 강등한다.
- 정책 후보 생성을 차단한다.
- access failure 또는 관찰 실패를 Evidence에 기록한다.

Evidence 요구사항:

- Main Thread Confidence History
- Main Thread Switch Count
- thread access failure count
- Low/Medium/High/VeryHigh 판정 사유

### 3.3 CoreIsolationPlanner

목적: 메인 스레드가 실행되는 코어의 간섭을 줄이기 위한 코어 격리(Core Isolation) 후보를 계획한다.

입력:

- TopologyResult
- MainThreadConfidence
- 현재 mainThreadId
- processorGroup
- KAFFINITY
- SMT sibling 정보
- background process 후보
- 현재 SchedulerMode

출력:

- CoreIsolationPlan 후보
- backgroundExclusionMask 후보
- isolationStrength 후보
- fallbackMask 후보
- risk level

상태:

- 현재 main thread core 후보
- 최근 core relocation 횟수
- background restriction 적용 가능성
- Processor Group 1+ 제한 상태

금지사항:

- background process affinity 직접 변경 금지
- Processor Group 1+에서 안전하지 않은 process-wide restriction 후보 생성 금지
- topology 불확실 상태에서 강한 isolation 후보 생성 금지

실패 시 동작:

- Core Isolation 후보를 생성하지 않는다.
- monitoring-only로 전환한다.
- processor group 또는 topology 제한 사유를 Evidence에 기록한다.

Evidence 요구사항:

- Core Isolation Plan
- Core Relocation Count
- background exclusion 후보
- Processor Group 제한 사유
- isolationStrength와 fallback 사유

### 3.4 CCXStabilityPlanner

목적: AMD Ryzen 계열처럼 CCX/L3 구조가 성능에 영향을 줄 수 있는 환경에서 불필요한 이동을 줄이는 CCX 안정성(CCX Stability) 후보를 판단한다.

입력:

- TopologyResult
- L3 cache group
- CCX/CCD 추정 정보
- processorGroup
- main thread current core
- candidate core list
- migration history

출력:

- CCX Stability 판단
- preferred core range 후보
- disallowed migration range 후보
- monitoring-only 여부
- topology confidence

상태:

- 현재 L3/CCX 범위
- 최근 CCX/CCD 경계 이동 추정
- topology completeness 상태
- group-aware 판단 상태

금지사항:

- 불완전한 topology 정보만으로 강제 적용 후보 생성 금지
- Processor Group 정보 누락 시 group 0으로 조용히 보정 금지
- Intel Hybrid 정책을 AMD CCX 정책으로 단정 금지

실패 시 동작:

- CCX 정책은 monitoring-only로 남긴다.
- topology 불완전 사유를 Evidence에 기록한다.
- Core Isolation 또는 Aggressive Mode의 강한 후보 생성을 제한한다.

Evidence 요구사항:

- topology completeness
- CCX/L3 판단 가능 여부
- migration history 요약
- monitoring-only fallback 사유

### 3.5 AggressiveSingleCoreController

목적: 충분한 신뢰도와 안전 조건이 확보된 경우에만 공격적 싱글코어 모드(Aggressive Single-Core Mode)를 활성화 후보로 판단한다.

입력:

- MainThreadConfidenceLevel
- 관찰 지속 시간
- migration 안정 상태
- rollback state 저장 가능성
- ApplyGuard Verify 가능성
- SchedulerMode
- Access Denied 및 Anti-Cheat boundary telemetry
- Processor Group 안전성

출력:

- Aggressive Single-Core Mode 활성 후보
- 비활성 또는 monitoring-only 전환 판단
- activation reason
- deactivation condition
- cooldown hint

상태:

- aggressive mode 후보 지속 시간
- active duration
- 비활성 사유
- cooldown 상태
- 최근 rollback 또는 validation 위험 상태

금지사항:

- 직접 정책 적용 금지
- confidence가 VeryHigh 미만인 상태에서 aggressive 후보 생성 금지
- rollback state 저장 실패 가능성이 있는 정책 후보 생성 금지
- ApplyGuard Verify 불가능 상태에서 활성 후보 생성 금지

실패 시 동작:

- aggressive 후보를 제거한다.
- monitoring-only 또는 soft-apply 후보로 낮춘다.
- 비활성 사유를 Evidence에 기록한다.

Evidence 요구사항:

- Aggressive Mode Active Duration
- activation reason
- deactivation condition
- confidence drop 기록
- rollback/validation 위험 기록

### 3.6 PerformanceEvidencePlanner

목적: 성능 정책의 Before/After 비교와 릴리즈 판단에 필요한 성능 증거(Performance Evidence) 요구사항을 정의한다.

입력:

- confidence history
- migration count
- switch count
- Core Isolation Plan
- policy activation history
- RTT Jitter
- DPC Spike Count
- Rollback Result
- Runtime Validation Summary

출력:

- Baseline Window 요구사항
- Applied Window 요구사항
- Before/After Summary 요구사항
- 필수 evidence field 목록
- 누락 field 경고

상태:

- baseline window 기록 상태
- applied window 기록 상태
- 비교 가능 여부
- missing evidence field 목록

금지사항:

- Evidence 없는 정책 성공 판정 금지
- Before/After 없이 성능 개선으로 분류 금지
- rollback 실패를 WARN 또는 INFO로 낮추는 판단 금지

실패 시 동작:

- 정책 결과를 성능 개선으로 분류하지 않는다.
- missing evidence 상태를 기록한다.
- 필요 시 release-facing evidence에서 BLOCKER 또는 WARN 판단으로 넘긴다.

Evidence 요구사항:

- Main Thread Confidence History
- Main Thread Migration Count
- Core Isolation Plan
- Policy Activation Count
- Rollback Result
- Runtime Validation Summary
- Before/After Summary

## 4. Main Thread Confidence

### 4.1 목적

메인 스레드 신뢰도(Main Thread Confidence)는 메인 스레드 후보가 실제 최적화 대상인지 판단하기 위한 핵심 기준이다.

```text
Confidence가 낮은 스레드에는 affinity/priority 정책을 적용하지 않는다.
```

Main Thread Confidence는 단일 cycle의 CPU 사용량이 아니라 지속 관찰과 안정성 신호를 함께 반영해야 한다.

### 4.2 입력

Main Thread Confidence는 다음 입력을 사용한다.

```text
ThreadTracker EMA score
CPU delta
후보 유지 시간
관찰 지속 시간
thread migration count
thread lifetime stability
thread access failure count
main thread switch frequency
```

입력 중 일부가 누락되거나 access failure가 누적되면 confidence는 상향될 수 없다. 이 경우 Performance Engine은 정책 후보를 생성하지 않거나 monitoring-only로 남긴다.

### 4.3 출력

문서 레벨에서 다음 상태를 정의한다.

```text
MainThreadConfidenceLevel:
- Low
- Medium
- High
- VeryHigh
```

각 상태의 의미는 다음과 같다.

```text
Low: 관찰만 허용
Medium: soft-apply 후보
High: 제한적 affinity/priority 후보
VeryHigh: Aggressive Single-Core Mode 후보
```

이 상태 정의는 구현 enum을 확정하지 않는다. 구체적인 타입 이름과 PolicyCommand 연결은 `docs/proposals/performance/PolicySpecification.md`에서 확정한다.

### 4.4 기본 정책

```text
Confidence < High:
- affinity 적용 금지
- priority 적용 금지
- aggressive mode 금지
- monitoring-only 유지

Confidence == High:
- 제한적 정책 후보
- ApplyGuard 검증 필수
- rollback state 저장 필수

Confidence == VeryHigh:
- aggressive single-core mode 후보
- 최소 관찰 시간 조건 필요
- migration 안정 조건 필요
```

Confidence가 High 이상이어도 즉시 적용을 의미하지 않는다. Performance Engine은 정책 후보를 만들 뿐이며, 실제 적용은 PolicyDispatcher와 담당 Controller의 안전 계약을 통과해야 한다.

### 4.5 기준값 처리

High/VeryHigh의 구체 수치는 아직 확정하지 않는다.

```text
High threshold: TBD
VeryHigh threshold: TBD
Minimum observation time: 기본 후보 30초, 최종값 TBD
```

Threshold 값은 실제 싱글코어 병목 게임 검증과 Performance Validation Plan에서 확정한다. 임의의 고정값을 이 문서에서 만들지 않는다.

## 5. Core Isolation

### 5.1 목적

코어 격리(Core Isolation)는 메인 스레드가 실행되는 코어의 간섭을 줄이는 정책 후보다. 목적은 성능을 보장하는 것이 아니라, 백그라운드 프로세스와 스케줄링 변동으로 인한 간섭 가능성을 낮추는 것이다.

### 5.2 입력

```text
TopologyResult
MainThreadConfidence
현재 mainThreadId
processorGroup
KAFFINITY
SMT sibling 정보
background process 후보
현재 SchedulerMode
```

Core Isolation은 MainThreadConfidence가 낮거나 topology가 불완전한 경우 강한 후보를 만들 수 없다.

### 5.3 출력

문서 레벨에서 다음 개념 구조를 정의한다.

```text
CoreIsolationPlan:
- processorGroup
- mainThreadMask
- fallbackMask
- backgroundExclusionMask
- isolationStrength
```

이 구조는 개념 정의이며 코드 구조체를 확정하지 않는다. 필드명, 타입, 저장 위치는 `PolicySpecification.md` 또는 후속 Module Design Specification에서 확정한다.

### 5.4 정책

Core Isolation의 기본 정책은 다음과 같다.

```text
1. Main Thread Core는 가능한 한 background process에서 제외한다.
2. SMT sibling은 기본적으로 main thread와 동시에 강한 부하를 받지 않도록 계획한다.
3. Processor Group 1+ 환경에서는 안전한 경로가 확인되지 않으면 process-wide restriction을 적용하지 않는다.
4. Core Isolation은 성능 보장이 아니라 간섭 감소 정책이다.
```

Core Isolation 후보는 BackgroundController가 수행할 수 있는 안전한 범위 안에서만 생성되어야 한다. Processor Group 1+ process-wide restriction은 현재 안전 경로가 확인되지 않으면 monitoring-only로 남긴다.

## 6. CCX Stability

### 6.1 목적

CCX 안정성(CCX Stability)은 AMD Ryzen 계열처럼 CCX/L3 구조가 성능에 영향을 줄 수 있는 환경에서 불필요한 이동을 줄이기 위한 판단이다.

이 정책은 CPU topology가 성능 안정성에 영향을 줄 수 있다는 전제를 사용하지만, 불확실한 topology 추정을 강제 적용의 근거로 사용하지 않는다.

### 6.2 입력

```text
TopologyResult
L3 cache group
CCX/CCD 추정 정보
processorGroup
main thread current core
candidate core list
migration history
```

### 6.3 우선순위

다음 순서를 기본 정책 후보로 둔다.

```text
1. 동일 logical core 유지
2. 동일 physical core의 SMT sibling 회피
3. 동일 L3/CCX 내부 유지
4. 동일 CCD 내부 유지
5. 다른 CCD 또는 다른 group 이동은 fallback으로만 허용
```

이 우선순위는 최종 적용 순서가 아니라 판단 후보의 기본 방향이다. 실제 적용 가능성은 Main Thread Confidence, SchedulerMode, ApplyGuard 검증 가능성, rollback state 저장 가능성을 함께 봐야 한다.

### 6.4 안전 조건

```text
Topology 정보가 불완전하면 CCX 정책은 monitoring-only로 남긴다.
CCX 정책은 불확실한 추정값만으로 강제 적용하지 않는다.
Processor Group 정보가 누락되면 group 0으로 조용히 보정하지 않는다.
```

Intel Hybrid 구조는 AMD CCX 정책과 동일하게 취급하지 않는다. Intel Hybrid 포함 여부와 정책 방식은 Open Question으로 남긴다.

## 7. Aggressive Single-Core Mode

### 7.1 목적

공격적 싱글코어 모드(Aggressive Single-Core Mode)는 충분한 신뢰도와 안전 조건이 확보된 경우에만 강한 싱글코어 최적화 정책을 활성화 후보로 판단하는 모드다.

Aggressive Single-Core Mode는 기본 동작이 아니다. 낮은 confidence, 불완전한 topology, 반복된 Access Denied, rollback 위험, runtime validation 위험이 있으면 활성 후보가 될 수 없다.

### 7.2 활성 조건

다음 조건을 모두 만족해야 한다.

```text
MainThreadConfidence >= VeryHigh
Minimum observation time 충족
Migration 안정 상태
Rollback state 저장 성공 가능
ApplyGuard Verify 가능
SchedulerMode가 apply 허용 상태
Anti-Cheat 또는 Access Denied 위험이 낮음
Processor Group 정책이 안전함
```

기본 후보값은 다음과 같다.

```text
Minimum observation time: 30초 후보, 최종값 TBD
Migration stability threshold: TBD
```

### 7.3 적용 후보

Aggressive Single-Core Mode에서 고려 가능한 정책 후보는 다음과 같다.

```text
1. Main Thread priority 상향
2. Game process priority 제한적 상향
3. Main Thread affinity 안정화
4. Background process restriction 강화
5. Timer Resolution 적용 유지
```

Aggressive Single-Core Mode는 직접 적용 기능이 아니라 정책 활성 조건이다.
실제 적용은 SchedulerController, BackgroundController, TimerResolutionController 등 기존 담당 모듈이 수행한다.

Timer Resolution 관련 정책은 기존 원상 복구와 release evidence 계약을 만족하는 범위에서만 유지 후보가 될 수 있다. 원상 상태 저장과 rollback 계약이 불명확한 제어는 aggressive mode 후보가 될 수 없다.

### 7.4 비활성 조건

다음 경우 즉시 비활성 또는 monitoring-only로 전환한다.

```text
MainThreadConfidence 하락
main thread 후보 교체
rollback state 저장 실패
ApplyGuard verify 실패
Access Denied 반복
runtime validation BLOCKER 발생
processor topology 불확실
target process 종료 또는 identity mismatch
```

비활성 전환은 실패를 숨기지 않는다. Performance Engine은 비활성 사유와 관련 Evidence 요구사항을 남겨야 한다.

## 8. Performance Evidence

### 8.1 목적

성능 증거(Performance Evidence)는 성능 향상을 감각이 아니라 기록과 수치로 판단하기 위한 근거다. Performance Engine은 정책 후보와 적용 결과가 비교 가능한 형태로 남도록 Evidence 요구사항을 정의해야 한다.

### 8.2 필수 기록 항목

다음 항목을 포함한다.

```text
Main Thread Confidence History
Main Thread Migration Count
Main Thread Switch Count
Core Isolation Plan
Core Relocation Count
Policy Activation Count
Aggressive Mode Active Duration
RTT Jitter
DPC Spike Count
Rollback Result
Runtime Validation Summary
Before/After Summary
```

필수 기록 항목이 누락되면 해당 정책 결과는 성능 개선으로 분류하지 않는다. Rollback failure 또는 runtime validation BLOCKER는 release-facing evidence에서 낮은 severity로 처리할 수 없다.

### 8.3 Before/After 비교

Before/After 비교는 다음 구조를 따른다.

```text
Baseline Window:
- 정책 적용 전 관찰 구간

Applied Window:
- 정책 적용 후 관찰 구간

Comparison:
- Migration 변화
- Confidence 변화
- RTT jitter 변화
- DPC spike 변화
- policy activation 변화
```

FPS/frame time 직접 측정 여부는 PerformanceValidationPlan.md에서 확정한다.
본 문서에서는 외부 측정값 입력 또는 별도 계측 연동 가능성을 Open Question으로 남긴다.

## 9. Runtime Flow

Performance Engine은 Runtime에서 다음 순서로 동작한다.

```text
ThreadTracker Sample
↓
MainThreadConfidence Update
↓
Topology/Isolation Plan Review
↓
Performance Policy Candidate 생성
↓
PolicyDispatcher 전달
↓
담당 Controller가 Save/Arm/Apply/Verify/Commit 수행
↓
Performance Evidence 기록
```

금지 흐름은 다음과 같다.

```text
Performance Engine
↓
Win32 제어 API 직접 호출
```

위 흐름은 반드시 금지한다. Performance Engine이 생성한 판단은 PolicyDispatcher와 담당 Controller의 안전 계약을 통과해야 하며, 적용과 rollback 책임은 기존 소유 모듈에 남아야 한다.

## 10. Safety Contract

모든 성능 정책은 다음 순서를 따라야 한다.

```text
Observe
↓
Analyze
↓
Decide
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

실패 시에는 다음 흐름을 따른다.

```text
Rollback
↓
Evidence
↓
Monitoring-only fallback 또는 Safe Shutdown
```

Rollback 없는 성능 기능은 Performance Engine에 포함될 수 없다.

Evidence 없는 성능 기능은 Performance Engine에 포함될 수 없다.

Performance Engine은 Observe, Analyze, Decide까지만 직접 책임진다. Save, Arm, Apply, Verify, Commit, Rollback은 PolicyDispatcher 이후 담당 Controller와 기존 안전 계약이 수행한다.

## 11. Policy Handoff

Performance Engine은 직접 정책을 실행하지 않는다.

정책 handoff는 다음 개념 흐름을 따른다.

```text
Performance Engine
↓
Policy Candidate
↓
Policy Specification
↓
PolicyDispatcher
↓
담당 Controller
```

Policy Candidate에는 다음 정보가 포함되어야 한다.

```text
policy type
confidence level
activation reason
required controller
required rollback scope
risk level
evidence fields
cooldown hint
deactivation condition
```

Policy Candidate는 실행 명령이 아니라 정책 판단 결과다. 구체적인 PolicyCommand enum 확장은 다음 문서인 `PolicySpecification.md`에서 확정한다.

## 12. Non-Goals

다음은 Performance Engine의 명확한 비목표다.

```text
모든 게임 자동 최적화
AI/ML 자동 튜닝
Self-Learning 최적화
DLL Injection
Kernel Driver
Game Memory Patch
Anti-Cheat 우회
Raw Input 추정 기반 강제 pinning
검증 없는 registry 자동 변경
Processor Group 1+ process-wide restriction 강제 적용
FPS 또는 frame time 개선을 보증하는 주장
```

Performance Engine은 더 많은 시스템 영역을 건드리기 위한 계층이 아니다. v3.1의 방향은 더 정확한 대상에 더 안전하게 제한된 정책 후보를 생성하는 것이다.

## 13. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

```text
1. Main Thread Confidence 개념이 정의되어 있다.
2. Confidence에 따른 정책 허용/금지 기준이 정의되어 있다.
3. Core Isolation의 입력/출력/안전 조건이 정의되어 있다.
4. CCX Stability의 적용 조건과 fallback 조건이 정의되어 있다.
5. Aggressive Single-Core Mode의 활성/비활성 조건이 정의되어 있다.
6. Performance Evidence 필수 기록 항목이 정의되어 있다.
7. Performance Engine이 직접 Win32 제어 API를 호출하지 않는다는 금지 규칙이 명시되어 있다.
8. PolicySpecification.md로 넘길 handoff 정보가 정의되어 있다.
9. Rollback/Evidence 없는 성능 기능 금지 원칙이 포함되어 있다.
```

Acceptance Criteria는 이 문서의 승인 기준이다. 구현 완료, 성능 검증 완료, 릴리즈 승인 기준은 각각 후속 문서와 RC Gate에서 별도로 확정한다.

## 14. Open Questions

1. MainThreadConfidence의 High/VeryHigh 기준값은 얼마로 둘 것인가?
2. Aggressive Single-Core Mode의 최소 관찰 시간은 30초로 충분한가?
3. Migration 안정 기준은 몇 cycle 또는 몇 초로 정의할 것인가?
4. Core Isolation 강도는 단계형으로 둘 것인가?
5. CCX 정책은 AMD Ryzen 우선으로 둘 것인가, Intel Hybrid까지 포함할 것인가?
6. Performance Evidence에서 FPS/frame time을 직접 측정할 것인가?
7. Policy Candidate와 기존 PolicyCommand enum을 어떻게 연결할 것인가?
8. Background Restriction과 Core Isolation의 충돌 우선순위는 어떻게 둘 것인가?
9. Processor Group 1+ 환경에서 Performance Engine은 어느 수준까지 개입할 것인가?
