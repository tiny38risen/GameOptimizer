# GameOptimizer v3.1 Implementation Plan

## 1. 문서 개요

문서명: GameOptimizer v3.1 Implementation Plan

버전: v1.0

작성 목적: GameOptimizer v3.1의 실제 구현 순서, 패치 단위(Patch Unit), 검증 순서, 위험 관리 기준을 정의한다. 이 문서는 코드 구현 자체가 아니라, 제품 문서, 성능 엔진 문서, 정책 문서, 아키텍처 문서, MDS 문서, 검증 문서, 릴리즈 문서를 기준으로 개발을 어떤 순서로 진행할지 정하는 실행 계획 문서다.

적용 범위:

- v3.1 구현 Phase 순서
- Phase별 목표, 작업 범위, 영향 모듈
- Phase별 금지사항과 완료 기준(Definition of Done)
- Phase별 필수 테스트와 성능 증거(Performance Evidence)
- 개발 중단 조건과 후순위 작업 기준
- 릴리즈 후보(Release Candidate) 검증으로 연결되는 구현 순서

비적용 범위:

- 구체 C++ 코드 작성
- 패치 파일 생성
- 테스트 코드 작성
- 빌드 스크립트 구현
- CI/CD 구현
- UI 구현
- 공식 지원 게임 목록 확정
- 성능 향상 보장
- Anti-Cheat 우회 기능 구현
- Kernel Driver, DLL Injection, Game Memory Patch 구현

상위 문서:

