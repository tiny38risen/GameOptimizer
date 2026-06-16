# MDS-009 RuntimeOrchestrator

## 1. 문서 개요

문서명: MDS-009 RuntimeOrchestrator

버전: v1.0

대상 모듈: RuntimeOrchestrator

작성 목적: 프로그램 실행 흐름, StartupPipeline, Watchdog, ShutdownPipeline, RuntimeStateMachine 흐름을 조립하는 Application/Runtime 경계 모듈의 책임을 정의한다.

적용 범위: runtime context 구성, startup pipeline 실행, watchdog lifecycle 관리, shutdown signal 처리, safe point shutdown 조정, final evidence flush 흐름 조립

비적용 범위: 세부 scheduling mutation, ThreadTracker 내부 상태 조작, RollbackManager state 직접 변조, PolicyCandidate 직접 생성

상위 문서:

- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Evidence_Schema.md`

관련 모듈:

- StartupPipeline
- ShutdownPipeline
- WatchdogCycleRunner
- TrackingWatchdog
- RuntimeContext
- RuntimeSignalState
- RuntimeValidationMonitor

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

RuntimeOrchestrator의 목적은 GameOptimizer runtime의 시작, 실행, 종료 흐름을 조립하고 RuntimeStateMachine의 안전 전이 원칙을 유지하는 것이다.

RuntimeOrchestrator는 조립자다. 세부 정책 판단과 mutation 소유권을 가져가면 안 된다.

## 3. 책임 범위

이 모듈의 책임:

1. runtime context 구성 흐름 조립
2. startup pipeline 실행
3. watchdog lifecycle 관리
4. shutdown signal 처리
5. watchdog safe point shutdown 조정
6. final evidence flush 흐름 조립
7. exit code 반환 흐름 관리

이 모듈의 비책임:

1. 세부 scheduling mutation 직접 수행
2. ThreadTracker 내부 상태 직접 조작
3. RollbackManager state 직접 변조
4. PolicyCandidate 직접 생성
5. PerformanceEngine 내부 판단 수행
6. release final decision 직접 생성

## 4. 입력

Input: argc/argv

- Source: process entry
- Required: Yes
- Validation: StartupPipeline에서 CLI parse
- Missing behavior: FatalError 또는 usage result

Input: startup pipeline result

- Source: StartupPipeline
- Required: Yes
- Validation: RuntimeContext 또는 failure reason 필요
- Missing behavior: FatalError

Input: shutdown signal

- Source: RuntimeSignalState / OS signal / timeout
- Required: Yes
- Validation: reason-coded signal이어야 함
- Missing behavior: safe shutdown reason unknown

Input: watchdog result

- Source: WatchdogCycleRunner / TrackingWatchdog
- Required: Yes
- Validation: runtime validation status와 shutdown reason 필요
- Missing behavior: runtime failure evidence

## 5. 출력

Output: startup result

- Consumer: logs/evidence
- Meaning: runtime initialization 결과
- 신뢰도: StartupPipeline result에 의존
- Failure behavior: FatalError

Output: runtime result

- Consumer: ShutdownPipeline, Evidence Layer
- Meaning: watchdog loop 결과
- 신뢰도: runtime validation summary에 의존
- Failure behavior: shutdown with failure reason

Output: shutdown result

- Consumer: exit code mapping, Evidence Layer
- Meaning: rollback/evidence flush 결과
- 신뢰도: ShutdownPipeline result
- Failure behavior: FatalError or non-zero exit

Output: final evidence path

- Consumer: user/log/release validation
- Meaning: final evidence artifact location
- 신뢰도: evidence flush success에 의존
- Failure behavior: missing evidence warning/blocker candidate

Output: exit code

- Consumer: OS/process caller
- Meaning: final runtime result
- 신뢰도: shutdown result mapping
- Failure behavior: failure exit code

## 6. 내부 상태

State: RuntimeContext ownership

- 목적: runtime component lifecycle 관리
- 수명: process runtime
- 초기화 시점: StartupPipeline success
- 갱신 시점: runtime lifecycle events
- 보존 여부: no, summary evidence only

State: RuntimeSignalState

- 목적: shutdown reason과 signal 상태 관리
- 수명: process runtime
- 초기화 시점: RuntimeOrchestrator start
- 갱신 시점: signal/timeout/runtime failure
- 보존 여부: shutdown evidence

State: current runtime phase

- 목적: RuntimeStateMachine 전이 추적
- 수명: process runtime
- 초기화 시점: Uninitialized
- 갱신 시점: startup/watchdog/shutdown
- 보존 여부: runtime state evidence

## 7. 소유권 규칙

RuntimeOrchestrator는 runtime lifecycle orchestration을 소유한다.

RuntimeOrchestrator는 Controller mutation, ThreadTracker observation internals, PolicyCandidate generation, RollbackManager state internals를 소유하지 않는다.

RuntimeOrchestrator는 ShutdownPipeline이 final rollback attempt를 수행하도록 연결한다.

## 8. 허용 동작

- StartupPipeline 호출
- RuntimeContext lifecycle 관리
- WatchdogCycleRunner 시작/종료
- shutdown signal reason 기록
- ShutdownPipeline 호출
- final evidence path 출력
- exit code 반환

## 9. 금지 동작

- 세부 scheduling mutation 직접 수행 금지
- ThreadTracker 내부 상태 직접 조작 금지
- RollbackManager state 직접 변조 금지
- PolicyCandidate 직접 생성 금지
- ApplyGuard 직접 생성 금지
- PerformanceEngine 내부 confidence 계산 수행 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Uninitialized | Yes | object startup |
| Initializing | Yes | StartupPipeline 실행 |
| Observing | Limited | watchdog lifecycle 관리 |
| Evaluating | Limited | orchestration only |
| PolicyPending | Limited | orchestration only |
| Applying | Limited | no direct mutation |
| Running | Yes | runtime loop supervision |
| MonitoringOnly | Yes | observation loop supervision |
| RollbackPending | Limited | rollback flow coordination |
| ShutdownRequested | Yes | safe point coordination |
| ShuttingDown | Yes | ShutdownPipeline 실행 |
| Terminated | Yes | exit code 반환 |
| FatalError | Yes | safe shutdown attempt |

## 11. 실패 처리

Failure: StartupPipeline failure

- Severity: FatalError or monitoring-only depending recoverability
- Fallback: stop startup or enter MonitoringOnly if safe
- Evidence: startup failure reason
- Next State: FatalError 또는 MonitoringOnly

Failure: Watchdog runtime failure

- Severity: WARN/BLOCKER depending RuntimeValidation
- Fallback: ShutdownRequested or RollbackPending
- Evidence: runtime validation summary
- Next State: ShutdownRequested 또는 RollbackPending

Failure: shutdown signal while applying

- Severity: INFO/WARN
- Fallback: wait safe point, then RollbackPending
- Evidence: shutdown reason, active policy count
- Next State: ShutdownRequested 또는 RollbackPending

Failure: ShutdownPipeline failure

- Severity: BLOCKER candidate
- Fallback: FatalError with evidence attempt
- Evidence: shutdown result, rollback preserved state
- Next State: FatalError

## 12. Evidence 요구사항

- startup result
- runtime state transition summary
- shutdown reason
- watchdog safe point reached
- runtime validation summary
- rollback required
- shutdown result
- final evidence path
- exit code

Evidence 없는 정상 종료 판정은 금지한다.

## 13. 테스트 기준

Unit Test:

- startup failure path mapping
- shutdown reason propagation
- exit code mapping

Integration Test:

- StartupPipeline → WatchdogCycleRunner → ShutdownPipeline flow
- RuntimeValidation BLOCKER → shutdown/rollback flow

Regression Test:

- RuntimeOrchestrator가 direct mutation API를 소유하지 않는지 static check

Failure Test:

- startup failure
- watchdog failure
- shutdown while active policy
- shutdown rollback failure

Evidence Test:

- shutdown reason, runtime validation summary, final evidence path 기록 검증

## 14. Non-Goals

- 세부 Controller 구현
- ThreadTracker sample algorithm
- PolicyCandidate generation
- RollbackManager internal state implementation
- UI 설계
- Release Gate script 구현

## 15. Acceptance Criteria

1. RuntimeOrchestrator가 조립자임이 명확하다.
2. 세부 mutation 소유권을 가져가지 않는다는 금지사항이 정의되어 있다.
3. Startup/Watchdog/Shutdown 흐름이 정의되어 있다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. 후속 구현 작업에서 runtime orchestration 경계를 판단할 수 있다.

## 16. Open Questions

1. RuntimeStateMachine의 소유자는 RuntimeOrchestrator인가, 별도 RuntimeStateController인가?
2. exit code mapping은 RuntimeOrchestrator MDS에서 확정할 것인가, RuntimeStateMachine 후속 문서에서 확정할 것인가?
3. watchdog safe point 대기 최대 시간은 어디서 정의할 것인가?
4. final evidence flush 실패 시 exit code 정책은 어디서 확정할 것인가?
