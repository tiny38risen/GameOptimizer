# GameOptimizer v3.1 Patch Plan — Phase 1: Observation & Topology Foundation
> Archive notice: This Phase Patch Plan is historical. Active implementation work and execution status are tracked in GitHub Issues. This file is not a current source of truth for work ordering, completion status, or release approval.

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 1

버전: v1.0

작성 목적: GameOptimizer v3.1의 첫 번째 실제 구현 단계인 Phase 1 — Observation & Topology Foundation을 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, Phase 1에서 어떤 순서로 어떤 파일과 모듈을 수정하고, 각 패치의 완료 기준(Definition of Done), 회귀 테스트(Regression Test), Evidence 기준을 무엇으로 둘지 정의하는 작업 계획 문서다.

적용 범위:

- ThreadTracker 관찰 전용(Observation-only) 계약 검증
- Thread sample snapshot 계약 정리
- thread access failure, stale thread, partial observation 처리 기준
- migration / switch raw signal 제공 기준
- TopologyAnalyzer의 Processor Group / SMT / L3 / CCX 정보 정리
- topology completeness와 monitoring-only reason 기준
- Phase 1 Runtime Validation, Regression Test, Evidence 기준

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 성능 개선 수치 보장
- MainThreadConfidenceAnalyzer 최종 구현
- SchedulerController mutation 변경
- Aggressive Single-Core Mode 구현
- GameProfileRegistry 구현
- Release Gate 자동화

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/proposals/performance/PerformanceEngineSpec.md`
- `docs/proposals/performance/PolicySpecification.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`

후속 문서:

- `docs/archive/patch-plans/PatchPlan_Phase2.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 1의 목표는 성능 향상을 직접 만드는 것이 아니라, 이후 Main Thread Confidence와 Scheduler Policy가 안전하게 동작할 수 있는 관찰·토폴로지 기반을 고정하는 것이다.

## 2. Phase 1 목표

Phase 1의 핵심 목표는 ThreadTracker와 TopologyAnalyzer의 책임 경계를 고정하고, 후속 Phase가 신뢰할 수 있는 raw observation과 group-aware topology를 제공하는 것이다.

필수 목표:

1. ThreadTracker observation-only 계약 검증
2. Thread sample snapshot 출력 정리
3. thread access failure / stale thread 처리 기준 정리
4. migration / switch raw signal 제공
5. TopologyAnalyzer group-aware 결과 정리
6. SMT sibling 정보 제공
7. L3/CCX topology 정보 제공
8. topology completeness 및 fallback reason 제공
9. RuntimeValidation에서 Phase 1 관련 blocker 판단 가능하게 정리

Phase 1 완료 후에도 성능 정책 적용 여부는 결정하지 않는다. Phase 1 결과는 Phase 2 Main Thread Confidence Engine과 Phase 3 Policy Candidate & Dispatch Layer의 입력으로 사용된다.

## 3. Phase 1 비목표

다음은 Phase 1에서 구현하지 않는다.

```text
MainThreadConfidenceAnalyzer 최종 구현
PolicyCandidate 생성
SchedulerController mutation 변경
BackgroundController mutation 변경
Aggressive Single-Core Mode
Game Profile runtime integration
Before/After performance classification
Release Gate 자동화
UI 변경
```

Phase 1에서 Win32 mutation 정책을 확장하지 않는다.

Phase 1의 PASS는 성능 향상 PASS가 아니다. Phase 1의 PASS는 observation/topology foundation이 안전하게 준비되었다는 의미다.

## 4. 영향 모듈

Phase 1에서 영향을 받을 수 있는 모듈은 직접 영향, 간접 영향, 변경 금지 또는 최소 변경 대상으로 구분한다.

### 4.1 직접 영향 모듈

```text
ThreadTracker
TopologyAnalyzer
RuntimeContext
RuntimeValidationMonitor
```

### 4.2 간접 영향 모듈

