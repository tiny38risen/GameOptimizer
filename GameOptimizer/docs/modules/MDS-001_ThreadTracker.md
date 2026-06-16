# MDS-001 ThreadTracker

## 1. 문서 개요

문서명: MDS-001 ThreadTracker

버전: v1.0

대상 모듈: ThreadTracker

작성 목적: 대상 프로세스의 thread runtime 정보를 관찰하고, MainThreadConfidenceAnalyzer에 필요한 observation snapshot을 제공하는 모듈 경계를 정의한다.

적용 범위: thread enumeration, CPU delta 관찰, EMA 계산, lifetime 관찰, migration 관찰, access failure 기록, snapshot 제공

비적용 범위: thread affinity/priority mutation, policy generation, rollback state ownership, ApplyGuard transaction ownership

상위 문서:

- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/contracts/Runtime_Contract.md`

관련 모듈:

- MainThreadConfidenceAnalyzer
- WatchdogCycleRunner
- RuntimeValidationMonitor
- SchedulerController
- RollbackManager

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

ThreadTracker의 목적은 대상 프로세스의 thread runtime 정보를 관찰하고, CPU 사용량 변화와 후보 thread 상태를 수집하는 것이다.

ThreadTracker는 정책을 적용하지 않는다. ThreadTracker는 관찰 데이터를 제공하는 모듈이다.

핵심 원칙:

```text
ThreadTracker는 observation-only 모듈이다.
```

## 3. 책임 범위

이 모듈의 책임:

1. 대상 프로세스의 thread enumeration
2. thread CPU delta 계산
3. EMA 계산
4. thread lifetime observation
5. migration observation
6. access failure 기록
7. ThreadSampleSnapshot 제공

이 모듈의 비책임:

1. thread affinity 또는 priority 적용
2. PolicyCandidate 생성
3. RollbackManager 호출 또는 rollback state 소유
4. ApplyGuard 생성 또는 transaction boundary 소유
5. Main Thread Confidence 최종 판정

## 4. 입력

Input: Target process identity

- Source: RuntimeContext / ProcessFinder
- Required: Yes
- Validation: process id와 identity가 관찰 시점에 유효해야 함
- Missing behavior: empty snapshot + observation failure evidence

Input: Thread sample interval

- Source: RuntimeConfig 또는 watchdog cycle configuration
- Required: Yes
- Validation: 과도한 polling 금지, 구체 수치는 TBD
- Missing behavior: default interval 사용 또는 initialization failure

Input: Stop signal

- Source: RuntimeSignalState / watchdog stop token
- Required: Yes
- Validation: shutdown 요청 시 sampling 중단 가능해야 함
- Missing behavior: shutdown safety risk로 evidence 기록

## 5. 출력

Output: ThreadSampleSnapshot

- Consumer: MainThreadConfidenceAnalyzer, RuntimeValidationMonitor
- Meaning: 관찰된 thread별 CPU delta, EMA, lifetime, migration 정보
- 신뢰도: access failure와 sample count에 따라 달라짐
- Failure behavior: empty snapshot + observation failure evidence

Output: ThreadRuntimeStats

- Consumer: PerformanceEngine, RuntimeValidationMonitor
- Meaning: thread별 누적 runtime 관찰 요약
- 신뢰도: sample continuity가 높을수록 증가
- Failure behavior: partial stats + skipped thread count

Output: main thread candidate raw signal

- Consumer: MainThreadConfidenceAnalyzer
- Meaning: confidence 계산 전 후보 raw signal
- 신뢰도: confidence 판정 아님
- Failure behavior: no candidate

Output: access failure count

- Consumer: RuntimeValidationMonitor, Evidence Layer
- Meaning: query-only handle 접근 실패 횟수
- 신뢰도: WARN 또는 monitoring-only reason 판단에 사용
- Failure behavior: count unavailable이면 observation quality 낮음

Output: migration count

- Consumer: MainThreadConfidenceAnalyzer, PerformanceEvidencePlanner
- Meaning: thread core migration 관찰값
- 신뢰도: topology와 sample 연속성에 의존
- Failure behavior: unknown

## 6. 내부 상태

State: observed thread set

- 목적: 관찰 중인 thread 목록 유지
- 수명: target process 관찰 중
- 초기화 시점: Observing 진입
- 갱신 시점: watchdog sample cycle
- 보존 여부: runtime evidence summary에 요약 가능

State: per-thread CPU delta history

- 목적: EMA와 후보 raw signal 계산 지원
- 수명: observation session
- 초기화 시점: first sample
- 갱신 시점: each sample
- 보존 여부: summary만 보존

State: access failure accumulator

- 목적: access boundary 판단
- 수명: runtime session
- 초기화 시점: startup
- 갱신 시점: OpenThread/GetThreadTimes 실패
- 보존 여부: Evidence로 보존

State: migration observation history

- 목적: migration 변화 관찰
- 수명: observation window
- 초기화 시점: first valid sample
- 갱신 시점: sample cycle
- 보존 여부: Performance Evidence summary로 보존

## 7. 소유권 규칙

ThreadTracker는 thread observation data를 소유한다.

ThreadTracker는 thread affinity, thread priority, process priority, rollback state를 소유하지 않는다.

ThreadTracker는 Win32 query-only access를 사용할 수 있으나 set-right handle을 요구해서는 안 된다.

## 8. 허용 동작

- OpenThread with query permission
- GetThreadTimes
- thread enumeration
- thread lifetime observation
- sample aggregation
- EMA update
- migration observation
- access failure 기록
- snapshot 제공

## 9. 금지 동작

- SetThreadAffinityMask 호출 금지
- SetThreadGroupAffinity 호출 금지
- SetThreadPriority 호출 금지
- SetPriorityClass 호출 금지
- SetProcessAffinityMask 호출 금지
- RollbackManager 호출 금지
- ApplyGuard 생성 금지
- PolicyCandidate 생성 금지
- SchedulerController mutation path 호출 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Uninitialized | No | 없음 |
| Initializing | Limited | 초기 관찰 준비만 가능 |
| Observing | Yes | sample collection |
| Evaluating | Yes | snapshot read |
| PolicyPending | Limited | read-only snapshot 제공 |
| Applying | No | direct mutation forbidden |
| Verifying | No | direct mutation forbidden |
| Running | Yes | runtime sample collection |
| MonitoringOnly | Yes | observation-only sample collection |
| RollbackPending | No | rollback 소유권 없음 |
| ShutdownRequested | Limited | final summary only |
| ShuttingDown | Limited | final summary only |
| Terminated | No | 없음 |

## 11. 실패 처리

Failure: OpenThread access denied

- Severity: WARN 또는 monitoring-only reason
- Fallback: 해당 thread 제외, access failure count 증가
- Evidence: accessDeniedCount, skippedThreadId
- Next State: Observing 또는 MonitoringOnly

Failure: GetThreadTimes failure

- Severity: WARN
- Fallback: 해당 sample 제외
- Evidence: threadQueryFailureCount
- Next State: Observing

Failure: target process exited

- Severity: INFO 또는 shutdown reason
- Fallback: sampling 중단
- Evidence: target process exit marker
- Next State: ShutdownRequested 또는 Terminated

Failure: sample window insufficient

- Severity: INFO/WARN
- Fallback: confidence 계산에 insufficient evidence 전달
- Evidence: sample count, observation duration
- Next State: Observing 또는 Evaluating

## 12. Evidence 요구사항

ThreadTracker는 다음 Evidence를 제공해야 한다.

- sample count
- observed thread count
- access failure count
- skipped thread count
- migration count
- candidate raw signal summary
- confidence input quality
- target process identity summary

Evidence가 없는 thread observation은 Main Thread Confidence 성공 판정의 근거가 될 수 없다.

## 13. 테스트 기준

Unit Test:

- CPU delta 계산이 sample order에 따라 안정적으로 산출되는지 검증
- EMA update가 reset 조건을 처리하는지 검증
- access failure가 count와 Evidence로 기록되는지 검증

Integration Test:

- WatchdogCycleRunner에서 ThreadSampleSnapshot을 소비할 수 있는지 검증
- RuntimeValidationMonitor가 missing candidate를 처리하는지 검증

Regression Test:

- ThreadTracker에 mutation API 또는 RollbackManager 호출이 추가되지 않았는지 static gate 검증

Failure Test:

- OpenThread access denied
- target thread 종료
- target process 종료

Evidence Test:

- access failure count, sample count, migration count가 evidence summary에 연결되는지 검증

## 14. Non-Goals

- 구체 C++ 함수 시그니처 확정
- 구체 enum 값 확정
- Main Thread Confidence threshold 확정
- UI 설계
- Release Gate 스크립트 구현
- thread mutation 구현

## 15. Acceptance Criteria

1. ThreadTracker가 observation-only 모듈로 정의되어 있다.
2. 입력과 출력이 정의되어 있다.
3. 소유권과 금지사항이 구체적으로 정의되어 있다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. 후속 구현 작업에서 ThreadTracker의 모듈 경계를 판단할 수 있다.

## 16. Open Questions

1. ThreadSampleSnapshot의 최종 field set은 어디서 확정할 것인가?
2. sample interval 기본값은 Performance Validation Plan에서 확정할 것인가?
3. migration observation은 OS가 제공하는 마지막 processor 정보를 사용할 것인가, 별도 sampling heuristic을 둘 것인가?
4. access failure count가 몇 회 이상이면 MonitoringOnly로 전환할 것인가?
