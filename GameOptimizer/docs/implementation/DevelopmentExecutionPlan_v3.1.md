> Status: Proposal  
> 이 문서는 제안 또는 미래 설계를 설명합니다. `docs/DOCUMENT_REGISTER.md`에서 승격되기 전까지 현재 구현 동작으로 간주하지 않습니다.

# GameOptimizer v3.1 Development Execution Plan

## 1. 문서 개요

문서명: GameOptimizer v3.1 Development Execution Plan

버전: v1.0

작성 목적: GameOptimizer v3.1의 Phase 1~10을 GitHub Issues 기반 개발 작업으로 실행하기 위한 개발 실행 계획(Development Execution Plan)을 정의한다.

Development Execution Plan의 목적은 많은 코드를 빠르게 생성하는 것이 아니라, 각 변경을 검증 가능한 작은 패치로 제한하고 저장소의 실제 상태에 근거해 안전하게 통합하는 것이다.

적용 범위:

- 저장소 기준선(Repository Baseline) 확보
- 차이 분석(Gap Analysis)
- 패치 범위 고정(Patch Scope Lock)
- 작업 단위(Execution Unit)
- 빌드, 테스트, 정적 안전 검사, 런타임 검증
- Evidence Review와 커밋 기준
- Phase 1~10 단계 종료 게이트(Phase Exit Gate)
- AI 개발 에이전트 입력, 출력, 제한 규칙

비적용 범위:

- 구체 C++ 구현 코드 작성
- 실제 패치 파일 생성
- 실제 테스트 실행
- 개발 완료 날짜 확정
- 인력별 공수 산정
- 성능 향상 보장
- 공식 지원 게임 확정
- Phase 전체 일괄 구현

상위 문서:

- `GameOptimizer_v3_SRS_Rev1_3_CPP23_Safety_Update.docx`
- `docs/vision/PVD_v1.0.md`
- `docs/proposals/performance/PerformanceEngineSpec.md`
- `docs/proposals/performance/PolicySpecification.md`
- `docs/proposals/performance/GameProfileSpecification.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-005_RollbackManager.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/modules/MDS-009_RuntimeOrchestrator.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/validation/RegressionMatrix_v3.1.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/release/RC_Report_Template_v3.1.md`
- `docs/implementation/ImplementationPlan_v3.1.md`
- GitHub Issues
- `docs/archive/patch-plans/PatchPlan_Phase1.md` ~ `docs/archive/patch-plans/PatchPlan_Phase10.md` (archived historical reference)

후속 문서:

- `docs/implementation/RepositoryGapAnalysis_v3.1.md`
- `docs/workorders/WO-000_RepositoryBaselineAndGapAnalysis.md`
- `docs/workorders/Phase1_WorkOrder.md`

문서 경로 기준: `GameOptimizer/docs/`

기본 실행 흐름:

```text
Repository Inspection
→ Gap Analysis
→ Patch Scope Lock
→ Interface Inspection
→ Implementation
→ Compile
→ Unit / Integration Test
→ Static Safety Check
→ Runtime Validation
→ Evidence Review
→ Commit
→ Phase Gate
```

## 2. 실행 목표

실행 목표:

1. 현재 저장소 상태를 기준선으로 고정
2. 문서 요구사항과 실제 코드 간 Gap 식별
3. Phase 1~10을 Patch ID 단위로 실행
4. 패치별 수정 범위 통제
5. 패치별 빌드, 테스트, Evidence 의무화
6. Phase별 Exit Gate 적용
7. BLOCKER의 다음 Phase 전파 금지
8. AI 에이전트 작업 결과의 재현성 확보
9. 커밋과 Evidence의 1:1 추적성 확보
10. 최종 RC Gate까지 동일한 계약 유지

핵심 원칙:

1. 실제 저장소를 조사하기 전 구현 계획을 확정하지 않는다.
2. 전체 Phase를 하나의 패치로 구현하지 않는다.
3. 한 번에 하나의 Patch ID만 실행한다.
4. 현재 코드에 이미 구현된 기능은 중복 구현하지 않는다.
5. 문서와 실제 코드가 다르면 실제 코드를 조사한 뒤 Gap을 기록한다.
6. 인터페이스 변경 전 실제 헤더 시그니처를 확인한다.
7. 인터페이스 변경 시 선언, 정의, 모든 호출부, 테스트를 함께 수정한다.
8. 컴파일 성공 전 다음 Patch로 진행하지 않는다.
9. 테스트 실패 상태에서 다음 Patch로 진행하지 않는다.
10. Release-facing BLOCKER를 미해결 상태로 다음 Phase에 넘기지 않는다.
11. 코드 변경과 문서 변경을 무관하게 분리하지 않는다.
12. Evidence가 없는 패치를 완료로 처리하지 않는다.
13. AI가 추측한 파일명, 클래스, 함수, 테스트 타깃을 실제 존재하는 것으로 취급하지 않는다.
14. 기능 추가보다 ownership, safety, rollback, evidence 계약을 우선한다.
15. Aggressive Single-Core Mode는 Phase 1~8의 Gate 통과 전 구현하지 않는다.

## 3. 비목표

DevelopmentExecutionPlan_v3.1.md의 비목표:

- 구체 C++ 구현 코드 작성
- 실제 패치 파일 생성
- 실제 테스트 실행
- 개발 완료 날짜 확정
- 인력별 공수 산정
- 성능 향상 보장
- 공식 지원 게임 확정
- Phase 전체 일괄 구현
- AI가 저장소를 조사하지 않고 파일 구조를 추측하게 하는 지시