```text
PerformanceEngine
MainThreadConfidenceAnalyzer
SchedulerController
Evidence Recorder
Release Gate static checks
```

간접 영향 모듈은 Phase 1 결과를 읽거나 검증할 수 있지만, Phase 1에서 policy 생성 또는 mutation 책임을 새로 가져서는 안 된다.

### 4.3 변경 금지 또는 최소 변경 모듈

```text
SchedulerController
BackgroundController
RollbackManager
ApplyGuard
PolicyDispatcher
AggressiveSingleCoreController
```

주의:

Phase 1에서 SchedulerController, RollbackManager, ApplyGuard의 동작을 변경해야 하는 경우, 별도 Phase로 분리한다.

## 5. 패치 단위 개요

Phase 1은 다음 Patch ID 구조를 사용한다.

```text
P1-01 Documentation / Contract Alignment
P1-02 ThreadTracker Static Safety Audit
P1-03 Thread Sample Snapshot Contract
P1-04 Thread Access Failure Handling
P1-05 Migration / Switch Raw Signal
P1-06 TopologyResult Group-Aware Contract
P1-07 SMT / L3 / CCX Topology Mapping
P1-08 Topology Completeness & Monitoring-only Reason
P1-09 RuntimeValidation Phase 1 Checks
P1-10 Phase 1 Regression Test & Evidence
```

각 패치는 다음 양식을 따른다.

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

패치 하나가 observation, topology, mutation, rollback, release gate를 동시에 변경해서는 안 된다.

## 6. P1-01 Documentation / Contract Alignment

Patch ID: `P1-01`

목적:

Phase 1 구현 전 문서와 현재 코드의 책임 경계를 맞춘다.

작업 범위:

```text
ThreadTracker observation-only 계약 확인
TopologyAnalyzer group-aware 계약 확인
MDS-001 / MDS-003 기준과 현재 코드 차이 목록화
Phase 1에서 다루지 않을 항목 명시
```

수정 가능 파일:

```text
docs/archive/patch-plans/PatchPlan_Phase1.md
docs/implementation/MIGRATION_NOTES_Phase1.md
```

수정 금지 파일:

```text
src/*
tests/*
```

구현 규칙:

```text
코드 변경 없이 문서/계약 차이만 식별한다.
미확정 항목은 TBD 또는 Open Question으로 남긴다.
문서 기준 경로는 docs/... 형식을 사용한다.
```

완료 기준:

```text
Phase 1 구현 범위가 문서로 고정된다.
BLOCKING open question이 식별된다.
Out-of-scope 항목이 명시된다.
```

필수 테스트:

```text
required document existence check
Phase 1 scope review
blocking question review
out-of-scope review
```

필수 Evidence:

```text
Phase 1 scope summary
Blocking question list
Out-of-scope list
```

리스크:

```text
문서 계약이 모호하면 코드 패치가 mutation 범위를 잘못 확장할 수 있다.
Open Question을 방치하면 Phase 1과 Phase 2 경계가 흐려질 수 있다.
```

Rollback 계획:

```text
문서 변경만 포함하므로 잘못된 범위 정의는 후속 문서 패치로 정정한다.
코드 변경이 포함되었다면 해당 패치는 Phase 1 범위 위반으로 분리한다.
```

## 7. P1-02 ThreadTracker Static Safety Audit

Patch ID: `P1-02`

목적:

ThreadTracker가 observation-only 계약을 위반하지 않는지 정적으로 확인한다.

작업 범위:

```text
ThreadTracker 내부 Win32 mutation API 호출 여부 확인
SetThreadAffinityMask 호출 여부 확인
SetThreadGroupAffinity 호출 여부 확인
SetThreadPriority 호출 여부 확인
SetPriorityClass 호출 여부 확인
RollbackManager / ApplyGuard 직접 호출 여부 확인
PolicyCandidate 생성 여부 확인
```

수정 가능 파일:

```text
ThreadTracker 관련 소스/헤더
정적 검사 스크립트
관련 테스트
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
BackgroundController
```

