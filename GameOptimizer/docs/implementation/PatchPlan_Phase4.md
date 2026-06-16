# GameOptimizer v3.1 Patch Plan — Phase 4: SchedulerController Safety Hardening

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 4

버전: v1.0

작성 목적: GameOptimizer v3.1의 네 번째 구현 단계인 Phase 4 — SchedulerController Safety Hardening을 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, 스케줄러 제어기(Scheduler Controller)의 thread/process scheduling mutation 경로를 안전 계약(Safety Contract)에 맞게 강화하기 위한 작업 계획 문서다.

적용 범위:

- SchedulerController mutation ownership 정리
- 스레드 우선순위(Thread Priority) 변경 경로 안전화
- 스레드 그룹 선호도(Thread Group Affinity) 변경 경로 안전화
- 프로세스 우선순위(Process Priority) 변경 경로 안전화
- current state query, rollback state save, ApplyGuard arm, apply, verify, rollback request 경계 정의
- Processor Group safety gate와 group 0 hardcoding fallback 금지
- scheduler apply / verify / rollback request Evidence 기록

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 성능 개선 수치 보장
- MainThreadConfidenceAnalyzer 구현 변경
- PolicyDispatcher 구조 변경
- RollbackManager 내부 대규모 변경
- BackgroundController restriction 확장
- Aggressive Single-Core Mode 구현
- Release Gate 자동화

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/implementation/PatchPlan_Phase1.md`
- `docs/implementation/PatchPlan_Phase2.md`
- `docs/implementation/PatchPlan_Phase3.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-005_RollbackManager.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/performance/PolicySpecification.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`

후속 문서:

- `docs/implementation/PatchPlan_Phase5.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 4의 목표는 더 강한 스케줄링 정책을 추가하는 것이 아니라, 기존 또는 신규 scheduling mutation 경로를 rollback-safe하고 evidence-visible한 형태로 강화하는 것이다.

## 2. Phase 4 목표

Phase 4의 핵심 목표는 SchedulerController가 scheduling mutation의 유일한 소유자임을 명확히 하고, 모든 mutation 경로가 current state query, rollback state save, ApplyGuard arm, apply, verify, rollback request 경계를 따르도록 강화하는 것이다.

필수 목표:

1. SchedulerController mutation ownership 정리
2. thread priority apply 경로 안전화
3. thread group affinity apply 경로 안전화
4. process priority apply 경로 안전화
5. apply 전 current state query 보장
6. rollback state save 보장
7. ApplyGuard arm 보장
8. apply 후 verify 보장
9. failure rollback request 보장
10. scheduler apply evidence 기록

Phase 4는 실제 Win32 scheduling mutation 경로를 다루므로 이전 Phase보다 안전 조건을 더 엄격하게 적용한다. 실패한 apply는 성공으로 위장하지 않고, mutation 결과는 Evidence로 남겨야 한다.

## 3. Phase 4 비목표

다음은 Phase 4에서 구현하지 않는다.

```text
MainThreadConfidenceAnalyzer 변경
PolicyCandidate 구조 변경
PolicyDispatcher validation 구조 변경
RollbackManager 내부 state preservation 대규모 변경
BackgroundController restriction 확장
Aggressive Single-Core Mode 구현
GameProfileRegistry 확장
Before/After performance classification 구현
Release Gate 자동화
```

Phase 4는 SchedulerController의 mutation 안전성을 강화하는 단계이며, 새로운 성능 정책을 무리하게 추가하는 단계가 아니다.

RollbackManager 내부 동작을 대규모로 변경해야 하는 경우 Phase 5로 분리한다.

## 4. 영향 모듈

Phase 4에서 영향을 받을 수 있는 모듈은 직접 영향, 간접 영향, 변경 금지 또는 최소 변경 대상으로 구분한다.

### 4.1 직접 영향 모듈

```text
SchedulerController
RollbackManager call boundary
ApplyGuard call boundary
TopologyAnalyzer consumer
RuntimeValidationMonitor
PolicyTimelineRecorder
PerformanceEvidenceRecorder
```

### 4.2 간접 영향 모듈

```text
PolicyDispatcher
PolicyCandidate
RuntimeContext
ShutdownPipeline
ReleaseChecklist
RC_Runbook
```

간접 영향 모듈은 SchedulerController의 결과를 읽거나 검증할 수 있지만, Phase 4에서 scheduling mutation 책임을 새로 가져서는 안 된다.

### 4.3 변경 금지 또는 최소 변경 모듈

```text
ThreadTracker
MainThreadConfidenceAnalyzer
PerformanceEngine
GameProfileRegistry
BackgroundController
AggressiveSingleCoreController
```