본 문서는 구현을 시작하기 위한 실행 계약이며, 실제 저장소 조사 결과를 대체하지 않는다.

## 4. 실행 상태 정의

각 Patch의 상태는 다음 값만 사용한다.

```text
NOT_STARTED
INSPECTING
GAP_IDENTIFIED
SCOPE_LOCKED
IMPLEMENTING
BUILDING
TESTING
VALIDATING
EVIDENCE_REVIEW
READY_TO_COMMIT
COMMITTED
BLOCKED
FAILED
DEFERRED
SUPERSEDED
```

상태 규칙:

- `NOT_STARTED`에서 `INSPECTING` 없이 `IMPLEMENTING`으로 전환하지 않는다.
- `INSPECTING` 이후 상태는 `GAP_IDENTIFIED` 또는 `SCOPE_LOCKED`다.
- `SCOPE_LOCKED` 이후 수정 범위를 무단 확장하지 않는다.
- `BUILDING` 실패 상태에서 `TESTING`으로 전환하지 않는다.
- `TESTING` 실패 상태에서 `READY_TO_COMMIT`으로 전환하지 않는다.
- Evidence 누락 상태에서 `COMMITTED` 완료 처리하지 않는다.
- `BLOCKED` 상태는 blocking reason을 기록한다.
- `DEFERRED` 상태는 이관 Phase와 사유를 기록한다.

## 5. 실행 우선순위

### P0 - 안전성 및 무결성

- Rollback / ApplyGuard
- RuntimeValidation BLOCKER
- Evidence completeness
- Processor Group safety
- ThreadTracker observation-only
- Mutation ownership
- `std::expected` contract
- Shutdown safety

### P1 - 판단 및 정책 경계

- MainThreadConfidenceAnalyzer
- PolicyCandidate
- PolicyDispatcher
- Game Profile restriction
- Performance classification

### P2 - 확장 기능

- Aggressive Single-Core Mode
- Background restriction 확장
- Long Soak 자동화
- 외부 FPS / frame time 연동
- 추가 Game Profile

규칙:

- P0 미완료 상태에서 P2 구현을 시작하지 않는다.
- P0 BLOCKER가 있으면 모든 성능 기능 확장을 중단한다.

## 6. Execution Step 0 - Repository Baseline

실제 Phase 1 착수 전에 저장소 기준선(Repository Baseline)을 확보한다.

필수 조사 항목:

- repository root
- current branch
- git commit
- working tree state
- project/solution files
- Release x64 build target
- test build target
- compiler/toolset
- C++ standard
- source directory layout
- test directory layout
- script directory layout
- artifact directory layout
- existing documentation paths
- existing release gate scripts

필수 산출물:

- repositoryBaseline
- baselineGitCommit
- baselineBuildResult
- baselineTestResult
- baselineStaticCheckResult
- existingArtifactInventory

규칙:

- working tree가 dirty이면 기존 변경과 신규 작업을 구분한다.
- 기존 변경을 AI가 임의로 삭제하거나 덮어쓰지 않는다.
- baseline build 실패는 신규 코드로 숨기지 않는다.
- baseline 단계에서 실행하지 못한 검사는 `NOT_RUN` 또는 `BLOCKED`로 기록한다.

## 7. Execution Step 1 - Repository Gap Analysis

Repository Gap Analysis는 문서 요구사항과 실제 구현을 비교한다.

비교 대상:

- Phase 1~10 Patch ID
- MDS-001~009
- Safety Contract
- Runtime Contract
- Regression Matrix
- Release Checklist

각 Patch ID 분류:

```text
NOT_IMPLEMENTED
PARTIALLY_IMPLEMENTED
IMPLEMENTED_UNVERIFIED
IMPLEMENTED_WITH_GAP
IMPLEMENTED_AND_VERIFIED
OBSOLETE
CONFLICT
```

Gap 항목 필드:

- gapId
- phase
- patchId
- requirement
- currentImplementation
- missingBehavior
- contractViolation
- affectedFiles
- affectedTests
- severity
- recommendedAction
- dependency

코드가 이미 존재한다는 사실만으로 IMPLEMENTED_AND_VERIFIED로 분류하지 않는다.
테스트와 Evidence가 없으면 IMPLEMENTED_UNVERIFIED 또는 IMPLEMENTED_WITH_GAP으로 분류한다.

## 8. Repository Gap Analysis 종료 조건

다음 조건을 충족하기 전 P1-01 구현을 시작하지 않는다.

1. 주요 소스 디렉터리 확인
2. 실제 클래스와 헤더 위치 확인
3. 실제 빌드 타깃 확인
4. 실제 테스트 타깃 확인
5. 기존 Rollback / ApplyGuard 구조 확인
6. 기존 Evidence / RuntimeValidation 구조 확인
7. Processor Group 관련 현재 구현 확인
8. Phase 1~10 Patch 상태 분류 완료
9. P0 BLOCKER 목록 작성
10. 첫 번째 실행 Patch 선정

## 9. 브랜치 전략

기본 브랜치 모델:

```text
master 또는 main
└─ development/v3.1
   ├─ phase1/p1-01-contract-alignment
   ├─ phase1/p1-02-threadtracker-safety
   ├─ phase1/p1-03-thread-snapshot
   └─ ...
```

브랜치 명명 규칙:

```text
phase<번호>/p<번호>-<패치번호>-<짧은설명>
```

예시:

