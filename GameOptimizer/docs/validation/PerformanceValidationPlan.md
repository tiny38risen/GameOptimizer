# GameOptimizer v3.1 Performance Validation Plan

## 1. 문서 개요

문서명: GameOptimizer v3.1 Performance Validation Plan

버전: v1.0

작성 목적: GameOptimizer v3.1의 성능 엔진(Performance Engine), 정책 체계(Policy Layer), 런타임 상태 머신(Runtime State Machine), 모듈 설계(Module Design Specification)가 실제 싱글코어 병목 게임에서 유효한지 검증하기 위한 성능 검증 계획(Performance Validation Plan)을 정의한다.

적용 범위:

- 성능 검증 목표
- 검증 대상 게임군과 비대상 게임군
- 검증 환경 기록 항목
- dry-run, soft-apply, apply, aggressive, rollback, soak 검증 모드
- 필수/선택 측정 지표
- Baseline Window / Applied Window / Comparison 구조
- PASS / PASS_WITH_WARNINGS / NEUTRAL / REGRESSION / BLOCKER 판정 기준
- 실제 게임 검증 원칙
- Evidence 요구사항
- 실패 처리 기준

비적용 범위:

- 구체 테스트 코드 작성
- 자동화 스크립트 구현
- FPS 측정 도구 구현
- UI 리포트 구현
- 게임별 공식 지원 선언
- AI/ML 기반 성능 분석
- Anti-Cheat 우회 테스트

상위 문서:

- `docs/vision/PVD_v1.0.md`
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
- `docs/release/Evidence_Schema.md`

후속 문서:

- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/RealGameValidationPlan.md`
- `docs/release/ReleaseChecklist_v3.1.md`

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

Performance Validation Plan의 목적은 모든 게임에서 성능 향상을 증명하는 것이 아니라, v3.1이 목표로 삼은 싱글코어 병목 게임에서 제한적이고 검증 가능한 개선이 있는지 확인하는 것이다.

## 2. 검증 목표

성능 검증은 평균 FPS만으로 판단하지 않는다. GameOptimizer v3.1의 핵심 검증 대상은 메인 스레드 안정성, migration 감소, jitter 안정성, rollback 안전성, runtime validation 결과다.

필수 목표:

1. Main Thread Migration이 감소했는지 확인한다.
2. Main Thread Confidence가 안정적으로 유지되는지 확인한다.
3. Main Thread Switch가 줄어드는지 확인한다.
4. Core Isolation 적용 후 background interference가 줄어드는지 확인한다.
5. RTT Jitter와 DPC Spike가 악화되지 않는지 확인한다.
6. Aggressive Single-Core Mode가 안전 조건을 만족할 때만 활성화되는지 확인한다.
7. Rollback이 항상 정상적으로 수행되는지 확인한다.
8. Runtime Validation BLOCKER가 없는지 확인한다.
9. 성능 개선 여부를 Before/After Evidence로 설명할 수 있는지 확인한다.

성능 검증은 성능 수치만 보는 작업이 아니다. 정책이 안전하게 거부되었는지, monitoring-only fallback이 적절했는지, rollback state가 보존되었는지도 검증 대상이다.

## 3. 검증 대상 게임군

검증 대상은 “공식 지원 게임”이 아니라 “우선 검증 대상 후보”다. 특정 게임을 공식 지원한다고 단정하지 않는다.

### 3.1 우선 검증 대상 후보

```text
마비노기
메이플스토리
던전앤파이터
라그나로크 온라인
구형 MMORPG
구형 DirectX 기반 온라인 게임
일부 구버전 Unity/엔진 기반 게임
```

각 게임은 dry-run, soft-apply, apply, rollback 결과를 통해 개별 검증되어야 한다.

### 3.2 비검증 또는 낮은 우선순위 대상

```text
최신 DX12/Vulkan 기반 게임
GPU 병목 게임
Job System 기반 멀티코어 게임
Anti-Cheat 위험성이 높은 게임
외부 제어 개입 이득보다 리스크가 큰 게임
```

비대상 게임에서 monitoring-only 또는 policy rejected가 발생하는 것은 실패가 아니라 올바른 안전 동작일 수 있다.

## 4. 검증 환경

검증 결과는 환경 의존성이 크므로, 다음 항목을 필수로 기록한다.

```text
OS version
CPU model
core/thread count
processor group count
SMT enabled 여부
RAM
GPU
game process name
GameOptimizer build hash
EXE SHA-256
Git commit hash
실행 권한
테스트 모드
테스트 시간
```

예시:

```text
OS: Windows 10/11 x64
CPU: AMD Ryzen 7 3700X 또는 유사한 CCX/L3 구조 CPU
SMT: enabled
Build: Release x64
Mode: dry-run / soft-apply / apply
```

추가 권장 기록:

- game client version
- game server/channel
- graphics settings
- foreground/background application list
- network connection type
- power plan
- security/anti-cheat boundary observation

## 5. 검증 모드

### 5.1 Dry-run

목적:

```text
정책 후보를 생성하지만 실제 mutation은 수행하지 않는다.
```

검증 항목:

```text
Main Thread Confidence 계산
PolicyCandidate 생성 여부
Monitoring-only reason
Policy rejection reason
Evidence field completeness
```

판정 기준:

- Win32 mutation이 발생하지 않아야 한다.
- PolicyCandidate와 rejection/monitoring-only reason이 Evidence로 남아야 한다.

### 5.2 Soft-apply

목적:

```text
제한적이고 안전한 검증 경로만 사용한다.
```

검증 항목:

```text
ApplyGuard 흐름
Rollback state save 가능성
Verify 가능성
Evidence completeness
```

판정 기준:

- soft-apply evidence는 rollback state로 오인되면 안 된다.
- verify 가능성과 fallback reason이 기록되어야 한다.

### 5.3 Apply

목적:

```text
검증된 정책을 실제 적용하고 Before/After를 비교한다.
```

검증 항목:

```text
apply result
verify result
migration before/after
confidence stability
rollback result
runtime validation result
```

판정 기준:

- rollback state 저장, ApplyGuard arm, apply, verify, commit/evidence 흐름이 확인되어야 한다.
- RuntimeValidation BLOCKER가 없어야 PASS 가능하다.

### 5.4 Aggressive Single-Core Mode

목적:

```text
VeryHigh confidence와 안전 조건이 충족된 경우에만 강한 정책 묶음을 검증한다.
```

검증 항목:

```text
activation condition
deactivation condition
aggressive mode duration
enabled policy list
rollback scope completeness
runtime validation blocker absence
```

판정 기준:

- Confidence < VeryHigh이면 활성화되면 안 된다.
- RuntimeValidation BLOCKER 발생 시 즉시 rollback 또는 safe fallback이 우선해야 한다.

### 5.5 Rollback Test

목적:

```text
정책 적용 후 원상 복구가 정확히 수행되는지 확인한다.
```

검증 항목:

```text
thread priority restored
thread affinity restored
process priority restored
background restriction restored
timer resolution restored if modified
failed state preserved if rollback fails
```

판정 기준:

- rollback 실패는 BLOCKER다.
- rollback 실패 상태는 폐기하지 않고 preserved state로 기록되어야 한다.

### 5.6 Long Soak Test

목적:

```text
장시간 실행 중 thread switch, migration, policy thrashing, evidence loss, rollback failure가 발생하지 않는지 검증한다.
```

검증 항목:

```text
policy activation count
policy rejection count
cooldown behavior
runtime validation summary
evidence flush
memory growth
CPU overhead
```

판정 기준:

- Long Soak 기본 시간은 TBD다.
- policy thrashing, evidence loss, runtime validation failure가 없어야 PASS 가능하다.

## 6. 측정 지표

### 6.1 필수 지표

```text
Main Thread Migration Count
Main Thread Switch Count
Main Thread Confidence History
Confidence Level Stability
Core Relocation Count
Policy Activation Count
Policy Rejection Count
Aggressive Mode Active Duration
RTT Jitter
DPC Spike Count
Rollback Result
Runtime Validation Summary
Evidence Completeness
```

필수 지표는 v3.1의 제품 목표와 직접 연결된다. 필수 지표가 누락되면 성능 성공으로 분류하지 않는다.

### 6.2 선택 지표

```text
FPS average
Frame Time average
Frame Time p95
Frame Time p99
1% Low
0.1% Low
Input latency external measurement
External benchmark result
```

주의사항:

```text
FPS/frame time 직접 측정 방식은 본 문서에서 확정하지 않는다.
내부 직접 측정이 불가능하면 외부 도구 결과를 Evidence 입력으로 받을 수 있다.
```

선택 지표는 성능 해석을 돕지만, rollback failure나 RuntimeValidation BLOCKER를 덮을 수 없다.

## 7. Before/After 비교 구조

Before/After 비교 구조는 다음 흐름을 따른다.

```text
Baseline Window
↓
Policy Apply
↓
Applied Window
↓
Comparison
↓
Classification
```

Baseline Window와 Applied Window의 기본 길이는 TBD다.

### 7.1 Baseline Window

목적:

```text
정책 적용 전 기본 상태를 관찰한다.
```

필수 기록:

```text
migration count
confidence history
RTT jitter
DPC spike count
thread switch count
core relocation count
runtime validation state
```

Baseline Window 중 mutation이 발생하면 해당 run은 INVALID_RUN 후보가 된다.

### 7.2 Applied Window

목적:

```text
정책 적용 후 상태를 관찰한다.
```

필수 기록:

```text
applied policy list
migration count
confidence history
RTT jitter
DPC spike count
rollback state saved
verify result
runtime validation state
```

Applied Window는 verify success 이후에 시작해야 한다.

### 7.3 Comparison

비교 항목:

```text
migration delta
confidence stability delta
switch count delta
RTT jitter delta
DPC spike delta
policy activation stability
rollback success
runtime validation blocker existence
```

주의사항:

```text
Baseline Window와 Applied Window가 없으면 성능 개선으로 분류하지 않는다.
```

Comparison은 개선, 중립, 회귀를 판단하기 위한 근거다. 단일 cycle의 우연한 변화는 성능 개선으로 분류하지 않는다.

## 8. 판정 등급

### 8.1 PASS

조건:

```text
필수 Evidence가 모두 존재한다.
RuntimeValidation BLOCKER가 없다.
Rollback이 성공했다.
주요 안정성 지표가 개선되거나 악화되지 않았다.
Before/After 비교가 가능하다.
```

PASS는 제한적이고 검증 가능한 개선 또는 안정적 비악화를 의미한다.

### 8.2 PASS_WITH_WARNINGS

조건:

```text
주요 안전 조건은 통과했다.
성능 지표 일부가 불명확하거나 개선 폭이 작다.
비핵심 Evidence 일부가 부족하다.
경미한 warning이 있으나 rollback과 runtime validation은 정상이다.
```

PASS_WITH_WARNINGS는 릴리즈 후보가 될 수 있지만 warning 해석이 Evidence에 남아야 한다.

### 8.3 NEUTRAL

조건:

```text
안전성 문제는 없다.
성능 개선이 통계적으로 불명확하다.
Before/After 결과가 큰 차이를 보이지 않는다.
```

NEUTRAL은 실패가 아니다. 목표 게임군과 정책 조건을 다시 검토해야 하는 결과다.

### 8.4 REGRESSION

조건:

```text
migration 증가
confidence instability 증가
RTT jitter 악화
DPC spike 증가
frame time 악화
policy thrashing 발생
```

REGRESSION은 정책 조정 또는 rollback/fallback 조건 재검토를 요구한다.

### 8.5 BLOCKER

조건:

```text
Rollback 실패
RuntimeValidation BLOCKER
identity mismatch 상태에서 mutation 시도
rollback state 없이 mutation
ApplyGuard 우회
Evidence write fatal
target process identity 검증 실패
Processor Group 정보 누락 상태에서 group 0 hardcoding 적용
```

성능 지표가 좋아져도 Rollback 실패 또는 RuntimeValidation BLOCKER가 있으면 PASS가 될 수 없다.

BLOCKER는 성능 결과보다 우선한다.

## 9. 테스트 시나리오

### 9.1 Baseline Observation Test

목적:

```text
정책 적용 없이 대상 게임의 thread/migration/confidence 지표를 수집한다.
```

기대 결과:

- ThreadTracker sample과 confidence input quality가 기록된다.
- mutation이 발생하지 않는다.
- Baseline Window가 생성된다.

### 9.2 Main Thread Confidence Test

목적:

```text
MainThreadConfidence가 Low/Medium/High/VeryHigh로 안정적으로 분류되는지 확인한다.
```

기대 결과:

- Confidence < High인 후보에는 affinity/priority 정책 후보가 생성되지 않는다.
- confidence reason이 Evidence로 남는다.

### 9.3 Core Isolation Candidate Test

목적:

```text
Core Isolation 후보가 안전 조건을 만족할 때만 생성되는지 확인한다.
```

기대 결과:

- Topology incomplete 또는 Processor Group unsupported path에서는 monitoring-only로 전환된다.
- CoreIsolationPlan과 fallback reason이 기록된다.

### 9.4 Main Thread Affinity Stabilization Test

목적:

```text
High 이상 confidence에서만 affinity stabilization 후보가 생성되고, migration 변화가 기록되는지 확인한다.
```

기대 결과:

- processorGroup과 KAFFINITY가 함께 기록된다.
- group 0 hardcoding이 발생하지 않는다.
- migration before/after가 비교 가능하다.

### 9.5 Background Restriction Safety Test

목적:

```text
protected process와 anti-cheat 의심 process가 restriction 대상에서 제외되는지 확인한다.
```

기대 결과:

- skipped process list와 skip reason이 기록된다.
- unsafe Processor Group 1+ process-wide restriction은 차단된다.

### 9.6 Aggressive Mode Gate Test

목적:

```text
Aggressive Single-Core Mode가 VeryHigh confidence와 안전 조건 없이는 활성화되지 않는지 확인한다.
```

기대 결과:

- VeryHigh 미만에서는 activation이 차단된다.
- RuntimeValidation BLOCKER가 있으면 aggressive mode가 우선하지 않는다.

### 9.7 Rollback Integrity Test

목적:

```text
적용한 모든 정책이 shutdown 또는 failure 시 원상 복구되는지 확인한다.
```

기대 결과:

- 저장된 rollback scope가 모두 처리된다.
- failed state가 있으면 preserved state로 기록된다.

### 9.8 RuntimeValidation BLOCKER Test

목적:

```text
RuntimeValidation BLOCKER 발생 시 aggressive mode보다 rollback/safe shutdown이 우선되는지 확인한다.
```

기대 결과:

- active policy가 있으면 RollbackPending으로 전이된다.
- BLOCKER는 release-facing severity로 기록된다.

### 9.9 Long Soak Test

목적:

```text
장시간 실행 중 evidence 누락, policy thrashing, memory growth, rollback state 손실이 없는지 확인한다.
```

기대 결과:

- policy activation/rejection count가 비정상적으로 증가하지 않는다.
- evidence flush와 runtime validation summary가 유지된다.
- rollback preserved state가 없어야 PASS 가능하다.

## 10. Evidence 요구사항

성능 검증 결과는 다음 Evidence를 포함해야 한다.

```text
testId
runId
targetProcess
gameCandidateName
mode
gitCommit
buildHash
exeSha256
startedUtc
finishedUtc
baselineWindow
appliedWindow
policyActivationSummary
mainThreadConfidenceHistory
migrationSummary
rttJitterSummary
dpcSpikeSummary
rollbackSummary
runtimeValidationSummary
classification
warnings
blockers
```

Evidence가 없으면 성능 성공으로 분류하지 않는다.

Evidence는 사람이 읽을 수 있는 summary와 기계 검증 가능한 structured fields를 모두 가져야 한다. 구체 schema는 `docs/evidence/EvidenceSpecification.md`에서 확정한다.

## 11. 실제 게임 검증 원칙

실제 게임 검증은 다음 원칙을 따른다.

```text
1. 동일 장소 또는 동일 시나리오에서 비교한다.
2. 동일 그래픽 설정을 사용한다.
3. 동일 네트워크 환경을 사용한다.
4. 테스트 중 백그라운드 조건을 기록한다.
5. 최소 2회 이상 반복한다.
6. 게임 업데이트 또는 서버 상태 변화는 결과 해석에 반영한다.
7. 체감 결과는 보조 기록으로만 사용한다.
```

체감 기록 항목:

```text
입력 반응성 느낌
스킬 사용 지연 느낌
로딩 중 끊김
마을/필드/전투 상황 차이
```

주의:

```text
체감 기록은 Performance Evidence를 대체하지 않는다.
```

실제 게임 검증은 대상 후보의 특성과 서버/네트워크 상태를 함께 기록해야 한다. 같은 게임이라도 업데이트, 서버 상태, anti-cheat 정책 변화에 따라 결과 해석이 달라질 수 있다.

## 12. 실패 처리

검증 중 실패 시 처리 기준은 다음과 같다.

```text
test setup failure:
- INVALID_RUN
- 결과 비교 대상에서 제외

