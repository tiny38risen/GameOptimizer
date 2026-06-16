# GameOptimizer v3.1 Patch Plan — Phase 5: Rollback / ApplyGuard Contract Hardening

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 5

버전: v1.0

작성 목적: GameOptimizer v3.1의 다섯 번째 구현 단계인 Phase 5 — Rollback / ApplyGuard Contract Hardening을 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, 롤백 관리자(Rollback Manager)와 적용 가드(Apply Guard)의 원상 상태 저장(Original State Save), 신원 재검증(Identity Revalidation), rollback execution, 실패 상태 보존(Failed State Preservation), 트랜잭션 경계(Transaction Boundary), 소멸자 감사(Destructor Audit), rollback evidence를 강화하기 위한 작업 계획 문서다.

적용 범위:

- RollbackManager state ownership 정리
- rollback state save result model 정리
- identity revalidation 계약 정리
- rollback execution classification 정리
- failed rollback state preservation 정리
- ApplyGuard state machine과 destructor audit 정리
- shutdown rollback path 정리
- rollback evidence와 RuntimeValidation 연계

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 직접 생성
- 테스트 코드 직접 작성
- 성능 개선 수치 보장
- SchedulerController 신규 정책 구현
- BackgroundController restriction 확장
- PerformanceEngine 정책 변경
- Aggressive Single-Core Mode 구현
- Release Gate 자동화

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/implementation/PatchPlan_Phase1.md`
- `docs/implementation/PatchPlan_Phase2.md`
- `docs/implementation/PatchPlan_Phase3.md`
- `docs/implementation/PatchPlan_Phase4.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-005_RollbackManager.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`

후속 문서:

- `docs/implementation/PatchPlan_Phase6.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 5의 목표는 rollback을 단순 복구 기능으로 두는 것이 아니라, GameOptimizer의 모든 mutation이 실패해도 추적 가능하고 보존 가능한 transaction으로 남도록 만드는 것이다.

## 2. Phase 5 목표

Phase 5의 핵심 목표는 RollbackManager와 ApplyGuard가 mutation safety의 최종 방어선으로 동작하도록 계약을 강화하는 것이다.

필수 목표:

1. RollbackManager responsibility 정리
2. Rollback state save contract 정리
3. identity revalidation contract 정리
4. rollback execution result classification 정리
5. failed rollback state preservation 정리
6. ApplyGuard state machine 정리
7. ApplyGuard destructor audit 정리
8. rollback evidence integration 정리
9. RuntimeValidation rollback checks 정리
10. ReleaseChecklist rollback blocker 연결

Phase 5는 mutation 전 rollback state save와 ApplyGuard arm을 필수 계약으로 고정한다. rollback failure는 release-facing evidence와 RuntimeValidation에 반영되어야 하며, 실패 상태는 폐기하지 않는다.

## 3. Phase 5 비목표

다음은 Phase 5에서 구현하지 않는다.

```text
SchedulerController 신규 정책 추가
BackgroundController restriction 확장
PerformanceEngine 정책 판단 변경
PolicyDispatcher 구조 변경
Aggressive Single-Core Mode 구현
Before/After performance classification 구현
GameProfileRegistry 확장
Release Gate 자동화
UI 변경
```

Phase 5는 새로운 성능 정책을 추가하는 단계가 아니라, 기존 mutation을 안전하게 되돌릴 수 있는 계약을 강화하는 단계다.

Phase 5에서 SchedulerController의 apply 정책 자체를 변경해야 하는 경우 Phase 4 범위로 되돌려 분리한다. Phase 5에서 BackgroundController의 restriction 정책 자체를 확장해야 하는 경우 Phase 6 또는 별도 Phase로 분리한다.

## 4. 영향 모듈

Phase 5에서 영향을 받을 수 있는 모듈은 직접 영향, 간접 영향, 변경 금지 또는 최소 변경 대상으로 구분한다.

### 4.1 직접 영향 모듈

```text
RollbackManager
ApplyGuard
SchedulerController rollback call boundary
BackgroundController rollback call boundary
ShutdownPipeline
RuntimeValidationMonitor
RuntimeSnapshotRecorder
PolicyTimelineRecorder
```

### 4.2 간접 영향 모듈

```text
SchedulerController
BackgroundController
RuntimeOrchestrator
RuntimeStateMachine
ReleaseChecklist
RC_Runbook
Evidence recorder
```