구현 규칙:

```text
ThreadTracker는 OpenThread, GetThreadTimes 등 query 목적 API만 사용할 수 있다.
ThreadTracker는 mutation owner가 아니다.
ThreadTracker는 PolicyCandidate를 만들지 않는다.
ThreadTracker는 SchedulerController, RollbackManager, ApplyGuard에 직접 의존하지 않는다.
```

완료 기준:

```text
ThreadTracker에서 mutation API 호출이 정적으로 차단된다.
ThreadTracker mutation leak 테스트가 추가된다.
정적 검사 결과가 Phase 1 Evidence에 기록된다.
```

필수 테스트:

```text
ThreadTracker forbidden API static check
ThreadTracker does not include SchedulerController dependency
ThreadTracker does not create ApplyGuard
ThreadTracker does not call RollbackManager
```

필수 Evidence:

```text
threadTrackerStaticSafetySummary
forbiddenApiScanResult
forbiddenDependencyScanResult
mutationLeakCheckResult
```

리스크:

```text
정적 검사가 문자열 기반이면 false positive 또는 false negative가 발생할 수 있다.
ThreadTracker helper가 다른 파일로 분리되어 있으면 검사 범위 누락이 발생할 수 있다.
```

Rollback 계획:

```text
정적 검사 추가가 과도하게 차단하면 검사 rule을 완화하되 forbidden mutation API 차단은 유지한다.
ThreadTracker 코드 변경이 필요해지면 mutation 제거 패치와 정적 검사 패치를 분리한다.
```

BLOCKER 기준:

```text
ThreadTracker에서 Win32 mutation API 호출 발견
ThreadTracker가 PolicyCandidate 생성
ThreadTracker가 RollbackManager 또는 ApplyGuard 직접 호출
```

## 8. P1-03 Thread Sample Snapshot Contract

Patch ID: `P1-03`

목적:

ThreadTracker가 MainThreadConfidenceAnalyzer에 제공할 raw observation snapshot 계약을 정리한다.

작업 범위:

```text
ThreadSampleSnapshot 개념 정리
ThreadRuntimeStats 개념 정리
thread id, cpu delta, EMA input, lifetime, access state 포함
snapshot incomplete 처리 기준 정의
```

수정 가능 파일:

```text
ThreadTracker 관련 소스/헤더
RuntimeContext 관련 타입
ThreadTracker 테스트
```

수정 금지 파일:

```text
SchedulerController
PolicyDispatcher
RollbackManager
ApplyGuard
AggressiveSingleCoreController
```

구현 규칙:

```text
snapshot은 정책 판단 결과가 아니라 observation 결과다.
snapshot은 thread priority/affinity 적용 여부를 결정하지 않는다.
snapshot incomplete는 insufficient evidence 입력으로만 사용한다.
```

완료 기준:

```text
ThreadTracker가 confidence 계산에 필요한 raw signal을 제공한다.
incomplete snapshot은 insufficient evidence로 사용할 수 있다.
snapshot 결과가 mutation 경로로 직접 연결되지 않는다.
```

필수 테스트:

```text
empty thread snapshot test
valid thread snapshot test
partial snapshot test
short-lived thread snapshot test
```

필수 Evidence:

```text
sampleCount
observedThreadCount
partialSnapshotReason
accessFailureCount
staleThreadCount
```

리스크:

```text
snapshot 계약을 크게 바꾸면 후속 MainThreadConfidenceAnalyzer와 RuntimeContext 호출부가 영향을 받을 수 있다.
raw signal과 policy eligibility가 섞이면 Phase 1 범위를 초과할 수 있다.
```

Rollback 계획:

```text
새 snapshot 타입이 불안정하면 기존 observation 출력과 병행하는 adapter 형태로 되돌린다.
policy 판단 로직이 들어간 경우 해당 로직은 Phase 2 이후로 분리한다.
```

## 9. P1-04 Thread Access Failure Handling

Patch ID: `P1-04`

목적:

