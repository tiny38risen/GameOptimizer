# GameOptimizer v3.1 Runtime State Machine

## 1. 문서 개요

문서명: GameOptimizer v3.1 Runtime State Machine

버전: v1.0

작성 목적: GameOptimizer v3.1 실행 중 런타임 상태 머신(Runtime State Machine)을 정의한다. 각 상태(State)의 목적, 진입 조건(Entry Condition), 허용 동작, 금지 동작, 탈출 조건(Exit Condition), 전이(Transition), fallback, 롤백(Rollback), 성능 증거(Performance Evidence) 요구사항을 정의한다.

적용 범위:

- Runtime state 목록
- 상태별 허용/금지 동작
- 상태 전이 조건
- mutation 허용 경계
- shutdown 상태 전이
- Runtime Validation BLOCKER 처리
- Monitoring-only 전환
- Rollback failure preservation
- 상태 전이 Evidence 요구사항

비적용 범위:

- C++ enum 최종 구현
- 구체 클래스 시그니처 확정
- threading primitive 구현 방식 확정
- UI 상태 표시 설계
- 로그 포맷 최종 확정
- Release Gate 스크립트 구현
- 상세 테스트 코드 작성

상위 문서:

- `docs/architecture/SAD_v1.0.md`
- `docs/performance/PolicySpecification.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/vision/PVD_v1.0.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Evidence_Schema.md`
- `docs/release/Release_Gate_Spec.md`

후속 문서:

- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-008_RuntimeOrchestrator.md`
- `docs/modules/MDS-009_TrackingWatchdog.md`
- `docs/modules/MDS-010_PolicyDispatcher.md`
- `docs/modules/MDS-011_RollbackManager.md`
- `docs/modules/MDS-012_EvidenceRecorder.md`

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

본 문서는 코드 구현 문서가 아니다. 상태 이름은 구현 enum 확정을 의미하지 않으며, 후속 MDS와 구현 단계에서 구체 타입을 확정한다.

## 2. Runtime State Machine 목적

Runtime State Machine의 목적은 성능 기능을 더 공격적으로 만들기 위한 것이 아니라, 성능 정책이 안전한 상태에서만 적용되도록 실행 경계를 정의하는 것이다.

본 문서의 목적은 다음과 같다.

1. 런타임 상태를 명확히 분리한다.
2. 상태별 허용 동작과 금지 동작을 정의한다.
3. 정책 적용 가능한 상태와 불가능한 상태를 구분한다.
4. rollback이 필요한 상태를 명확히 한다.
5. shutdown 중 신규 mutation을 방지한다.
6. Evidence flush와 release-facing 상태 기록 기준을 정의한다.

핵심 전제는 다음과 같다.

- 상태가 불명확한 상태에서 정책 적용을 수행하면 안 된다.
- Performance Engine은 직접 Win32 제어 API를 호출하지 않는다.
- Policy Layer는 직접 mutation을 수행하지 않는다.
- 실제 mutation은 담당 Controller가 ApplyGuard와 RollbackManager 계약을 통과한 경우에만 수행한다.
- Rollback 없는 mutation은 허용하지 않는다.
- Evidence 없는 정책 성공 판정은 허용하지 않는다.
- RuntimeValidation BLOCKER는 aggressive mode보다 우선한다.
- target process 종료, identity mismatch, access denied 반복은 안전 전이 조건으로 처리한다.
- shutdown 중 신규 정책 적용은 금지한다.

## 3. 상태 목록

v3.1 Runtime State Machine은 다음 상태를 정의한다.

```text
Uninitialized
Initializing
Observing
Evaluating
PolicyPending
PolicyRejected
Applying
Verifying
Running
MonitoringOnly
RollbackPending
RollbackFailedPreserved
ShutdownRequested
ShuttingDown
Terminated
FatalError
```

각 상태는 다음 항목을 가진다.

```text
상태명
목적
진입 조건
허용 동작
금지 동작
탈출 조건
Evidence 요구사항
다음 가능 상태
```

상태 전이는 Evidence를 남길 수 있어야 한다. Evidence를 남길 수 없는 성능 성공 판정은 허용하지 않는다.

## 4. 상태별 상세 정의

### 4.1 Uninitialized

상태명: `Uninitialized`

목적:

```text
프로그램 객체는 생성되었지만 runtime context, topology, target process, rollback/evidence 상태가 아직 준비되지 않은 상태.
```

진입 조건:

- process start 직후
- RuntimeOrchestrator 생성 전 또는 초기 field 준비 전

허용 동작:

```text
CLI parsing
configuration loading
static validation 준비
```

금지 동작:

```text
target process mutation
thread sampling
policy generation
rollback execution
```

탈출 조건:

- CLI와 기본 configuration 준비 성공
- 치명적인 startup parameter 오류 발생

Evidence 요구사항:

- startup event
- CLI parse result
- configuration load result

다음 가능 상태:

```text
Initializing
FatalError
```

### 4.2 Initializing

상태명: `Initializing`

목적:

```text
target process 탐색, topology 분석, runtime context 구성, evidence session 초기화를 수행하는 상태.
```

진입 조건:

- `Uninitialized`에서 startup 준비 완료
- restart가 아닌 fresh runtime initialization

허용 동작:

```text
target process discovery
TopologyAnalyzer 실행
RuntimeContext 초기화
Evidence session 생성
StartupPipeline 실행
```

금지 동작:

```text
Performance policy 적용
Aggressive mode 활성
background restriction 적용
thread priority/affinity mutation
```

탈출 조건:

```text
초기화 성공 → Observing
target process 없음 → MonitoringOnly 또는 Terminated
필수 초기화 실패 → FatalError 또는 Safe Shutdown
```

Evidence 요구사항:

```text
startup result
target process identity
topology result
processor group capability
evidence session id
```

다음 가능 상태:

```text
Observing
MonitoringOnly
Terminated
FatalError
```

### 4.3 Observing

상태명: `Observing`

목적:

```text
시스템 상태를 관찰하고 thread/runtime/latency sample을 수집하는 기본 상태.
```

진입 조건:

- initialization 성공
- rollback 성공 후 관찰 재개
- monitoring-only 조건 회복

허용 동작:

```text
ThreadTracker sampling
Latency sensor sampling
DPC/RTT observation
RuntimeValidation sampling
PerformanceEngine 분석 준비
```

금지 동작:

```text
Win32 mutation 직접 수행
rollback state 없는 policy apply
aggressive mode 즉시 적용
```

탈출 조건:

```text
충분한 sample 수집 → Evaluating
target process 종료 → ShutdownRequested 또는 Terminated
access denied 반복 → MonitoringOnly
runtime validation BLOCKER → RollbackPending 또는 FatalError
```

Evidence 요구사항:

```text
sample count
main thread candidate
confidence raw signal
latency sample summary
access failure count
```

다음 가능 상태:

```text
Evaluating
MonitoringOnly
RollbackPending
ShutdownRequested
Terminated
FatalError
```

### 4.4 Evaluating

상태명: `Evaluating`

목적:

```text
Performance Engine과 Policy Layer가 관찰 데이터를 분석하여 정책 후보를 만들 수 있는지 평가하는 상태.
```

진입 조건:

- Observing에서 충분한 sample 확보
- cooldown 또는 monitoring-only 복귀 조건 충족

허용 동작:

```text
MainThreadConfidence 계산
CoreIsolationPlan 후보 생성
CCXStability 판단
Aggressive mode 후보 판단
PolicyCandidate 생성
```

금지 동작:

```text
SetThreadAffinityMask 호출
SetThreadGroupAffinity 호출
SetThreadPriority 호출
SetPriorityClass 호출
RollbackManager 직접 호출
ApplyGuard 직접 생성
```

탈출 조건:

```text
유효한 정책 후보 생성 → PolicyPending
정책 조건 미충족 → PolicyRejected 또는 MonitoringOnly
runtime validation BLOCKER → RollbackPending
```

Evidence 요구사항:

```text
confidence level
candidate policy type
activation reason
rejection reason
monitoring-only reason
```

다음 가능 상태:

```text
PolicyPending
PolicyRejected
MonitoringOnly
RollbackPending
ShutdownRequested
```

### 4.5 PolicyPending

상태명: `PolicyPending`

목적:

```text
정책 후보가 생성되었고, PolicyResolver 또는 PolicyDispatcher가 적용 가능성을 검증하는 상태.
```

진입 조건:

- Evaluating에서 Policy Candidate 생성
- Running 중 deactivation 또는 secondary policy candidate 생성

허용 동작:

```text
policy type validation
cooldown validation
risk level validation
rollback scope validation
evidence field validation
required controller validation
```

금지 동작:

```text
검증 전 Controller 호출
rollback scope 없는 정책 전달
evidence field 없는 정책 성공 처리
```

탈출 조건:

```text
검증 성공 → Applying
검증 실패 → PolicyRejected
충돌 발생 → MonitoringOnly 또는 PolicyRejected
```

Evidence 요구사항:

```text
policy candidate
validation result
conflict result
cooldown result
risk level
required rollback scope
```

다음 가능 상태:

```text
Applying
PolicyRejected
MonitoringOnly
ShutdownRequested
```

### 4.6 PolicyRejected

상태명: `PolicyRejected`

목적:

```text
정책 후보가 안전 조건, cooldown, confidence, topology, rollback scope, evidence 조건을 만족하지 못해 거부된 상태.
```

진입 조건:

- PolicyPending validation 실패
- Evaluating에서 policy condition 미충족
- policy conflict 또는 cooldown 위반

허용 동작:

```text
rejection evidence 기록
monitoring-only 전환
다음 cycle 대기
```

금지 동작:

```text
거부된 정책의 부분 적용
fallback 없는 재시도 루프
silent failure
```

탈출 조건:

```text
관찰 계속 → Observing
안전상 적용 불가 지속 → MonitoringOnly
shutdown 요청 → ShutdownRequested
```

Evidence 요구사항:

```text
rejected policy type
rejection reason
confidence level
topology state
rollback scope state
evidence field state
```

다음 가능 상태:

```text
Observing
MonitoringOnly
ShutdownRequested
```

### 4.7 Applying

상태명: `Applying`

목적:

```text
담당 Controller가 ApplyGuard와 RollbackManager 계약에 따라 정책 적용을 시작하는 상태.
```

진입 조건:

- PolicyPending validation 성공
- required controller, rollback scope, evidence fields가 모두 지정됨
- scheduler mode가 apply 허용 상태

허용 동작:

```text
rollback state save
ApplyGuard arm
Controller mutation
apply result 기록
```

금지 동작:

```text
rollback state 저장 전 mutation
ApplyGuard arm 전 mutation
Performance Engine 직접 mutation
Policy Layer 직접 mutation
```

탈출 조건:

```text
apply 성공 → Verifying
apply 실패 → RollbackPending
target process identity mismatch → RollbackPending
```

Evidence 요구사항:

```text
controller name
policy type
rollbackStateSaved
applyResult
target identity
```

다음 가능 상태:

```text
Verifying
RollbackPending
ShutdownRequested
FatalError
```

### 4.8 Verifying

상태명: `Verifying`

목적:

```text
적용된 정책이 실제로 원하는 상태가 되었는지 검증하는 상태.
```

진입 조건:

- Applying에서 apply 성공
- Controller가 post-apply query를 수행할 수 있음

허용 동작:

```text
applied state query
affinity/priority verification
timer state verification
background restriction verification
verify result 기록
```

금지 동작:

```text
verify 실패 무시
verify 없이 commit
부분 실패를 성공으로 분류
```

탈출 조건:

```text
verify 성공 → Running
verify 실패 → RollbackPending
runtime validation BLOCKER → RollbackPending
```

Evidence 요구사항:

```text
verifyResult
expected state
actual state
mismatch reason
```

다음 가능 상태:

```text
Running
RollbackPending
ShutdownRequested
```

### 4.9 Running

상태명: `Running`

목적:

```text
정책이 검증되어 적용된 상태에서 runtime monitoring을 계속하는 상태.
```

진입 조건:

- Verifying에서 verify 성공
- ApplyGuard commit 가능
- RuntimeValidation BLOCKER 없음

허용 동작:

```text
Performance Evidence 수집
RuntimeValidationMonitor 실행
Before/After 비교 구간 기록
cooldown 관리
deactivation condition 감시
```

금지 동작:

```text
증거 없이 성능 성공 판정
동일 정책 무제한 반복 적용
shutdown 중 신규 policy apply
```

탈출 조건:

```text
deactivation condition 발생 → RollbackPending 또는 MonitoringOnly
runtime validation BLOCKER → RollbackPending
target process 종료 → ShutdownRequested
shutdown 요청 → ShutdownRequested
```

Evidence 요구사항:

```text
active policy list
active duration
before/after summary
runtime validation state
deactivation condition
```

다음 가능 상태:

```text
Observing
Evaluating
RollbackPending
MonitoringOnly
ShutdownRequested
```

### 4.10 MonitoringOnly

상태명: `MonitoringOnly`

목적:

```text
안전 조건이 부족하거나 적용 가치가 낮아 관찰만 수행하는 상태.
```

진입 조건:

```text
Confidence 부족
Topology 불완전
Processor Group 제한
Access Denied 반복
Anti-Cheat boundary suspicion
rollback state 저장 불가능
evidence field 부족
```

허용 동작:

```text
thread/latency/runtime observation
evidence 기록
사용자 안내 또는 로그 출력
```

금지 동작:

```text
thread/process mutation
aggressive mode 활성
background restriction 적용
registry 변경
```

탈출 조건:

```text
조건 회복 → Observing
shutdown 요청 → ShutdownRequested
target process 종료 → Terminated
```

Evidence 요구사항:

```text
monitoringOnlyReason
blocked policy type
confidence state
topology state
access denied count
```

다음 가능 상태:

```text
Observing
ShutdownRequested
Terminated
FatalError
```

### 4.11 RollbackPending

상태명: `RollbackPending`

목적:

```text
적용 실패, 검증 실패, runtime validation failure, shutdown, target process 종료 등에 의해 rollback이 필요한 상태.
```

진입 조건:

- Applying 실패
- Verifying 실패
- Running 중 RuntimeValidation BLOCKER
- ShutdownRequested에서 active mutation 존재
- target process 종료 또는 identity mismatch

허용 동작:

```text
rollback scope 확인
RollbackManager 실행
Controller별 rollback 요청
rollback result 기록
failed state preservation
```

금지 동작:

```text
신규 policy apply
aggressive mode 유지
rollback 실패를 WARN/INFO로 낮춤
rollback 없이 Terminated 전이
```

탈출 조건:

```text
rollback 성공 → Observing 또는 ShuttingDown
rollback 실패 + state preserved → RollbackFailedPreserved
rollback 실패 + fatal → FatalError
```

Evidence 요구사항:

```text
rollback reason
rollback scope
rollback result
preserved state count
failed rollback state
```

다음 가능 상태:

```text
Observing
ShuttingDown
RollbackFailedPreserved
FatalError
```

### 4.12 RollbackFailedPreserved

상태명: `RollbackFailedPreserved`

목적:

```text
rollback이 실패했지만 실패 상태를 보존하여 shutdown retry 또는 release-facing evidence로 넘기는 상태.
```

진입 조건:

- RollbackPending에서 rollback failure 발생
- 실패 상태를 RollbackManager가 보존함
- shutdown retry 또는 release-facing BLOCKER 기록 필요

허용 동작:

```text
failed state preservation
shutdown retry 준비
release-facing BLOCKER 후보 기록
사용자 경고 로그 기록
```

금지 동작:

```text
성공 종료 처리
rollback failure를 무시
state 폐기
```

탈출 조건:

```text
shutdown retry → ShuttingDown
fatal 판단 → FatalError
```

Evidence 요구사항:

```text
failed rollback target
preserved state
retry eligibility
release severity
```

다음 가능 상태:

```text
ShuttingDown
FatalError
```

### 4.13 ShutdownRequested

상태명: `ShutdownRequested`

목적:

```text
사용자 종료, Ctrl+C, timeout, target process 종료 등으로 종료 요청이 들어온 상태.
```

진입 조건:

- 사용자 Ctrl+C
- timeout
- target process 종료
- normal completion
- fatal path에서 safe shutdown 가능

허용 동작:

```text
신규 정책 생성 차단
watchdog safe point 대기
active policy 확인
rollback 필요 여부 판단
evidence flush 준비
```

금지 동작:

```text
신규 policy apply
aggressive mode 신규 활성
background restriction 신규 적용
```

탈출 조건:

```text
active mutation 있음 → RollbackPending
active mutation 없음 → ShuttingDown
```

Evidence 요구사항:

```text
shutdown reason
active policy count
rollback required
watchdog safe point reached
```

다음 가능 상태:

```text
RollbackPending
ShuttingDown
FatalError
```

### 4.14 ShuttingDown

상태명: `ShuttingDown`

목적:

```text
남은 rollback, recorder flush, final snapshot, release evidence 기록을 수행하는 상태.
```

진입 조건:

- ShutdownRequested에서 active mutation 없음
- RollbackPending에서 rollback 성공
- RollbackFailedPreserved에서 shutdown retry 필요

허용 동작:

```text
final rollback retry
runtime snapshot final write
timeline flush
evidence bundle flush
controller stop
```

금지 동작:

```text
신규 sample cycle 시작
신규 policy candidate 생성
신규 mutation
```

탈출 조건:

```text
정상 종료 → Terminated
rollback/evidence fatal → FatalError
```

Evidence 요구사항:

```text
final snapshot
shutdown result
rollback summary
evidence flush result
runtime validation summary
```

다음 가능 상태:

```text
Terminated
FatalError
```

### 4.15 Terminated

상태명: `Terminated`

목적:

```text
프로그램이 정상 또는 안전 종료를 완료한 상태.
```

진입 조건:

- ShuttingDown 완료
- MonitoringOnly에서 target process 종료와 mutation 없음
- FatalError에서 최소 로그 후 종료

허용 동작:

```text
종료 코드 반환
final evidence path 출력
```

금지 동작:

```text
thread sampling
policy apply
rollback retry
```

탈출 조건:

- 없음. Terminal state다.

Evidence 요구사항:

```text
exit code
final status
evidence location
```

다음 가능 상태:

```text
없음
```

### 4.16 FatalError

상태명: `FatalError`

목적:

```text
안전하게 계속 실행하거나 rollback/evidence 계약을 만족할 수 없는 치명 상태.
```

진입 조건:

```text
필수 초기화 실패
rollback fatal failure
evidence write fatal failure
runtime contract violation
unrecoverable identity mismatch
```

허용 동작:

```text
fatal evidence 기록 시도
최소 로그 출력
안전 종료
```

금지 동작:

```text
정책 재시도
aggressive mode 유지
정상 종료로 위장
```

탈출 조건:

- 최소 evidence/log 처리 후 종료

Evidence 요구사항:

```text
fatal reason
last known state
rollback state
evidence write result
exit code
```

다음 가능 상태:

```text
Terminated
```

## 5. 상태 전이 표

| From | Event / Condition | To | Required Action | Evidence |
|---|---|---|---|---|
| Uninitialized | start requested | Initializing | load config and prepare startup | startup event |
| Uninitialized | invalid startup parameter | FatalError | record fatal reason | fatal reason |
| Initializing | init success | Observing | start watchdog and sensors | startup result |
| Initializing | required init failed | FatalError | stop startup and write failure | startup failure |
| Initializing | target process unavailable | MonitoringOnly | record unavailable target | monitoring-only reason |
| Observing | enough samples | Evaluating | analyze samples | sample summary |
| Observing | access denied repeated | MonitoringOnly | block policy generation | access failure count |
| Observing | runtime validation BLOCKER | RollbackPending | stop policy creation | runtime validation state |
| Observing | target process exited | ShutdownRequested | request safe shutdown | shutdown reason |
| Evaluating | valid policy candidate | PolicyPending | validate candidate | policy candidate |
| Evaluating | policy condition not met | PolicyRejected | record rejection | rejection reason |
| Evaluating | safety condition insufficient | MonitoringOnly | switch to observation-only | monitoring-only reason |
| Evaluating | runtime validation BLOCKER | RollbackPending | cancel evaluation | runtime validation state |
| PolicyPending | validation success | Applying | handoff to controller | validation result |
| PolicyPending | validation failed | PolicyRejected | reject policy | validation result |
| PolicyPending | conflict or cooldown violation | MonitoringOnly | block policy | conflict result |
| Applying | apply success | Verifying | query applied state | apply result |
| Applying | apply failed | RollbackPending | rollback affected scope | apply result |
| Applying | identity mismatch | RollbackPending | rollback or preserve state | target identity |
| Verifying | verify success | Running | commit guarded transaction | verify result |
| Verifying | verify failed | RollbackPending | rollback affected scope | mismatch reason |
| Verifying | runtime validation BLOCKER | RollbackPending | rollback affected scope | runtime validation state |
| Running | deactivation condition | RollbackPending | rollback active policy | deactivation condition |
| Running | runtime validation BLOCKER | RollbackPending | rollback active policy | runtime validation state |
| Running | shutdown requested | ShutdownRequested | stop new policy apply | shutdown reason |
| Running | target process exited | ShutdownRequested | stop runtime loop | target identity |
| MonitoringOnly | conditions recovered | Observing | resume observation cycle | recovery reason |
| MonitoringOnly | shutdown requested | ShutdownRequested | prepare shutdown | shutdown reason |
| MonitoringOnly | target process exited | Terminated | exit without mutation | final status |
| RollbackPending | rollback success and continue | Observing | resume observation | rollback result |
| RollbackPending | rollback success and shutdown | ShuttingDown | final flush | rollback result |
| RollbackPending | rollback failed with preserved state | RollbackFailedPreserved | preserve state and classify | preserved state count |
| RollbackPending | rollback fatal | FatalError | record fatal rollback | rollback result |
| RollbackFailedPreserved | shutdown retry | ShuttingDown | retry or flush evidence | retry eligibility |
| RollbackFailedPreserved | unrecoverable | FatalError | record fatal reason | release severity |
| ShutdownRequested | active mutation exists | RollbackPending | rollback active state | rollback required |
| ShutdownRequested | no active mutation | ShuttingDown | flush and stop | shutdown reason |
| ShuttingDown | shutdown complete | Terminated | return exit code | shutdown result |
| ShuttingDown | rollback/evidence fatal | FatalError | record fatal path | fatal reason |
| FatalError | final log/evidence attempted | Terminated | return failure code | exit code |

## 6. 상태별 Mutation 허용 규칙

| State | Policy Candidate 생성 | Win32 Mutation | Rollback | Evidence Write |
|---|---:|---:|---:|---:|
| Uninitialized | No | No | No | Limited |
| Initializing | No | No | No | Yes |
| Observing | No | No | No | Yes |
| Evaluating | Yes | No | No | Yes |
| PolicyPending | No | No | No | Yes |
| PolicyRejected | No | No | No | Yes |
| Applying | No | Yes, guarded | Yes if fail | Yes |
| Verifying | No | No | Yes if fail | Yes |
| Running | Conditional | No new mutation without new cycle | Yes if deactivation | Yes |
| MonitoringOnly | No | No | No | Yes |
| RollbackPending | No | Rollback only | Yes | Yes |
| RollbackFailedPreserved | No | Rollback retry only | Yes, retry path | Yes |
| ShutdownRequested | No | No new mutation | Conditional | Yes |
| ShuttingDown | No | Rollback retry only | Conditional | Yes |
| Terminated | No | No | No | No |
| FatalError | No | No new mutation | Best-effort only | Best-effort |

필수 원칙:

```text
1. Win32 mutation은 Applying 상태에서만 가능하다.
2. Applying 상태에서도 ApplyGuard와 RollbackManager 계약이 필요하다.
3. Verifying 상태에서는 추가 mutation이 아니라 검증만 수행한다.
4. ShutdownRequested 이후 신규 mutation은 금지한다.
5. RollbackPending에서는 rollback 목적의 mutation만 허용한다.
```

Running 상태에서 새 정책 적용이 필요하면 반드시 Evaluating 또는 PolicyPending 흐름을 새로 거쳐야 한다.

## 7. Shutdown 상태 전이 규칙

종료 트리거:

```text
사용자 Ctrl+C
timeout
target process 종료
runtime validation BLOCKER
fatal error
normal completion
```

종료 원칙:

```text
1. 종료 요청 시 신규 정책 후보 생성을 중단한다.
2. watchdog safe point에서 종료한다.
3. active mutation이 있으면 RollbackPending으로 이동한다.
4. rollback 후 ShuttingDown으로 이동한다.
5. final evidence flush 후 Terminated로 이동한다.
```

ShutdownRequested 이후에는 성능 개선보다 원상 복구와 Evidence flush가 우선한다.

ShutdownRequested 이후에는 Performance Engine, Policy Layer, PolicyDispatcher가 신규 성능 정책 후보를 생성하거나 적용 요청을 전달하지 않는다. 이미 저장된 rollback state와 active policy state만 shutdown 판단에 사용한다.

## 8. RuntimeValidation BLOCKER 처리

RuntimeValidation BLOCKER 발생 시 처리 규칙은 다음과 같다.

```text
1. RuntimeValidation BLOCKER는 aggressive mode보다 우선한다.
2. Running 상태에서 BLOCKER 발생 시 RollbackPending으로 전이한다.
3. Observing/Evaluating 상태에서 BLOCKER 발생 시 정책 생성을 중단한다.
4. BLOCKER는 Evidence에 release-facing severity로 기록한다.
5. BLOCKER 상태에서 신규 aggressive policy 후보를 생성하지 않는다.
```

RuntimeValidation BLOCKER는 단순 경고가 아니다. active mutation이 있으면 rollback 경로로 연결하고, active mutation이 없더라도 신규 policy apply를 차단한다.

## 9. Monitoring-only 전환 규칙

MonitoringOnly 상태로 전환되는 필수 조건은 다음과 같다.

```text
MainThreadConfidence Low 또는 Medium
Topology incomplete
Processor Group unsupported path
Access Denied 반복
Anti-Cheat boundary suspicion
Rollback state 저장 불가능
Evidence field 부족
Raw Input thread 불확실
```

MonitoringOnly 상태의 의미:

```text
실패가 아니라 안전한 비적용 상태다.
```

MonitoringOnly 상태에서도 observation과 Evidence 기록은 계속될 수 있다. 단, mutation, aggressive mode, background restriction 신규 적용은 금지한다.

## 10. Rollback 실패 처리

Rollback 실패 처리 규칙은 다음과 같다.

```text
1. Rollback 실패를 WARN/INFO로 낮추지 않는다.
2. 실패한 rollback state는 폐기하지 않는다.
3. RollbackFailedPreserved 상태로 전이할 수 있다.
4. shutdown retry 또는 release-facing BLOCKER 후보로 기록한다.
5. 정상 종료로 위장하지 않는다.
```

Rollback failure가 발생하면 Runtime State Machine은 failed state preservation을 우선한다. ApplyGuard 또는 RollbackManager가 final retry responsibility를 ShutdownPipeline으로 넘기는 경우에도 Evidence는 보존되어야 한다.

## 11. Evidence 요구사항

모든 상태 전이는 Evidence를 남길 수 있어야 한다.

필수 Evidence 항목:

```text
stateBefore
stateAfter
transitionReason
policyType
runtimeValidationState
rollbackRequired
rollbackResult
monitoringOnlyReason
fatalReason
timestamp
cycleId
```

Evidence 누락 시:

```text
성능 성공으로 분류하지 않는다.
release-facing WARN 또는 BLOCKER 후보가 된다.
```

Evidence는 상태 전이를 설명하는 최소 추적성이다. 상태 전이가 발생했는데 reason이나 result가 없으면 후속 release validation에서 판단 가능한 근거가 부족해진다.

## 12. Non-Goals

다음은 RuntimeStateMachine.md의 비목표다.

```text
C++ enum 최종 구현
구체 클래스 시그니처 확정
threading primitive 구현 방식 확정
UI 상태 표시 설계
로그 포맷 최종 확정
Release Gate 스크립트 구현
상세 테스트 코드 작성
```

본 문서는 상태와 전이 계약을 정의한다. 실제 enum, class, logger field, artifact schema 변경은 후속 MDS와 구현 단계에서 확정한다.

## 13. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

```text
1. Runtime 상태 목록이 정의되어 있다.
2. 각 상태의 목적, 진입 조건, 허용 동작, 금지 동작, 탈출 조건이 정의되어 있다.
3. 상태 전이 표가 포함되어 있다.
4. 상태별 mutation 허용 규칙이 정의되어 있다.
5. ShutdownRequested 이후 신규 mutation 금지 원칙이 포함되어 있다.
6. RuntimeValidation BLOCKER 처리 규칙이 정의되어 있다.
7. MonitoringOnly 상태의 의미와 전환 조건이 정의되어 있다.
8. Rollback 실패 보존 상태가 정의되어 있다.
9. Evidence 요구사항이 정의되어 있다.
10. 후속 MDS 문서 작성에 필요한 런타임 상태 기준이 충분하다.
```

Acceptance Criteria는 이 문서의 승인 기준이다. 구현 완료 또는 RC 승인 기준으로 해석하지 않는다.

## 14. Open Questions

1. RuntimeState enum을 실제 코드에 도입할 것인가, 문서상 상태로만 유지할 것인가?
2. RuntimeStateMachine의 소유자는 RuntimeOrchestrator인가, 별도 RuntimeStateController인가?
3. PolicyPending과 Evaluating을 코드상 별도 상태로 둘 것인가?
4. RollbackFailedPreserved 상태를 release-facing BLOCKER로 즉시 분류할 것인가?
5. MonitoringOnly에서 다시 Observing으로 복귀하는 조건은 어느 정도로 엄격하게 둘 것인가?
6. ShutdownRequested 이후 watchdog safe point 대기 최대 시간은 어디서 정의할 것인가?
7. 상태 전이 Evidence는 PolicyTimelineRecorder가 담당할 것인가, 별도 RuntimeStateRecorder를 둘 것인가?
8. FatalError와 Terminated의 exit code mapping은 어디서 정의할 것인가?