간접 영향 모듈은 rollback request 또는 rollback result를 생성/소비할 수 있지만, RollbackManager의 state ownership을 침범해서는 안 된다.

### 4.3 변경 금지 또는 최소 변경 모듈

```text
ThreadTracker
MainThreadConfidenceAnalyzer
PerformanceEngine
PolicyDispatcher
TopologyAnalyzer
GameProfileRegistry
AggressiveSingleCoreController
```

주의:

Phase 5에서 SchedulerController의 apply 정책 자체를 변경해야 하는 경우 Phase 4 범위로 되돌려 분리한다.
Phase 5에서 BackgroundController의 restriction 정책 자체를 확장해야 하는 경우 Phase 6 또는 별도 Phase로 분리한다.

## 5. 패치 단위 개요

Phase 5는 다음 Patch ID 구조를 사용한다.

```text
P5-01 Rollback / ApplyGuard Contract Alignment
P5-02 Rollback State Save Result Model
P5-03 Identity Revalidation Contract
P5-04 Rollback Execution Classification
P5-05 Failed Rollback State Preservation
P5-06 ApplyGuard State Machine Hardening
P5-07 ApplyGuard Destructor Audit
P5-08 Rollback Request Ownership Boundary
P5-09 Shutdown Rollback Path
P5-10 Rollback Evidence Integration
P5-11 RuntimeValidation Rollback Checks
P5-12 Phase 5 Regression Test & Evidence
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

패치 하나가 RollbackManager contract, SchedulerController 신규 정책, BackgroundController restriction 확장, Release Gate 자동화를 동시에 변경해서는 안 된다.

## 6. P5-01 Rollback / ApplyGuard Contract Alignment

Patch ID: `P5-01`

목적:

RollbackManager와 ApplyGuard의 책임과 비책임을 MDS-005 및 Safety Contract 기준으로 정렬한다.

작업 범위:

```text
RollbackManager state ownership 확인
ApplyGuard transaction ownership 확인
SchedulerController / BackgroundController와의 호출 경계 확인
RollbackManager가 policy 판단을 하지 않는지 확인
ApplyGuard가 apply 결과를 임의로 성공 처리하지 않는지 확인
```

수정 가능 파일:

```text
RollbackManager 관련 소스/헤더
ApplyGuard 관련 소스/헤더
관련 테스트
docs/implementation/MIGRATION_NOTES_Phase5.md
```

수정 금지 파일:

```text
ThreadTracker
MainThreadConfidenceAnalyzer
PerformanceEngine
PolicyDispatcher
TopologyAnalyzer
```

구현 규칙:

```text
RollbackManager는 rollback state와 rollback execution의 소유자다.
ApplyGuard는 transaction boundary의 소유자다.
둘 다 policy 판단자가 아니다.
```

완료 기준:

```text
RollbackManager와 ApplyGuard의 책임 경계가 명확하다.
policy 판단 로직이 RollbackManager 또는 ApplyGuard로 유입되지 않는다.
SchedulerController / BackgroundController 호출 경계가 문서와 일치한다.
```

필수 테스트:

```text
rollback manager ownership review test
apply guard transaction boundary review test
no policy decision in rollback manager test
controller call boundary review test
```

필수 Evidence:

```text
rollbackContractSummary
applyGuardContractSummary
outOfScopeResponsibilityList
```

리스크:

```text
RollbackManager가 policy success/failure를 판단하기 시작하면 Policy Layer와 책임이 충돌한다.
ApplyGuard가 verify 결과 없이 commit을 허용하면 Phase 4 안전 계약이 깨진다.
```

Rollback 계획:

```text
책임 경계 변경이 불안정하면 계약 문서와 테스트만 남기고 코드 변경은 후속 패치로 분리한다.
policy 판단 로직이 발견되면 PerformanceEngine 또는 PolicyDispatcher 경계로 되돌린다.
```

## 7. P5-02 Rollback State Save Result Model

Patch ID: `P5-02`

목적:

rollback state save 결과를 명확한 result model로 분류한다.

작업 범위:

```text
new state saved
existing state reused
save failed
invalid target
identity mismatch
unsupported rollback scope
save result evidence
```

수정 가능 파일:

```text
RollbackManager
RollbackManager tests
SchedulerController call site if needed
BackgroundController call site if needed
```

수정 금지 파일:

```text
ThreadTracker
MainThreadConfidenceAnalyzer
PerformanceEngine
PolicyDispatcher
TopologyAnalyzer
```

구현 규칙:

```text
save result는 반드시 호출부에서 검사되어야 한다.
std::expected 결과를 검사 없이 return func(...) 형태로 전파하지 않는다.
save failed 상태에서 mutation이 진행되면 BLOCKER다.
```

완료 기준:

```text
rollback state save result가 Created / Reused / Failed / InvalidTarget 등으로 분류된다.
호출부가 save result를 검사한 뒤 다음 단계로 진행한다.
save result Evidence가 release-facing summary에 연결 가능하다.
```

필수 테스트:

```text
rollback state created test
rollback state reused test
rollback state save failure test
invalid target save failure test
expected save result inspected test
```

필수 Evidence:

```text
rollbackStateSaveResult
saveDisposition
rollbackScope
saveFailureReason
savedTargetIdentity
```

리스크:

```text
save disposition taxonomy가 불명확하면 caller가 실패를 성공처럼 처리할 수 있다.
expected 결과 무검사 전파가 남아 있으면 mutation gate가 우회될 수 있다.
```

Rollback 계획:

```text
세부 disposition이 불안정하면 Created / Reused / Failed / InvalidTarget 최소 모델로 축소한다.
caller integration이 불안정하면 save failure 시 mutation 전면 차단을 유지한다.
```

BLOCKER 기준:

```text
rollback save result 무검사 전파
save failure 후 mutation 진행
save result Evidence 누락
```

## 8. P5-03 Identity Revalidation Contract

Patch ID: `P5-03`

목적:

rollback 전 target process/thread identity를 재검증하는 계약을 정의한다.

작업 범위:

```text
process id revalidation
process creation time revalidation
thread id revalidation
stale/missing identity classification
living same identity classification
identity mismatch evidence
```

수정 가능 파일:

```text
RollbackManager
RollbackManager tests
RuntimeValidationMonitor 일부
```

수정 금지 파일:

```text
ThreadTracker
MainThreadConfidenceAnalyzer
PerformanceEngine
PolicyDispatcher
SchedulerController apply policy logic
```

구현 규칙:

```text
PID만으로 동일 프로세스라고 단정하지 않는다.
가능한 경우 creation time 또는 동등한 identity evidence를 사용한다.
identity mismatch를 성공 rollback으로 처리하지 않는다.
```

완료 기준:

```text
rollback 전 identity revalidation이 수행된다.
stale/missing target과 living same identity target이 구분된다.
identity mismatch는 success rollback으로 분류되지 않는다.
```

필수 테스트:

```text
same process identity valid test
process exited stale identity test
pid reused identity mismatch test
thread missing identity test
identity mismatch blocks success classification test
```

필수 Evidence:

```text
identityValidationResult
targetProcessId
targetProcessCreationTime
targetThreadId
identityMismatchReason
staleOrMissingIdentity
```

리스크:

```text
process creation time 확보가 환경별로 실패할 수 있다.
stale/missing identity와 living same identity failure를 혼동하면 rollback severity가 잘못 분류된다.
```

Rollback 계획:

```text
creation time 확보가 불가능한 경우 fallback 기준은 TBD로 두고 rollback success 분류를 보수적으로 제한한다.
identity evidence가 부족하면 recoverable 또는 success로 단정하지 않는다.
```

BLOCKER 기준:

```text
identity validation 없이 rollback success 처리
PID만으로 identity 확정
identity mismatch 무시
```

## 9. P5-04 Rollback Execution Classification

Patch ID: `P5-04`

목적:

rollback 실행 결과를 release-facing severity 기준으로 분류한다.

작업 범위:

```text
rollback success
rollback partially failed
rollback failed stale/missing identity
rollback failed living same identity
rollback unsupported
rollback skipped with reason
release severity mapping
```

수정 가능 파일:

```text
RollbackManager
RuntimeValidationMonitor
Rollback summary recorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply policy logic
BackgroundController restriction policy logic
PerformanceEngine
PolicyDispatcher
```

구현 규칙:

```text
living same identity rollback failure는 BLOCKER 후보로 분류한다.
stale/missing identity는 recoverable로 분류할 수 있으나 Evidence는 필수다.
rollback failure를 WARN/INFO로 임의 하향하지 않는다.
```

완료 기준:

```text
rollback result가 명확히 분류된다.
release-facing severity가 rollback result에 포함된다.
rollback classification이 RuntimeValidation과 Release Evidence에 전달 가능하다.
```

필수 테스트:

```text
rollback success classification test
stale identity recoverable classification test
living same identity failure blocker test
partial rollback failure test
unsupported rollback classification test
```

필수 Evidence:

```text
rollbackResult
rollbackClassification
rollbackReleaseSeverity
rollbackFailureReason
recoverableReason
```

리스크:

```text
recoverable classification을 과도하게 사용하면 실제 rollback failure가 낮은 severity로 보일 수 있다.
unsupported rollback을 success처럼 처리하면 RC 승인 기준이 왜곡된다.
```

Rollback 계획:

```text
classification이 불안정하면 living same identity failure와 unsupported rollback을 BLOCKER 후보로 보수 분류한다.
recoverable 분류는 stale/missing identity evidence가 충분할 때만 허용한다.
```

## 10. P5-05 Failed Rollback State Preservation

Patch ID: `P5-05`

목적:

rollback 실패 시 실패한 rollback state를 폐기하지 않고 보존한다.

작업 범위:

```text
failed rollback target preservation
failed state serialization or retention policy
retry eligibility
shutdown retry eligibility
release evidence linkage
```

수정 가능 파일:

```text
RollbackManager
Rollback summary recorder
RuntimeSnapshotRecorder
관련 테스트
```

수정 금지 파일:

```text
SchedulerController apply policy logic
BackgroundController restriction policy logic
PerformanceEngine
PolicyDispatcher
```

구현 규칙:

```text
rollback 실패 state를 성공처럼 제거하지 않는다.
failed state preserved=false는 BLOCKER 후보다.
retry 가능 여부를 Evidence로 남긴다.
```

완료 기준:

```text
rollback 실패 시 failed state가 보존된다.
failed state preservation 결과가 Evidence에 기록된다.
retry eligibility와 shutdown retry eligibility가 구분된다.
```

필수 테스트:

```text
failed rollback state preserved test
failed state not removed as success test
retry eligibility recorded test
failed state evidence test
```

필수 Evidence:

```text
failedRollbackTarget
failedStatePreserved
preservedStateId
retryEligibility
shutdownRetryEligibility
```

리스크:

```text
failed state 보존이 메모리 누수 또는 stale state 누적으로 이어질 수 있다.
artifact serialization 범위를 과도하게 넓히면 민감한 runtime state가 남을 수 있다.
```

Rollback 계획:

```text
serialization이 불안정하면 in-memory preservation과 summary evidence만 우선 유지한다.
보존된 state cleanup은 success가 확인된 뒤에만 수행한다.
```

BLOCKER 기준:

```text
rollback 실패 state 폐기
failedStatePreserved 누락
rollback 실패를 성공으로 정리
```

## 11. P5-06 ApplyGuard State Machine Hardening

Patch ID: `P5-06`

목적:

ApplyGuard의 내부 상태 전이를 명확히 하고 illegal transition을 차단한다.

작업 범위:

```text
Created
Armed
Applied
Verified
Committed
RollbackRequested
RolledBack
Audited
Failed
state transition validation
illegal transition evidence
```

수정 가능 파일:

```text
ApplyGuard
ApplyGuard tests
SchedulerController call site if needed
BackgroundController call site if needed
```

수정 금지 파일:

```text
SchedulerController apply policy logic
BackgroundController restriction policy logic
RollbackManager policy interpretation
PerformanceEngine
PolicyDispatcher
```

구현 규칙:

```text
commit은 verify 성공 이후에만 가능하다.
rollback 또는 audit 없이 armed state를 방치하지 않는다.
illegal transition은 BLOCKER 후보로 기록한다.
```

완료 기준:

```text
ApplyGuard state transition이 명확하게 제한된다.
illegal transition이 테스트로 차단된다.
state transition evidence가 RuntimeValidation과 연결 가능하다.
```

필수 테스트:

```text
apply guard normal transition test
commit before verify forbidden test
rollback from armed state test
double commit forbidden test
unresolved armed guard audit test
illegal transition evidence test
```

필수 Evidence:

```text
applyGuardStateBefore
applyGuardStateAfter
applyGuardTransitionReason
applyGuardIllegalTransition
```

리스크:

```text
state machine을 과도하게 엄격히 만들면 기존 valid flow가 illegal로 오분류될 수 있다.
call site와 ApplyGuard state transition이 동시에 바뀌면 원인 추적이 어려울 수 있다.
```

Rollback 계획:

```text
새 state machine이 불안정하면 illegal transition reporting만 유지하고 commit-before-verify 차단은 유지한다.
call site 변경은 SchedulerController와 BackgroundController별로 분리한다.
```

## 12. P5-07 ApplyGuard Destructor Audit

Patch ID: `P5-07`

목적:

ApplyGuard destructor에서 미정리 transaction을 감사 가능하게 만든다.

작업 범위:

```text
armed but not committed detection
applied but not verified detection
rollback not completed detection
audit result evidence
destructor failure classification
```

수정 가능 파일:

```text
ApplyGuard
ApplyGuard tests
Logger or Evidence recorder if existing boundary allows
```

수정 금지 파일:

```text
SchedulerController apply policy logic
RollbackManager execution logic
PerformanceEngine
PolicyDispatcher
```

구현 규칙:

```text
destructor는 예외를 던지지 않는다.
미정리 transaction은 조용히 무시하지 않는다.
audit evidence 또는 log를 남긴다.
```

완료 기준:

```text
ApplyGuard destructor가 unresolved transaction을 감지한다.
unresolved transaction audit 결과가 Evidence 또는 release-facing log로 남는다.
destructor no-throw 계약이 테스트된다.
```

필수 테스트:

```text
armed guard destructor audit test
applied unverified guard destructor audit test
rollback pending guard destructor audit test
destructor no throw test
```

필수 Evidence:

```text
applyGuardDestructorAudit
unresolvedTransactionDetected
destructorAuditSeverity
destructorAuditReason
```

리스크:

```text
destructor에서 Evidence recorder에 직접 접근하면 lifetime 문제를 만들 수 있다.
audit을 log-only로 둘 경우 RuntimeValidation과 release evidence 연결이 약할 수 있다.
```

Rollback 계획:

```text
Evidence recorder 연동이 위험하면 no-throw log-only audit으로 축소한다.
unresolved transaction detection은 유지하고 RuntimeValidation 연결은 후속 패치로 분리한다.
```

BLOCKER 기준:

```text
armed guard가 audit 없이 소멸
applied-but-unverified 상태가 조용히 무시됨
destructor에서 예외 발생
```

## 13. P5-08 Rollback Request Ownership Boundary

Patch ID: `P5-08`

목적:

rollback request 생성 주체와 rollback 실행 주체를 구분한다.

작업 범위:

```text
SchedulerController rollback request
BackgroundController rollback request
RuntimeStateMachine RollbackPending 연결
RollbackManager execution ownership
duplicate rollback request handling
```

수정 가능 파일:

```text
RollbackManager
SchedulerController call boundary
BackgroundController call boundary
RuntimeStateMachine integration point
관련 테스트
```

수정 금지 파일:

```text
PerformanceEngine
PolicyDispatcher
ThreadTracker
MainThreadConfidenceAnalyzer
TopologyAnalyzer
```

구현 규칙:

```text
Controller는 rollback request를 만들 수 있다.
RollbackManager는 rollback execution의 소유자다.
RollbackManager가 policy success/failure 판단을 대신하지 않는다.
```

완료 기준:

```text
rollback request와 rollback execution의 경계가 명확하다.
중복 rollback request가 안전하게 처리된다.
RollbackPending 상태와 rollback execution ownership이 충돌하지 않는다.
```

필수 테스트:

```text
scheduler rollback request test
background rollback request test
duplicate rollback request test
rollback manager execution ownership test
```

필수 Evidence:

```text
rollbackRequestSource
rollbackRequestReason
rollbackExecutionOwner
duplicateRollbackRequestHandled
```

리스크:

```text
request와 execution ownership이 섞이면 rollback 중복 실행 또는 누락이 발생할 수 있다.
RuntimeStateMachine RollbackPending 전이가 직접 rollback 실행과 충돌할 수 있다.
```

Rollback 계획:

```text
RuntimeStateMachine 연결이 불안정하면 rollback request evidence만 남기고 execution은 기존 경로를 유지한다.
duplicate rollback request는 idempotent reject로 처리한다.
```

## 14. P5-09 Shutdown Rollback Path

Patch ID: `P5-09`

목적:

ShutdownRequested / ShuttingDown 상태에서 rollback path가 안전하게 수행되도록 정리한다.

작업 범위:

```text
shutdown rollback ordering
new apply blocked after ShutdownRequested
active mutation rollback
final rollback summary
shutdown retry handling
clean shutdown classification
```

수정 가능 파일:

```text
ShutdownPipeline
RollbackManager
RuntimeOrchestrator
RuntimeValidationMonitor
관련 테스트
```

수정 금지 파일:

```text
PerformanceEngine
PolicyDispatcher
ThreadTracker
MainThreadConfidenceAnalyzer
GameProfileRegistry
```

구현 규칙:

```text
ShutdownRequested 이후 신규 apply 금지.
shutdown 중 rollback evidence flush는 필수다.
rollback failure가 있으면 clean shutdown으로 위장하지 않는다.
```

완료 기준:

```text
shutdown path에서 active mutation이 rollback된다.
final rollback summary가 생성된다.
rollback failure가 있으면 shutdownFailureReason에 반영된다.
```

필수 테스트:

```text
shutdown blocks new apply test
shutdown rollback active policy test
shutdown rollback failure classification test
final rollback summary required test
clean shutdown with rollback success test
```

필수 Evidence:

```text
shutdownRollbackStarted
shutdownRollbackResult
activePolicyAtShutdown
finalRollbackSummary
cleanShutdown
shutdownFailureReason
```

리스크:

```text
shutdown 중 evidence flush와 rollback execution 순서가 꼬이면 final summary가 누락될 수 있다.
shutdown retry를 무제한 허용하면 종료가 지연될 수 있다.
```

Rollback 계획:

```text
shutdown rollback ordering이 불안정하면 신규 apply 차단과 final summary 생성만 우선 적용한다.
retry 횟수는 TBD로 두고 bounded retry 또는 no retry로 보수 처리한다.
```

## 15. P5-10 Rollback Evidence Integration

Patch ID: `P5-10`

목적:

rollback state save, rollback execution, failure preservation 결과를 Evidence에 통합한다.

작업 범위:

```text
rollback summary
rollback timeline event
runtime snapshot rollback state
release evidence rollback section
failed state evidence
```

수정 가능 파일:

```text
RuntimeSnapshotRecorder
PolicyTimelineRecorder
PerformanceEvidenceRecorder
Rollback summary recorder
RC report generator if existing
관련 테스트
```

수정 금지 파일:

```text
RollbackManager policy interpretation
SchedulerController apply policy logic
BackgroundController restriction policy logic
PerformanceEngine
PolicyDispatcher
```

구현 규칙:

```text
Rollback Summary 누락은 Release 단계에서 BLOCKER 후보다.
rollback result와 release severity는 Evidence에 포함되어야 한다.
failed state preservation 결과는 Evidence에서 누락되면 안 된다.
```

완료 기준:

```text
Rollback Evidence가 session / runtime / release evidence에 반영된다.
RC report에서 rollback 상태를 확인할 수 있다.
rollback evidence completeness를 판정할 수 있다.
```

필수 테스트:

```text
rollback summary generated test
rollback timeline event test
failed rollback evidence test
rollback release severity evidence test
missing rollback summary blocker test
```

필수 Evidence:

```text
rollbackSummary
rollbackTimelineEventCount
rollbackReleaseSeverity
failedRollbackStateList
rollbackEvidenceCompleteness
```

리스크:

```text
Rollback evidence가 여러 recorder에 중복 저장되면 불일치가 발생할 수 있다.
failed state list가 과도하게 상세하면 artifact 크기와 민감 정보 위험이 생길 수 있다.
```

Rollback 계획:

```text
Evidence integration이 불안정하면 rollbackSummary를 primary source로 두고 timeline/snapshot은 summary reference만 남긴다.
failed state detail은 최소 식별자와 reason 중심으로 축소한다.
```

## 16. P5-11 RuntimeValidation Rollback Checks

Patch ID: `P5-11`

목적:

rollback 관련 계약 위반을 RuntimeValidation에서 감지하게 한다.

작업 범위:

```text
mutation without rollback state validation
rollback state consistency validation
rollback failure severity validation
failed state preservation validation
shutdown rollback consistency validation
ApplyGuard unresolved transaction validation
```

수정 가능 파일:

```text
RuntimeValidationMonitor
RuntimeSnapshotRecorder
RollbackManager
ApplyGuard
관련 테스트
```

수정 금지 파일:

```text
PerformanceEngine
PolicyDispatcher
ThreadTracker
MainThreadConfidenceAnalyzer
GameProfileRegistry
```

구현 규칙:

```text
RuntimeValidation은 rollback failure를 낮은 severity로 숨기지 않는다.
RuntimeValidation BLOCKER는 RC PASS를 차단해야 한다.
failed state preservation missing은 BLOCKER 후보다.
```

완료 기준:

```text
rollback contract violation이 RuntimeValidation Summary에 반영된다.
BLOCKER 조건이 ReleaseChecklist와 연결된다.
ApplyGuard unresolved transaction이 RuntimeValidation에서 확인 가능하다.
```

필수 테스트:

```text
mutation without rollback state blocker test
rollback failure blocker test
failed state not preserved blocker test
unresolved apply guard blocker test
shutdown rollback missing blocker test
```

필수 Evidence:

```text
rollbackValidationState
rollbackContractWarnCount
rollbackContractBlockerCount
applyGuardValidationState
runtimeValidationSummary
```

리스크:

```text
RuntimeValidation rule이 과도하면 recoverable stale/missing identity도 BLOCKER로 오분류될 수 있다.
반대로 living same identity rollback failure를 낮추면 release safety가 깨진다.
```

Rollback 계획:

```text
초기 rule은 mutation without rollback state, living same identity failure, failed state not preserved, unresolved ApplyGuard에 집중한다.
stale/missing identity recoverable 판단은 evidence가 충분할 때만 허용한다.
```

## 17. P5-12 Phase 5 Regression Test & Evidence

Patch ID: `P5-12`

목적:

Phase 5 패치 묶음의 회귀 테스트와 Evidence 기준을 정리한다.

작업 범위:

```text
Phase 5 test list 작성
Phase 5 static check list 작성
Phase 5 runtime evidence list 작성
Phase 5 release blocker list 작성
RegressionMatrix_v3.1 연결
ReleaseChecklist_v3.1 연결
RC_Runbook_v3.1 연결
```

수정 가능 파일:

```text
docs/validation/RegressionMatrix_v3.1.md
docs/release/ReleaseChecklist_v3.1.md
docs/release/RC_Runbook_v3.1.md
validation scripts if existing
tests/*
```

수정 금지 파일:

```text
SchedulerController 신규 정책 구현
BackgroundController restriction 확장
PerformanceEngine 정책 변경
AggressiveSingleCoreController
```

구현 규칙:

```text
Phase 5 PASS는 성능 향상 PASS가 아니다.
Phase 5 PASS는 rollback/apply transaction 계약이 safety contract를 만족한다는 의미다.
Regression test는 RollbackManager, ApplyGuard, shutdown rollback, rollback evidence, RuntimeValidation을 분리해 기록한다.
```

완료 기준:

```text
Phase 5 regression checklist가 존재한다.
Phase 5 blocker 조건이 ReleaseChecklist와 연결된다.
Phase 5 Evidence Bundle 최소 항목이 정의된다.
```

필수 테스트:

```text
RollbackManager save/rollback classification suite
ApplyGuard state machine suite
ApplyGuard destructor audit suite
shutdown rollback suite
rollback evidence suite
RuntimeValidation rollback suite
```

필수 Evidence:

```text
phase5RegressionSummary
rollbackManagerSummary
applyGuardSummary
shutdownRollbackSummary
rollbackEvidenceSummary
runtimeValidationSummary
```

리스크:

```text
RegressionMatrix_v3.1.md가 아직 없으면 테스트 목록이 문서상 계획에 머물 수 있다.
ReleaseChecklist_v3.1.md 변경이 과도하면 릴리즈 기준과 Phase 5 기준이 섞일 수 있다.
```

Rollback 계획:

```text
신규 regression 문서가 준비되지 않았으면 PatchPlan_Phase5.md의 Evidence 기준을 우선 source of truth로 유지한다.
ReleaseChecklist와 RC_Runbook 수정은 Phase 5 blocker 항목만 최소 반영한다.
```

## 18. Phase 5 완료 기준

Phase 5 전체 완료 기준은 다음과 같다.

1. RollbackManager의 state ownership이 명확하다.
2. rollback state save result가 명확히 분류된다.
3. rollback 전 identity revalidation이 수행된다.
4. stale/missing identity와 living same identity failure가 구분된다.
5. rollback failure를 WARN/INFO로 낮추지 않는다.
6. failed rollback state가 보존된다.
7. ApplyGuard state machine이 명확하다.
8. ApplyGuard illegal transition이 차단된다.
9. ApplyGuard destructor audit이 가능하다.
10. ShutdownRequested 이후 신규 apply가 차단되고 rollback path가 수행된다.
11. Rollback Evidence가 release-facing evidence에 반영된다.
12. RuntimeValidation이 rollback/apply transaction violation을 감지한다.
13. Phase 5 regression test가 정의되어 있다.

완료 판단은 성능 개선 여부가 아니라 rollback/apply transaction 계약의 추적 가능성과 실패 상태 보존을 기준으로 한다.

## 19. Phase 5 BLOCKER 조건

다음 조건은 Phase 5 BLOCKER로 분류한다.

```text
rollback state save 실패 후 mutation 진행
identity validation 없이 rollback success 처리
PID만으로 process identity 확정
living same identity rollback failure를 WARN/INFO로 처리
failed rollback state 폐기
ApplyGuard arm 상태가 audit 없이 소멸
verify 전 ApplyGuard commit 허용
Rollback Summary 누락
RuntimeValidation이 rollback failure를 무시
ShutdownRequested 이후 신규 apply 허용
```

BLOCKER가 발생하면 후속 patch보다 해당 blocker 제거를 우선한다. 특히 P5-02, P5-03, P5-05, P5-06의 BLOCKER는 Phase 5 전체 진행을 차단한다.

## 20. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P5-01 Rollback / ApplyGuard Contract Alignment
2. P5-02 Rollback State Save Result Model
3. P5-03 Identity Revalidation Contract
4. P5-04 Rollback Execution Classification
5. P5-05 Failed Rollback State Preservation
6. P5-06 ApplyGuard State Machine Hardening
7. P5-07 ApplyGuard Destructor Audit
8. P5-08 Rollback Request Ownership Boundary
9. P5-09 Shutdown Rollback Path
10. P5-10 Rollback Evidence Integration
11. P5-11 RuntimeValidation Rollback Checks
12. P5-12 Phase 5 Regression Test & Evidence
```

주의:

```text
P5-02, P5-03, P5-05, P5-06은 Phase 5의 핵심 안전 패치다.
P5-05에서 failed rollback state가 보존되지 않으면 Phase 5를 중단한다.
P5-06에서 ApplyGuard illegal transition이 허용되면 Phase 5를 중단한다.
```

## 21. 패치 작성 규칙

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

패치 작성자는 Phase 5 범위를 벗어난 신규 performance policy, PolicyDispatcher 구조 변경, Aggressive Single-Core Mode 구현을 같은 패치에 섞지 않는다.

## 22. Non-Goals

다음은 PatchPlan_Phase5.md의 비목표(Non-Goals)다.

```text
구체 C++ 코드 작성
패치 파일 직접 생성
테스트 코드 직접 작성
성능 개선 수치 보장
SchedulerController 신규 정책 구현
BackgroundController restriction 확장
PerformanceEngine 정책 변경
Aggressive Single-Core Mode 구현
Release Gate 자동화
```

Phase 5는 rollback/apply transaction 계약을 강화하는 문서다.

## 23. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 5의 목적과 비목표가 명확하다.
2. Phase 5 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P5-01부터 P5-12까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 완료 기준, 테스트, Evidence가 정의되어 있다.
5. RollbackManager state ownership이 명확하다.
6. ApplyGuard transaction boundary가 명확하다.
7. identity revalidation과 failed state preservation 기준이 명확하다.
8. Phase 5 BLOCKER 조건이 정의되어 있다.
9. 패치 순서가 정의되어 있다.
10. 후속 `docs/implementation/PatchPlan_Phase6.md` 작성에 필요한 기준이 충분하다.

## 24. Open Questions

다음 질문은 Phase 5 실제 패치 착수 전 또는 P5-01에서 분류한다.

```text
1. rollback state save disposition은 Created / Reused / Failed 외에 어떤 값을 둘 것인가?
2. identity revalidation에서 process creation time을 항상 확보할 수 없는 경우 fallback 기준은 무엇인가?
3. stale/missing identity는 언제 recoverable로 분류할 것인가?
4. living same identity rollback failure는 항상 BLOCKER로 둘 것인가?
5. failed rollback state는 메모리 내 보존만 할 것인가, artifact로도 남길 것인가?
6. ApplyGuard destructor audit은 log-only인가, RuntimeValidation BLOCKER로 연결할 것인가?
7. rollback retry는 shutdown 중 몇 회까지 허용할 것인가?
8. Rollback Summary는 JSON과 Markdown 중 무엇을 primary로 둘 것인가?
```

Open Questions가 해결되기 전까지 관련 구현 범위와 수치는 `TBD`로 유지한다. 단, rollback failure를 낮은 severity로 처리하지 않는 것, failed rollback state 폐기를 허용하지 않는 것, ApplyGuard arm 없는 mutation을 허용하지 않는 것은 Open Question이 아니라 Phase 5 안전 계약이다.