주의:

Phase 4에서 RollbackManager 내부 동작을 대규모로 변경해야 하면 Phase 5로 분리한다.

## 5. 패치 단위 개요

Phase 4는 다음 Patch ID 구조를 사용한다.

```text
P4-01 SchedulerController Contract Alignment
P4-02 Current State Query Boundary
P4-03 Rollback State Save Gate
P4-04 ApplyGuard Arm Gate
P4-05 Thread Priority Apply Safety
P4-06 Thread Group Affinity Apply Safety
P4-07 Process Priority Apply Safety
P4-08 Verify After Apply
P4-09 Failure Rollback Request Path
P4-10 Processor Group Safety Gate
P4-11 Scheduler Evidence Integration
P4-12 RuntimeValidation Scheduler Checks
P4-13 Phase 4 Regression Test & Evidence
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

패치 하나가 SchedulerController safety gate, RollbackManager 내부 구현, PolicyDispatcher validation 구조, Release Gate 자동화를 동시에 변경해서는 안 된다.

## 6. P4-01 SchedulerController Contract Alignment

Patch ID: `P4-01`

목적:

SchedulerController의 책임과 비책임을 MDS-004 기준으로 정렬한다.

작업 범위:

```text
SchedulerController mutation ownership 확인
PolicyDispatcher와의 handoff boundary 확인
RollbackManager 호출 경계 확인
ApplyGuard 생성/arm 경계 확인
ThreadTracker/PerformanceEngine 책임 침범 여부 확인
```

수정 가능 파일:

```text
SchedulerController 관련 소스/헤더
SchedulerController 관련 테스트
docs/implementation/MIGRATION_NOTES_Phase4.md
```

수정 금지 파일:

```text
ThreadTracker
MainThreadConfidenceAnalyzer
PerformanceEngine direct logic
PolicyDispatcher validation internals
```

구현 규칙:

```text
SchedulerController는 판단 계층이 아니라 control layer다.
SchedulerController는 이미 검증된 policy만 처리한다.
SchedulerController 외 모듈에 scheduling mutation 책임을 부여하지 않는다.
```

완료 기준:

```text
SchedulerController의 입력과 mutation 책임이 명확하다.
정책 판단 로직이 SchedulerController 내부로 새로 유입되지 않는다.
PolicyDispatcher와 handoff boundary가 문서/코드 기준으로 일치한다.
```

필수 테스트:

```text
scheduler ownership review test
no performance decision in scheduler test
policy dispatcher handoff boundary test
no ThreadTracker dependency leak test
```

필수 Evidence:

```text
schedulerContractSummary
schedulerMutationOwnershipSummary
outOfScopeResponsibilityList
```

리스크:

```text
SchedulerController에 policy decision logic이 유입되면 Phase 3 경계가 무너질 수 있다.
handoff boundary가 모호하면 invalid policy가 apply 경로로 들어갈 수 있다.
```

Rollback 계획:

```text
책임 경계 변경이 불안정하면 SchedulerController 입력 adapter만 남기고 내부 판단 로직은 제거한다.
PolicyDispatcher validation 변경이 필요하면 Phase 3 후속 패치로 분리한다.
```

## 7. P4-02 Current State Query Boundary

Patch ID: `P4-02`

목적:

모든 scheduling mutation 전에 현재 상태를 조회하는 경계를 고정한다.

작업 범위:

```text
thread priority current state query
thread affinity current state query
process priority current state query
query failure classification
query result evidence
```

수정 가능 파일:

```text
SchedulerController
SchedulerController tests
Evidence recorder 일부
```

수정 금지 파일:

```text
ThreadTracker
PerformanceEngine
PolicyDispatcher validation internals
RollbackManager internals
```

구현 규칙:

```text
current state query 실패 시 mutation 금지.
query result 없이 rollback state save 성공으로 처리하지 않는다.
query failure는 PolicyRejected 또는 MonitoringOnly로 이어질 수 있다.
```

완료 기준:

```text
모든 mutation 경로가 current state query를 선행한다.
query 실패는 Evidence와 함께 apply 차단으로 처리된다.
query result가 rollback state save 입력으로 사용 가능하다.
```

필수 테스트:

```text
thread priority query success test
thread affinity query success test
process priority query success test
query failure blocks mutation test
query evidence recorded test
```

필수 Evidence:

```text
schedulerQueryResult
targetThreadId
targetProcessId
queriedPriority
queriedAffinity
queryFailureReason
```

리스크:

```text
query 실패를 apply 실패로 오분류하면 rollback state가 없는 mutation처럼 보일 수 있다.
query result와 rollback state 저장 범위가 불일치하면 복구가 불가능할 수 있다.
```

Rollback 계획:

```text
query integration이 불안정하면 mutation path를 차단하고 query evidence만 남긴다.
query failure classification은 conservative reject로 되돌린다.
```

## 8. P4-03 Rollback State Save Gate

Patch ID: `P4-03`

목적:

RollbackManager state save 성공 전 mutation을 차단한다.

작업 범위:

```text
thread state save request
process state save request
rollback scope validation
save result inspection
save failure blocks mutation
```

수정 가능 파일:

```text
SchedulerController
RollbackManager call site
SchedulerController tests
```

수정 금지 파일:

```text
RollbackManager internals
ApplyGuard internals
PolicyDispatcher validation internals
BackgroundController
```

구현 규칙:

```text
rollback state save 결과는 반드시 Assign -> Check -> Bind 형태로 검사한다.
return rollbackManager.save(...) 같은 무검사 expected 전파를 금지한다.
save 실패 시 Win32 mutation을 수행하지 않는다.
```

완료 기준:

```text
모든 scheduler mutation은 rollback state save 성공 후에만 수행된다.
save failure는 Evidence와 함께 apply 차단으로 처리된다.
expected save result inspection이 테스트 또는 정적 검사로 확인된다.
```

필수 테스트:

```text
rollback state save success allows next gate test
rollback state save failure blocks mutation test
missing rollback scope blocks mutation test
expected save result inspected test
```

필수 Evidence:

```text
rollbackStateSaveResult
rollbackScope
saveDisposition
saveFailureReason
mutationBlockedBySaveFailure
```

리스크:

```text
RollbackManager internals를 건드리면 Phase 5 범위를 침범할 수 있다.
save failure를 WARN으로 낮추면 mutation safety가 깨진다.
```

Rollback 계획:

```text
state save gate가 불안정하면 mutation을 전면 차단하고 save result evidence만 유지한다.
RollbackManager 내부 변경이 필요하면 Phase 5로 분리한다.
```

BLOCKER 기준:

```text
rollback state 없이 mutation 가능
rollback save 실패 후 mutation 진행
expected save result 무검사 전파
```

## 9. P4-04 ApplyGuard Arm Gate

Patch ID: `P4-04`

목적:

ApplyGuard arm 전 Win32 mutation을 차단한다.

작업 범위:

```text
ApplyGuard 생성 시점 확인
ApplyGuard arm 시점 확인
arm failure handling
arm 이후 apply 경로 확인
commit/rollback/audit 경계 확인
```

수정 가능 파일:

```text
SchedulerController
ApplyGuard call site
SchedulerController tests
```

수정 금지 파일:

```text
ApplyGuard internals
RollbackManager internals
PolicyDispatcher validation internals
BackgroundController
```

구현 규칙:

```text
ApplyGuard arm 실패 시 mutation 금지.
ApplyGuard commit은 verify 성공 이후에만 가능하다.
armed guard를 commit/rollback/audit 없이 방치하지 않는다.
```

완료 기준:

```text
모든 scheduler mutation은 ApplyGuard arm 이후에만 수행된다.
arm failure는 Evidence와 함께 apply 차단으로 처리된다.
verify 성공 전 commit이 불가능하다.
```

필수 테스트:

```text
apply guard arm success allows apply test
apply guard arm failure blocks mutation test
armed guard without commit rollback audit test
verify required before commit test
```

필수 Evidence:

```text
applyGuardCreated
applyGuardArmed
applyGuardArmResult
applyGuardFailureReason
```

리스크:

```text
ApplyGuard lifecycle이 모호하면 destructor failure 또는 uncommitted armed guard가 발생할 수 있다.
verify 전에 commit이 가능하면 실패한 mutation이 성공으로 기록될 수 있다.
```

Rollback 계획:

```text
ApplyGuard arm gate가 불안정하면 mutation을 차단하고 current state query/save까지만 허용한다.
ApplyGuard internals 변경은 Phase 5로 분리한다.
```

BLOCKER 기준:

```text
ApplyGuard arm 전 mutation
ApplyGuard arm 실패 후 mutation
verify 없이 commit
```

## 10. P4-05 Thread Priority Apply Safety

Patch ID: `P4-05`

목적:

thread priority 변경 경로를 안전 계약에 맞게 정리한다.

작업 범위:

```text
target thread identity validation
current priority query
rollback state save
ApplyGuard arm
SetThreadPriority apply
verify priority
failure rollback request
```

수정 가능 파일:

```text
SchedulerController
SchedulerController tests
RuntimeValidationMonitor 일부
```

수정 금지 파일:

```text
ThreadTracker
MainThreadConfidenceAnalyzer
PolicyDispatcher validation internals
RollbackManager internals
ApplyGuard internals
```

구현 규칙:

```text
target thread identity가 불명확하면 apply 금지.
priority apply 후 verify를 수행한다.
verify 실패 시 rollback request를 생성한다.
```

완료 기준:

```text
thread priority apply 경로가 Save -> Arm -> Apply -> Verify -> Commit 또는 Rollback 흐름을 따른다.
identity mismatch 상태에서 mutation이 발생하지 않는다.
```

필수 테스트:

```text
thread priority apply success test
thread identity mismatch blocks apply test
SetThreadPriority failure rollback request test
thread priority verify failure rollback request test
```

필수 Evidence:

```text
threadPriorityApplyResult
targetThreadId
threadIdentityValidationResult
previousThreadPriority
requestedThreadPriority
verifiedThreadPriority
rollbackRequested
```

리스크:

```text
short-lived thread에서 identity mismatch와 stale state가 자주 발생할 수 있다.
SetThreadPriority API 성공만으로 success 처리하면 verify 실패를 놓칠 수 있다.
```

Rollback 계획:

```text
thread identity validation이 불안정하면 thread priority apply를 차단하고 monitoring-only로 fallback한다.
verify 실패는 rollback request로 유지한다.
```

## 11. P4-06 Thread Group Affinity Apply Safety

Patch ID: `P4-06`

목적:

thread group affinity 변경 경로를 Processor Group 안전 계약에 맞게 정리한다.

작업 범위:

```text
TopologyResult processorGroup 확인
KAFFINITY mask 확인
target thread group compatibility 확인
SetThreadGroupAffinity 또는 해당 경로 apply
verify group affinity
unsafe topology block
```

수정 가능 파일:

```text
SchedulerController
TopologyAnalyzer consumer
SchedulerController tests
RuntimeValidationMonitor 일부
```

수정 금지 파일:

```text
TopologyAnalyzer core discovery internals
ThreadTracker
PolicyDispatcher validation internals
RollbackManager internals
ApplyGuard internals
```

구현 규칙:

```text
Processor Group 정보가 없으면 affinity apply 금지.
group 0 hardcoding 금지.
KAFFINITY mask가 target processor group과 일치해야 한다.
topology incomplete 상태에서 aggressive affinity apply 금지.
```

완료 기준:

```text
thread group affinity apply가 group-aware 방식으로만 수행된다.
Processor Group unsafe 상태에서는 policy가 차단된다.
verify group affinity 실패 시 rollback request가 생성된다.
```

필수 테스트:

```text
valid group affinity apply test
missing processor group blocks affinity test
group 0 hardcoding static check
invalid KAFFINITY mask blocks apply test
verify group affinity failure rollback request test
```

필수 Evidence:

```text
threadGroupAffinityApplyResult
targetThreadId
targetProcessorGroup
requestedAffinityMask
verifiedProcessorGroup
verifiedAffinityMask
processorGroupSafetyState
affinityBlockedReason
```

리스크:

```text
Processor Group API와 KAFFINITY mask 해석이 환경별로 달라질 수 있다.
기존 affinity path가 group 0 전제를 갖고 있으면 영향 범위가 커질 수 있다.
```

Rollback 계획:

```text
group-aware affinity path가 불안정하면 affinity mutation을 차단하고 monitoring-only reason을 남긴다.
SetThreadAffinityMask 기반 legacy path는 group-safe 조건이 없으면 사용하지 않는다.
```

BLOCKER 기준:

```text
Processor Group 정보 누락 상태에서 affinity apply
group 0 hardcoding fallback
verify 없이 affinity success 처리
```

## 12. P4-07 Process Priority Apply Safety

Patch ID: `P4-07`

목적:

process priority 변경 경로를 rollback-safe하게 정리한다.

작업 범위:

```text
target process identity validation
current process priority query
rollback state save
ApplyGuard arm
SetPriorityClass apply
verify process priority
failure rollback request
```

수정 가능 파일:

```text
SchedulerController
SchedulerController tests
RuntimeValidationMonitor 일부
```

수정 금지 파일:

```text
PerformanceEngine
PolicyDispatcher validation internals
RollbackManager internals
ApplyGuard internals
BackgroundController
```

구현 규칙:

```text
process identity가 불명확하면 apply 금지.
process priority 변경은 rollback state save 이후에만 가능하다.
verify 실패 시 rollback request를 생성한다.
```

완료 기준:

```text
process priority apply 경로가 Save -> Arm -> Apply -> Verify -> Commit 또는 Rollback 흐름을 따른다.
process identity mismatch 상태에서 mutation이 발생하지 않는다.
```

필수 테스트:

```text
process priority apply success test
process identity mismatch blocks apply test
SetPriorityClass failure rollback request test
process priority verify failure rollback request test
```

필수 Evidence:

```text
processPriorityApplyResult
targetProcessId
processIdentityValidationResult
previousProcessPriority
requestedProcessPriority
verifiedProcessPriority
rollbackRequested
```

리스크:

```text
process priority 변경은 profile 또는 anti-cheat risk와 충돌할 수 있다.
identity validation 없이 apply하면 다른 process에 mutation이 적용될 위험이 있다.
```

Rollback 계획:

```text
process identity validation이 불안정하면 process priority apply를 차단한다.
profile 조건부 적용이 필요하면 Phase 7 또는 profile integration 작업으로 분리한다.
```

## 13. P4-08 Verify After Apply

Patch ID: `P4-08`

목적:

모든 scheduler mutation 이후 verify를 필수화한다.

작업 범위:

```text
thread priority verify
thread group affinity verify
process priority verify
verify failure classification
verify result evidence
commit only after verify success
```

수정 가능 파일:

```text
SchedulerController
SchedulerController tests
Evidence recorder 일부
```

수정 금지 파일:

```text
RollbackManager internals
ApplyGuard internals
PolicyDispatcher validation internals
ThreadTracker
```

구현 규칙:

```text
apply API 성공만으로 success 처리하지 않는다.
verify 성공 전 commit 금지.
verify 실패 시 rollback request.
```

완료 기준:

```text
모든 scheduler mutation 경로에 verify 단계가 존재한다.
verify result가 Evidence에 기록된다.
verify 성공 후에만 commit할 수 있다.
```

필수 테스트:

```text
apply success verify success commit test
apply success verify failure rollback request test
verify result required test
commit before verify forbidden test
```

필수 Evidence:

```text
verifyResult
verifyTarget
expectedState
actualState
verifyFailureReason
commitAllowed
```

리스크:

```text
verify가 지연되거나 불완전하면 정상 apply가 false failure로 분류될 수 있다.
verify result 누락은 RC Evidence에서 scheduler success를 신뢰할 수 없게 만든다.
```

Rollback 계획:

```text
verify가 불안정하면 success commit을 차단하고 rollback request 또는 safe rejection으로 처리한다.
verify 구현 범위는 priority/affinity/process priority별로 분리한다.
```

BLOCKER 기준:

```text
verify 없이 success 처리
verify 실패 후 commit
verify result Evidence 누락
```

## 14. P4-09 Failure Rollback Request Path

Patch ID: `P4-09`

목적:

apply 또는 verify 실패 시 rollback request 경로를 명확히 한다.

작업 범위:

```text
apply failure rollback request
verify failure rollback request
identity mismatch rollback decision
rollback request evidence
RollbackPending transition trigger
```

수정 가능 파일:

```text
SchedulerController
RuntimeStateMachine integration point
RuntimeValidationMonitor
SchedulerController tests
```

수정 금지 파일:

```text
RollbackManager internals
ApplyGuard internals
PolicyDispatcher validation internals
BackgroundController
```

구현 규칙:

```text
apply failure를 단순 WARN으로 종료하지 않는다.
verify failure는 rollback request를 생성해야 한다.
RollbackManager 내부 구현 변경은 Phase 5로 분리한다.
```

완료 기준:

```text
SchedulerController failure가 RollbackPending 또는 safe rejection으로 이어진다.
rollback request가 Evidence에 기록된다.
apply failure와 verify failure가 구분된다.
```

필수 테스트:

```text
apply failure creates rollback request test
verify failure creates rollback request test
identity mismatch blocks or rollback request test
rollback request evidence test
```

필수 Evidence:

```text
rollbackRequested
rollbackReason
failedPolicyType
failedApplyStep
rollbackRequestTimestamp
```

리스크:

```text
RollbackPending 전이와 직접 rollback 실행 경계가 모호하면 Phase 5와 충돌할 수 있다.
identity mismatch 상황에서 rollback request를 잘못 생성하면 다른 대상 복구 위험이 생길 수 있다.
```

Rollback 계획:

```text
RollbackPending integration이 불안정하면 rollback request evidence만 생성하고 실제 transition은 Phase 5로 분리한다.
identity mismatch는 safe rejection 또는 stale/missing classification으로 보수 처리한다.
```

## 15. P4-10 Processor Group Safety Gate

Patch ID: `P4-10`

목적:

Processor Group unsafe 상태에서 scheduler affinity policy를 차단한다.

작업 범위:

```text
TopologyResult completeness 확인
processorGroup availability 확인
group-aware mask 확인
HEDT / Processor Group 1+ 조건 확인
monitoring-only fallback reason 제공
```

수정 가능 파일:

```text
SchedulerController
TopologyAnalyzer consumer
Policy eligibility consumer
RuntimeValidationMonitor 일부
```

수정 금지 파일:

```text
TopologyAnalyzer core discovery internals
ThreadTracker
RollbackManager internals
ApplyGuard internals
```

구현 규칙:

```text
Processor Group 정보가 불완전하면 affinity apply 금지.
process-wide affinity가 group 0 제한을 갖는 경우 unsafe로 분류한다.
HEDT 환경에서 group 0 하드코딩 금지.
```

완료 기준:

```text
processor group unsafe 상태에서 scheduler affinity mutation이 차단된다.
차단 reason이 Evidence에 기록된다.
monitoring-only fallback reason을 제공할 수 있다.
```

필수 테스트:

```text
processor group complete allows eligibility test
processor group incomplete blocks affinity test
group 1 plus unsafe process-wide affinity block test
monitoring-only fallback reason test
```

필수 Evidence:

```text
processorGroupSafetyGateResult
processorGroupCount
selectedProcessorGroup
groupAwareMaskValid
schedulerAffinityBlockedReason
monitoringOnlyReason
```

리스크:

```text
Processor Group safety gate가 Policy Layer와 SchedulerController 양쪽에 중복되면 판정 불일치가 생길 수 있다.
unsafe 판단을 누락하면 group 0 fallback이 숨어 들어갈 수 있다.
```

Rollback 계획:

```text
gate 판단이 불안정하면 affinity mutation을 전면 차단하고 monitoring-only로 fallback한다.
Policy eligibility consumer 연결은 최소화하고 SchedulerController에서 최종 차단을 유지한다.
```

## 16. P4-11 Scheduler Evidence Integration

Patch ID: `P4-11`

목적:

SchedulerController의 apply / verify / rollback request 결과를 Evidence로 남긴다.

작업 범위:

```text
scheduler apply summary
scheduler verify summary
scheduler failure summary
rollback request summary
policy timeline event 기록
runtime snapshot 반영
```

수정 가능 파일:

```text
PolicyTimelineRecorder
RuntimeSnapshotRecorder
PerformanceEvidenceRecorder
SchedulerController
관련 테스트
```

수정 금지 파일:

```text
RollbackManager internals
ApplyGuard internals
PolicyDispatcher validation internals
BackgroundController
```

구현 규칙:

```text
scheduler mutation success는 Evidence 없이 인정하지 않는다.
apply result와 verify result는 분리해서 기록한다.
rollback request는 apply failure와 verify failure를 구분해야 한다.
```

완료 기준:

```text
SchedulerController lifecycle이 Evidence에 기록된다.
RC Report에서 scheduler apply/verify 결과를 확인할 수 있다.
policy timeline에 scheduler event가 남는다.
```

필수 테스트:

```text
scheduler apply evidence test
scheduler verify evidence test
scheduler failure evidence test
scheduler rollback request evidence test
policy timeline scheduler event test
```

필수 Evidence:

```text
schedulerApplySummary
schedulerVerifySummary
schedulerFailureSummary
schedulerRollbackRequestSummary
policyTimelineEventCount
```

리스크:

```text
apply result와 verify result를 합치면 실패 원인을 추적하기 어렵다.
Evidence volume이 커지면 RC Bundle 크기와 report 가독성이 떨어질 수 있다.
```

Rollback 계획:

```text
상세 evidence가 과도하면 summary 우선으로 축소하되 apply/verify/failure/rollback request 구분은 유지한다.
Timeline integration이 불안정하면 runtime snapshot summary를 source of truth로 유지한다.
```

## 17. P4-12 RuntimeValidation Scheduler Checks

Patch ID: `P4-12`

목적:

SchedulerController 계약 위반을 RuntimeValidation에서 감지하게 한다.

작업 범위:

```text
save-before-apply validation
arm-before-apply validation
verify-after-apply validation
processor group safety validation
rollback request validation
active scheduler policy consistency validation
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
SchedulerController
관련 테스트
```

수정 금지 파일:

```text
RollbackManager internals
ApplyGuard internals
PolicyDispatcher validation internals
ThreadTracker
```

구현 규칙:

```text
RuntimeValidation은 mutation을 수행하지 않는다.
RuntimeValidation은 scheduler safety violation을 BLOCKER로 분류할 수 있어야 한다.
save/arm/verify 순서 위반은 release-facing BLOCKER 후보로 남긴다.
```

완료 기준:

```text
SchedulerController 계약 위반이 RuntimeValidation Summary에 반영된다.
scheduler safety violation이 WARN/BLOCKER로 분류된다.
```

필수 테스트:

```text
mutation without rollback state blocker test
mutation without ApplyGuard blocker test
missing verify blocker test
processor group unsafe blocker test
active scheduler policy consistency test
```

필수 Evidence:

```text
schedulerValidationState
schedulerContractWarnCount
schedulerContractBlockerCount
schedulerRuntimeValidationSummary
```

리스크:

```text
RuntimeValidation이 scheduler lifecycle을 잘못 해석하면 정상 safe rejection이 BLOCKER가 될 수 있다.
반대로 sequence violation을 WARN으로 낮추면 release safety가 깨진다.
```

Rollback 계획:

```text
초기 rule은 rollback state, ApplyGuard, verify, Processor Group safety에 집중한다.
과도한 warning rule은 조정하되 mutation without save/arm/verify는 BLOCKER로 유지한다.
```

## 18. P4-13 Phase 4 Regression Test & Evidence

Patch ID: `P4-13`

목적:

Phase 4 패치 묶음의 회귀 테스트와 Evidence 기준을 정리한다.

작업 범위:

```text
Phase 4 test list 작성
Phase 4 static check list 작성
Phase 4 runtime evidence list 작성
Phase 4 release blocker list 작성
RegressionMatrix_v3.1 연결
ReleaseChecklist_v3.1 연결
```

수정 가능 파일:

```text
docs/validation/RegressionMatrix_v3.1.md
docs/release/ReleaseChecklist_v3.1.md
validation scripts if existing
tests/*
```

수정 금지 파일:

```text
MainThreadConfidenceAnalyzer
PolicyDispatcher validation structure
RollbackManager internals
ApplyGuard internals
AggressiveSingleCoreController
```

구현 규칙:

```text
Phase 4 PASS는 성능 향상 PASS가 아니다.
Phase 4 PASS는 scheduler mutation 경로가 safety contract를 만족한다는 의미다.
Regression test는 save/arm/apply/verify, Processor Group safety, failure rollback request, evidence, RuntimeValidation을 분리해 기록한다.
```

완료 기준:

```text
Phase 4 regression checklist가 존재한다.
Phase 4 blocker 조건이 ReleaseChecklist와 연결된다.
Phase 4 Evidence Bundle 최소 항목이 정의된다.
```

필수 테스트:

```text
SchedulerController save arm apply verify suite
Processor Group safety suite
scheduler failure rollback request suite
scheduler evidence suite
RuntimeValidation scheduler suite
```

필수 Evidence:

```text
phase4RegressionSummary
schedulerSafetySummary
processorGroupSafetySummary
schedulerEvidenceSummary
runtimeValidationSummary
```

리스크:

```text
RegressionMatrix_v3.1.md가 아직 없으면 테스트 목록이 문서상 계획에 머물 수 있다.
ReleaseChecklist_v3.1.md 변경이 과도하면 릴리즈 기준과 Phase 4 기준이 섞일 수 있다.
```

Rollback 계획:

```text
신규 regression 문서가 준비되지 않았으면 PatchPlan_Phase4.md의 Evidence 기준을 우선 source of truth로 유지한다.
ReleaseChecklist 수정은 Phase 4 blocker 항목만 최소 반영한다.
```

## 19. Phase 4 완료 기준

Phase 4 전체 완료 기준은 다음과 같다.

1. SchedulerController의 mutation ownership이 명확하다.
2. 모든 scheduler mutation 전에 current state query가 수행된다.
3. rollback state save 성공 전 mutation이 차단된다.
4. ApplyGuard arm 성공 전 mutation이 차단된다.
5. thread priority apply 경로가 verify를 포함한다.
6. thread group affinity apply 경로가 group-aware 방식이다.
7. process priority apply 경로가 verify를 포함한다.
8. verify 실패 시 rollback request가 생성된다.
9. Processor Group unsafe 상태에서 affinity apply가 차단된다.
10. scheduler apply/verify/failure 결과가 Evidence에 기록된다.
11. RuntimeValidation이 scheduler safety violation을 감지한다.
12. Phase 4 regression test가 정의되어 있다.

완료 판단은 성능 개선 여부가 아니라 scheduler mutation 경로의 rollback safety와 evidence visibility를 기준으로 한다.

## 20. Phase 4 BLOCKER 조건

다음 조건은 Phase 4 BLOCKER로 분류한다.

```text
rollback state 없이 scheduler mutation
ApplyGuard arm 없이 scheduler mutation
verify 없이 scheduler success 처리
verify 실패 후 commit
Processor Group 정보 누락 상태에서 affinity apply
group 0 hardcoding fallback
identity mismatch 상태에서 mutation
apply failure를 rollback request 없이 무시
scheduler success Evidence 누락
RuntimeValidation이 scheduler contract violation을 무시
```

BLOCKER가 발생하면 후속 patch보다 해당 blocker 제거를 우선한다. 특히 P4-03, P4-04, P4-08의 BLOCKER는 Phase 4 전체 진행을 차단한다.

## 21. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P4-01 SchedulerController Contract Alignment
2. P4-02 Current State Query Boundary
3. P4-03 Rollback State Save Gate
4. P4-04 ApplyGuard Arm Gate
5. P4-05 Thread Priority Apply Safety
6. P4-06 Thread Group Affinity Apply Safety
7. P4-07 Process Priority Apply Safety
8. P4-08 Verify After Apply
9. P4-09 Failure Rollback Request Path
10. P4-10 Processor Group Safety Gate
11. P4-11 Scheduler Evidence Integration
12. P4-12 RuntimeValidation Scheduler Checks
13. P4-13 Phase 4 Regression Test & Evidence
```

주의:

```text
P4-03, P4-04, P4-08은 Phase 4의 핵심 안전 패치다.
이 세 항목 중 하나라도 실패하면 실제 scheduling mutation 확장은 중단한다.
P4-06과 P4-10에서 group 0 hardcoding이 발견되면 즉시 BLOCKER로 처리한다.
```

## 22. 패치 작성 규칙

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

패치 작성자는 Phase 4 범위를 벗어난 confidence logic, policy validation structure, rollback manager internals, aggressive mode 구현을 같은 패치에 섞지 않는다.

## 23. Non-Goals

다음은 PatchPlan_Phase4.md의 비목표(Non-Goals)다.

```text
구체 C++ 코드 작성
패치 파일 직접 생성
테스트 코드 직접 작성
성능 개선 수치 보장
MainThreadConfidenceAnalyzer 구현 변경
PolicyDispatcher 구조 변경
RollbackManager 내부 대규모 변경
BackgroundController restriction 확장
Aggressive Single-Core Mode 구현
Release Gate 자동화
```

Phase 4는 SchedulerController scheduling mutation 경로를 안전하게 만드는 문서다.

## 24. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 4의 목적과 비목표가 명확하다.
2. Phase 4 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P4-01부터 P4-13까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 완료 기준, 테스트, Evidence가 정의되어 있다.
5. SchedulerController의 mutation safety gate가 명확하다.
6. Save -> Arm -> Apply -> Verify -> Commit/Rollback 흐름이 명확하다.
7. Processor Group unsafe 상태에서 affinity apply 금지가 명확하다.
8. Phase 4 BLOCKER 조건이 정의되어 있다.
9. 패치 순서가 정의되어 있다.
10. 후속 `docs/implementation/PatchPlan_Phase5.md` 작성에 필요한 기준이 충분하다.

## 25. Open Questions

다음 질문은 Phase 4 실제 패치 착수 전 또는 P4-01에서 분류한다.

```text
1. SchedulerController의 thread priority apply와 affinity apply를 별도 transaction으로 둘 것인가, 하나의 transaction으로 묶을 것인가?
2. process priority apply는 v3.1에서 항상 허용할 것인가, profile 조건부로 둘 것인가?
3. verify 실패 시 즉시 rollback할 것인가, RuntimeStateMachine의 RollbackPending으로 넘길 것인가?
4. Thread Group Affinity apply는 SetThreadGroupAffinity를 기본으로 둘 것인가, 기존 SetThreadAffinityMask 경로를 제한적으로 유지할 것인가?
5. Processor Group 1+ 환경에서 process-wide affinity는 monitoring-only로 제한할 것인가?
6. SchedulerController evidence는 RuntimeSnapshotRecorder와 PolicyTimelineRecorder 중 어디를 primary로 둘 것인가?
7. apply 실패와 verify 실패의 release severity를 동일하게 둘 것인가?
8. Phase 4 완료 전 실제 게임 soft-apply validation을 수행할 것인가?
```

Open Questions가 해결되기 전까지 관련 구현 범위는 `TBD`로 유지한다. 단, Rollback state save 없는 mutation, ApplyGuard arm 없는 mutation, Processor Group group 0 hardcoding을 허용하지 않는 것은 Open Question이 아니라 Phase 4 안전 계약이다.