- `phase1/p1-02-threadtracker-static-safety`
- `phase4/p4-06-group-affinity-safety`
- `phase5/p5-05-failed-state-preservation`
- `phase10/p10-12-gate-result-aggregation`

규칙:

- 하나의 브랜치에 서로 다른 Phase를 혼합하지 않는다.
- 긴급 BLOCKER 수정은 `fix/<관련패치ID>-<설명>`을 사용할 수 있다.
- 브랜치 생성 전 기준 commit을 기록한다.
- 기본 개발 브랜치 확정 전에는 Open Questions에 남긴다.

## 10. 커밋 전략

하나의 Patch ID는 하나 이상의 작은 커밋으로 나눌 수 있다.

권장 커밋 구조:

1. 계약 또는 타입 정리
2. 구현 변경
3. 테스트 추가
4. Evidence / validator 연결
5. 문서 동기화

Conventional Commit 형식:

```text
feat(module): ...
fix(module): ...
refactor(module): ...
test(module): ...
docs(module): ...
build(module): ...
chore(release): ...
```

예시:

- `fix(topology): remove processor group zero fallback`
- `refactor(thread-tracker): expose observation snapshot`
- `test(rollback): preserve failed rollback state`
- `feat(evidence): add runtime validation summary`

커밋 메시지 본문에 포함할 항목:

- Patch ID
- 변경 이유
- 안전 계약 영향
- 테스트 결과
- Evidence 경로
- Known limitations

## 11. Patch Execution Unit

모든 Patch ID는 다음 작업 단위(Execution Unit)를 따른다.

```text
Step 1. Inspect
Step 2. Gap Confirm
Step 3. Scope Lock
Step 4. Interface Review
Step 5. Patch Design
Step 6. Implement
Step 7. Compile
Step 8. Test
Step 9. Static Safety Check
Step 10. Runtime Validation
Step 11. Evidence Review
Step 12. Commit
Step 13. Patch Close
```

## 12. Step 1 - Inspect

AI 에이전트는 구현 전에 다음을 확인한다.

- 관련 실제 헤더
- 관련 실제 소스
- 모든 직접 호출부
- 관련 테스트
- 빌드 프로젝트 파일
- release/static validation scripts
- 문서 계약

필수 출력:

```text
[INSPECTION]
- files inspected
- actual declarations
- actual definitions
- direct callers
- related tests
- existing behavior
- detected gaps
- uncertainty
```

금지:

- 실제 파일을 열지 않고 함수 시그니처 추측
- 파일명 추측
- 클래스가 존재한다고 가정
- 문서 예시 타입을 실제 타입으로 단정

## 13. Step 2 - Gap Confirm

현재 코드와 Patch Plan의 차이를 분류한다.

- missing implementation
- partial implementation
- contract mismatch
- test gap
- evidence gap
- documentation gap
- no change required

`no change required`인 경우에도 다음을 확인한다.

- 빌드
- 관련 테스트
- Regression Matrix 연결
- Evidence 존재

## 14. Step 3 - Scope Lock

각 Patch 착수 전 수정 범위를 고정한다.

필수 필드:

- patchId
- targetBehavior
- allowedFiles
- forbiddenFiles
- directCallers
- testsToModify
- documentsToUpdate
- outOfScope

규칙:

- Scope Lock 이후 새로운 모듈 수정이 필요하면 작업을 중단하고 범위를 재승인한다.
- AI가 편의를 위해 인접 모듈을 함께 리팩터링하지 않는다.
- allowedFiles 밖 변경은 `SCOPE_CONFLICT`로 기록한다.

## 15. Step 4 - Interface Review

다음 변경이 있는지 확인한다.

- function signature
- return type
- error type
- struct/class layout
- enum
- ownership
- thread-safety contract
- Evidence schema

인터페이스 변경 시 필수 대상:

- declaration
- definition
- all call sites
- unit tests
- integration tests
- mock/fake
- documentation
- validator/report consumer

Partial Interface Update는 금지한다.

## 16. Step 5 - Patch Design

구현 전 설계 요약을 작성한다.

필수 출력:

```text
[PATCH DESIGN]
- patchId
- current behavior
- target behavior
- ownership impact
- error handling impact
- rollback impact
- evidence impact
- test plan
- rollback plan
```

설계 단계에서 확인할 질문:

- mutation이 발생하는가?
- rollback state가 필요한가?
- ApplyGuard가 필요한가?
- Processor Group 정보가 필요한가?
- identity validation이 필요한가?
- RuntimeValidation 항목을 추가해야 하는가?
- Evidence 필드가 추가되는가?
- Release Gate가 영향을 받는가?

## 17. Step 6 - Implement

구현 규칙:

1. C++23 / MSVC v143 기준
2. camelCase identifier 사용
3. 기능과 테스트 타깃 분리
4. named constant 사용
5. ownership 명시
6. RAII 사용
7. query와 mutation 분리
8. controller ownership 준수
9. 최소 변경
10. 불필요한 추상화 추가 금지

### std::expected Canonical Pattern

반드시 다음 순서를 사용한다.

```text
Assign
→ Check
→ Log / Add Context
→ Bind
→ Consume
```

금지:

- `return func(...);`
- `auto value = *func(...);`
- `func(...)->member;`
- `for (auto& value : func(...));`
- expected를 검사 없이 다른 함수에 전달

허용 예시의 개념:

```text
auto result = function();

if (!result)
{
    log error with context;
    return unexpected(...);
}

auto& value = *result;
```

## 18. Windows Platform 규칙

다음을 확인한다.