OpenThread 실패, Access Denied, stale thread를 안전하게 처리한다.

작업 범위:

```text
OpenThread 실패 분류
Access Denied count 기록
thread disappeared 처리
stale thread 처리
partial observation evidence 기록
```

수정 가능 파일:

```text
ThreadTracker 관련 소스/헤더
ThreadTracker 테스트
RuntimeValidationMonitor 일부
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
PolicyDispatcher
```

구현 규칙:

```text
Access Denied는 즉시 fatal이 아니다.
반복 Access Denied는 confidence 상승 차단 또는 monitoring-only reason이 될 수 있다.
stale thread는 정책 적용 대상으로 전달하지 않는다.
failure handling은 crash-free observation을 우선한다.
```

완료 기준:

```text
access failure가 crash 없이 처리된다.
partial observation이 Evidence로 남는다.
stale thread가 policy 대상 후보로 전달되지 않는다.
```

필수 테스트:

```text
OpenThread access denied test
thread exits during sampling test
partial observation evidence test
repeated access failure test
```

필수 Evidence:

```text
accessFailureCount
accessDeniedThreadCount
staleThreadCount
partialObservationReason
```

리스크:

```text
Access Denied를 과도하게 BLOCKER로 처리하면 정상 dry-run이 불필요하게 실패할 수 있다.
반대로 반복 실패를 무시하면 anti-cheat boundary suspicion 또는 insufficient evidence를 놓칠 수 있다.
```

Rollback 계획:

```text
failure 분류가 불안정하면 crash-free counting만 유지하고 policy eligibility 연결은 후속 Phase로 미룬다.
RuntimeValidation 연동이 과도하면 WARN-only evidence로 되돌리고 blocker 기준은 별도 패치로 분리한다.
```

## 10. P1-05 Migration / Switch Raw Signal

Patch ID: `P1-05`

목적:

메인 스레드 신뢰도 판단에 필요한 migration / switch raw signal을 제공한다.

작업 범위:

```text
thread core observation 가능 범위 정리
migration count raw signal 정의
candidate switch count raw signal 정의
sampling window 기준 정의
```

수정 가능 파일:

```text
ThreadTracker 관련 소스/헤더
ThreadTracker 테스트
Evidence recorder 일부
```

수정 금지 파일:

```text
SchedulerController
PolicyDispatcher
RollbackManager
AggressiveSingleCoreController
```

구현 규칙:

```text
ThreadTracker는 migration raw signal만 제공한다.
migration 감소 정책은 생성하지 않는다.
ThreadTracker는 sticky policy를 직접 적용하지 않는다.
sampling window는 observation 단위이며 apply window가 아니다.
```

완료 기준:

```text
Migration / switch raw signal이 MainThreadConfidenceAnalyzer 입력으로 사용할 수 있다.
sampling window reset 기준이 명확하다.
ThreadTracker에서 policy가 생성되지 않는다.
```

필수 테스트:

```text
migration signal accumulation test
candidate switch signal test
sampling window reset test
no policy generated from tracker test
```

필수 Evidence:

```text
migrationCount
candidateSwitchCount
samplingWindowStart
samplingWindowEnd
```

리스크:

```text
코어 관찰 방식이 OS/권한에 따라 불완전할 수 있다.
migration raw signal이 confidence 판단으로 오해되어 policy 적용 조건처럼 사용될 수 있다.
```

Rollback 계획:

```text
migration 관찰이 불안정하면 signal을 optional evidence로 낮추고 policy eligibility 연결은 차단한다.
sampling window 로직이 불안정하면 count 누적만 유지하고 window reset은 후속 패치로 분리한다.
```

## 11. P1-06 TopologyResult Group-Aware Contract

Patch ID: `P1-06`

목적:

TopologyAnalyzer가 실제 Processor Group 정보를 기반으로 TopologyResult를 제공하도록 계약을 정리한다.

작업 범위:

```text
processorGroup 필드 정리
logical processor mask 정리
KAFFINITY 가능 범위 명시
process/thread group compatibility 판단
group 0 hardcoding 제거 또는 차단
```

수정 가능 파일:

```text
TopologyAnalyzer 관련 소스/헤더
TopologyAnalyzer 테스트
RuntimeContext 관련 타입
```

수정 금지 파일:

```text
SchedulerController
RollbackManager
ApplyGuard
ThreadTracker mutation 경로
```

구현 규칙:

```text
Processor Group 정보가 없으면 group 0으로 조용히 보정하지 않는다.
불완전한 group 정보는 topologyIncomplete로 표시한다.
KAFFINITY mask는 group-aware context 없이 전역 core set처럼 취급하지 않는다.
```

완료 기준:

```text
TopologyResult가 processorGroup을 명시적으로 제공한다.
group 정보 누락 시 monitoring-only reason을 제공할 수 있다.
group 0 hardcoding이 정적 검사 또는 테스트로 차단된다.
```

필수 테스트:

```text
single processor group topology test
multi processor group topology test
missing group info test
group 0 hardcoding static check
```

필수 Evidence:

```text
processorGroupCount
selectedProcessorGroup
groupAware
topologyCompleteness
topologyFallbackReason
```

리스크:

```text
Processor Group API 결과가 테스트 환경마다 달라 재현성이 낮을 수 있다.
기존 코드가 group 0 전제를 암묵적으로 사용하고 있으면 영향 범위가 커질 수 있다.
```

Rollback 계획:

```text
group-aware 변경이 광범위하면 read-only TopologyResult 확장부터 적용하고 affinity eligibility 연결은 차단한다.
기존 동작과 충돌하면 group information unavailable 상태를 monitoring-only로 처리한다.
```

BLOCKER 기준:

```text
Processor Group 정보 누락 시 group 0으로 강제 fallback
TopologyResult가 group 정보를 숨김
```

## 12. P1-07 SMT / L3 / CCX Topology Mapping

Patch ID: `P1-07`

목적:

SMT sibling, L3 cache group, CCX-like locality 정보를 topology result에 반영한다.

작업 범위:

```text
SMT sibling map 정리
physical core mapping 정리
L3/cache relation 정리
CCX-like grouping 가능 여부 정리
topology unsupported reason 제공
```

수정 가능 파일:

```text
TopologyAnalyzer 관련 소스/헤더
TopologyAnalyzer 테스트
Evidence recorder 일부
```

수정 금지 파일:

```text
SchedulerController
PolicyDispatcher
AggressiveSingleCoreController
RollbackManager
```

구현 규칙:

```text
AMD CCX와 Intel Hybrid topology를 동일하게 단정하지 않는다.
불완전한 cache topology는 complete로 표시하지 않는다.
CCX-like grouping은 confidence input 또는 policy hint일 뿐 apply 결정이 아니다.
```

완료 기준:

```text
SMT sibling 정보가 제공된다.
L3/cache group 정보가 가능 범위에서 제공된다.
불완전한 topology는 fallback reason을 남긴다.
```

필수 테스트:

```text
SMT sibling detection test
L3 cache relation test
incomplete cache topology test
unsupported topology fallback test
```

필수 Evidence:

```text
smtSiblingMapAvailable
l3CacheGroupAvailable
ccxLikeGroupingAvailable
topologyUnsupportedReason
```

리스크:

```text
하드웨어별 topology 표현 차이로 unsupported case가 많이 발생할 수 있다.
CCX-like grouping을 과신하면 잘못된 affinity policy로 이어질 수 있다.
```

Rollback 계획:

```text
SMT 정보만 안정적으로 유지하고 L3/CCX-like 정보는 unsupported reason으로 fallback한다.
unsupported topology는 monitoring-only reason으로 남기고 policy eligibility는 차단한다.
```

## 13. P1-08 Topology Completeness & Monitoring-only Reason

Patch ID: `P1-08`

목적:

Topology 정보가 불완전할 때 정책 적용 대신 monitoring-only 또는 conservative fallback을 가능하게 한다.

