# GameOptimizer v3.1 Patch Plan — Phase 10: RC Gate Integration
> Archive notice: This Phase Patch Plan is historical. Active implementation work and execution status are tracked in GitHub Issues. This file is not a current source of truth for work ordering, completion status, or release approval.

## 1. 문서 개요

문서명: GameOptimizer v3.1 Patch Plan — Phase 10

버전: v1.0

작성 목적: GameOptimizer v3.1의 열 번째 구현 단계인 Phase 10 — RC Gate Integration을 구체적인 패치 단위(Patch Unit)로 분해한다. 이 문서는 코드 구현 문서가 아니라, Build, Static Safety Check, Runtime Validation, Rollback Validation, Performance Validation, Game Profile Validation, Evidence Bundle, RC Report를 하나의 릴리즈 후보 게이트(Release Candidate Gate)로 통합하기 위한 작업 계획 문서다.

적용 범위:

- RC Gate 실행 모델 정의
- Build Identity Gate 정의
- Documentation Gate 정의
- Static Safety Gate 정의
- Runtime Safety Gate 정의
- Rollback Gate 정의
- Game Profile Gate 정의
- Performance Validation Gate 정의
- Evidence Completeness Gate 정의
- Long Soak Gate 정의
- Final Decision Aggregator 정의
- Gate exit code 계약 정의
- RC Report 생성 기준 정의
- Evidence traceability와 manifest 기준 정의
- failure preservation과 resume policy 정의
- false pass 방지 suite 정의

비적용 범위:

- 구체 C++ 구현 코드 작성
- 실제 release script 작성
- 테스트 코드 직접 작성
- 새로운 성능 정책 추가
- ThreadTracker 알고리즘 변경
- MainThreadConfidence threshold 변경
- SchedulerController mutation 변경
- RollbackManager 내부 복구 로직 변경
- Game Profile 정책 변경
- Aggressive Mode 활성 조건 변경
- 성능 개선 수치 보장
- 공식 지원 게임 확정
- 설치 프로그램 구현
- 자동 배포 구현
- UI 리포트 구현
- 클라우드 텔레메트리 구현

