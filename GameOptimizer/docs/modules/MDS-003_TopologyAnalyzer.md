# MDS-003 TopologyAnalyzer

## 1. 문서 개요

문서명: MDS-003 TopologyAnalyzer

버전: v1.0

대상 모듈: TopologyAnalyzer

작성 목적: processor group, logical processor, SMT, L3/CCX 정보를 분석하여 SchedulerController와 PerformanceEngine이 사용할 topology 정보를 제공하는 모듈 경계를 정의한다.

적용 범위: processor group 탐지, group-aware core mask 정보 제공, logical/physical core 매핑, SMT sibling 정보, L3/cache group 정보, topology completeness 판단, fallback topology 제공

비적용 범위: thread/process affinity 적용, Win32 mutation, scheduling policy apply, group 0 hardcoding, CCX 정책 강제 적용

상위 문서:

- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`

관련 모듈:

- PerformanceEngine
- CoreIsolationPlanner
- CCXStabilityPlanner
- SchedulerController
- BackgroundController

문서 경로 기준: `GameOptimizer/docs/`. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 2. 모듈 목적

TopologyAnalyzer의 목적은 CPU topology를 group-aware 방식으로 분석하여 정책 판단과 Controller 적용이 안전한 processor group, KAFFINITY, SMT, L3/cache 정보를 사용할 수 있게 하는 것이다.

TopologyAnalyzer는 topology 정보를 제공하지만 mutation을 수행하지 않는다.

핵심 원칙:

```text
Processor Group 정보가 불완전하면 monitoring-only 또는 conservative fallback 판단을 가능하게 해야 한다.
```

## 3. 책임 범위

이 모듈의 책임:

1. processor group 탐지
2. KAFFINITY 계산에 필요한 group 정보 제공
3. logical/physical core 매핑
4. SMT sibling 정보 제공
5. L3/cache group 정보 제공
6. topology completeness 판단
7. conservative fallback topology 제공

이 모듈의 비책임:

1. thread/process affinity 적용
2. SchedulerController mutation 수행
3. BackgroundController restriction 수행
4. Main Thread Confidence 계산
5. CCX 정책 강제 적용
6. Processor Group 1+ process-wide restriction 허용 판단 단독 결정

## 4. 입력

Input: Win32 processor topology data

- Source: OS topology APIs
- Required: Yes
- Validation: buffer record walking이 span-bound safe 해야 함
- Missing behavior: topology incomplete

Input: process affinity data

- Source: target process query
- Required: Yes
- Validation: group-aware 정보와 일관되어야 함
- Missing behavior: conservative fallback 또는 monitoring-only

Input: processor group data

- Source: Win32 group APIs
- Required: Yes for group-aware policy
- Validation: group id와 KAFFINITY-capable mask가 함께 제공되어야 함
- Missing behavior: group 0 보정 금지, policy block

Input: cache/core record

- Source: OS topology APIs
- Required: No
- Validation: L3/cache group 정보가 완전해야 CCX 판단 가능
- Missing behavior: CCX policy monitoring-only

## 5. 출력

Output: TopologyResult

- Consumer: PerformanceEngine, SchedulerController, BackgroundController
- Meaning: group-aware topology summary
- 신뢰도: completeness state에 의존
- Failure behavior: incomplete topology result

Output: processorGroup

- Consumer: SchedulerController, PolicyResolver
- Meaning: thread affinity policy에 필요한 group identity
- 신뢰도: OS query success에 의존
- Failure behavior: unknown group, no silent group 0

Output: KAFFINITY-capable core mask

- Consumer: SchedulerController, CoreIsolationPlanner
- Meaning: group-local affinity mask 후보
- 신뢰도: processor group과 함께 사용될 때만 유효
- Failure behavior: no apply candidate

Output: SMT sibling map

- Consumer: CoreIsolationPlanner
- Meaning: SMT sibling 회피 판단 입력
- 신뢰도: topology completeness에 의존
- Failure behavior: SMT unknown

Output: L3/cache group map

- Consumer: CCXStabilityPlanner
- Meaning: CCX/L3 stability 판단 입력
- 신뢰도: cache record completeness에 의존
- Failure behavior: CCX monitoring-only

Output: topology completeness

- Consumer: PolicyResolver, Evidence Layer
- Meaning: policy generation 가능성 판단
- 신뢰도: required fields 존재 여부에 기반
- Failure behavior: incomplete

## 6. 내부 상태

State: topology snapshot

- 목적: 현재 runtime의 topology 분석 결과 보관
- 수명: runtime session
- 초기화 시점: Initializing
- 갱신 시점: topology refresh가 필요한 경우
- 보존 여부: Evidence summary로 보존

State: processor group capability

- 목적: group-aware policy 가능 여부 판단
- 수명: runtime session
- 초기화 시점: topology analysis
- 갱신 시점: processor group query refresh
- 보존 여부: Evidence로 보존

State: completeness flags

- 목적: conservative fallback 판단
- 수명: topology snapshot
- 초기화 시점: topology parse 완료
- 갱신 시점: missing field 발견
- 보존 여부: Evidence로 보존

## 7. 소유권 규칙

TopologyAnalyzer는 topology facts와 completeness 판단을 소유한다.

TopologyAnalyzer는 thread affinity, process affinity, process priority, rollback state를 소유하지 않는다.

TopologyAnalyzer는 group-aware 정보를 제공해야 하며, processor group 정보가 없을 때 group 0으로 조용히 보정하지 않는다.

## 8. 허용 동작

- processor topology query
- processor group query
- logical/physical core mapping
- SMT sibling map 생성
- L3/cache group map 생성
- group-aware mask candidate 제공
- topology completeness 계산
- conservative fallback reason 제공

## 9. 금지 동작

- group 0 hardcoding 금지
- 불완전한 topology를 완전한 것으로 표시 금지
- Win32 mutation 금지
- thread/process affinity 적용 금지
- SetThreadAffinityMask 호출 금지
- SetThreadGroupAffinity 호출 금지
- SetProcessAffinityMask 호출 금지
- RollbackManager 호출 금지
- ApplyGuard 생성 금지

## 10. 상태 머신 연계

| Runtime State | 동작 가능 여부 | 허용 동작 |
|---|---:|---|
| Initializing | Yes | topology analysis |
| Observing | Limited | topology snapshot read |
| Evaluating | Yes | topology result 제공 |
| PolicyPending | Yes | processor group validation input |
| Applying | No | mutation forbidden |
| Verifying | Limited | expected topology read |
| Running | Limited | topology completeness read |
| MonitoringOnly | Yes | fallback topology reason 제공 |
| RollbackPending | No | rollback 소유권 없음 |
| ShutdownRequested | Limited | final topology summary |
| ShuttingDown | Limited | final summary only |

## 11. 실패 처리

Failure: processor group query failed

- Severity: WARN/BLOCKER depending policy need
- Fallback: group-aware policy block, monitoring-only
- Evidence: processorGroupState, topologyCompleteness
- Next State: MonitoringOnly 또는 PolicyRejected

Failure: cache topology incomplete

- Severity: WARN
- Fallback: CCX_STABILITY_PREFER 차단
- Evidence: cacheGroupState
- Next State: Evaluating 또는 MonitoringOnly

Failure: invalid topology record

- Severity: WARN 또는 FatalError if parser safety risk
- Fallback: topology incomplete
- Evidence: topologyParseFailure
- Next State: MonitoringOnly 또는 FatalError

Failure: HEDT/Processor Group 1+ unsupported process-wide restriction

- Severity: WARN with monitoring-only evidence
- Fallback: process-wide background restriction block
- Evidence: processorGroupMode, backgroundRestrictionMode
- Next State: MonitoringOnly 또는 PolicyRejected

## 12. Evidence 요구사항

- topology result
- processor group capability
- KAFFINITY-capable mask availability
- SMT sibling map availability
- L3/cache group availability
- topology completeness
- fallback topology reason
- Processor Group 1+ limitation reason

Evidence가 없는 topology 판단은 scheduling policy handoff의 근거가 될 수 없다.

## 13. 테스트 기준

Unit Test:

- topology buffer parsing
- group-aware mask preservation
- incomplete topology flagging

Integration Test:

- SchedulerController가 processorGroup과 KAFFINITY를 함께 소비하는지 검증
- CoreIsolationPlanner가 SMT sibling 정보를 소비할 수 있는지 검증

Regression Test:

- group 0 coercion이 재도입되지 않았는지 static/regression gate 검증

Failure Test:

- processor group query failure
- incomplete cache topology
- invalid topology record

Evidence Test:

- topology completeness와 Processor Group limitation이 evidence summary에 기록되는지 검증

## 14. Non-Goals

- 구체 C++ 함수 시그니처 확정
- NUMA/hybrid full policy 확정
- thread/process affinity 적용 구현
- CCX threshold 확정
- UI 설계

## 15. Acceptance Criteria

1. TopologyAnalyzer 책임과 비책임이 명확하다.
2. processorGroup과 KAFFINITY를 함께 다루는 원칙이 포함되어 있다.
3. group 0 hardcoding 금지가 명시되어 있다.
4. RuntimeStateMachine과 연결되어 있다.
5. 실패 처리와 Evidence 요구사항이 정의되어 있다.
6. 후속 구현 작업에서 topology 안전 경계를 판단할 수 있다.

## 16. Open Questions

1. ProcessorGroupManager를 TopologyAnalyzer 내부로 둘 것인가, 별도 모듈로 둘 것인가?
2. Intel Hybrid topology는 v3.1에서 어느 수준까지 표현할 것인가?
3. L3/CCX 정보가 없는 CPU에서 CCXStabilityPlanner는 항상 monitoring-only인가?
4. topology refresh는 runtime 중 허용할 것인가?