- `docs/vision/PVD_v1.0.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`
- `docs/performance/GameProfileSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
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

- `docs/implementation/PatchPlan_Phase1.md`
- `docs/validation/RegressionMatrix_v3.1.md`

문서 경로 기준: `GameOptimizer/docs/`

Implementation Plan의 목적은 기능을 빠르게 많이 추가하는 것이 아니라, v3.1의 성능 정책을 안전하고 검증 가능한 패치 단위로 구현하는 것이다.

## 2. 구현 전략

v3.1 구현 전략은 안전한 단계적 확장이다. 구현은 문서 계약(Document Contract), 관찰(Observation), 판단(Decision), 정책 후보(Policy Candidate), dispatch, mutation, rollback, evidence, validation 순서로 진행한다.

필수 전략:

1. 문서 계약 우선
2. Observation Layer 우선
3. Confidence / Topology 판단 분리
4. Policy Candidate 생성과 dispatch 분리
5. Mutation owner 제한
6. Rollback / ApplyGuard 우선 검증
7. Evidence recorder와 validation을 병행 구축
8. Aggressive mode는 가장 마지막 단계에서만 구현

Aggressive Single-Core Mode는 v3.1의 핵심 목표 중 하나지만, 구현 순서상 가장 먼저 개발하면 안 된다.

구현 전제:

1. 문서 계약이 없는 기능은 구현하지 않는다.
2. ThreadTracker는 observation-only를 유지한다.
3. Performance Engine은 직접 Win32 mutation을 수행하지 않는다.
4. PolicyDispatcher는 직접 Win32 mutation을 수행하지 않는다.
5. SchedulerController와 BackgroundController만 실제 mutation owner가 될 수 있다.
6. RollbackManager와 ApplyGuard 계약 없는 mutation은 금지한다.
7. Evidence 없는 성능 성공 판정은 금지한다.
8. RuntimeValidation BLOCKER가 있으면 다음 단계로 진행하지 않는다.
9. Processor Group 정보 누락 시 group 0 hardcoding으로 보정하지 않는다.
10. Anti-Cheat 우회, DLL Injection, Kernel Driver, Game Memory Patch는 구현 범위가 아니다.

## 3. 구현 단계 요약

v3.1은 다음 Phase 구조를 사용한다.

```text
Phase 0. Documentation Alignment
Phase 1. Observation & Topology Foundation
Phase 2. Main Thread Confidence Engine
Phase 3. Policy Candidate & Dispatch Layer
Phase 4. SchedulerController Safety Hardening
Phase 5. Rollback / ApplyGuard Contract Hardening
Phase 6. Evidence & Runtime Validation Integration
Phase 7. Game Profile Integration
Phase 8. Performance Validation Flow
Phase 9. Aggressive Single-Core Mode
Phase 10. RC Gate Integration
```

각 Phase는 다음 양식을 따른다.

```text
목적
작업 범위
영향 모듈
금지사항
완료 기준
필수 테스트
필수 Evidence
다음 단계 진입 조건
```

Phase 순서는 안전성 의존성 순서다. 후행 Phase가 선행 Phase의 완료 기준을 우회해서 구현되어서는 안 된다.

## 4. Phase 0 — Documentation Alignment

목적:

구현 전 문서와 코드 기준을 정렬한다.

작업 범위:

```text
docs/README.md 정리
문서 경로 정합성 확인
PVD / PerformanceEngineSpec / PolicySpecification / SAD / MDS 참조 확인
Open Questions 중 BLOCKING 항목 분류
```

영향 모듈:

```text
docs/README.md
MIGRATION_NOTES.md
release planning documents
implementation planning documents
```

금지사항:

```text
코드 변경 금지
기능 구현 금지
미확정 문서를 확정된 계약처럼 사용 금지
```

완료 기준:

```text
필수 문서가 존재한다.
문서 경로가 docs/... 기준으로 통일된다.
구현 전 blocking question이 식별된다.
```

필수 테스트:

```text
required document existence check
docs path consistency check
blocking open question review
release checklist alignment review
```

필수 Evidence:

```text
documentation_alignment_summary
required_document_list
missing_document_list
blocking_open_questions
path_consistency_result
```

다음 단계 진입 조건:

Documentation Alignment가 PASS이고 blocking question이 구현 범위를 차단하지 않아야 Phase 1로 이동한다.

## 5. Phase 1 — Observation & Topology Foundation

목적:

ThreadTracker와 TopologyAnalyzer 기반을 v3.1 계약에 맞게 정리한다.

작업 범위:

```text
ThreadTracker observation-only 검증
thread sample snapshot 구조 정리
migration/switch signal 제공
TopologyResult group-aware 정리
Processor Group group 0 hardcoding 제거 또는 차단
topology completeness reason 제공
```

영향 모듈:

```text
ThreadTracker
TopologyAnalyzer
RuntimeContext
RuntimeValidationMonitor
```

금지사항:

```text
ThreadTracker에서 mutation 금지
TopologyAnalyzer에서 affinity apply 금지
group 0 hardcoding 금지
```

완료 기준:

```text
ThreadTracker가 observation-only로 정적 검증된다.
TopologyAnalyzer가 processor group 정보를 명시적으로 제공한다.
topology incomplete 시 monitoring-only reason을 제공할 수 있다.
```

필수 테스트:

```text
thread enumeration test
short-lived thread test
access denied thread test
migration signal test
processor group topology test
topology incomplete fallback test
```

필수 Evidence:

```text
thread_observation_summary
migration_signal_summary
switch_signal_summary
topology_result
topology_completeness_reason
monitoring_only_reason
```

다음 단계 진입 조건:

ThreadTracker observation-only와 group-aware TopologyResult가 확인되어야 Phase 2로 이동한다.

## 6. Phase 2 — Main Thread Confidence Engine

목적:

ThreadTracker의 raw signal을 기반으로 MainThreadConfidence를 계산한다.

작업 범위:

```text
MainThreadConfidenceAnalyzer 추가 또는 정리
Low / Medium / High / VeryHigh 분류
confidence reason 기록
insufficient evidence reason 기록
policy eligibility 제공
```

영향 모듈:

```text
MainThreadConfidenceAnalyzer
PerformanceEngine
ThreadTracker
RuntimeValidationMonitor
```

금지사항:

```text
Win32 mutation 금지
SchedulerController 직접 호출 금지
PolicyDispatcher 직접 호출 금지
Confidence < High에서 affinity/priority 가능 판정 금지
```

완료 기준:

```text
confidence level이 Evidence로 기록된다.
insufficient evidence 시 monitoring-only reason을 제공한다.
policy eligibility가 confidence level에 따라 제한된다.
```

필수 테스트:

```text
low confidence test
medium confidence test
high confidence test
very high confidence test
unstable candidate downgrade test
access failure confidence block test
```

필수 Evidence:

```text
main_thread_confidence_level
confidence_reason
confidence_history
insufficient_evidence_reason
policy_eligibility_result
monitoring_only_reason
```

다음 단계 진입 조건:

Confidence 결과가 mutation 없이 evidence로 기록되고, Low/Medium/High/VeryHigh 분류가 테스트되어야 Phase 3으로 이동한다.

## 7. Phase 3 — Policy Candidate & Dispatch Layer

목적:

PerformanceEngine의 판단 결과를 PolicyCandidate로 표현하고 PolicyDispatcher를 통해 controller로 전달한다.

작업 범위:

```text
PolicyCandidate 구조 정리
requiredController 검증
requiredRollbackScope 검증
requiredEvidenceFields 검증
cooldown / conflict 처리
monitoring-only dispatch 처리
```

영향 모듈:

```text
PerformanceEngine
PolicyDispatcher
PolicyResolver
SchedulerController
BackgroundController
PerformanceEvidencePlanner
```

금지사항:

```text
PolicyDispatcher에서 Win32 mutation 금지
rollback scope 없는 mutation policy 전달 금지
requiredController 없는 policy 전달 금지
```

완료 기준:

```text
검증 실패 policy는 controller로 전달되지 않는다.
blocked policy는 Evidence와 함께 reject된다.
monitoring-only reason이 기록된다.
```

필수 테스트:

```text
missing controller reject test
missing rollback scope reject test
blocked policy reject test
cooldown block test
conflict resolution test
monitoring-only dispatch test
```

필수 Evidence:

```text
policy_candidate_summary
policy_reject_reason
required_controller_result
required_rollback_scope_result
cooldown_result
conflict_resolution_result
monitoring_only_reason
```

다음 단계 진입 조건:

PolicyCandidate validation과 reject evidence가 안정적으로 기록되어야 Phase 4로 이동한다.

## 8. Phase 4 — SchedulerController Safety Hardening

목적:

SchedulerController의 mutation 경로를 v3.1 안전 계약에 맞게 강화한다.

작업 범위:

```text
thread priority apply 경로 검증
thread group affinity apply 경로 검증
process priority apply 경로 검증
apply 전 state query
RollbackManager save 요청
ApplyGuard arm
verify
failure rollback request
```

영향 모듈:

```text
SchedulerController
RollbackManager
ApplyGuard
TopologyAnalyzer
RuntimeValidationMonitor
```

금지사항:

```text
rollback state 저장 전 mutation 금지
ApplyGuard arm 전 mutation 금지
verify 없이 commit 금지
Processor Group 정보 없을 때 group 0 보정 금지
```

완료 기준:

```text
모든 mutation 경로가 Save -> Arm -> Apply -> Verify -> Commit 또는 Rollback 흐름을 따른다.
실패 시 RollbackPending으로 이어질 수 있다.
```

필수 테스트:

```text
state save failure blocks mutation
apply guard arm failure blocks mutation
apply failure rollback request
verify failure rollback request
processor group missing blocks affinity policy
```

필수 Evidence:

```text
scheduler_apply_summary
rollback_state_save_result
apply_guard_result
verify_result
processor_group_safety_result
failure_rollback_request
```

다음 단계 진입 조건:

SchedulerController가 Save -> Arm -> Apply -> Verify 흐름을 강제하고 실패 시 rollback 경로를 제공해야 Phase 5로 이동한다.

## 9. Phase 5 — Rollback / ApplyGuard Contract Hardening

목적:

RollbackManager와 ApplyGuard의 실패 처리, state preservation, identity validation을 강화한다.

작업 범위:

```text
rollback state save result 명확화
identity revalidation
living same identity rollback failure classification
stale/missing identity recoverable classification
failed rollback state preservation
ApplyGuard destructor failure audit
```

영향 모듈:

```text
RollbackManager
ApplyGuard
SchedulerController
BackgroundController
ShutdownPipeline
RuntimeValidationMonitor
```

금지사항:

```text
rollback failure를 WARN/INFO로 낮추기 금지
failed state 폐기 금지
identity mismatch 무시 금지
성공 rollback으로 위장 금지
```

완료 기준:

```text
rollback 실패가 release-facing evidence에 기록된다.
failed state가 보존된다.
rollback 결과가 RuntimeValidation과 RC Evidence에 반영된다.
```

필수 테스트:

```text
rollback success test
stale identity rollback classification test
living same identity rollback failure test
failed state preservation test
ApplyGuard rollback-on-failure test
shutdown rollback test
```

필수 Evidence:

```text
rollback_summary
rollback_state_preservation_result
identity_revalidation_result
living_same_identity_failure
stale_identity_classification
apply_guard_audit_result
```

다음 단계 진입 조건:

Rollback failure가 BLOCKER로 기록되고 failed state가 보존되어야 Phase 6으로 이동한다.

## 10. Phase 6 — Evidence & Runtime Validation Integration

목적:

성능 정책, 런타임 상태, rollback, validation 결과를 Evidence로 연결한다.

작업 범위:

```text
session manifest 확장
runtime snapshot latest/final 정리
policy timeline 정리
performance summary 정리
rollback summary 정리
runtime validation summary 정리
evidence completeness report 정리
```

영향 모듈:

```text
RuntimeSnapshotRecorder
PerformanceEvidenceRecorder
PolicyTimelineRecorder
RuntimeValidationMonitor
Evidence validator
RC report generator
```

금지사항:

```text
Evidence 없는 성능 성공 판정 금지
rollback failure severity 하향 금지
runtime validation blocker 무시 금지
```

완료 기준:

```text
필수 Evidence Bundle이 생성된다.
Evidence completeness를 판정할 수 있다.
RuntimeValidation BLOCKER가 RC 판정에 반영된다.
```

필수 테스트:

```text
manifest completeness test
snapshot freshness test
policy timeline monotonicity test
rollback summary required test
runtime validation blocker test
evidence missing classification test
```

필수 Evidence:

```text
session_manifest
runtime_snapshot_latest
runtime_snapshot_final
policy_timeline
performance_summary
rollback_summary
runtime_validation_summary
evidence_completeness_report
```

다음 단계 진입 조건:

Evidence Bundle 필수 항목과 RuntimeValidation BLOCKER 반영이 확인되어야 Phase 7로 이동한다.

## 11. Phase 7 — Game Profile Integration

목적:

GameProfileSpecification 기준으로 profile selection과 policy restriction을 런타임에 반영한다.

작업 범위:

```text
Unknown Default Profile 처리
Candidate Profile 처리
allowedPolicies / blockedPolicies / conditionalPolicies 적용
profileStatus 반영
validationRequired 반영
monitoring-only fallback 반영
```

영향 모듈:

```text
GameProfileRegistry
RuntimeContext
PerformanceEngine
PolicyDispatcher
Evidence recorder
```

금지사항:

```text
검증되지 않은 profile을 Validated로 처리 금지
Unknown profile에서 aggressive mode 허용 금지
AntiCheatSensitive profile에서 mutation 허용 금지
targetProcessName을 추측 확정 금지
```

완료 기준:

```text
profile selection result가 Evidence에 기록된다.
blockedPolicies가 allowedPolicies보다 우선한다.
Unknown profile은 monitoring-only 또는 dry-run으로 제한된다.
```

필수 테스트:

```text
unknown profile fallback test
candidate profile restriction test
blocked policy precedence test
anti-cheat sensitive blocks mutation test
validated profile not auto-created test
```

필수 Evidence:

```text
selected_profile
profile_type
profile_status
profile_selection_reason
allowed_policies
blocked_policies
conditional_policies
monitoring_only_reason
validation_required
```

다음 단계 진입 조건:

Profile restriction이 PolicyDispatcher 전에 적용되고 Evidence에 남아야 Phase 8로 이동한다.

## 12. Phase 8 — Performance Validation Flow

목적:

PerformanceValidationPlan 기준으로 dry-run, soft-apply, apply, rollback, soak 검증 흐름을 구현 가능하게 정리한다.

작업 범위:

```text
Baseline Window 수집
Applied Window 수집
Before/After Comparison 생성
classification result 생성
dry-run summary
soft-apply summary
apply summary
soak summary
```

영향 모듈:

```text
PerformanceEvidenceRecorder
RuntimeValidationMonitor
PolicyDispatcher
SchedulerController
RollbackManager
RC report generator
```

금지사항:

```text
Before/After 없이 성능 개선 판정 금지
Rollback 실패가 있으면 PASS 금지
RuntimeValidation BLOCKER가 있으면 PASS 금지
```

완료 기준:

```text
성능 결과가 PASS / PASS_WITH_WARNINGS / NEUTRAL / REGRESSION / BLOCKER / INVALID_RUN 중 하나로 분류된다.
성능 개선 주장이 Evidence로 설명 가능하다.
```

필수 테스트:

```text
baseline only test
before after comparison test
neutral classification test
regression classification test
blocker overrides performance improvement test
```

필수 Evidence:

```text
baseline_window
applied_window
comparison_summary
dry_run_summary
soft_apply_summary
apply_summary
soak_summary
classification_result
```

다음 단계 진입 조건:

Before/After 비교와 BLOCKER override가 동작해야 Phase 9로 이동한다.

## 13. Phase 9 — Aggressive Single-Core Mode

목적:

모든 안전 조건을 만족한 경우에만 Aggressive Single-Core Mode를 활성화한다.

작업 범위:

```text
VeryHigh confidence gate
minimum observation time gate
migration stability gate
rollback availability gate
processor group safety gate
runtime validation clean gate
profile permission gate
deactivation condition
cooldown
```

영향 모듈:

```text
AggressiveSingleCoreController
PerformanceEngine
PolicyDispatcher
SchedulerController
RollbackManager
RuntimeValidationMonitor
Evidence recorder
```

금지사항:

```text
VeryHigh confidence 전 aggressive mode 금지
Unknown profile에서 aggressive mode 금지
rollback scope 없는 aggressive mode 금지
RuntimeValidation BLOCKER 상태에서 aggressive mode 금지
```

완료 기준:

```text
Aggressive mode는 조건 충족 시에만 활성화된다.
해제 조건이 명확하다.
duration과 enabled policy list가 Evidence에 기록된다.
```

필수 테스트:

```text
confidence gate blocks aggressive mode
unknown profile blocks aggressive mode
rollback missing blocks aggressive mode
runtime blocker disables aggressive mode
deactivation condition test
cooldown test
```

필수 Evidence:

```text
aggressive_mode_eligibility
aggressive_mode_gate_result
enabled_policy_list
aggressive_mode_duration
deactivation_reason
cooldown_result
```

다음 단계 진입 조건:

Aggressive mode가 모든 gate를 통과한 경우에만 활성화되고 실패 시 safe reject evidence를 남겨야 Phase 10으로 이동한다.

## 14. Phase 10 — RC Gate Integration

목적:

RC_Runbook_v3.1.md 기준으로 Release Candidate 검증을 자동 또는 반자동으로 연결한다.

작업 범위:

```text
preflight check
documentation check
build check
static safety check
dry-run validation
soft-apply validation
rollback validation
evidence bundle validation
RC report generation
final decision mapping
```

영향 모듈:

```text
release scripts
validation scripts
artifact validator
RC report generator
build scripts
```

금지사항:

```text
BLOCKER를 무시하고 PASS 생성 금지
binary hash 누락 상태에서 Release PASS 금지
rollback summary 누락 상태에서 PASS 금지
runtime validation summary 누락 상태에서 PASS 금지
```

완료 기준:

```text
RC Evidence Report가 생성된다.
Final decision이 PASS / PASS_WITH_WARNINGS / NEUTRAL / REGRESSION / BLOCKER / INVALID_RUN 중 하나로 나온다.
BLOCKER 발생 시 exit code가 실패로 반영된다.
```

필수 테스트:

```text
missing binary hash blocker test
missing rollback summary blocker test
runtime validation blocker test
evidence bundle complete test
RC report generation test
```

필수 Evidence:

```text
rc_report
final_decision
checklist_summary
binary_hash
git_commit
blocker_count
fatal_count
evidence_bundle_result
```

다음 단계 진입 조건:

RC Gate Integration이 PASS하면 v3.1 RC 검토로 이동할 수 있다. BLOCKER가 있으면 release candidate 생성은 금지된다.

## 15. 구현 우선순위

우선순위를 다음으로 고정한다.

```text
P0:
- Rollback / ApplyGuard safety
- RuntimeValidation BLOCKER handling
- Evidence completeness
- Processor Group safety
- ThreadTracker observation-only