상위 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/archive/patch-plans/PatchPlan_Phase1.md`
- `docs/archive/patch-plans/PatchPlan_Phase2.md`
- `docs/archive/patch-plans/PatchPlan_Phase3.md`
- `docs/archive/patch-plans/PatchPlan_Phase4.md`
- `docs/archive/patch-plans/PatchPlan_Phase5.md`
- `docs/archive/patch-plans/PatchPlan_Phase6.md`
- `docs/archive/patch-plans/PatchPlan_Phase7.md`
- `docs/archive/patch-plans/PatchPlan_Phase8.md`
- `docs/archive/patch-plans/PatchPlan_Phase9.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/validation/RegressionMatrix_v3.1.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/proposals/performance/GameProfileSpecification.md`
- `docs/proposals/performance/PolicySpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`

후속 문서:

- `docs/validation/RegressionMatrix_v3.1.md`
- `docs/release/RC_Report_Template_v3.1.md`
- `docs/implementation/DevelopmentExecutionPlan_v3.1.md`

문서 경로 기준: `GameOptimizer/docs/`

Phase 10의 목표는 테스트를 실행했다는 사실을 남기는 것이 아니라, 어떤 소스와 바이너리로 어떤 검증을 수행했고 왜 해당 RC 판정이 내려졌는지를 재현 가능한 Evidence로 증명하는 것이다.

## 2. Phase 10 목표

Phase 10은 새로운 최적화 기능을 추가하는 단계가 아니다. 지금까지 정의한 기능, 안전 계약, rollback 계약, profile 제한, performance validation, evidence bundle이 릴리즈 가능한 상태인지 자동 또는 반자동으로 판정하는 단계다.

필수 목표:

1. RC Gate 실행 모델 정의
2. Build Identity Gate 정의
3. Documentation Gate 정의
4. Static Safety Gate 정의
5. Runtime Safety Gate 정의
6. Rollback Gate 정의
7. Game Profile Gate 정의
8. Performance Validation Gate 정의
9. Evidence Completeness Gate 정의
10. Soak Validation Gate 정의
11. Final Decision Aggregator 정의
12. RC Report 생성 기준 정의
13. Gate exit code 계약 정의
14. 원본 Evidence 추적성 정의
15. False Pass 방지 기준 정의

핵심 원칙:

1. 성능 개선보다 안전성 Gate를 우선한다.
2. Build 식별 정보 없는 RC 검증은 INVALID_RUN이다.
3. Release binary의 EXE SHA-256 누락은 BLOCKER다.
4. RuntimeValidation BLOCKER가 있으면 PASS 금지.
5. Rollback failure가 있으면 PASS 금지.
6. failed rollback state가 보존되지 않았으면 PASS 금지.
7. Evidence fatal missing이 있으면 PASS 금지.
8. ApplyGuard 우회가 있으면 PASS 금지.
9. Processor Group group 0 hardcoding이 있으면 PASS 금지.
10. Unknown 또는 MonitoringOnly Profile에서 mutation이 발생하면 PASS 금지.
11. Aggressive Mode 계약 위반이 있으면 PASS 금지.
12. Gate 내부 FAIL이 최종 결과에 반영되지 않는 false pass를 금지한다.
13. RC Report의 모든 핵심 값은 원본 Evidence에서 추적 가능해야 한다.
14. 사람이 확인하지 않은 항목을 자동 PASS로 위장하지 않는다.

정량 기준 후보:

- `BLOCKER` 또는 `FATAL` count가 1 이상이면 final PASS count는 0이어야 한다.
- `RuntimeValidation BLOCKER`, `rollback failure`, `evidence fatal missing` 중 하나라도 있으면 exit code 0은 금지한다.
- 필수 artifact hash mismatch count가 1 이상이면 `artifactIntegrityState`는 PASS가 될 수 없다.
- nested check failure count가 1 이상이면 Gate Result Aggregation에 반영되어야 한다.
- PASS_WITH_WARNINGS의 exit code 정책은 확정 전까지 `TBD`로 둔다.

## 3. Phase 10 비목표

Phase 10에서 Gate 통과를 위해 제품 동작을 임시로 완화하거나 안전 기준을 낮추지 않는다.

비목표:

1. 새로운 성능 정책 추가
2. ThreadTracker 알고리즘 변경
3. MainThreadConfidence threshold 변경
4. SchedulerController mutation 변경
5. RollbackManager 내부 복구 로직 변경
6. Game Profile 정책 변경
7. Aggressive Mode 활성 조건 변경
8. 성능 개선 수치 보장
9. 공식 지원 게임 확정
10. 설치 프로그램 구현
11. 자동 배포 구현
12. UI 리포트 구현
13. 클라우드 텔레메트리 구현

Phase 10은 release gate를 통과시키는 단계가 아니라, 통과 가능한지 증명하거나 통과할 수 없는 이유를 보존하는 단계다.

## 4. 영향 모듈

### 4.1 직접 영향 모듈

```text
Release Gate runner
Build manifest generator
Static check runner
Runtime artifact validator
EvidenceCompletenessValidator
Performance validation runner
Soak validation runner
RCReportGenerator
FinalDecisionAggregator
Release scripts
```

### 4.2 간접 영향 모듈

```text
RuntimeSnapshotRecorder
PolicyTimelineRecorder
PerformanceEvidenceRecorder
RuntimeValidationMonitor
Rollback summary recorder
GameProfileRegistry
SchedulerController
RollbackManager
ApplyGuard
AggressiveSingleCoreController
```

### 4.3 변경 금지 또는 최소 변경 모듈

```text
ThreadTracker core logic
TopologyAnalyzer detection logic
MainThreadConfidenceAnalyzer classification logic
PolicyDispatcher decision logic
SchedulerController apply internals
RollbackManager rollback internals
ApplyGuard transaction internals
Aggressive Mode activation internals
```

주의:

```text
Phase 10에서 Gate를 통과시키기 위해 제품 핵심 로직을 변경해야 한다면 해당 원인 Phase로 되돌려 별도 패치로 수정한다.
Release Gate script 안에서 제품 오류를 우회하거나 숨기지 않는다.
```

## 5. 패치 단위 개요

Phase 10은 다음 Patch ID 구조를 사용한다.

```text
P10-01 RC Gate Contract Alignment
P10-02 Gate Execution Model
P10-03 Build Identity Gate
P10-04 Documentation Gate
P10-05 Static Safety Gate
P10-06 Runtime Safety Gate
P10-07 Rollback Gate
P10-08 Game Profile Gate
P10-09 Performance Validation Gate
P10-10 Evidence Completeness Gate
P10-11 Long Soak Gate
P10-12 Gate Result Aggregation
P10-13 Exit Code Contract
P10-14 RC Report Generation
P10-15 Evidence Traceability and Manifest
P10-16 Failure Preservation and Resume Policy
P10-17 Phase 10 Regression Test and False Pass Suite
```

각 Patch Unit은 다음 항목을 반드시 포함한다.

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

패치 하나가 제품 동작 완화와 Gate 판정 기준 변경을 동시에 수행해서는 안 된다.

## 6. P10-01 RC Gate Contract Alignment

Patch ID: `P10-01`

목적:

ReleaseChecklist, RC Runbook, EvidenceSpecification의 Gate 기준과 현재 release script 구조를 정렬한다.

작업 범위:

```text
Gate 목록 확인
Gate 실행 순서 확인
Gate별 입력 확인
Gate별 출력 확인
Gate별 severity 확인
필수/선택 Gate 구분
자동/수동 검사 구분
Gate owner 정의
```

수정 가능 파일:

```text
docs/archive/patch-plans/PatchPlan_Phase10.md
docs/implementation/MIGRATION_NOTES_Phase10.md
release 관련 문서
release script 계약 문서
```

수정 금지 파일:

```text
제품 core source
Controller internals
Rollback internals
Runtime safety 기준 완화
```

구현 규칙:

```text
문서와 스크립트의 Gate 명칭을 일치시킨다.
Gate 결과 값과 최종 판정 값의 의미를 분리한다.
manual Gate와 automated Gate를 구분한다.
Gate severity와 final decision severity를 혼동하지 않는다.
```

완료 기준:

```text
모든 Gate의 이름, 책임, 입력, 출력, severity가 정의된다.
중복 Gate와 누락 Gate가 식별된다.
manual/automated Gate 목록이 생성된다.
```

필수 테스트:

```text
gate inventory review test
gate owner map review test
manual automated gate classification test
severity mapping review test
missing gate review test
```

필수 Evidence:

```text
rcGateContractSummary
gateInventory
gateOwnerMap
manualGateList
automatedGateList
blockingQuestionList
```

리스크:

```text
Gate 이름과 severity가 문서와 스크립트에서 다르면 RC Report가 다른 의미로 해석될 수 있다.
manual Gate가 자동 PASS로 기록되면 사람이 확인해야 하는 항목이 누락된다.
```

Rollback 계획:

```text
Gate owner가 불명확하면 해당 Gate를 PASS 근거로 사용하지 않는다.
contract alignment가 완료될 때까지 final decision은 INVALID_RUN 또는 BLOCKER 후보로 제한한다.
```

## 7. P10-02 Gate Execution Model

Patch ID: `P10-02`

목적:

RC Gate의 실행 순서와 중단 정책을 정의한다.

권장 실행 순서:

```text
1. Preflight
2. Build Identity
3. Documentation
4. Static Safety
5. Dry-run Validation
6. Soft-apply Validation
7. Runtime Safety
8. Rollback Validation
9. Game Profile Validation
10. Performance Validation
11. Evidence Completeness
12. Long Soak
13. RC Report Generation
14. Final Decision
```

작업 범위:

```text
fail-fast Gate 정의
continue-for-evidence Gate 정의
dependency 정의
skip 조건 정의
manual confirmation point 정의
safe termination path 정의
```

수정 가능 파일:

```text
Release Gate runner
RC Runbook
release scripts
관련 테스트
```

수정 금지 파일:

```text
제품 core source
RuntimeValidation BLOCKER semantics
Rollback failure severity
SchedulerController apply internals
```

구현 규칙:

```text
안전성 BLOCKER 발생 시 신규 mutation 검증을 중단한다.
중단 후에도 가능한 Evidence 수집과 안전 종료는 수행한다.
선행 Gate 실패 시 종속 Gate를 PASS로 기록하지 않는다.
skip reason 없이 Gate를 생략하지 않는다.
```

완료 기준:

```text
Gate 실행 순서와 dependency가 명확하다.
중단된 Gate는 SKIPPED_WITH_REASON 또는 NOT_RUN으로 기록된다.
BLOCKER 이후 safe termination과 evidence preservation이 수행된다.
```

필수 테스트:

```text
normal gate sequence test
build failure stops runtime gate test
static blocker stops mutation validation test
runtime blocker triggers safe termination test
skipped gate reason test
evidence collection after blocker test
```

필수 Evidence:

```text
gateExecutionOrder
gateDependencyMap
gateExecutionState
skippedGateList
gateStopReason
safeTerminationResult
```

리스크:

```text
선행 Gate 실패 후 종속 Gate가 PASS가 되면 false pass가 발생한다.
BLOCKER 이후 신규 mutation 검증을 계속하면 실패 원인을 악화시킬 수 있다.
```

Rollback 계획:

```text
Gate dependency 판단이 불명확하면 종속 Gate를 SKIPPED_WITH_REASON으로 기록한다.
BLOCKER 이후에는 safe termination과 artifact preservation만 수행한다.
```

BLOCKER 기준:

```text
선행 Gate 실패 후 종속 Gate를 PASS 처리
BLOCKER 후 신규 mutation 검증 계속
skip reason 누락
```

## 8. P10-03 Build Identity Gate

Patch ID: `P10-03`

목적:

RC 검증 대상 소스와 바이너리를 고유하게 식별한다.

작업 범위:

```text
git commit hash
working tree cleanliness
build configuration
compiler/toolset
build timestamp
binary path
build hash
EXE SHA-256
configuration hash
profile hash if applicable
```

수정 가능 파일:

```text
Build manifest generator
release build script
session manifest writer
RCReportGenerator input
관련 테스트
```

수정 금지 파일:

```text
제품 core source
Build output 조작 fallback
hash mismatch 무시 rule
```

구현 규칙:

```text
Release RC에서 EXE SHA-256 누락은 BLOCKER다.
git commit 누락은 BLOCKER다.
dirty working tree는 WARN 또는 BLOCKER 정책을 명시한다.
binary path만으로 동일 바이너리라고 판단하지 않는다.
manifest와 RC Report의 identity 값은 일치해야 한다.
```

완료 기준:

```text
검증한 바이너리와 source revision을 연결할 수 있다.
build identity가 Session Manifest와 RC Report에 기록된다.
hash mismatch가 감지된다.
```

필수 테스트:

```text
valid build identity test
missing git commit blocker test
missing exe hash blocker test
binary path missing blocker test
dirty working tree classification test
build manifest consistency test
hash mismatch blocker test
```

필수 Evidence:

```text
gitCommit
workingTreeState
buildConfiguration
compilerToolset
buildTimestamp
binaryPath
buildHash
exeSha256
configHash
```

리스크:

```text
build identity가 없으면 어떤 바이너리를 검증했는지 재현할 수 없다.
hash mismatch를 무시하면 RC Report가 다른 binary를 승인할 수 있다.
```

Rollback 계획:

```text
build identity가 불완전하면 runtime Gate를 실행하지 않고 INVALID_RUN 또는 BLOCKER로 분류한다.
dirty working tree 정책이 미정이면 WARN/BLOCKER 기준을 `TBD`로 두고 final PASS 근거에서 제외한다.
```

BLOCKER 기준:

```text
EXE SHA-256 누락
git commit 누락
실행 바이너리 식별 불가
manifest와 RC Report의 hash 불일치
```

## 9. P10-04 Documentation Gate

Patch ID: `P10-04`

목적:

v3.1 구현과 릴리즈 판단에 필요한 기준 문서가 존재하고 일관되는지 확인한다.

작업 범위:

```text
필수 문서 존재 검사
문서 버전 검사
문서 경로 검사
상위/후속 문서 참조 검사
Open Questions 중 blocking 항목 검사
문서와 구현 Phase 매핑 검사
```

수정 가능 파일:

```text
documentation validator
release scripts
docs index
관련 테스트
```

수정 금지 파일:

```text
제품 core source
Gate 통과를 위한 문서 기준 완화
Safety / Rollback / Evidence 기준 삭제
```

구현 규칙:

```text
Safety, Rollback, Evidence 문서 누락은 BLOCKER다.
Open Question이 구현을 차단하는 항목이면 미해결 상태에서 PASS 금지.
단순 문체나 형식 문제는 WARN으로 둘 수 있다.
문서 경로는 docs/... 기준으로 기록한다.
```

완료 기준:

```text
필수 문서 존재와 참조 정합성을 자동 또는 수동으로 확인할 수 있다.
blocking Open Question이 final decision에 반영된다.
```

필수 테스트:

```text
all required docs present test
missing safety document blocker test
missing evidence document blocker test
broken document reference test
blocking open question test
docs path consistency test
```

필수 Evidence:

```text
documentationGateResult
requiredDocumentList
missingDocumentList
brokenReferenceList
blockingOpenQuestionList
```

리스크:

```text
기준 문서가 없으면 Gate 결과가 어떤 계약을 검증했는지 알 수 없다.
blocking question을 무시하면 미정 기준을 통과 기준처럼 사용할 수 있다.
```

Rollback 계획:

```text
필수 문서가 누락되면 Documentation Gate를 BLOCKER로 분류한다.
참조가 깨진 문서는 release 기준에서 제외하고 brokenReferenceList에 남긴다.
```

## 10. P10-05 Static Safety Gate

Patch ID: `P10-05`

목적:

실행 전 핵심 안전 계약 위반을 정적으로 검사한다.

필수 검사:

```text
ThreadTracker observation-only
PerformanceEngine direct mutation 금지
PolicyDispatcher direct mutation 금지
AggressiveSingleCoreController direct mutation 금지
Rollback state save 전 mutation 금지 패턴
ApplyGuard 우회 금지
std::expected Assign → Check → Bind
uninspected expected propagation 금지
Processor Group group 0 hardcoding 금지
forbidden Anti-Cheat bypass API 또는 구현 금지
```

작업 범위:

```text
정적 검사 rule 목록 작성
forbidden API pattern 검사
mutation ownership 검사
expected handling 검사
Processor Group hardcoding 검사
finding severity mapping
false positive suppression 기준
```

수정 가능 파일:

```text
static check scripts
release gate runner
static check tests
```

수정 금지 파일:

```text
제품 core source
Controller ownership 우회
ApplyGuard transaction 계약 완화
Anti-Cheat 우회 탐지 예외 처리
```

구현 규칙:

```text
정적 검사는 heuristic임을 문서화한다.
정적 검사 PASS를 제어 흐름 안전성의 완전한 증명으로 표현하지 않는다.
명확한 mutation ownership 위반은 BLOCKER다.
Processor Group group 0 hardcoding은 BLOCKER다.
```

완료 기준:

```text
핵심 금지 패턴을 자동 검사할 수 있다.
static check 결과가 RC Report에 반영된다.
finding severity와 source location이 기록된다.
```

필수 테스트:

```text
thread tracker forbidden API detection test
performance engine mutation detection test
policy dispatcher mutation detection test
aggressive controller mutation detection test
expected uninspected propagation detection test
group zero hardcoding detection test
false positive suppression test
```

필수 Evidence:

```text
staticSafetyGateResult
staticCheckCount
staticCheckFailureCount
forbiddenMutationFindingList
expectedContractFindingList
processorGroupFindingList
```

리스크:

```text
정적 검사를 과신하면 runtime-only 위반을 놓칠 수 있다.
false positive suppression이 넓으면 실제 forbidden mutation이 숨겨질 수 있다.
```

Rollback 계획:

```text
정적 검사 도구가 실패하면 Static Safety Gate를 INVALID_RUN 또는 FATAL 후보로 분류한다.
명확한 ownership 위반은 suppression하지 않고 BLOCKER로 유지한다.
```

BLOCKER 기준:

```text
관찰 계층에서 mutation 호출
Controller 외 affinity/priority 직접 적용
ApplyGuard 우회
std::expected 무검사 전파
Processor Group group 0 hard코딩
```

## 11. P10-06 Runtime Safety Gate

Patch ID: `P10-06`

목적:

실제 실행 중 상태 전이와 안전 계약이 유지되는지 확인한다.

작업 범위:

```text
RuntimeStateMachine transition
watchdog heartbeat
snapshot freshness
policy timeline monotonicity
active policy consistency
rollback state consistency
target identity consistency
shutdown cleanliness
RuntimeValidation blockerCount
```

수정 가능 파일:

```text
runtime artifact validator
RuntimeValidation summary consumer
release scripts
관련 테스트
```

수정 금지 파일:

```text
RuntimeValidation BLOCKER semantics
RuntimeStateMachine core transition semantics
SchedulerController apply internals
RollbackManager rollback internals
```

구현 규칙:

```text
RuntimeValidation BLOCKER가 하나라도 있으면 Gate FAIL.
RuntimeValidation Summary 누락은 Gate FAIL.
최신 snapshot과 final snapshot의 검증 규칙을 구분한다.
identity mismatch 상태의 mutation은 BLOCKER다.
```

완료 기준:

```text
Runtime Safety Gate가 RuntimeValidation Summary와 원본 runtime artifact를 함께 검증한다.
runtime artifact 위반이 Gate 결과와 RC Report에 반영된다.
```

필수 테스트:

```text
runtime safety pass test
runtime blocker gate fail test
heartbeat stalled test
latest snapshot stale test
final snapshot consistency test
timeline monotonicity failure test
active policy rollback mismatch test
unclean shutdown test
```

필수 Evidence:

```text
runtimeSafetyGateResult
runtimeValidationState
runtimeBlockerCount
heartbeatProgression
snapshotFreshness
timelineMonotonicity
activePolicyConsistency
rollbackStateConsistency
cleanShutdown
```

리스크:

```text
RuntimeValidation Summary만 보고 원본 artifact를 확인하지 않으면 summary 생성 오류를 놓칠 수 있다.
unclean shutdown을 clean으로 위장하면 final evidence flush 실패가 숨겨진다.
```

Rollback 계획:

```text
RuntimeValidation Summary가 누락되면 Runtime Safety Gate를 FAIL로 둔다.
runtime artifact 일부가 누락되면 해당 field의 severity를 BLOCKER 또는 FATAL 후보로 분류한다.
```

BLOCKER 기준:

```text
RuntimeValidation BLOCKER
heartbeat 중단
timeline monotonicity 실패
active policy와 rollback state 불일치
identity mismatch 상태의 mutation
clean shutdown 위장
```

## 12. P10-07 Rollback Gate

Patch ID: `P10-07`

목적:

적용된 모든 mutation이 정상적으로 원복되었는지 확인한다.

작업 범위:

```text
rollback state save result
thread priority rollback
thread affinity rollback
process priority rollback
background restriction rollback
timer resolution rollback if modified
identity revalidation
failed state preservation
shutdown rollback result
mutation rollback coverage
```

수정 가능 파일:

```text
rollback artifact validator
Rollback Summary consumer
release scripts
관련 테스트
```

수정 금지 파일:

```text
RollbackManager rollback internals
Rollback failure severity 하향
failedStatePreserved=false 무시 rule
SchedulerController apply internals
```

구현 규칙:

```text
living same identity rollback failure는 BLOCKER다.
failedStatePreserved=false는 BLOCKER다.
stale/missing identity는 recoverable일 수 있으나 Evidence가 필수다.
Rollback Summary 누락은 BLOCKER다.
rollback state 없는 mutation은 BLOCKER다.
```

완료 기준:

```text
rollback 결과와 release severity를 검증할 수 있다.
모든 mutation과 rollback state의 대응 관계를 확인할 수 있다.
failed rollback state 보존 여부가 RC Report에 반영된다.
```

필수 테스트:

```text
rollback gate pass test
missing rollback summary blocker test
living identity rollback failure blocker test
stale identity recoverable test
failed state not preserved blocker test
mutation without rollback state blocker test
identity validation missing blocker test
```

필수 Evidence:

```text
rollbackGateResult
rollbackSummaryPath
rollbackResult
rollbackReleaseSeverity
failedStatePreserved
identityValidationResult
mutationRollbackCoverage
```

리스크:

```text
rollback failure를 WARN으로 낮추면 시스템 상태가 복구되지 않은 RC가 승인될 수 있다.
failed state가 보존되지 않으면 사고 분석과 수동 복구가 어려워진다.
```

Rollback 계획:

```text
Rollback Summary가 없으면 RC PASS를 차단한다.
identity validation이 불명확하면 rollback 성공을 PASS 근거로 사용하지 않는다.
```

BLOCKER 기준:

```text
Rollback Summary 누락
living same identity rollback 실패
failed rollback state 폐기
rollback state 없는 mutation
identity validation 없는 rollback 성공
```

## 13. P10-08 Game Profile Gate

Patch ID: `P10-08`

목적:

선택된 Game Profile과 실제 실행 정책이 일치하는지 확인한다.

작업 범위:

```text
selectedProfile
profileType
profileStatus
target binding
allowedPolicies
blockedPolicies
conditionalPolicies
monitoringOnly state
AntiCheatSensitive state
profile evidence completeness
```

수정 가능 파일:

```text
profile artifact validator
RuntimeValidation profile summary consumer
release scripts
관련 테스트
```

수정 금지 파일:

```text
GameProfileRegistry lookup internals
Game Profile 정책 완화
PolicyDispatcher decision logic
profile blocker severity 하향
```

구현 규칙:

```text
Candidate를 Validated로 위장하면 BLOCKER다.
MonitoringOnly에서 mutation이 발생하면 BLOCKER다.
AntiCheatSensitive에서 mutation이 발생하면 BLOCKER다.
blocked policy 적용은 BLOCKER다.
target binding mismatch 상태의 mutation은 BLOCKER다.
```

완료 기준:

```text
profile 제한과 실제 policy timeline을 비교할 수 있다.
profile selection 결과가 RC Report에 반영된다.
profile violation이 final decision에 반영된다.
```

필수 테스트:

```text
profile gate pass test
candidate treated as validated blocker test
unknown profile mutation blocker test
monitoring-only mutation blocker test
anti-cheat sensitive mutation blocker test
blocked policy applied blocker test
target binding mismatch blocker test
```

필수 Evidence:

```text
gameProfileGateResult
selectedProfileId
selectedProfileStatus
profileTargetBindingResult
allowedPolicyList
blockedPolicyList
profileViolationList
```

리스크:

```text
profile 제한을 RC에서 확인하지 않으면 Phase 7의 safety fallback이 release 단계에서 무력화된다.
Candidate를 Validated처럼 다루면 검증되지 않은 게임 정책이 승인될 수 있다.
```

Rollback 계획:

```text
profile Evidence가 불완전하면 Game Profile Gate를 BLOCKER 후보로 둔다.
profile violation이 하나라도 있으면 final PASS를 차단한다.
```

BLOCKER 기준:

```text
Candidate를 Validated로 위장
Unknown Profile mutation
MonitoringOnly Profile mutation
AntiCheatSensitive Profile mutation
blocked policy applied
target binding mismatch mutation
```

## 14. P10-09 Performance Validation Gate

Patch ID: `P10-09`

목적:

성능 판정이 Baseline / Applied Window / Comparison / Safety Override 기준을 만족하는지 확인한다.

작업 범위:

```text
Baseline Window
Applied Window
Before/After Comparison
Metric Completeness
Regression Detection
Safety Override
Performance Classification
Actual Game Validation record
```

수정 가능 파일:

```text
performance artifact validator
classification consumer
release scripts
관련 테스트
```

수정 금지 파일:

```text
Performance classification 기준 완화
Safety Override 우회
성능 개선 수치 보장 문구
RuntimeValidation BLOCKER 무시 rule
```

구현 규칙:

```text
Baseline 또는 Applied Window가 없으면 improvement PASS 금지.
Safety Override가 적용되면 성능 개선 여부와 무관하게 최종 PASS를 차단할 수 있다.
NEUTRAL은 성능 향상 주장 금지.
REGRESSION은 성능 회귀로 기록한다.
```

완료 기준:

```text
Performance Validation Gate가 성능 분류의 입력 완전성과 안전성 override를 검증한다.
performance classification이 RC Report에 반영된다.
improvement claim 가능 여부가 Evidence와 일치한다.
```

필수 테스트:

```text
performance gate pass test
missing baseline blocker test
missing applied window blocker test
missing comparison blocker test
neutral classification allowed without improvement claim test
regression gate fail test
safety override gate fail test
```

필수 Evidence:

```text
performanceValidationGateResult
baselineWindowValid
appliedWindowValid
comparisonValid
metricCompletenessState
regressionDetectionResult
safetyOverrideApplied
performanceClassification
```

리스크:

```text
성능 개선 수치가 안전성 override를 덮으면 위험한 RC가 승인될 수 있다.
NEUTRAL을 improvement로 표현하면 release report가 과장된다.
```

Rollback 계획:

```text
performance artifact가 불완전하면 improvement claim을 금지한다.
Safety Override가 적용되면 final decision aggregator가 PASS를 차단하도록 연결한다.
```

BLOCKER 기준:

```text
Baseline 없이 improvement PASS
Applied Window 없이 improvement PASS
comparison 없이 improvement 판정
Safety Override 무시
REGRESSION 상태에서 PASS 생성
```

## 15. P10-10 Evidence Completeness Gate

Patch ID: `P10-10`

목적:

RC 승인에 필요한 Evidence Bundle이 완전한지 확인한다.

필수 산출물:

```text
session_manifest
runtime_snapshot_latest
runtime_snapshot_final
policy_timeline
performance_summary
rollback_summary
runtime_validation_summary
evidence_completeness_report
classification_result
binary hash
git commit
```

작업 범위:

```text
required evidence list 검사
optional evidence list 검사
missing required evidence severity 분류
missing optional evidence severity 분류
evidence completeness report 검증
artifact existence 확인
artifact identity와 build identity 연결
```

수정 가능 파일:

```text
EvidenceCompletenessValidator
artifact validator
release scripts
관련 테스트
```

수정 금지 파일:

```text
필수 Evidence 목록 임의 축소
Evidence fatal missing severity 하향
RC Report missing value default 채움
RuntimeValidation Summary 생성 기준 완화
```

구현 규칙:

```text
필수 Evidence 누락은 BLOCKER 또는 FATAL이다.
선택 Evidence 누락만 WARN으로 허용할 수 있다.
Evidence completeness report 자체가 없으면 PASS 금지.
누락값을 임의 기본값으로 채우지 않는다.
```

완료 기준:

```text
Evidence Bundle의 필수/선택 항목을 구분해 검증할 수 있다.
누락 필드와 severity가 RC Report에 반영된다.
Evidence fatal missing이 final decision에 반영된다.
```

필수 테스트:

```text
complete evidence bundle test
missing final snapshot blocker test
missing rollback summary blocker test
missing runtime validation summary blocker test
missing binary hash blocker test
missing optional metric warning test
```

필수 Evidence:

```text
evidenceCompletenessGateResult
requiredEvidenceList
missingRequiredEvidenceList
missingOptionalEvidenceList
evidenceCompletenessSeverity
```

리스크:

```text
필수 Evidence 누락을 WARN으로 낮추면 재현 불가능한 RC가 승인될 수 있다.
completeness report가 없으면 누락 여부 자체를 검증할 수 없다.
```

Rollback 계획:

```text
completeness report가 없으면 final PASS를 차단한다.
required evidence 목록이 불명확하면 EvidenceSpecification을 source of truth로 유지한다.
```

BLOCKER 기준:

```text
final snapshot 누락
rollback summary 누락
runtime validation summary 누락
classification result 누락
binary hash 누락
evidence completeness report 누락
```

## 16. P10-11 Long Soak Gate

Patch ID: `P10-11`

목적:

장시간 실행 중 안정성과 Evidence 지속성을 검증한다.

작업 범위:

```text
soak duration
watchdog heartbeat progression
snapshot freshness
timeline monotonicity
policy activation count
policy rejection count
policy thrashing
memory growth
CPU overhead
rollback state consistency
shutdown cleanliness
```

수정 가능 파일:

```text
soak runner
soak artifact validator
release scripts
관련 테스트
```

수정 금지 파일:

```text
RuntimeValidation blocker semantics
Rollback failure severity
Soak failure severity 하향
성능 개선 Gate로 목적 변경
```

구현 규칙:

```text
Soak Gate는 성능 개선 Gate가 아니라 장시간 안정성 Gate다.
watchdog hang, rollback state loss, timeline monotonicity failure는 BLOCKER다.
soak duration은 확정 전까지 TBD로 둔다.
```

완료 기준:

```text
Long Soak 결과를 PASS / WARN / BLOCKER로 분류할 수 있다.
soak 결과가 RC Report와 final decision input으로 연결된다.
```

필수 테스트:

```text
soak gate pass test
watchdog hang blocker test
snapshot freshness blocker test
timeline monotonicity blocker test
policy thrashing blocker test
memory growth warning test
rollback state loss blocker test
unclean shutdown blocker test
```

필수 Evidence:

```text
longSoakGateResult
soakDuration
heartbeatProgression
snapshotFreshnessOverTime
timelineMonotonicity
policyThrashingDetected
memoryGrowthSummary
cpuOverheadSummary
```

리스크:

```text
Long Soak를 생략하면 policy thrashing, stale snapshot, slow evidence failure를 놓칠 수 있다.
Soak Gate를 performance Gate로 해석하면 안정성 목적이 흐려진다.
```

Rollback 계획:

```text
Long Soak가 미실행이면 skip reason과 policy를 기록한다.
soak 필수 여부가 확정되지 않았으면 Open Question으로 남기고 임의 PASS를 만들지 않는다.
```

BLOCKER 기준:

```text
watchdog hang
snapshot freshness failure
timeline monotonicity failure
policy thrashing
rollback state loss
unclean shutdown
```

## 17. P10-12 Gate Result Aggregation

Patch ID: `P10-12`

목적:

개별 Gate 결과를 최종 RC 판정 입력으로 집계한다.

작업 범위:

```text
Gate result collection
severity normalization
BLOCKER count
FATAL count
WARN count
SKIPPED count
dependency failure propagation
final classification candidate
nested check failure propagation
```

수정 가능 파일:

```text
FinalDecisionAggregator
RCReportGenerator
release gate runner
관련 테스트
```

수정 금지 파일:

```text
Gate failure severity 하향
BLOCKER를 WARN으로 변환하는 fallback
Final decision PASS 우회 rule
report 생성 성공과 PASS 동치 처리
```

구현 규칙:

```text
개별 Gate의 FAIL을 최종 PASS에서 무시하지 않는다.
checks 배열 내부 FAIL이 최종 exit code와 final decision에 반드시 반영되어야 한다.
BLOCKER 또는 FATAL이 하나라도 있으면 PASS 금지.
SKIPPED Gate는 skip reason과 dependency 상태를 가져야 한다.
```

완료 기준:

```text
모든 Gate 결과가 최종 판정에 반영된다.
false pass 경로가 차단된다.
nested check failure가 Gate result와 exit code로 전파된다.
```

필수 테스트:

```text
all gates pass aggregation test
single blocker overrides all pass test
single fatal overrides all pass test
warning aggregation test
skipped dependency aggregation test
nested check failure propagation test
```

필수 Evidence:

```text
gateAggregationResult
totalGateCount
passedGateCount
warnGateCount
blockedGateCount
fatalGateCount
skippedGateCount
finalDecisionCandidate
```

리스크:

```text
개별 Gate FAIL이 aggregation에서 누락되면 false pass가 발생한다.
nested checks를 무시하면 Gate summary는 PASS지만 내부 검사는 FAIL인 모순이 생긴다.
```

Rollback 계획:

```text
aggregation input이 불완전하면 final decision을 INVALID_RUN 또는 BLOCKER 후보로 제한한다.
nested failure propagation이 검증되지 않으면 PASS를 생성하지 않는다.
```

BLOCKER 기준:

```text
개별 Gate FAIL이 최종 결과에 미반영
checks 내부 FAIL 무시
BLOCKER 존재 상태에서 PASS 생성
```

## 18. P10-13 Exit Code Contract

Patch ID: `P10-13`

목적:

RC Gate 결과를 자동화 도구가 신뢰할 수 있도록 exit code 계약을 정의한다.

권장 계약:

```text
0: PASS 또는 허용된 PASS_WITH_WARNINGS
1: BLOCKER 또는 REGRESSION
2: INVALID_RUN 또는 환경 구성 실패
3: FATAL 또는 Gate 내부 실행 오류
```

작업 범위:

```text
Gate runner exit code
validator exit code
report generator exit code
script chaining behavior
batch/cmd compatibility
PowerShell 미설치 fallback 기준
child process exit code propagation
```

수정 가능 파일:

```text
release scripts
batch files
Python validators
관련 테스트
```

수정 금지 파일:

```text
BLOCKER 상태 exit code 0 허용
하위 validator failure suppression
report generation success를 RC PASS로 처리
final decision severity 하향
```

구현 규칙:

```text
BLOCKER 상태에서 exit code 0 금지.
report 생성 성공과 RC PASS를 동일한 의미로 사용하지 않는다.
도구 미설치와 제품 실패를 구분한다.
하위 프로세스 exit code를 반드시 검사한다.
```

완료 기준:

```text
exit code와 final decision이 일관된다.
CI 또는 batch 실행에서 실패가 전파된다.
child script 실패가 상위 exit code에 반영된다.
```

필수 테스트:

```text
pass exit code test
warning exit code policy test
blocker exit code test
invalid run exit code test
fatal tool error exit code test
child script failure propagation test
```

필수 Evidence:

```text
gateExitCode
gateExitCodeMeaning
childProcessExitCodes
exitCodeConsistency
```

리스크:

```text
BLOCKER에서 exit code 0을 반환하면 CI와 release automation이 실패를 성공으로 처리한다.
report generator 성공을 RC PASS로 오판하면 보고서만 생성된 실패 run이 승인될 수 있다.
```

Rollback 계획:

```text
exit code mapping이 불명확하면 non-PASS final decision은 non-zero exit code로 처리한다.
child process failure propagation이 검증되지 않으면 gate runner를 PASS로 종료하지 않는다.
```

BLOCKER 기준:

```text
BLOCKER 상태에서 exit code 0
하위 validator 실패 무시
report 생성 성공을 RC PASS로 오판
```

## 19. P10-14 RC Report Generation

Patch ID: `P10-14`

목적:

RC 검증 결과를 사람이 검토 가능한 보고서와 구조화된 결과로 생성한다.

필수 보고서 항목:

```text
runId
target process
selected profile
test mode
git commit
build hash
exeSha256
startedUtc
finishedUtc
Gate summary
WARN list
BLOCKER list
FATAL list
performance classification
rollback result
runtime validation result
evidence completeness result
soak result
final decision
```

작업 범위:

```text
Markdown report 생성
structured report 생성
report field source mapping
missing input handling
final decision consistency
human review section
```

수정 가능 파일:

```text
RCReportGenerator
report template
release scripts
관련 테스트
```

수정 금지 파일:

```text
원본 Evidence 없는 값 생성
missing input default pass 처리
Markdown/structured decision 불일치 허용
BLOCKER list 숨김
```

구현 규칙:

```text
Markdown 보고서와 구조화된 보고서의 final decision이 일치해야 한다.
보고서의 값은 원본 Evidence에서 추적 가능해야 한다.
누락값을 임의 기본값으로 채우지 않는다.
사람이 확인해야 하는 항목을 자동 PASS로 위장하지 않는다.
```

완료 기준:

```text
RC Report가 생성된다.
Gate별 결과와 최종 판정 근거를 확인할 수 있다.
Markdown과 구조화된 결과의 final decision이 일치한다.
```

필수 테스트:

```text
markdown rc report generation test
structured rc report generation test
report decision consistency test
report missing input blocker test
report source traceability test
manual review item rendering test
```

필수 Evidence:

```text
rcReportMarkdownPath
rcReportStructuredPath
rcReportGenerationResult
rcReportDecision
rcReportSourceMap
```

리스크:

```text
보고서가 생성되었다는 사실이 RC PASS로 오해될 수 있다.
source traceability가 없으면 보고서 값이 원본 artifact에서 나온 것인지 검증할 수 없다.
```

Rollback 계획:

```text
report generation이 실패하면 실패 report와 input bundle을 보존한다.
report decision consistency가 깨지면 final PASS를 차단한다.
```

## 20. P10-15 Evidence Traceability and Manifest

Patch ID: `P10-15`

목적:

RC Report의 각 핵심 결과를 원본 Evidence 파일과 연결한다.

작업 범위:

```text
artifact path mapping
artifact hash
source field mapping
Gate result source mapping
report value source mapping
manifest final commit rule
artifact modification time consistency
artifact integrity validation
```

수정 가능 파일:

```text
session manifest writer
artifact manifest generator
RCReportGenerator
artifact validator
관련 테스트
```

수정 금지 파일:

```text
artifact hash mismatch 무시
manifest finalization 순서 우회
원본 artifact 없는 report value 생성
tampered artifact 정상 처리
```

구현 규칙:

```text
RC Report 값의 출처가 불명확하면 PASS 근거로 사용하지 않는다.
필수 artifact에는 hash 또는 동등한 무결성 정보를 기록한다.
manifest는 final artifact 생성 후 최종 반영한다.
artifact modification time과 manifest finalization 순서를 검증한다.
```

완료 기준:

```text
Gate 결과와 RC Report 값이 원본 artifact까지 추적된다.
artifact 변조 또는 불일치를 감지할 수 있다.
manifest finalization 순서가 보존된다.
```

필수 테스트:

```text
artifact path traceability test
artifact hash consistency test
report source field mapping test
manifest final commit order test
artifact mtime consistency test
tampered artifact detection test
```

필수 Evidence:

```text
artifactManifestPath
artifactHashMap
gateSourceArtifactMap
reportSourceFieldMap
artifactIntegrityState
```

리스크:

```text
Traceability가 없으면 RC 판정을 재현하거나 감사할 수 없다.
manifest가 final artifact보다 오래되면 보고서가 오래된 값을 참조할 수 있다.
```

Rollback 계획:

```text
artifact integrity가 불명확하면 해당 report value를 PASS 근거에서 제외한다.
manifest finalization 순서가 깨지면 RC run을 INVALID_RUN 또는 BLOCKER 후보로 분류한다.
```

BLOCKER 기준:

```text
RC Report 핵심 결과의 출처 불명
artifact hash 불일치
manifest가 final artifact보다 오래됨
변조 artifact를 정상 처리
```

## 21. P10-16 Failure Preservation and Resume Policy

Patch ID: `P10-16`

목적:

RC Gate 실패 시 Evidence를 보존하고 재실행 정책을 정의한다.

작업 범위:

```text
failed run artifact preservation
partial Gate result preservation
retry eligibility
resume vs fresh run 기준
runId 재사용 금지 기준
failed report generation
cleanup policy
failure reason preservation
```

수정 가능 파일:

```text
release gate runner
artifact manager
RCReportGenerator
관련 테스트
```

수정 금지 파일:

```text
실패 Evidence 자동 삭제
partial run을 full PASS로 재사용
runId 재사용으로 신규 run 위장
BLOCKER artifact cleanup
```

구현 규칙:

```text
실패한 RC run의 Evidence를 자동 삭제하지 않는다.
재실행은 새로운 runId를 기본으로 한다.
부분 실행 결과를 완전한 RC PASS Evidence로 재사용하지 않는다.
cleanup은 BLOCKER Evidence를 삭제하지 않는다.
```

완료 기준:

```text
실패한 Gate의 artifact와 원인이 보존된다.
재실행과 resume 정책이 구분된다.
failed report generation 결과가 추적된다.
```

필수 테스트:

```text
failed run artifact preserved test
new run id on retry test
partial run not reused as pass test
failed report preserved test
cleanup does not delete blocker evidence test
resume eligibility test
```

필수 Evidence:

```text
failedRunPreserved
failedRunId
retryEligibility
resumeEligibility
failureArtifactPath
cleanupResult
```

리스크:

```text
실패 artifact를 삭제하면 BLOCKER 분석과 재현이 불가능하다.
partial Gate result를 PASS 근거로 재사용하면 false pass가 발생한다.
```

Rollback 계획:

```text
failure preservation이 실패하면 cleanup을 중단하고 현재 artifact path를 보존한다.
resume 정책이 불명확하면 fresh run만 허용한다.
```

## 22. P10-17 Phase 10 Regression Test and False Pass Suite

Patch ID: `P10-17`

목적:

RC Gate 자체의 회귀와 false pass 가능성을 검증한다.

작업 범위:

```text
Phase 10 test list 작성
Gate별 failure injection
nested check failure propagation
exit code 검증
report consistency 검증
artifact traceability 검증
false pass matrix 작성
RegressionMatrix_v3.1 연결
```

필수 실패 주입:

```text
missing binary hash
missing final snapshot
timeline event count zero
timeline monotonicity failure
missing rollback summary
rollback failure
RuntimeValidation BLOCKER
blocked policy applied
Unknown Profile mutation
Aggressive Mode contract violation
Evidence write failure
nested check FAIL
report decision mismatch
child script non-zero exit
```

수정 가능 파일:

```text
docs/validation/RegressionMatrix_v3.1.md
docs/release/ReleaseChecklist_v3.1.md
docs/release/RC_Runbook_v3.1.md
release validation scripts
tests/*
```

수정 금지 파일:

```text
제품 core source
BLOCKER severity 하향
exit code 0 우회
report mismatch 허용
```

구현 규칙:

```text
Phase 10 PASS는 제품 성능 향상 보장이 아니다.
Phase 10 PASS는 정의된 릴리즈 기준과 Evidence 계약을 모두 만족했다는 의미다.
false pass 가능성을 실제 실패 주입으로 검증한다.
모든 BLOCKER 주입은 non-zero exit code로 이어져야 한다.
```

완료 기준:

```text
Phase 10 regression checklist와 false pass suite가 존재한다.
모든 BLOCKER 주입이 최종 non-zero exit code와 BLOCKER 판정으로 이어진다.
report consistency와 artifact traceability가 검증된다.
```

필수 테스트:

```text
all phase10 gate suites
false pass injection suite
nested failure propagation suite
exit code consistency suite
report consistency suite
artifact traceability suite
```

필수 Evidence:

```text
phase10RegressionSummary
falsePassTestSummary
failureInjectionSummary
gateAggregationSummary
exitCodeSummary
reportConsistencySummary
artifactTraceabilitySummary
```

리스크:

```text
false pass suite가 없으면 Gate 자체가 실패를 PASS로 만드는지 검증할 수 없다.
하위 script failure가 전파되지 않으면 CI와 release automation이 실패를 놓친다.
```

Rollback 계획:

```text
false pass injection 중 하나라도 PASS로 종료되면 Phase 10 Gate 구현을 release 후보에서 제외한다.
report mismatch가 발견되면 RC Report Generation과 Gate Aggregation 기준으로 되돌려 수정한다.
```

BLOCKER 기준:

```text
실패 주입이 최종 PASS로 종료
BLOCKER 주입에서 exit code 0
Markdown/구조화 보고서 판정 불일치
하위 script failure 무시
```

## 23. Phase 10 완료 기준

Phase 10 전체 완료 기준은 다음과 같다.

1. RC Gate 계약과 실행 순서가 정의되어 있다.
2. Build Identity Gate가 정의되어 있다.
3. Documentation Gate가 정의되어 있다.
4. Static Safety Gate가 정의되어 있다.
5. Runtime Safety Gate가 정의되어 있다.
6. Rollback Gate가 정의되어 있다.
7. Game Profile Gate가 정의되어 있다.
8. Performance Validation Gate가 정의되어 있다.
9. Evidence Completeness Gate가 정의되어 있다.
10. Long Soak Gate가 정의되어 있다.
11. Gate 결과 집계 기준이 정의되어 있다.
12. BLOCKER 또는 FATAL이 최종 PASS를 차단한다.
13. exit code 계약이 정의되어 있다.
14. RC Report 생성 기준이 정의되어 있다.
15. Evidence traceability 기준이 정의되어 있다.
16. 실패 run 보존과 재실행 정책이 정의되어 있다.
17. false pass 검증 suite가 정의되어 있다.
18. Phase 10 regression test가 정의되어 있다.

완료 기준은 성능 향상 보장이 아니라 릴리즈 후보 판정이 원본 Evidence와 일관되고, false pass를 만들 수 없는 구조라는 것이다.

## 24. Phase 10 BLOCKER 조건

다음 조건은 Phase 10 BLOCKER로 분류한다.

```text
Release binary EXE SHA-256 누락
git commit 누락
Static Safety Gate 위반
RuntimeValidation BLOCKER
Rollback failure
failed rollback state 미보존
Processor Group group 0 hardcoding
ApplyGuard 우회
Unknown 또는 MonitoringOnly Profile mutation
AntiCheatSensitive Profile mutation
Aggressive Mode 계약 위반
final runtime snapshot 누락
policy timeline monotonicity 실패
rollback summary 누락
runtime validation summary 누락
Evidence fatal missing
BLOCKER 존재 상태에서 최종 PASS 생성
BLOCKER 상태에서 exit code 0
Gate 내부 FAIL이 최종 판정에 미반영
RC Report와 구조화 보고서 판정 불일치
RC Report 핵심 값의 원본 Evidence 추적 불가
```

BLOCKER는 성능 수치와 무관하게 RC PASS를 차단한다.

## 25. 패치 순서

패치 적용 순서는 다음을 따른다.

```text
1. P10-01 RC Gate Contract Alignment
2. P10-02 Gate Execution Model
3. P10-03 Build Identity Gate
4. P10-04 Documentation Gate
5. P10-05 Static Safety Gate
6. P10-06 Runtime Safety Gate
7. P10-07 Rollback Gate
8. P10-08 Game Profile Gate
9. P10-09 Performance Validation Gate
10. P10-10 Evidence Completeness Gate
11. P10-11 Long Soak Gate
12. P10-12 Gate Result Aggregation
13. P10-13 Exit Code Contract
14. P10-14 RC Report Generation
15. P10-15 Evidence Traceability and Manifest
16. P10-16 Failure Preservation and Resume Policy
17. P10-17 Phase 10 Regression Test and False Pass Suite
```

주의:

```text
P10-03, P10-06, P10-07, P10-10, P10-12, P10-13은 Phase 10의 핵심 안전 패치다.

P10-12에서 개별 Gate FAIL이 최종 결과에 반영되지 않으면 Phase 10을 중단한다.
P10-13에서 BLOCKER가 exit code 0으로 종료되면 Phase 10을 중단한다.
P10-15에서 RC Report의 핵심 값을 원본 Evidence로 추적할 수 없으면 Phase 10을 중단한다.
```

## 26. 패치 작성 규칙

각 실제 코드 또는 스크립트 패치는 다음 규칙을 따른다.

```text
1. [PATCH] 모드로 작성한다.
2. 변경 전 실제 파일 경로와 함수/스크립트 진입점을 확인한다.
3. 인터페이스 변경 시 선언, 정의, 호출부, 테스트를 모두 수정한다.
4. std::expected는 Assign → Check → Bind 패턴을 따른다.
5. expected 결과를 검사 없이 return func(...) 형태로 전파하지 않는다.
6. 하위 프로세스 exit code를 반드시 검사한다.
7. main/Test build target을 분리한다.
8. B9 Gate 검증 결과를 기록한다.
9. BEFORE/AFTER 문맥을 제공한다.
10. Conventional Commit 메시지를 제안한다.
11. BLOCKER를 WARN으로 낮추지 않는다.
12. Gate 내부 FAIL을 최종 결과에 반드시 반영한다.
13. report 생성 성공과 RC PASS를 구분한다.
14. 실패 run의 Evidence를 삭제하지 않는다.
15. 원본 artifact 없이 보고서 값을 임의 생성하지 않는다.
```

문서 작성 규칙:

```text
1. 구현 코드를 작성하지 않는다.
2. 구체 함수 시그니처를 확정하지 않는다.
3. 확정되지 않은 도구와 수치는 TBD로 둔다.
4. 성능 향상을 보장하지 않는다.
5. 특정 게임을 공식 지원한다고 쓰지 않는다.
6. Anti-Cheat 우회 정책을 작성하지 않는다.
7. RuntimeValidation BLOCKER 상태에서 PASS를 허용하지 않는다.
8. Rollback failure 상태에서 PASS를 허용하지 않는다.
9. Evidence fatal missing 상태에서 PASS를 허용하지 않는다.
10. Gate 내부 FAIL을 최종 판정에서 누락하지 않는다.
11. BLOCKER 상태의 exit code 0을 허용하지 않는다.
12. RC Report 생성 성공을 RC PASS로 간주하지 않는다.
13. Markdown 형식으로 작성한다.
```

## 27. Non-Goals

다음은 PatchPlan_Phase10.md의 비목표다.

```text
구체 C++ 구현 코드 작성
실제 release script 작성
테스트 코드 직접 작성
성능 향상 수치 보장
새로운 최적화 정책 추가
공식 지원 게임 목록 확정
설치 프로그램 구현
GUI release dashboard 구현
자동 배포 구현
클라우드 업로드 구현
Anti-Cheat 우회
DLL Injection
Kernel Driver
Game Memory Patch
```

Non-Goals를 위반하는 변경은 Phase 10 범위 밖이며 별도 승인 문서가 필요하다.

## 28. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. Phase 10의 목적과 비목표가 명확하다.
2. Phase 10 영향 모듈과 변경 금지 모듈이 구분되어 있다.
3. P10-01부터 P10-17까지 패치 단위가 정의되어 있다.
4. 각 패치의 목적, 작업 범위, 수정 가능 파일, 수정 금지 파일, 구현 규칙, 완료 기준, 테스트, Evidence, 리스크, Rollback 계획이 정의되어 있다.
5. Build / Documentation / Static / Runtime / Rollback / Profile / Performance / Evidence / Soak Gate가 정의되어 있다.
6. Gate Result Aggregation 기준이 정의되어 있다.
7. exit code 계약이 정의되어 있다.
8. RC Report와 Evidence Traceability 기준이 정의되어 있다.
9. False Pass 방지 suite가 정의되어 있다.
10. Phase 10 BLOCKER 조건이 정의되어 있다.
11. 패치 순서가 정의되어 있다.
12. 이후 실제 개발 착수에 필요한 전체 Patch Plan이 완성되어 있다.

Approval Criteria:

1. RuntimeValidation BLOCKER, Rollback failure, Evidence fatal missing 상태에서 PASS를 허용하는 문장이 없어야 한다.
2. Gate 내부 FAIL과 nested check failure가 최종 판정과 exit code에 반영되어야 한다.
3. RC Report 생성 성공과 RC PASS가 명확히 분리되어야 한다.
4. Build identity와 원본 Evidence traceability가 RC 승인 근거로 정의되어야 한다.
5. 사람이 확인해야 하는 항목을 자동 PASS로 위장하지 않는 기준이 명확해야 한다.

## 29. Open Questions

### Blocking

1. RC Gate runner의 primary 언어는 Python, Batch, PowerShell 중 무엇으로 둘 것인가?
2. PowerShell이 설치되지 않은 환경을 공식 지원할 것인가?
3. PASS_WITH_WARNINGS에서 exit code 0을 허용할 것인가?
4. NEUTRAL 성능 결과를 RC PASS로 허용할 것인가?
5. Long Soak Gate를 모든 RC에서 필수로 둘 것인가?
6. 실제 게임 검증을 RC 필수 Gate로 둘 것인가?
7. Documentation Gate를 자동화할 범위는 어디까지인가?
8. dirty working tree는 WARN인가 BLOCKER인가?
9. Evidence 파일 hash는 모든 artifact에 적용할 것인가, 필수 artifact에만 적용할 것인가?
10. RC Report의 구조화된 형식은 JSON으로 확정할 것인가?
11. failed RC run을 재실행할 때 이전 run의 artifact를 참조할 수 있는가?
12. Phase 10 완료 후 실제 구현은 P1-01부터 순서대로 시작할 것인가, 현재 코드 상태와의 gap analysis를 먼저 수행할 것인가?

### Non-blocking

1. Static Safety Gate는 lexical 검사만 유지할 것인가, clang-tidy 또는 별도 AST 검사를 추가할 것인가?
2. Release Gate 실패 시 자동 rollback 검증까지 수행할 것인가?

Blocking 질문은 Phase 10 구현 전 결정해야 한다. Non-blocking 질문은 false pass 방지와 evidence preservation 기준을 유지하는 조건에서 후속 개발 계획으로 이월할 수 있다.