- Windows header include 순서
- `NOMINMAX` / `WIN32_LEAN_AND_MEAN` 기준
- HANDLE ownership
- `CloseHandle` 누락
- `GetLastError` 보존 시점
- Processor Group API 사용
- `KAFFINITY`와 Group 일치
- `SetThreadGroupAffinity` 사용 경계
- PID reuse 방지
- process creation time 검증

금지:

- Processor Group 정보 누락 시 group 0 fallback
- `GetProcessAffinityMask`만으로 HEDT 전체 지원 주장
- identity validation 없는 mutation/rollback

## 19. Step 7 - Compile

빌드 순서:

1. 영향받은 Unit Test target
2. 전체 Test target
3. Main Debug x64 target
4. Main Release x64 target

기준:

- Visual Studio 2022
- MSVC v143
- `/std:c++latest` 또는 프로젝트 C++23 기준
- `/W4`
- `/WX`
- `/permissive-`
- Release `/O2`

규칙:

- 첫 번째 컴파일 오류부터 수정한다.
- 후속 cascade 오류를 독립 오류로 취급하지 않는다.
- 경고를 비활성화해 통과시키지 않는다.
- main 함수 중복으로 test target이 실패하지 않게 분리한다.

필수 출력:

```text
[BUILD]
- target
- configuration
- result
- first error if failed
- warning count
- binary path
```

## 20. Step 8 - Test

테스트 순서:

1. 신규 Unit Test
2. 수정 모듈 Unit Test
3. 직접 연계 Integration Test
4. Regression Matrix 관련 Test
5. Failure Injection Test
6. 전체 Test Suite

테스트 상태:

```text
PASS
PASS_WITH_WARNINGS
FAIL
NOT_RUN
BLOCKED
INVALID_RUN
```

규칙:

- `NOT_RUN`을 `PASS`로 기록하지 않는다.
- 기존 실패와 신규 실패를 구분한다.
- 첫 실패 후 전체 결과를 잃지 않도록 가능한 경우 테스트를 계속 수집한다.
- unsafe mutation 가능성이 있으면 즉시 중단한다.

## 21. Step 9 - Static Safety Check

필수 검사:

- ThreadTracker direct mutation
- PerformanceEngine direct mutation
- PolicyDispatcher direct mutation
- AggressiveSingleCoreController direct mutation
- ApplyGuard 우회
- rollback state 없는 mutation
- `std::expected` 무검사 사용
- Processor Group group 0 hardcoding
- forbidden API ownership

규칙:

- 정적 검사 PASS는 runtime correctness의 완전한 증명이 아니다.
- false positive와 false negative 가능성을 결과에 명시한다.

## 22. Step 10 - Runtime Validation

mutation 또는 runtime state에 영향을 주는 Patch는 다음을 확인한다.

- RuntimeState transition
- heartbeat progression
- snapshot freshness
- policy timeline monotonicity
- active policy consistency
- rollback state consistency
- identity consistency
- shutdown cleanliness
- RuntimeValidation BLOCKER count

규칙:

- RuntimeValidation BLOCKER가 있으면 Patch 완료 금지.
- RuntimeValidation을 실행하지 못하면 `NOT_RUN` 사유를 Evidence에 기록한다.

## 23. Step 11 - Evidence Review

각 Patch는 최소 다음 Evidence를 남긴다.

- patchId
- git commit candidate
- changed files
- build result
- test result
- static safety result
- runtime validation result
- evidence artifact paths
- blocker list
- accepted warnings
- reviewer notes

Evidence 상태:

```text
COMPLETE
INCOMPLETE
INVALID
MISSING
```

규칙:

- Evidence가 `MISSING` 또는 `INVALID`이면 `READY_TO_COMMIT`으로 전환하지 않는다.
- 실행하지 않은 검사 결과를 PASS로 대체하지 않는다.
- Evidence 경로와 커밋 후보는 1:1로 추적한다.

## 24. Step 12 - Commit

Commit은 다음 조건을 모두 만족한 뒤 수행한다.

- Scope Lock 위반 없음
- 빌드 결과 기록 완료
- 테스트 결과 기록 완료
- Static Safety Check 결과 기록 완료
- RuntimeValidation 결과 기록 완료
- Evidence Review 완료
- 문서 동기화 완료
- Conventional Commit 메시지 작성

필수 출력:

```text
[COMMIT CANDIDATE]
- patchId
- branch
- baseCommit
- changedFiles
- evidencePaths
- commitMessage
- remainingRisk
```

## 25. Step 13 - Patch Close

Patch 종료 기록 필드:

- patchId
- status
- commitHash
- changedFiles
- testIds
- evidencePaths
- releaseGateImpact
- remainingWarnings
- deferredItems
- nextPatch

완료 상태:

```text
COMMITTED
BLOCKED
DEFERRED
SUPERSEDED
```

## 26. Definition of Ready

Patch 착수 조건:

1. Patch Plan 존재
2. 관련 MDS 존재
3. 실제 파일 위치 확인
4. 실제 인터페이스 확인
5. 직접 호출부 확인
6. 테스트 타깃 확인
7. Scope Lock 완료
8. dependency Patch 완료
9. unresolved P0 BLOCKER 없음
10. rollback/evidence 영향 분석 완료

하나라도 충족하지 않으면 `NOT_READY`로 기록한다.

## 27. Definition of Done

Patch 완료 조건:

1. 목표 behavior 구현
2. scope violation 없음
3. Debug x64 빌드 성공
4. Release x64 빌드 성공
5. 신규/수정 테스트 PASS
6. 관련 Regression Test PASS
7. Static Safety Check PASS
8. RuntimeValidation BLOCKER 0
9. rollback 관련 Patch는 rollback test PASS
10. Evidence 생성 및 검증
11. 문서 동기화
12. Conventional Commit 완료
13. Regression Matrix 상태 갱신

코드 작성 완료는 Definition of Done이 아니다.

## 28. AI Agent 역할

AI 에이전트는 다음 역할만 수행한다.

- Repository Inspector
- Patch Engineer
- Test Author
- Build/Validation Operator
- Evidence Reporter

AI 에이전트는 다음 권한을 갖지 않는다.

- Architecture contract 임의 변경
- Release BLOCKER severity 하향
- 공식 지원 게임 선언
- 검증되지 않은 값 확정
- scope 밖 대규모 리팩터링
- 사용자 승인 없는 destructive change

## 29. AI Agent 필수 입력

각 작업 요청은 최소 다음 정보를 포함해야 한다.

- patchId
- target branch
- base commit
- target behavior
- reference documents
- allowed files
- forbidden files
- build targets
- test targets
- required evidence
- stop conditions

입력이 부족하면 AI는 구현을 시작하지 않고 누락 항목을 보고한다.

## 30. AI Agent 필수 출력

각 작업 종료 시 다음 형식으로 출력한다.

```text
[RESULT]
- patchId:
- status:
- inspectedFiles:
- changedFiles:
- interfaceChanges:
- buildResults:
- testResults:
- staticCheckResults:
- runtimeValidation:
- evidenceCreated:
- blockers:
- warnings:
- remainingGaps:
- commitMessage:
- nextRecommendedPatch:
```

코드를 요청받은 경우:

- 수정 코드는 파일 단위로 제공
- 변경 요약은 대화에 짧게 제공
- BEFORE / AFTER 문맥 제공

## 31. AI Agent Stop Conditions

다음 조건에서는 즉시 작업을 중단한다.

- 실제 헤더와 요청된 시그니처 불일치
- 호출부 범위를 확인할 수 없음
- Patch Scope 밖 파일 수정 필요
- rollback state 없이 mutation 필요
- ApplyGuard 우회 필요
- Processor Group 정보 없이 affinity 적용 필요
- identity validation 불가능
- RuntimeValidation BLOCKER 발견
- 기존 unrelated test failure 원인 불명
- Evidence contract 충돌
- 문서 간 BLOCKER 수준 충돌

중단 시 출력:

```text
[STOP]
- patchId
- stopReason
- affectedFiles
- detectedConflict
- requiredDecision
- safeNextAction
```

## 32. Phase 1 실행

목표: Observation & Topology Foundation

실행 순서:

```text
P1-01 → P1-02 → P1-03 → P1-04 → P1-05
→ P1-06 → P1-07 → P1-08 → P1-09 → P1-10
```

Phase Exit Gate:

- ThreadTracker observation-only
- Processor Group group-aware
- group 0 hardcoding 없음
- Topology incomplete fallback 안전
- Phase 1 Regression PASS

BLOCKER가 있으면 Phase 2 진입 금지.

## 33. Phase 2 실행

목표: Main Thread Confidence Engine

진입 조건:

- Phase 1 Exit Gate PASS
- ThreadTracker observation snapshot 안정
- TopologyResult 사용 가능

실행 순서:

```text
P2-01 → P2-02 → P2-03 → P2-04 → P2-05
→ P2-06 → P2-07 → P2-08 → P2-09 → P2-10
```

Phase Exit Gate:

- Low / Medium / High / VeryHigh 분류
- insufficient evidence 처리
- Confidence < High mutation eligibility 차단
- confidence history Evidence
- Phase 2 Regression PASS

## 34. Phase 3 실행

목표: Policy Candidate & Dispatch Layer

진입 조건:

- Phase 2 Exit Gate PASS
- Confidence eligibility contract 고정

실행 순서:

```text
P3-01 → P3-02 → P3-03 → P3-04 → P3-05 → P3-06
→ P3-07 → P3-08 → P3-09 → P3-10 → P3-11
```

Phase Exit Gate:

- requiredController 검증
- requiredRollbackScope 검증
- profile restriction
- cooldown/conflict
- PolicyDispatcher direct mutation 없음
- Phase 3 Regression PASS

## 35. Phase 4 실행

목표: SchedulerController Safety Hardening

진입 조건:

- Phase 3 Exit Gate PASS
- PolicyDispatcher handoff contract 고정

실행 순서:

```text
P4-01 → P4-02 → P4-03 → P4-04 → P4-05 → P4-06 → P4-07
→ P4-08 → P4-09 → P4-10 → P4-11 → P4-12 → P4-13
```

Phase Exit Gate:

- Query → Save → Arm → Apply → Verify → Commit/Rollback
- Processor Group safety
- identity validation
- scheduler Evidence
- Phase 4 Regression PASS

## 36. Phase 5 실행

목표: Rollback / ApplyGuard Contract Hardening

진입 조건:

- Phase 4 scheduler transaction 경로 확인

실행 순서:

```text
P5-01 → P5-02 → P5-03 → P5-04 → P5-05 → P5-06
→ P5-07 → P5-08 → P5-09 → P5-10 → P5-11 → P5-12
```

Phase Exit Gate:

- SaveDisposition
- identity revalidation
- living identity failure BLOCKER
- failed state preservation
- ApplyGuard state machine
- destructor audit
- shutdown rollback
- Phase 5 Regression PASS