target process exited:
- PARTIAL_RUN 또는 INVALID_RUN
- rollback 필요 여부 확인

rollback failure:
- BLOCKER

evidence missing:
- WARN 또는 BLOCKER 후보

runtime validation blocker:
- BLOCKER

access denied repeated:
- Monitoring-only 또는 WARN

anti-cheat suspicion:
- test stop
- mutation 금지
```

실패 처리는 결과를 좋게 보이기 위한 보정 절차가 아니다. 실패 원인을 Evidence로 남기고, 비교 불가능한 run은 INVALID_RUN 또는 PARTIAL_RUN으로 분리한다.

## 13. Non-Goals

다음은 PerformanceValidationPlan의 비목표다.

```text
구체 테스트 코드 작성
자동화 스크립트 구현
FPS 측정 도구 구현
UI 리포트 구현
게임별 공식 지원 선언
성능 향상 보장
AI/ML 기반 성능 분석
Anti-Cheat 우회 테스트
Kernel Driver 검증
DLL Injection 검증
Game Memory Patch 검증
```

본 문서는 검증 전략 문서다. 자동화 구현과 evidence schema 구현은 후속 문서와 구현 단계에서 다룬다.

## 14. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

```text
1. 성능 검증 목표가 정의되어 있다.
2. 검증 대상 게임군과 비대상 게임군이 구분되어 있다.
3. 검증 모드가 dry-run, soft-apply, apply, aggressive, rollback, soak로 정의되어 있다.
4. 필수 측정 지표와 선택 측정 지표가 구분되어 있다.
5. Baseline Window와 Applied Window 구조가 정의되어 있다.
6. PASS / PASS_WITH_WARNINGS / NEUTRAL / REGRESSION / BLOCKER 기준이 정의되어 있다.
7. 실제 게임 검증 원칙이 포함되어 있다.
8. Evidence 요구사항이 정의되어 있다.
9. Rollback 실패와 RuntimeValidation BLOCKER가 PASS를 막는다는 원칙이 포함되어 있다.
10. 후속 EvidenceSpecification.md 작성에 필요한 필드 기준이 충분하다.
```

Acceptance Criteria는 이 문서의 승인 기준이다. 성능 개선 보장이나 release approval을 의미하지 않는다.

## 15. Open Questions

1. 첫 번째 실제 검증 게임은 무엇으로 둘 것인가?
2. Baseline Window와 Applied Window의 기본 길이는 몇 분으로 둘 것인가?
3. Long Soak Test의 기본 시간은 30분, 60분, 90분 중 무엇으로 둘 것인가?
4. FPS/frame time 측정은 외부 도구 결과를 입력받을 것인가?
5. frame time p95/p99를 v3.1 필수 지표로 둘 것인가, 선택 지표로 둘 것인가?
6. 성능 개선 판정에서 migration 감소와 frame time 개선 중 무엇을 더 높은 우선순위로 둘 것인가?
7. PASS_WITH_WARNINGS와 NEUTRAL의 경계는 어디서 확정할 것인가?
8. 실제 게임 검증 결과를 Release Gate에 필수로 포함할 것인가?
9. 체감 기록은 Evidence bundle의 어느 위치에 포함할 것인가?