작업 범위:

```text
topologyCompleteness 판단 기준 정의
processor group unsafe reason 제공
CCX policy unavailable reason 제공
affinity policy unsafe reason 제공
```

수정 가능 파일:

```text
TopologyAnalyzer
RuntimeContext
Policy eligibility 입력 타입
RuntimeValidationMonitor 일부
```

수정 금지 파일:

```text
SchedulerController mutation 경로
PolicyDispatcher mutation 경로
AggressiveSingleCoreController
RollbackManager
```

구현 규칙:

```text
topology incomplete 상태에서 aggressive mode 후보가 생성되면 안 된다.
processor group unsafe 상태에서 affinity stabilization 후보가 생성되면 안 된다.
monitoring-only reason은 Evidence로 남길 수 있어야 한다.
```

완료 기준:

```text
Topology incomplete reason이 PerformanceEngine/Policy Layer로 전달 가능하다.
monitoring-only 전환 사유가 Evidence로 기록 가능하다.
unsafe topology가 complete로 위장되지 않는다.
```

필수 테스트:

```text
topology incomplete blocks aggressive mode input test
processor group unsafe blocks affinity eligibility test
monitoring-only reason propagation test
```

필수 Evidence:

```text
topologyCompleteness
affinityPolicyEligible
ccxPolicyEligible
monitoringOnlyReason
```

리스크:

```text
eligibility 입력 타입 변경이 PerformanceEngine 또는 PolicyDispatcher 호출부에 영향을 줄 수 있다.
monitoring-only reason이 누락되면 Release Evidence에서 원인 설명이 어려워진다.
```

Rollback 계획:

```text
policy eligibility 연결이 위험하면 TopologyAnalyzer evidence만 남기고 policy input 연결은 Phase 3으로 연기한다.
monitoring-only reason 전파가 실패하면 topologyIncomplete를 BLOCKER 후보로 남기고 mutation은 차단한다.
```

## 14. P1-09 RuntimeValidation Phase 1 Checks

Patch ID: `P1-09`

목적:

ThreadTracker와 TopologyAnalyzer의 Phase 1 계약 위반을 RuntimeValidation에서 감지할 수 있게 한다.

작업 범위:

```text
ThreadTracker observation-only validation
snapshot freshness validation
topology completeness validation
processor group safety validation
monitoring-only reason validation
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
Evidence recorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController
BackgroundController
RollbackManager
ApplyGuard
```

구현 규칙:

```text
RuntimeValidation은 mutation을 수행하지 않는다.
RuntimeValidation은 BLOCKER/WARN/INFO 분류만 제공한다.
RuntimeValidation은 unsafe topology를 무시하지 않는다.
```

완료 기준:

```text
Phase 1 관련 safety violation이 RuntimeValidation Summary에 반영된다.
snapshot freshness와 topology safety state가 evidence로 남는다.
```

필수 테스트:

```text
snapshot freshness failure test
topology missing warning test
processor group unsafe blocker test
thread tracker mutation static blocker test
```

필수 Evidence:

```text
runtimeValidationState
phase1ValidationSummary
snapshotFreshness
topologySafetyState
blockerCount
warnCount
```

리스크:

```text
RuntimeValidation이 과도하게 BLOCKER를 만들면 dry-run 진행이 어려워질 수 있다.
반대로 topology unsafe를 WARN으로 낮추면 Phase 4 이후 mutation 안전성이 깨질 수 있다.
```

Rollback 계획:

```text
초기에는 INFO/WARN evidence로 관찰하되 group unsafe와 mutation leak만 BLOCKER로 유지한다.
과도한 blocker rule은 별도 rule tuning 패치로 분리한다.
```

## 15. P1-10 Phase 1 Regression Test & Evidence

Patch ID: `P1-10`

목적:

Phase 1 패치 묶음의 회귀 테스트와 Evidence 기준을 정리한다.

작업 범위:

```text
Phase 1 test list 작성
Phase 1 static check list 작성
Phase 1 runtime evidence list 작성
Phase 1 release blocker list 작성
```

수정 가능 파일:

```text
docs/validation/RegressionMatrix_v3.1.md
docs/release/ReleaseChecklist_v3.1.md
validation scripts if already existing
tests/*
```

수정 금지 파일:

```text
SchedulerController mutation behavior
RollbackManager behavior
ApplyGuard behavior
AggressiveSingleCoreController
```

구현 규칙:

```text
Phase 1의 PASS는 성능 향상 PASS가 아니다.
Phase 1 PASS는 observation/topology foundation이 안전하게 준비되었다는 의미다.
Regression test는 ThreadTracker, TopologyAnalyzer, RuntimeValidation, Evidence를 분리해 기록한다.
```

완료 기준:

```text
Phase 1 regression checklist가 존재한다.
Phase 1 blocker 조건이 ReleaseChecklist와 연결된다.
Phase 1 Evidence Bundle 최소 항목이 정의된다.
```

필수 테스트:

```text
ThreadTracker static safety suite
TopologyAnalyzer group-aware suite
RuntimeValidation phase1 suite
Evidence completeness phase1 suite
```

필수 Evidence:

```text
phase1RegressionSummary
threadTrackerSafetySummary
topologyAnalyzerSummary
runtimeValidationSummary
```

리스크:

```text
RegressionMatrix_v3.1.md가 아직 없으면 테스트 목록이 문서상 계획에 머물 수 있다.
ReleaseChecklist_v3.1.md 변경이 과도하면 릴리즈 기준과 Phase 1 기준이 섞일 수 있다.
```

Rollback 계획:

```text
신규 regression 문서가 준비되지 않았으면 PatchPlan_Phase1.md의 Evidence 기준을 우선 source of truth로 유지한다.
ReleaseChecklist 수정은 Phase 1 blocker 항목만 최소 반영한다.
```

## 16. Phase 1 완료 기준

Phase 1 전체 완료 기준은 다음과 같다.

1. ThreadTracker가 observation-only 계약을 만족한다.
2. ThreadTracker가 mutation API를 호출하지 않는다.
3. ThreadTracker가 confidence 계산용 raw signal을 제공한다.
4. Access Denied, stale thread, partial snapshot을 안전하게 처리한다.
5. TopologyAnalyzer가 Processor Group 정보를 명시적으로 제공한다.
6. group 0 hardcoding이 없다.
7. SMT/L3/CCX topology 정보가 가능 범위에서 제공된다.
8. topology incomplete 상태를 complete로 위장하지 않는다.
9. topology incomplete 시 monitoring-only reason을 제공한다.
10. RuntimeValidation이 Phase 1 관련 위험을 감지할 수 있다.
11. Phase 1 Evidence가 생성된다.
12. Phase 1 regression test가 정의되어 있다.

완료 판단은 성능 개선 여부가 아니라 observation/topology foundation의 안전성과 증거 가능성을 기준으로 한다.

## 17. Phase 1 BLOCKER 조건

다음 조건은 Phase 1 BLOCKER로 분류한다.

```text
ThreadTracker에서 Win32 mutation API 호출
ThreadTracker가 SchedulerController / RollbackManager / ApplyGuard 직접 의존
ThreadTracker가 PolicyCandidate 생성
Processor Group 정보 누락 시 group 0 강제 fallback
Topology incomplete를 complete로 표시
TopologyAnalyzer가 직접 affinity apply
RuntimeValidation이 topology unsafe를 무시
Phase 1 Evidence 누락
```

BLOCKER가 발생하면 후속 patch보다 해당 blocker 제거를 우선한다. 특히 P1-02와 P1-06의 BLOCKER는 Phase 1 전체 진행을 차단한다.