## 37. Phase 6 실행

목표: Evidence & Runtime Validation Integration

진입 조건:

- Phase 1~5 주요 runtime contract 안정

실행 순서:

```text
P6-01 → P6-02 → P6-03 → P6-04 → P6-05 → P6-06 → P6-07
→ P6-08 → P6-09 → P6-10 → P6-11 → P6-12 → P6-13
```

Phase Exit Gate:

- session manifest
- latest/final snapshot
- policy timeline
- rollback summary
- runtime validation summary
- evidence completeness
- classification
- shutdown flush
- Phase 6 Regression PASS

## 38. Phase 7 실행

목표: Game Profile Integration

진입 조건:

- Policy/Evidence/RuntimeValidation 기반 완료

실행 순서:

```text
P7-01 → P7-02 → P7-03 → P7-04 → P7-05 → P7-06 → P7-07
→ P7-08 → P7-09 → P7-10 → P7-11 → P7-12 → P7-13
```

Phase Exit Gate:

- Profile Registry
- target binding
- Unknown fallback
- status enforcement
- blocked policy precedence
- MonitoringOnly/AntiCheat mutation 차단
- profile Evidence
- Phase 7 Regression PASS

## 39. Phase 8 실행

목표: Performance Validation Flow

진입 조건:

- Phase 6 Evidence 체계 완료
- Phase 7 Profile 제한 완료

실행 순서:

```text
P8-01 → P8-02 → P8-03 → P8-04 → P8-05 → P8-06 → P8-07
→ P8-08 → P8-09 → P8-10 → P8-11 → P8-12 → P8-13 → P8-14
```

Phase Exit Gate:

- Baseline Window
- Applied Window
- Before/After Comparison
- classification
- regression detection
- safety override
- soak flow
- Phase 8 Regression PASS

## 40. Phase 9 실행

목표: Aggressive Single-Core Mode

진입 조건:

- Phase 1~8 Exit Gate PASS
- P0 BLOCKER 0
- Rollback failure 0
- RuntimeValidation BLOCKER 0
- Evidence completeness PASS

실행 순서:

```text
P9-01 → P9-02 → P9-03 → P9-04 → P9-05 → P9-06 → P9-07 → P9-08
→ P9-09 → P9-10 → P9-11 → P9-12 → P9-13 → P9-14 → P9-15
```

Phase Exit Gate:

- Composite Activation Gate
- Policy Bundle
- Controller handoff
- partial failure rollback
- Active verification
- automatic deactivation
- cooldown/hysteresis
- failure injection
- Phase 9 Regression PASS

규칙:

- Phase 9 진입 조건 미충족 시 v3.2로 이관할 수 있다.
- 진입 조건을 완화해 v3.1에 강제 포함하지 않는다.

## 41. Phase 10 실행

목표: RC Gate Integration

진입 조건:

- 릴리즈 대상 Phase 구현 완료
- Regression Matrix 갱신
- Evidence schema 안정

실행 순서:

```text
P10-01 → P10-02 → P10-03 → P10-04 → P10-05 → P10-06
→ P10-07 → P10-08 → P10-09 → P10-10 → P10-11 → P10-12
→ P10-13 → P10-14 → P10-15 → P10-16 → P10-17
```

Phase Exit Gate:

- Build Identity
- Static Safety
- Runtime Safety
- Rollback
- Profile
- Performance
- Evidence Completeness
- Gate Aggregation
- Exit Code
- RC Report
- False Pass Suite

Phase 10 완료는 RC 승인 자체와 동일하지 않다.
실제 RC Run은 별도로 수행한다.

## 42. 병렬 가능 작업

다음 작업은 interface가 고정된 경우에만 병렬 수행할 수 있다.

- 문서 보완
- 독립 Unit Test 작성
- Static Check script 개선
- Evidence validator 작성
- Report template 작성

병렬 작업은 서로 다른 Patch ID라도 동일 인터페이스를 수정하지 않을 때만 허용한다.

## 43. 병렬 금지 작업

다음은 순차 수행한다.

- Phase 1 → Phase 2 confidence input
- Phase 2 → Phase 3 policy eligibility
- Phase 3 → Phase 4 controller handoff
- Phase 4 → Phase 5 rollback contract
- Phase 5 → Phase 6 rollback evidence
- Phase 6 → Phase 8 performance classification
- Phase 7 → Phase 9 profile gate
- Phase 8 → Phase 9 regression response
- Phase 9 → Phase 10 aggressive gate

동일 인터페이스를 수정하는 Patch의 병렬 작업을 금지한다.

## 44. Patch 실패 처리

Patch 실패 시 다음을 수행한다.

1. 실패 상태 보존
2. 첫 번째 원인 오류 식별
3. cascade 오류 분리
4. 변경 파일 목록 기록
5. 적용 전 commit으로 복귀 가능성 확인
6. 실패 Evidence 저장
7. Patch 상태를 `FAILED` 또는 `BLOCKED`로 변경
8. 다음 Patch 진행 중단

실패 상태를 숨기기 위해 테스트, validator, BLOCKER 판정을 완화하지 않는다.

## 45. 코드 롤백 정책

다음 방식 중 하나를 사용한다.

- commit revert
- branch reset before merge
- targeted patch reversal

금지:

- 기존 사용자 변경 삭제
- 원인 분석 없이 전체 디렉터리 덮어쓰기
- 테스트 통과를 위해 실패 검사를 제거
- BLOCKER 로그 억제