P1:
- MainThreadConfidenceAnalyzer
- PolicyCandidate / PolicyDispatcher
- Game Profile restriction
- Performance Evidence classification

P2:
- Aggressive Single-Core Mode
- Background restriction expansion
- Long Soak automation
- External FPS/frame time integration
```

주의:

Aggressive Single-Core Mode는 P2다.
문서상 핵심 기능이어도 안전 조건과 Evidence 체계가 완료되기 전에는 구현하지 않는다.

## 16. 패치 단위 원칙

각 패치는 다음 원칙을 따른다.

1. 하나의 패치는 하나의 책임만 변경한다.
2. 인터페이스 변경 시 선언, 정의, 호출부, 테스트를 모두 수정한다.
3. `std::expected`는 Assign -> Check -> Bind 패턴을 따른다.
4. expected 결과를 검사 없이 `return func(...)` 형태로 전파하지 않는다.
5. Win32 mutation 경로는 rollback state save와 ApplyGuard arm 이후에만 허용한다.
6. 테스트 없이 release-facing behavior를 변경하지 않는다.
7. Evidence 필드 변경 시 validator와 report 기준도 함께 갱신한다.

패치 단위는 RC Runbook의 단계와 매핑 가능해야 한다. 한 패치가 observation, policy decision, mutation, rollback, reporting을 동시에 변경하면 원인 추적이 어려워지므로 분리한다.

## 17. 개발 중단 조건

다음 조건이 발생하면 기능 개발을 중단하고 안전성 수정으로 전환한다.

```text
rollback failure
ApplyGuard bypass
RuntimeValidation BLOCKER
Evidence fatal missing
Processor Group unsafe fallback
std::expected uninspected propagation
ThreadTracker mutation responsibility leak
PerformanceEngine direct Win32 mutation
PolicyDispatcher direct Win32 mutation
```

중단 후 우선 조치:

1. 새 기능 구현을 멈춘다.
2. failed state와 evidence를 보존한다.
3. BLOCKER를 release-facing evidence에 기록한다.
4. 원인 Phase를 식별한다.
5. 해당 Phase의 완료 기준과 테스트를 보강한다.

## 18. Non-Goals

다음은 ImplementationPlan_v3.1.md의 비목표(Non-Goals)다.

```text
구체 C++ 코드 작성
패치 파일 생성
테스트 코드 작성
빌드 스크립트 구현
CI/CD 구현
UI 구현
공식 지원 게임 목록 확정
성능 향상 보장
Anti-Cheat 우회 기능 구현
Kernel Driver 구현
DLL Injection 구현
Game Memory Patch 구현
```

이 문서는 구현 계획(Implementation Plan)이며, 실제 patch 작성은 후속 `docs/implementation/PatchPlan_Phase1.md` 이후 진행한다.

## 19. Acceptance Criteria

본 문서의 완료 기준은 다음과 같다.

1. v3.1 구현 Phase가 순서대로 정의되어 있다.
2. 각 Phase의 목적, 작업 범위, 영향 모듈, 금지사항, 완료 기준이 정의되어 있다.
3. 각 Phase의 필수 테스트와 필수 Evidence가 정의되어 있다.
4. P0/P1/P2 우선순위가 정의되어 있다.
5. Aggressive Single-Core Mode가 후순위임이 명확하다.
6. Rollback / ApplyGuard / Evidence / RuntimeValidation이 최우선 구현 기준으로 정의되어 있다.
7. 패치 단위 원칙이 정의되어 있다.
8. 개발 중단 조건이 정의되어 있다.
9. 후속 `docs/implementation/PatchPlan_Phase1.md` 작성에 필요한 기준이 충분하다.

## 20. Open Questions

다음 질문은 후속 Patch Plan 또는 구현 전 설계 확정 단계에서 결정한다.

```text
1. Phase 1 구현 전에 기존 ThreadTracker 정적 검사부터 강화할 것인가?
2. Processor Group Phase를 Phase 1에 포함할 것인가, 별도 Phase로 분리할 것인가?
3. MainThreadConfidenceAnalyzer는 신규 모듈로 만들 것인가, PerformanceEngine 내부 하위 모듈로 둘 것인가?
4. GameProfileRegistry는 v3.1에서 실제 구현할 것인가, 문서/설정 기준으로만 둘 것인가?
5. EvidenceSpecification을 JSON schema까지 확장할 것인가?
6. RC Gate Integration을 이번 버전에 자동화할 것인가, 수동 runbook 기반으로 둘 것인가?
7. Aggressive Single-Core Mode는 v3.1 정식 범위인가, v3.2 후보인가?
8. 실제 게임 검증은 구현 완료 전 어느 시점부터 병행할 것인가?
```

Open Questions가 해결되기 전까지 관련 수치와 구현 범위는 `TBD`로 유지한다. 특히 Aggressive Single-Core Mode의 release scope, GameProfileRegistry 실제 구현 여부, RC Gate 자동화 범위는 Phase 1 착수 전에 owner와 판단 기준을 남겨야 한다.