## 18. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P1-01 Documentation / Contract Alignment
2. P1-02 ThreadTracker Static Safety Audit
3. P1-03 Thread Sample Snapshot Contract
4. P1-04 Thread Access Failure Handling
5. P1-05 Migration / Switch Raw Signal
6. P1-06 TopologyResult Group-Aware Contract
7. P1-07 SMT / L3 / CCX Topology Mapping
8. P1-08 Topology Completeness & Monitoring-only Reason
9. P1-09 RuntimeValidation Phase 1 Checks
10. P1-10 Phase 1 Regression Test & Evidence
```

주의:

```text
P1-02와 P1-06은 Phase 1의 핵심 안전 패치다.
P1-02에서 ThreadTracker mutation leak이 발견되면, 다른 패치보다 먼저 수정한다.
P1-06에서 group 0 hardcoding이 발견되면, 다른 topology 확장보다 먼저 수정한다.
```

## 19. 패치 작성 규칙

각 실제 코드 패치는 다음 규칙을 따른다.

1. `[PATCH]` 모드로 작성한다.
2. 변경 전 실제 헤더 시그니처를 확인한다.
3. 인터페이스 변경 시 선언, 정의, 호출부, 테스트를 모두 수정한다.
4. `std::expected`는 Assign -> Check -> Bind 패턴을 따른다.
5. expected 결과를 검사 없이 `return func(...)` 형태로 전파하지 않는다.
6. Windows header include 순서를 깨지 않는다.
7. main/Test build target을 분리한다.
8. B9 Gate 검증 결과를 기록한다.
9. BEFORE/AFTER 코드 문맥을 제공한다.
10. Conventional Commit 메시지를 제안한다.

패치 작성자는 Phase 1 범위를 벗어난 mutation, rollback behavior 변경, aggressive mode 구현을 같은 패치에 섞지 않는다.

## 20. Non-Goals

다음은 PatchPlan_Phase1.md의 비목표(Non-Goals)다.

```text
구체 C++ 코드 작성
패치 파일 직접 생성
테스트 코드 직접 작성
성능 개선 수치 보장
MainThreadConfidenceAnalyzer 최종 구현
SchedulerController mutation 변경
Aggressive Single-Core Mode 구현
GameProfileRegistry 구현
Release Gate 자동화
```

Phase 1은 안전한 관찰 계층(Observation Layer)과 토폴로지 분석(Topology Analysis)의 기반을 고정하는 문서다.

## 21. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 1의 목적과 비목표가 명확하다.
2. Phase 1 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P1-01부터 P1-10까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 완료 기준, 테스트, Evidence가 정의되어 있다.
5. ThreadTracker observation-only 계약이 최우선으로 다뤄진다.
6. Processor Group group-aware 계약과 group 0 hardcoding 금지가 명확하다.
7. Phase 1 BLOCKER 조건이 정의되어 있다.
8. 패치 순서가 정의되어 있다.
9. 후속 `docs/archive/patch-plans/PatchPlan_Phase2.md` 작성에 필요한 기준이 충분하다.

## 22. Open Questions

다음 질문은 Phase 1 실제 패치 착수 전 또는 P1-01에서 분류한다.

```text
1. ThreadTracker snapshot 타입은 기존 구조를 확장할 것인가, 신규 타입으로 분리할 것인가?
2. migration raw signal은 ThreadTracker가 직접 계산할 것인가, 별도 analyzer가 계산할 것인가?
3. TopologyResult의 processorGroup 필드는 단일 group 기준인가, 복수 group 후보를 포함할 것인가?
4. CCX-like grouping은 v3.1에서 어느 수준까지 지원할 것인가?
5. RuntimeValidation에서 topology incomplete는 WARN인가 BLOCKER인가?
6. Phase 1에서 static check script를 반드시 수정할 것인가?
7. Phase 1 완료 전 실제 게임 dry-run을 수행할 것인가?
```

Open Questions가 해결되기 전까지 관련 구현 범위는 `TBD`로 유지한다. 단, ThreadTracker observation-only와 Processor Group group 0 hardcoding 금지는 Open Question이 아니라 Phase 1 안전 계약이다.