## 46. 실패 Patch 재시도

재시도 시 다음을 기록한다.

- previousAttempt
- previousFailure
- newHypothesis
- scopeChange
- newBaseCommit
- reusedEvidence
- invalidatedEvidence

새 코드 또는 빌드가 생성되면 이전 build Evidence를 재사용하지 않는다.

## 47. Patch Merge Gate

Patch 브랜치 Merge 조건:

- Definition of Done 충족
- Review 완료
- Regression Matrix 갱신
- P0 BLOCKER 0
- Unexpected file change 0
- Evidence 존재
- Commit history 정리

## 48. Phase Exit Gate

각 Phase 완료 시 다음을 작성한다.

- Phase Exit Report
- Patch completion table
- Build result
- Test result
- Regression Matrix status
- Evidence completeness
- Open blocker
- Accepted warning
- Deferred item
- Next Phase readiness

Phase 상태:

```text
PASS
PASS_WITH_WARNINGS
BLOCKED
DEFERRED
```

BLOCKER가 있으면 `PASS_WITH_WARNINGS`를 사용하지 않는다.

## 49. Development Branch Merge Gate

`development/v3.1`에서 release 후보 브랜치로 Merge하기 전 조건:

- Phase 1~8 필수 범위 PASS
- Phase 9 포함 시 Phase 9 PASS
- Phase 10 Gate Integration PASS
- Regression Matrix BLOCKER gap 0
- Rollback failure 0
- RuntimeValidation BLOCKER 0
- Required Evidence complete
- Release x64 build success

## 50. Review 역할

다음 역할을 구분한다.

- Patch Author
- Code Reviewer
- Safety Reviewer
- Validation Reviewer
- Release Reviewer

한 사람이 여러 역할을 맡을 수 있으나, 각 역할의 확인 결과를 구분해 기록한다.

## 51. Code Review 기준

필수 검토 항목:

- compile correctness
- runtime correctness
- ownership
- lifetime
- thread safety
- error propagation
- `std::expected` contract
- Win32 API usage
- Processor Group safety
- rollback safety
- evidence completeness
- scope compliance
- test adequacy

## 52. Safety Review 기준

필수 검토 항목:

- mutation owner
- rollback state
- ApplyGuard
- identity validation
- failed state preservation
- shutdown safety
- RuntimeValidation
- Anti-Cheat boundary
- group 0 hardcoding

## 53. Development Execution Log

각 실행 세션은 다음 필드를 기록한다.

- executionId
- dateUtc
- agentOrDeveloper
- baseCommit
- branch
- phase
- patchId
- startState
- endState
- inspectedFiles
- changedFiles
- buildResult
- testResult
- evidence
- blockers
- warnings
- commitHash
- nextAction

## 54. Patch Progress Table

실제 결과가 없으므로 초깃값은 모든 Patch ID에 다음 값을 사용한다.

- Status: `NOT_STARTED`
- Branch: `TBD`
- Commit: `TBD`
- Build: `NOT_RUN`
- Tests: `NOT_RUN`
- Evidence: `MISSING`

Patch Progress Table 기본 형식:

| Phase | Patch ID | Priority | Dependency | Status | Branch | Commit | Build | Tests | Evidence | Blocker |
|---|---|---|---|---|---|---|---|---|---|---|
| Phase 1 | P1-01 | P0 | Baseline/Gap Analysis | NOT_STARTED | TBD | TBD | NOT_RUN | NOT_RUN | MISSING | None |

초기 Patch ID 집합:

| Phase | Patch IDs | Priority | Dependency |
|---|---|---|---|
| Phase 1 | P1-01 ~ P1-10 | P0 | Repository Baseline / Gap Analysis |
| Phase 2 | P2-01 ~ P2-10 | P1 | Phase 1 Exit Gate |
| Phase 3 | P3-01 ~ P3-11 | P1 | Phase 2 Exit Gate |
| Phase 4 | P4-01 ~ P4-13 | P0 | Phase 3 Exit Gate |
| Phase 5 | P5-01 ~ P5-12 | P0 | Phase 4 Scheduler Transaction Path |
| Phase 6 | P6-01 ~ P6-13 | P0 | Phase 1~5 Runtime Contract |
| Phase 7 | P7-01 ~ P7-13 | P1 | Phase 6 Evidence / RuntimeValidation |
| Phase 8 | P8-01 ~ P8-14 | P1 | Phase 6 Evidence + Phase 7 Profile |
| Phase 9 | P9-01 ~ P9-15 | P2 | Phase 1~8 Exit Gate + P0 BLOCKER 0 |
| Phase 10 | P10-01 ~ P10-17 | P0 | Release Target Scope + Regression Matrix |

WO-000 완료 후 실제 Patch Progress Table은 각 Patch ID를 개별 row로 확장한다.
임의로 `PASS`를 채우지 않는다.

## 55. Development Dashboard Summary

집계 필드:

- totalPatchCount
- notStartedCount
- inspectingCount
- implementingCount
- committedCount
- blockedCount
- failedCount
- deferredCount
- phasePassCount
- p0BlockerCount
- runtimeValidationBlockerCount
- rollbackFailureCount
- evidenceMissingCount

상태:

```text
ON_TRACK
AT_RISK
BLOCKED
INVALID
```

`ON_TRACK` 조건:

- P0 BLOCKER 0
- 활성 Patch가 정의된 scope 내 진행

`AT_RISK` 조건:

- WARN 누적
- dependency 지연
- 선택 Evidence 누락

`BLOCKED` 조건:

