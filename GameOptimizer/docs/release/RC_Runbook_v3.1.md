> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# GameOptimizer v3.1 RC Runbook

## 1. 문서 개요

문서명: GameOptimizer v3.1 RC Runbook

버전: v1.0

작성 목적: `docs/release/ReleaseChecklist_v3.1.md`에서 정의한 릴리즈 후보(Release Candidate) 승인 조건을 실제 검증 절차로 변환한다. 이 문서는 v3.1 RC 검증을 어떤 순서로 실행하고, 어떤 산출물을 확인하며, 어떤 조건에서 즉시 중단해야 하는지 정의하는 실행 절차서(Runbook)다.

적용 범위:

- Release Candidate 검증 실행 순서
- 단계별 입력, 절차, 필수 산출물
- PASS / WARN / BLOCKER 분류 기준
- 안전 종료와 Evidence 보존 기준
- RC Report 필수 항목
- Final RC Decision 기준

비적용 범위:

- Release Gate 스크립트 구현
- Python validator 구현
- CI/CD 설정 구현
- 성능 향상 보장
- 공식 지원 게임 목록 확정
- 설치 프로그램 제작
- UI 리포트 구현
- 자동 배포 구현

상위 문서:

- `docs/vision/PVD_v1.0.md`
- `docs/proposals/performance/PerformanceEngineSpec.md`
- `docs/proposals/performance/PolicySpecification.md`
- `docs/proposals/performance/GameProfileSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/release/ReleaseChecklist_v3.1.md`
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
- `docs/release/Release_Gate_Spec.md`

후속 문서:

- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/release/RC_Report_Template_v3.1.md`

문서 경로 기준: `GameOptimizer/docs/`

RC Runbook의 목적은 v3.1이 가장 빠른지 증명하는 것이 아니라, v3.1을 릴리즈 후보로 검토할 수 있을 만큼 안전하고 재현 가능한 검증 절차를 제공하는 것이다.

## 2. RC 실행 원칙

RC 실행 원칙은 다음과 같다.

1. BLOCKER가 발생하면 이후 성능 검증보다 안전 종료와 Evidence 보존을 우선한다.
2. Rollback 실패는 WARN으로 낮추지 않는다.
3. RuntimeValidation BLOCKER가 있으면 RC PASS 불가다.
4. Evidence fatal missing이 있으면 RC PASS 불가다.
5. Release binary는 git commit, build hash, EXE SHA-256과 함께 기록한다.
6. 실제 게임 검증은 공식 지원 선언이 아니라 후보 검증으로만 기록한다.
7. Unknown Game Profile은 aggressive mode를 허용하지 않는다.
8. ShutdownRequested 이후 신규 policy apply는 금지한다.

성능 지표가 개선되어도 Rollback 실패, RuntimeValidation BLOCKER, Evidence fatal missing이 있으면 RC PASS가 될 수 없다.

RC Runbook은 성능 향상 홍보 문서가 아니다. 성능 개선이 관찰되더라도 Before/After Evidence, Rollback Result, Runtime Validation Summary, Evidence completeness가 함께 충족되지 않으면 RC PASS 또는 성능 개선 주장으로 분류하지 않는다.

## 3. RC 실행 단계 요약

RC 검증은 다음 순서를 사용한다.

```text
1. Preflight Check
2. Documentation Check
3. Build Check
4. Static Safety Check
5. Dry-run Validation
6. Soft-apply Validation
7. Apply Validation
8. Rollback Validation
9. Performance Evidence Validation
10. Game Profile Validation
11. Long Soak Validation
12. Evidence Bundle Validation
13. RC Report Generation
14. Final RC Decision
```

각 단계는 다음 양식을 따른다.

```text
목적
입력
실행 절차
필수 산출물
PASS 기준
WARN 기준
BLOCKER 기준
다음 단계 조건
```

BLOCKER가 발생한 단계 이후에는 성능 검증을 계속하지 않는다. 단, 안전 종료, rollback 시도, failed state preservation, evidence flush, RC Report의 failure 기록은 계속 수행한다.

## 4. Preflight Check

목적:

RC 검증을 시작하기 전 환경과 대상 조건을 확인한다.

입력:

```text
target process name
selected profile
test mode
admin privilege
build configuration
output artifact directory
```

실행 절차:

1. 테스트 대상 process name을 확인한다.
2. 선택된 Game Profile을 확인한다.
3. 실행 권한을 확인한다.
4. Evidence 출력 경로를 확인한다.
5. 기존 artifact와 충돌하지 않는 runId를 준비한다.

필수 산출물:

```text
preflight_summary
run_id
target_process_identity
selected_profile_summary
artifact_output_directory
privilege_summary
```

PASS 기준:

```text
target process와 profile이 식별된다.
artifact output 경로가 준비된다.
runId가 고유하게 생성된다.
```

WARN 기준:

```text
관리자 권한이 없지만 dry-run만 수행한다.
선택된 profile이 Candidate 또는 Unknown이다.
실제 게임 검증이 아니라 synthetic 또는 dry-run 중심 검증이다.
```

BLOCKER 기준:

```text
target process identity를 확인할 수 없다.
artifact 출력 경로를 만들 수 없다.
Release 검증인데 build identity가 없다.
```

다음 단계 조건:

Preflight가 PASS 또는 허용 가능한 WARN이면 Documentation Check로 이동한다. BLOCKER가 있으면 안전 종료 후 RC Report에 `INVALID_RUN`으로 기록한다.

## 5. Documentation Check

목적:

RC 기준 문서들이 존재하고 서로 충돌하지 않는지 확인한다.

입력:

```text
document root
required document list
ReleaseChecklist_v3.1.md
```

실행 절차:

1. 필수 확인 문서가 존재하는지 확인한다.
2. 문서 내부 경로가 `docs/...` 기준으로 작성되었는지 확인한다.
3. Safety, Rollback, Evidence, Runtime Validation 책임 경계가 충돌하지 않는지 확인한다.
4. Open Questions가 RC 차단 항목인지 확인한다.

필수 확인 문서:

```text
docs/vision/PVD_v1.0.md
docs/proposals/performance/PerformanceEngineSpec.md
docs/proposals/performance/PolicySpecification.md
docs/proposals/performance/GameProfileSpecification.md
docs/validation/PerformanceValidationPlan.md
docs/evidence/EvidenceSpecification.md
docs/release/ReleaseChecklist_v3.1.md
docs/architecture/SAD_v1.0.md
docs/architecture/RuntimeStateMachine.md
docs/modules/MDS-001_ThreadTracker.md
docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md
docs/modules/MDS-003_TopologyAnalyzer.md
docs/modules/MDS-004_SchedulerController.md
docs/modules/MDS-005_RollbackManager.md
docs/modules/MDS-006_BackgroundController.md
docs/modules/MDS-007_PerformanceEvidencePlanner.md
docs/modules/MDS-008_PolicyDispatcher.md
docs/modules/MDS-009_RuntimeOrchestrator.md
```

필수 산출물:

```text
documentation_check_summary
missing_document_list
path_consistency_result
blocking_open_questions
```

PASS 기준:

```text
필수 문서가 존재한다.
문서 경로가 docs/... 기준으로 일관된다.
Safety/Rollback/Evidence/Runtime Validation 책임이 충돌하지 않는다.
```

WARN 기준:

```text
비차단 Open Questions가 남아 있다.
후속 문서가 Planned 상태이나 RC 실행 순서에는 영향이 없다.
문서 용어 일부가 다르지만 safety 판단에는 영향이 없다.
```

BLOCKER 기준:

```text
Safety/Rollback/Evidence 관련 문서 누락
ThreadTracker에 mutation 책임이 문서화됨
PerformanceEngine이 직접 Win32 mutation을 수행하는 구조로 문서화됨
ReleaseChecklist_v3.1.md 누락
```

다음 단계 조건:

Documentation Check가 PASS 또는 WARN이면 Build Check로 이동한다. Safety/Rollback/Evidence 관련 BLOCKER가 있으면 RC 실행을 중단한다.

## 6. Build Check

목적:

RC 검증에 사용할 바이너리를 식별 가능하게 만든다.

입력:

```text
source revision
build configuration
compiler/toolset
binary output path
```

실행 절차:

1. Release x64 빌드를 수행하거나 기존 Release binary를 식별한다.
2. compiler/toolset 정보를 기록한다.
3. git commit hash를 기록한다.
4. build hash를 기록한다.
5. EXE SHA-256을 기록한다.
6. binary path와 build timestamp를 기록한다.

필수 기록:

```text
build configuration
compiler/toolset
git commit hash
build hash
EXE SHA-256
binary path
build timestamp
```

필수 산출물:

```text
build_summary
build_log
binary_identity
build_hash
exe_sha256
```

PASS 기준:

```text
Release x64 빌드가 성공한다.
git commit, build hash, EXE SHA-256이 기록된다.
실행 바이너리가 Evidence Bundle과 연결된다.
```

WARN 기준:

```text
비차단 compiler warning이 존재한다.
symbol 또는 map 파일이 누락되었지만 RC binary 식별에는 영향이 없다.
Debug 빌드 검증은 가능하나 Release RC 판정은 보류해야 한다.
```

BLOCKER 기준:

```text
Release build 실패
EXE SHA-256 누락
git commit hash 누락
실행 바이너리 식별 불가
```

다음 단계 조건:

Release Candidate 검증에서 binary hash 누락은 BLOCKER로 분류한다. Build Check가 PASS이면 Static Safety Check로 이동한다.

## 7. Static Safety Check

목적:

실행 전 안전 계약(Safety Contract) 위반 가능성을 정적으로 확인한다.

입력:

```text
source tree
Safety_Contract.md
Runtime_Contract.md
module design documents
```

실행 절차:

1. ThreadTracker가 observation-only 책임만 가지는지 확인한다.
2. PerformanceEngine과 PolicyDispatcher가 direct Win32 mutation을 수행하지 않는지 확인한다.
3. SchedulerController mutation ownership을 확인한다.
4. RollbackManager state preservation 책임을 확인한다.
5. ApplyGuard 우회 경로가 없는지 확인한다.
6. `std::expected` Assign -> Check -> Bind 패턴을 확인한다.
7. Processor Group group 0 hardcoding이 없는지 확인한다.

필수 검사:

```text
ThreadTracker observation-only 유지
PerformanceEngine direct Win32 mutation 없음
PolicyDispatcher direct Win32 mutation 없음
SchedulerController mutation ownership 유지
RollbackManager state preservation 유지
ApplyGuard 우회 없음
std::expected Assign -> Check -> Bind 준수
uninspected expected propagation 금지
Processor Group group 0 hardcoding 금지
```

필수 산출물:

```text
static_safety_summary
mutation_ownership_review
direct_win32_mutation_scan
apply_guard_review
processor_group_review
expected_handling_review
```

PASS 기준:

```text
mutation ownership이 SchedulerController로 제한된다.
Rollback state 저장 전 mutation 가능 경로가 없다.
ApplyGuard arm 전 mutation 가능 경로가 없다.
Processor Group 정보 누락 시 안전 fallback이 유지된다.
```

WARN 기준:

```text
자동 정적 검사 스크립트가 없지만 수동 리뷰 결과가 존재한다.
비핵심 helper naming 불일치가 있으나 mutation 경로에 영향이 없다.
테스트 보강 필요 항목이 있으나 Safety Contract 위반은 아니다.
```

BLOCKER 기준:

```text
Rollback state 저장 전 mutation 가능 경로 존재
ApplyGuard arm 전 mutation 가능 경로 존재
ThreadTracker에서 SetThreadAffinityMask 또는 SetThreadPriority 호출
PerformanceEngine에서 Win32 mutation 호출
PolicyDispatcher에서 Win32 mutation 호출
Processor Group 정보 누락 시 group 0 fallback 강제
std::expected 무검사 전파
```

다음 단계 조건:

Static Safety Check가 PASS이면 Dry-run Validation으로 이동한다. 안전 계약 BLOCKER가 있으면 실행 검증을 시작하지 않는다.

## 8. Dry-run Validation

목적:

실제 mutation 없이 observation, confidence, policy candidate, profile restriction이 정상 동작하는지 확인한다.

입력:

```text
release binary
target process identity
selected profile
dry-run mode
artifact output directory
```

실행 절차:

1. 대상 프로세스를 dry-run mode로 실행한다.
2. ThreadTracker observation이 수행되는지 확인한다.
3. MainThreadConfidence가 계산되는지 확인한다.
4. PolicyCandidate가 생성 또는 거부되는지 확인한다.
5. Game Profile의 blockedPolicies가 적용되는지 확인한다.
6. Evidence가 생성되는지 확인한다.

필수 산출물:

```text
dry_run_summary
thread_observation_summary
confidence_summary
policy_candidate_summary
profile_selection_summary
runtime_validation_summary
```

PASS 기준:

```text
mutation 없이 observation과 evidence가 생성된다.
blocked policy가 전달되지 않는다.
RuntimeValidation BLOCKER가 없다.
```

WARN 기준:

```text
MainThreadConfidence가 낮아 policy candidate가 생성되지 않는다.
profileStatus가 Candidate 또는 Unknown이라 mutation이 제한된다.
선택 evidence 일부가 누락되었으나 dry-run 핵심 evidence는 존재한다.
```

BLOCKER 기준:

```text
dry-run에서 Win32 mutation 발생
ThreadTracker가 mutation 수행
profile blocked policy가 무시됨
Evidence 생성 실패
RuntimeValidation BLOCKER 발생
```

다음 단계 조건:

Dry-run Validation이 PASS 또는 허용 가능한 WARN이면 Soft-apply Validation으로 이동한다. dry-run mutation 발생은 즉시 BLOCKER다.

## 9. Soft-apply Validation

목적:

제한적 정책 적용 경로에서 ApplyGuard, RollbackManager, Verify, Evidence 흐름을 검증한다.

입력:

```text
release binary
soft-apply mode
dry-run evidence
selected profile
allowed policy candidate list
```

실행 절차:

1. Candidate 또는 Unknown profile의 default mode를 확인한다.
2. soft-apply 가능한 정책만 허용한다.
3. rollback state save 여부를 확인한다.
4. ApplyGuard arm 여부를 확인한다.
5. verify 결과를 확인한다.
6. RuntimeValidation BLOCKER 여부를 확인한다.

필수 산출물:

```text
soft_apply_summary
rollback_state_save_summary
apply_guard_summary
verify_summary
runtime_validation_summary
```

PASS 기준:

```text
rollback state 저장 후에만 apply가 수행된다.
ApplyGuard 경계가 지켜진다.
verify 결과가 기록된다.
RuntimeValidation BLOCKER가 없다.
```

WARN 기준:

```text
profile 제한으로 일부 policy가 rejected 된다.
verify 결과가 NEUTRAL이나 safety 기준은 충족한다.
Candidate profile이라 apply validation은 보류된다.
```

BLOCKER 기준:

```text
rollback state 없이 mutation
ApplyGuard arm 전 mutation
verify 없이 success 처리
RuntimeValidation BLOCKER 발생
blocked policy apply
```

다음 단계 조건:

Soft-apply Validation이 PASS이면 Apply Validation을 선택적으로 진행한다. Candidate 또는 Unknown profile에서는 soft-apply까지만 필수로 둘 수 있다.

## 10. Apply Validation

목적:

검증된 조건에서 실제 정책 적용 후 Before/After 비교를 수행한다.

입력:

```text
release binary
apply mode eligibility
baseline collection plan
selected profile
rollback readiness result
```

실행 절차:

1. Baseline Window를 수집한다.
2. 정책을 적용한다.
3. Applied Window를 수집한다.
4. Before/After Comparison을 생성한다.
5. rollback 가능성을 확인한다.

필수 산출물:

```text
baseline_window
applied_window
comparison_summary
policy_apply_summary
rollback_summary
runtime_validation_summary
```

PASS 기준:

```text
Before/After 비교가 가능하다.
Rollback Summary가 존재한다.
RuntimeValidation BLOCKER가 없다.
```

WARN 기준:

```text
성능 개선이 NEUTRAL이다.
FPS/frame time 외부 측정값이 없다.
Candidate profile이라 apply 결과를 Validated 승격 근거로 사용하지 않는다.
```

BLOCKER 기준:

```text
Baseline Window 누락
Applied Window 누락
rollback summary 누락
RuntimeValidation Summary 누락
Rollback 실패
```

다음 단계 조건:

Apply Validation은 모든 RC에서 필수일 필요는 없다. Candidate profile 또는 Unknown profile에서는 soft-apply까지만 필수로 둘 수 있으며, apply를 수행한 경우 Rollback Validation으로 이동한다.

## 11. Rollback Validation

목적:

적용된 모든 mutation이 정상적으로 복구되는지 확인한다.

입력:

```text
applied policy list
rollback state
target process identity
shutdown or rollback trigger
```

실행 절차:

1. 적용된 모든 policy의 rollback scope를 확인한다.
2. 저장된 rollback state를 확인한다.
3. thread priority rollback을 확인한다.
4. thread affinity rollback을 확인한다.
5. process priority rollback을 확인한다.
6. background restriction rollback을 확인한다.
7. timer resolution을 변경한 경우 rollback을 확인한다.
8. failed state preservation 결과를 확인한다.
9. identity validation result를 확인한다.

필수 검사:

```text
thread priority rollback
thread affinity rollback
process priority rollback
background restriction rollback
timer resolution rollback if modified
failed state preservation
identity validation result
```

필수 산출물:

```text
rollback_summary
rollback_scope
rollback_state_save_result
identity_validation_result
failed_state_preservation_result
```

PASS 기준:

```text
모든 적용 정책이 rollback되었다.
stale/missing identity는 recoverable로 분류되고 Evidence가 남았다.
failed rollback state가 보존된다.
```

WARN 기준:

```text
대상 thread/process가 종료되어 stale identity로 분류된다.
복구 대상이 이미 사라져 no-op rollback으로 분류된다.
living same identity 실패는 아니다.
```

BLOCKER 기준:

```text
living same identity rollback 실패
failed rollback state 폐기
rollback failure를 WARN/INFO로 낮춤
identity validation 없이 성공 처리
rollback state 없이 mutation
```

다음 단계 조건:

Rollback Validation이 PASS이면 Performance Evidence Validation으로 이동한다. Rollback 실패는 즉시 BLOCKER이며 WARN으로 낮추지 않는다.

## 12. Performance Evidence Validation

목적:

성능 관련 Evidence가 충분한지 확인한다.

입력:

```text
dry-run evidence
soft-apply evidence
apply evidence if applicable
rollback evidence
runtime validation evidence
```

실행 절차:

1. Main Thread 관련 evidence를 확인한다.
2. Policy activation evidence를 확인한다.
3. RTT Jitter와 DPC Spike summary를 확인한다.
4. Baseline Window와 Applied Window를 확인한다.
5. Comparison Summary를 확인한다.
6. Rollback Summary와 Runtime Validation Summary를 확인한다.
7. classification result가 evidence와 일치하는지 확인한다.

필수 확인 항목:

```text
Main Thread Confidence History
Main Thread Migration Count
Main Thread Switch Count
Policy Activation Count
RTT Jitter Summary
DPC Spike Summary
Baseline Window
Applied Window
Comparison Summary
Rollback Summary
Runtime Validation Summary
```

필수 산출물:

```text
performance_evidence_summary
comparison_summary
classification_result
evidence_gap_list
```

PASS 기준:

```text
필수 Evidence가 존재한다.
Before/After 비교가 가능하다.
RuntimeValidation BLOCKER가 없다.
Rollback이 정상이다.
```

WARN 기준:

```text
선택 지표 누락
성능 개선 폭 불명확
NEUTRAL classification
외부 FPS/frame time 측정값 없음
```

BLOCKER 기준:

```text
Evidence 없이 성능 개선 판정
Rollback Summary 누락
RuntimeValidation Summary 누락
Before/After 비교 불가
성능 개선이 있어도 안전성 BLOCKER 존재
```

다음 단계 조건:

Performance Evidence Validation이 PASS 또는 WARN이면 Game Profile Validation으로 이동한다. Evidence 없는 성능 성공 판정은 BLOCKER다.

## 13. Game Profile Validation

목적:

선택된 Game Profile이 안전한 정책 제한 기준으로 동작하는지 확인한다.

입력:

```text
selected profile
profile registry result
policy candidate list
policy dispatch result
profile evidence
```

실행 절차:

1. selectedProfile과 profileType을 확인한다.
2. profileStatus와 profileRiskLevel을 확인한다.
3. allowedPolicies, blockedPolicies, conditionalPolicies를 확인한다.
4. blockedPolicies가 allowedPolicies보다 우선했는지 확인한다.
5. validationRequired와 monitoringOnlyReason을 확인한다.
6. Candidate profile이 Validated로 위장되지 않았는지 확인한다.

필수 확인:

```text
selectedProfile
profileType
profileStatus
profileRiskLevel
allowedPolicies
blockedPolicies
conditionalPolicies
profileSelectionReason
monitoringOnlyReason
validationRequired
```

필수 산출물:

```text
profile_selection_summary
profile_policy_restriction_summary
profile_validation_summary
monitoring_only_reason
```

PASS 기준:

```text
Unknown game은 Unknown Default Profile 또는 MonitoringOnly로 처리된다.
Candidate profile은 Validated로 위장되지 않는다.
blockedPolicies가 allowedPolicies보다 우선한다.
```

WARN 기준:

```text
Candidate profile이라 validationRequired가 true다.
Unknown profile이라 aggressive mode가 차단된다.
profile optional evidence 일부가 누락되었으나 mutation은 제한된다.
```

BLOCKER 기준:

```text
검증되지 않은 게임을 Validated로 분류
targetProcessName을 검증 없이 확정
AntiCheatSensitive profile에서 mutation 허용
Unknown profile에서 aggressive mode 허용
blockedPolicies 무시
```

다음 단계 조건:

Game Profile Validation이 PASS 또는 WARN이면 Long Soak Validation으로 이동한다. profile BLOCKER가 있으면 안전 종료와 Evidence 보존으로 이동한다.

## 14. Long Soak Validation

목적:

장시간 실행 중 policy thrashing, rollback state 손실, evidence 누락, runtime validation failure가 없는지 확인한다.

입력:

```text
release binary
selected profile
soak duration
runtime monitor
artifact output directory
```

기본 시간:

```text
TBD
```

실행 절차:

1. long soak duration을 설정한다.
2. watchdog heartbeat progression을 확인한다.
3. memory growth와 CPU overhead를 확인한다.
4. policy activation/rejection count를 확인한다.
5. cooldown behavior를 확인한다.
6. runtime validation summary를 확인한다.
7. evidence flush result를 확인한다.
8. shutdown cleanliness를 확인한다.

필수 확인:

```text
watchdog heartbeat progression
memory growth
CPU overhead
policy activation count
policy rejection count
cooldown behavior
runtime validation summary
evidence flush result
shutdown cleanliness
```

필수 산출물:

```text
long_soak_summary
watchdog_summary
resource_growth_summary
policy_activity_summary
evidence_flush_summary
shutdown_summary
```

PASS 기준:

```text
RuntimeValidation BLOCKER가 없다.
Evidence flush가 정상이다.
Shutdown이 안전하게 완료된다.
```

WARN 기준:

```text
성능 개선이 불명확하다.
minor warning이 존재한다.
long soak 시간이 TBD 기준보다 짧지만 RC 범위에서 허용된다.
```

BLOCKER 기준:

```text
watchdog hang
snapshot freshness failure
timeline monotonicity failure
shutdown 중 active mutation 잔존
rollback state 손실
```

다음 단계 조건:

Long Soak Validation이 PASS 또는 WARN이면 Evidence Bundle Validation으로 이동한다. shutdown safety violation이 있으면 즉시 BLOCKER로 기록한다.

## 15. Evidence Bundle Validation

목적:

RC 승인에 필요한 Evidence Bundle이 완전한지 확인한다.

입력:

```text
artifact output directory
runId
build identity
validation summaries
```

실행 절차:

1. session manifest를 확인한다.
2. latest/final runtime snapshot을 확인한다.
3. policy timeline을 확인한다.
4. performance, rollback, runtime validation summary를 확인한다.
5. evidence completeness report를 확인한다.
6. binary hash와 git commit을 확인한다.
7. classification result를 확인한다.

필수 산출물:

```text
session_manifest.json 또는 txt
runtime_snapshot_latest
runtime_snapshot_final
policy_timeline.log
performance_summary.json
rollback_summary.json
runtime_validation_summary.json
evidence_completeness_report.json
rc_report.md
binary hash
git commit
classification result
```

PASS 기준:

```text
필수 evidence가 모두 존재한다.
Evidence completeness가 Complete 또는 허용 가능한 Partial이다.
Evidence Bundle과 binary identity가 연결된다.
```

WARN 기준:

```text
선택 evidence가 일부 누락되었지만 필수 evidence는 존재한다.
Partial evidence가 PASS_WITH_WARNINGS 범위로 설명 가능하다.
성능 외부 측정값은 없지만 safety evidence는 완전하다.
```

BLOCKER 기준:

```text
session manifest 누락
final snapshot 누락
rollback summary 누락
runtime validation summary 누락
binary hash 누락
classification result 누락
```

다음 단계 조건:

Evidence Bundle Validation이 PASS 또는 WARN이면 RC Report Generation으로 이동한다. 필수 evidence 누락은 RC PASS를 차단한다.

## 16. RC Report Generation

목적:

RC 검증 결과를 사람이 검토 가능한 보고서로 정리한다.

입력:

```text
all validation summaries
Evidence Bundle
build identity
classification result
warnings
blockers
```

실행 절차:

1. runId와 대상 정보를 기록한다.
2. build identity를 기록한다.
3. 단계별 checklist summary를 기록한다.
4. warnings와 blockers를 기록한다.
5. performance classification을 기록한다.
6. rollback summary와 runtime validation summary를 기록한다.
7. evidence completeness result를 기록한다.
8. final decision 후보를 기록한다.

RC Report 필수 항목:

```text
runId
target process
selected profile
profile status
test mode
git commit
build hash
exeSha256
start time
end time
checklist summary
warnings
blockers
performance classification
rollback summary
runtime validation summary
evidence completeness result
final decision
```

결과 등급:

```text
PASS
PASS_WITH_WARNINGS
NEUTRAL
REGRESSION
BLOCKER
INVALID_RUN
```

필수 산출물:

```text
rc_report.md
rc_report_summary
final_decision_candidate
```

PASS 기준:

```text
RC Report 필수 항목이 모두 작성된다.
Evidence Bundle과 RC Report의 runId가 일치한다.
final decision 후보가 checklist 결과와 일치한다.
```

WARN 기준:

```text
선택 항목 일부가 비어 있으나 필수 승인 항목은 완전하다.
NEUTRAL 또는 PASS_WITH_WARNINGS 사유가 명확히 기록된다.
```

BLOCKER 기준:

```text
rc_report.md 생성 실패
final decision 누락
rollback summary 또는 runtime validation summary 누락
Evidence Bundle과 runId 불일치
```

다음 단계 조건:

RC Report Generation이 PASS이면 Final RC Decision으로 이동한다. Report 생성 실패는 `INVALID_RUN` 또는 BLOCKER로 기록한다.

## 17. Final RC Decision

목적:

RC 최종 판정 기준을 정의한다.

입력:

```text
rc_report.md
checklist summary
blocker count
fatal count
rollback result
runtime validation result
evidence completeness result
```

실행 절차:

1. BLOCKER와 FATAL count를 확인한다.
2. Rollback 결과를 확인한다.
3. RuntimeValidation BLOCKER count를 확인한다.
4. Evidence Bundle 완전성을 확인한다.
5. Release binary 식별 가능성을 확인한다.
6. performance classification을 확인한다.
7. 최종 RC Decision을 기록한다.

필수 산출물:

```text
final_rc_decision
decision_reason
approval_blockers
rc_report.md
```

### PASS

```text
BLOCKER 0
FATAL 0
Rollback 성공
RuntimeValidation BLOCKER 0
Evidence Bundle 완성
Release binary 식별 가능
```

### PASS_WITH_WARNINGS

```text
BLOCKER 0
FATAL 0
Rollback 성공
RuntimeValidation BLOCKER 0
성능 개선이 작거나 일부 선택 Evidence 부족
```

### NEUTRAL

```text
안전성은 통과
성능 개선이 불명확
성능 향상 주장 금지
Validated profile 승격 금지
```

### REGRESSION

```text
성능 지표 악화
policy thrashing
jitter 악화
migration 증가
```

### BLOCKER

```text
Rollback 실패
RuntimeValidation BLOCKER
Evidence fatal missing
unsafe mutation
identity mismatch mutation
ApplyGuard 우회
```

### INVALID_RUN

```text
테스트 환경 구성 실패
target process identity 불명확
필수 session identity 없음
비교 구간 성립 실패
```

PASS 기준:

```text
Final RC Decision이 checklist 결과와 일치한다.
PASS 또는 PASS_WITH_WARNINGS 조건이 모두 충족된다.
```

WARN 기준:

```text
NEUTRAL이지만 safety와 evidence 기준은 충족된다.
PASS_WITH_WARNINGS 사유가 명확히 기록된다.
```

BLOCKER 기준:

```text
BLOCKER 또는 FATAL 존재
Rollback 실패
RuntimeValidation BLOCKER
Evidence fatal missing
Release binary 식별 불가
```

다음 단계 조건:

PASS 또는 PASS_WITH_WARNINGS이면 RC로 검토 가능하다. NEUTRAL은 RC 검토가 가능할 수 있으나 성능 향상 주장을 금지한다. REGRESSION, BLOCKER, INVALID_RUN은 RC PASS가 아니다.

## 18. 중단 조건

다음 조건에서는 즉시 성능 검증을 중단하고 안전 종료 및 Evidence 보존을 우선한다.

```text
Rollback 실패
RuntimeValidation BLOCKER
Evidence write fatal
target process identity mismatch
anti-cheat boundary suspicion
ApplyGuard 우회 감지
rollback state 없이 mutation 감지
shutdown safety violation
```

중단 시 수행해야 할 최소 절차:

1. 신규 policy apply를 중단한다.
2. 가능한 경우 rollback을 시도한다.
3. failed rollback state를 보존한다.
4. runtime snapshot final을 생성한다.
5. runtime_validation_summary를 생성한다.
6. evidence_completeness_report에 누락과 실패 사유를 기록한다.
7. rc_report.md에 `BLOCKER` 또는 `INVALID_RUN`으로 기록한다.

## 19. Non-Goals

다음은 RC_Runbook_v3.1.md의 비목표(Non-Goals)다.

```text
Release Gate 스크립트 구현
Python validator 구현
CI/CD 설정 구현
성능 향상 보장
공식 지원 게임 목록 확정
설치 프로그램 제작
UI 리포트 구현
자동 배포 구현
```

이 문서는 실행 절차서이며, 자동화 구현은 후속 `docs/implementation/ImplementationPlan_v3.1.md` 또는 Release Gate 작업에서 다룬다.

## 20. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. RC 실행 단계가 순서대로 정의되어 있다.
2. 각 단계의 목적, 입력, 산출물, PASS/WARN/BLOCKER 기준이 정의되어 있다.
3. Dry-run, soft-apply, apply, rollback, soak 검증 절차가 포함되어 있다.
4. Evidence Bundle 검증 절차가 포함되어 있다.
5. RC Report 필수 항목이 정의되어 있다.
6. Final RC Decision 기준이 정의되어 있다.
7. BLOCKER 발생 시 중단 조건이 정의되어 있다.
8. 성능 개선보다 안전성과 Evidence를 우선하는 원칙이 포함되어 있다.
9. 후속 `docs/implementation/ImplementationPlan_v3.1.md` 작성에 필요한 실행 순서가 충분하다.

## 21. Open Questions

다음 질문은 후속 구현 계획 또는 Release Gate 설계에서 결정한다.

```text
1. RC Runbook을 수동 절차로 둘 것인가, Release Gate 스크립트와 1:1 매핑할 것인가?
2. Long Soak 기본 시간을 몇 분으로 둘 것인가?
3. Apply Validation은 RC 필수인가, Candidate Profile에서는 선택인가?
4. 실제 게임 검증을 RC 필수 조건으로 둘 것인가?
5. RC Report는 Markdown과 JSON을 동시에 생성할 것인가?
6. Evidence Bundle 저장 위치는 artifacts/<runId>/로 확정할 것인가?
7. PASS_WITH_WARNINGS에서 허용 가능한 WARN 범위는 어디까지인가?
8. NEUTRAL 결과를 RC 허용으로 볼 것인가?
```

Open Questions가 해결되기 전까지 확정되지 않은 수치는 `TBD`로 유지한다. 특히 Long Soak 기본 시간, Apply Validation 필수 여부, 실제 게임 검증 필수 여부는 RC 승인 정책과 직접 연결될 수 있다.