- P0 BLOCKER
- Rollback failure
- RuntimeValidation BLOCKER
- Scope conflict

`INVALID` 조건:

- 진행 상태 또는 기준 commit 추적 불가

## 56. 실제 Patch 작업지시서 구조

Development Execution Plan 이후 각 Patch 작업지시서는 다음 구조를 사용한다.

- 작업명
- Patch ID
- 현재 기준 commit
- 목표
- 근거 문서
- 사전 조사 파일
- 수정 가능 파일
- 수정 금지 파일
- 구현 요구사항
- 테스트 요구사항
- Evidence 요구사항
- 중단 조건
- 완료 기준
- 출력 형식
- Commit 메시지 형식

## 57. Patch 작업지시서 금지사항

금지사항:

- 여러 Patch ID 동시 구현
- 실제 저장소 조사 없는 코드 생성
- 기존 테스트 삭제
- Release BLOCKER 검사 비활성화
- scope 밖 대규모 리팩터링
- 검증되지 않은 새 기능 추가
- 코드와 테스트를 서로 다른 인터페이스 상태로 남김

## 58. 첫 실제 작업지시서 생성 기준

첫 구현 작업은 바로 P1-01을 수정하는 작업이 아닐 수 있다.

먼저 다음 작업지시서를 생성한다.

```text
Repository Baseline & Gap Analysis Work Order
```

권장 파일:

```text
docs/workorders/WO-000_RepositoryBaselineAndGapAnalysis.md
```

이 작업의 결과로 실제 첫 Patch를 선정한다.

예시:

- P1-02가 이미 구현됨 → static verification만 수행
- P1-06에 group 0 hardcoding 발견 → P1-06을 최우선 실행
- Rollback BLOCKER 발견 → Phase 5 관련 긴급 Patch를 P0로 선행

규칙:

- 문서 순서보다 실제 P0 BLOCKER를 우선할 수 있다.
- 순서 변경 사유와 dependency 영향을 기록한다.

## 59. Non-Goals

DevelopmentExecutionPlan_v3.1.md의 추가 비목표:

- 실제 구현 코드 작성
- 실제 branch 생성
- 실제 commit 수행
- 실제 테스트 PASS 기록
- 임의 일정 확정
- 성능 향상 보장
- 공식 지원 게임 확정
- P0 BLOCKER를 우회한 개발 진행

## 60. Acceptance Criteria

본 문서의 완료 기준:

1. Repository Baseline 절차가 정의되어 있다.
2. Gap Analysis 절차가 정의되어 있다.
3. Patch 실행 상태가 정의되어 있다.
4. 브랜치 및 커밋 전략이 정의되어 있다.
5. Patch Execution Unit이 정의되어 있다.
6. Inspect / Scope Lock / Interface Review 절차가 정의되어 있다.
7. Build / Test / Static / Runtime / Evidence 절차가 정의되어 있다.
8. Definition of Ready가 정의되어 있다.
9. Definition of Done이 정의되어 있다.
10. AI Agent 필수 입력과 출력이 정의되어 있다.
11. AI Agent Stop Condition이 정의되어 있다.
12. Phase 1~10 실행 순서와 진입/종료 조건이 정의되어 있다.
13. 병렬 가능/금지 작업이 정의되어 있다.
14. 실패 Patch 보존 및 재시도 정책이 정의되어 있다.
15. Patch Merge Gate와 Phase Exit Gate가 정의되어 있다.
16. Review 역할과 기준이 정의되어 있다.
17. Development Execution Log가 정의되어 있다.
18. Patch Progress Table이 정의되어 있다.
19. 실제 결과를 임의로 PASS 처리하지 않는 원칙이 포함되어 있다.
20. 첫 실제 작업으로 Repository Baseline & Gap Analysis를 수행하도록 정의되어 있다.
21. 후속 WO-000 작업지시서 작성에 필요한 기준이 충분하다.

## 61. Open Questions

1. 기본 개발 브랜치는 master/main에서 직접 분기할 것인가, development/v3.1을 둘 것인가?
2. Patch ID 하나당 반드시 별도 브랜치를 사용할 것인가?
3. 문서 수정과 코드 수정을 하나의 커밋으로 허용할 것인가?
4. Merge 방식은 merge commit, squash, rebase 중 무엇으로 둘 것인가?
5. AI 에이전트가 직접 commit까지 수행할 수 있는가?
6. AI 에이전트가 테스트를 실행할 수 없는 환경에서는 Patch 완료를 어떻게 판정할 것인가?
7. Release x64 빌드를 모든 Patch에서 필수로 할 것인가, Phase Exit에서만 필수로 할 것인가?
8. Failure Injection Test를 모든 mutation Patch에서 필수로 할 것인가?
9. Regression Matrix 갱신을 각 Patch에서 수행할 것인가, Phase Exit에서 일괄 수행할 것인가?
10. Evidence artifact를 Git에 포함할 것인가, 외부 artifacts 디렉터리에만 둘 것인가?
11. P0 BLOCKER가 다른 Phase에서 발견되면 Phase 순서를 변경할 수 있는가?
12. Phase 9를 v3.1 Experimental로 유지할 것인가, 진입 조건 미충족 시 v3.2로 이관할 것인가?
13. Code Reviewer와 Safety Reviewer를 동일 인물로 허용할 것인가?
14. 실제 Game Validation은 어느 Phase부터 병행할 것인가?
15. 첫 Gap Analysis의 기준 commit은 어느 commit으로 고정할 것인가?
