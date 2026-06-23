# GameOptimizer Document Register

## 1. 목적

이 문서는 GameOptimizer 문서의 현재 역할과 lifecycle status를 기록한다.
개인 프로젝트 기준의 실제 진입 문서는 5개만 `Active`로 둔다.
기존 상세 문서는 삭제하지 않고 `Reference`, `Proposal`, `Archived`, `Superseded`, `NeedsReview`, `External`로 보존한다.

## 2. Active 문서

| 문서 | 역할 |
|---|---|
| `docs/PROJECT_SPEC.md` | 요구사항, 범위, 현재 상태 |
| `docs/ARCHITECTURE_AND_SAFETY.md` | 구조, mutation owner, 안전 규칙 |
| `docs/VALIDATION_RUNBOOK.md` | 빌드, 테스트, Evidence, RC 판단 |
| `docs/UI_SPEC.md` | UI 화면, 사용자 흐름, 안전 경계 |
| `docs/DOCUMENT_REGISTER.md` | 문서 상태 등록부 |

## 3. 상태값

| Status | 의미 |
|---|---|
| Active | 현재 읽어야 하는 핵심 문서 |
| Reference | 구현 이해와 세부 배경을 위한 참고 문서 |
| Proposal | 미래 설계 또는 아직 current implementation이 아닌 문서 |
| Archived | 과거 계획, 기록, 이력 보존 문서 |
| Superseded | 새 핵심 문서 또는 다른 기준으로 대체된 문서 |
| NeedsReview | 자동 생성 경로, 권위, 중복 여부 확인 필요 |
| External | repo 내부 canonical 문서가 아닌 외부 기준 |

## 4. 문서 등록표

| Document | Status | Role | Notes |
|---|---|---|---|
| `external:SRS Rev 1.3 DOCX` | External | 외부 요구사항 기준 | repo 내부 canonical Markdown 아님 |
| `docs/PROJECT_SPEC.md` | Active | 프로젝트 요구사항 요약 | 핵심 문서 |
| `docs/ARCHITECTURE_AND_SAFETY.md` | Active | 구조와 안전 규칙 요약 | 핵심 문서 |
| `docs/VALIDATION_RUNBOOK.md` | Active | 검증 절차 요약 | 핵심 문서 |
| `docs/UI_SPEC.md` | Active | UI 기준 요약 | 핵심 문서 |
| `docs/DOCUMENT_REGISTER.md` | Active | 문서 상태 등록부 | 핵심 문서 |
| `docs/README.md` | Reference | 핵심 문서 읽기 안내 | Active는 아님 |
| `docs/CORE_SPEC_v3.1.md` | Superseded | 이전 통합 진입점 | 새 4개 핵심 문서로 분리됨 |
| `docs/vision/PVD_v1.0.md` | Reference | 제품 방향성 | SRS 아님 |
| `docs/contracts/Safety_Contract.md` | Reference | 상세 안전 계약 | 핵심 요약은 `ARCHITECTURE_AND_SAFETY.md` |
| `docs/contracts/Runtime_Contract.md` | Reference | 상세 runtime 계약 | SafetyContract와 분리 유지 |
| `docs/architecture/Architecture_Decision_Record.md` | Reference | ADR와 release contract 배경 | 상세 참고 |
| `docs/architecture/Contract_Enforcement_Matrix.md` | Reference | contract enforcement 배경 | 상세 참고 |
| `docs/architecture/Contract_Enforcement_Status.md` | NeedsReview | enforcement 상태 기록 | generation path not verified |
| `docs/architecture/Module_Ownership_Matrix.md` | Reference | module owner 상세표 | 요약은 `ARCHITECTURE_AND_SAFETY.md` |
| `docs/architecture/RuntimeStateMachine.md` | Reference | runtime state 상세 | 요약은 핵심 문서 |
| `docs/architecture/SAD_v1.0.md` | Reference | 상세 architecture design | 핵심 요약으로 대체되지 않고 참고 보존 |
| `docs/architecture/Source_Bands.md` | NeedsReview | source band data | generation path not verified |
| `docs/modules/README.md` | Reference | MDS 안내 | 상세 참고 |
| `docs/modules/MDS-001_ThreadTracker.md` | Reference | ThreadTracker 상세 | 관측 전용 기준 참고 |
| `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md` | Reference | confidence analyzer 상세 | partial/proposal 요소 포함 |
| `docs/modules/MDS-003_TopologyAnalyzer.md` | Reference | topology 상세 | group-aware 참고 |
| `docs/modules/MDS-004_SchedulerController.md` | Reference | mutation owner 상세 | 상세 참고 |
| `docs/modules/MDS-005_RollbackManager.md` | Reference | rollback 상세 | 상세 참고 |
| `docs/modules/MDS-006_BackgroundController.md` | Reference | background restriction 상세 | 제한 포함 |
| `docs/modules/MDS-007_PerformanceEvidencePlanner.md` | Proposal | evidence planner 설계 | current implementation으로 보지 않음 |
| `docs/modules/MDS-008_PolicyDispatcher.md` | Reference | dispatch 상세 | 상세 참고 |
| `docs/modules/MDS-009_RuntimeOrchestrator.md` | Reference | runtime orchestration 상세 | 상세 참고 |
| `docs/implementation/ImplementationPlan_v3.1.md` | Proposal | 구현 계획 | active work source of truth 아님 |
| `docs/implementation/DevelopmentExecutionPlan_v3.1.md` | Proposal | 실행 계획 | active work source of truth 아님 |
| `docs/archive/patch-plans/PatchPlan_Phase1.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase2.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase3.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase4.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase5.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase6.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase7.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase8.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase9.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/archive/patch-plans/PatchPlan_Phase10.md` | Archived | 과거 patch plan | GitHub Issues로 대체 예정 |
| `docs/release/Evidence_Schema.md` | Reference | 현재 RC evidence schema 후보 | 상세 필드는 여기 보존 |
| `docs/evidence/EvidenceSpecification.md` | Proposal | 미래 evidence 설계 | 현재 machine schema 아님 |
| `docs/release/Release_Gate_Spec.md` | Superseded | 구버전 release gate 설명 | 현재 실행은 `run_rc_gate.bat`와 `VALIDATION_RUNBOOK.md` 참조 |
| `docs/release/Release_Blocker_List.md` | Reference | 상세 BLOCKER 목록 | 요약은 `VALIDATION_RUNBOOK.md` |
| `docs/release/RC_Runbook.md` | Superseded | 구버전 RC runbook | 요약 기준은 `VALIDATION_RUNBOOK.md` |
| `docs/release/RC_Runbook_v3.1.md` | Reference | 상세 v3.1 RC 절차 | 핵심 요약은 `VALIDATION_RUNBOOK.md` |
| `docs/release/Operational_Runbook.md` | Reference | 운영 절차 상세 | 필요 시 참고 |
| `docs/release/Real_Game_Validation_Runbook.md` | Reference | 실제 게임 검증 상세 | 필요 시 참고 |
| `docs/release/ReleaseChecklist_v3.1.md` | Reference | release checklist 상세 | 요약은 `VALIDATION_RUNBOOK.md` |
| `docs/release/Release_Performance_Checklist.md` | Reference | 성능 checklist 상세 | 필요 시 참고 |
| `docs/release/RC_Report_Template_v3.1.md` | Reference | RC report template | template, generated로 확정하지 않음 |
| `docs/release/Release_Regression_Matrix.md` | NeedsReview | regression matrix | generation path not verified |
| `docs/release/Game_Verification_Matrix.md` | NeedsReview | game validation matrix | generation path not verified |
| `docs/release/Game_Verification_Matrix.example.json` | NeedsReview | game validation example data | Markdown 아님, 참고 데이터 |
| `docs/release/Known_Limitations.md` | Reference | 알려진 제한 | safety contract 아님 |
| `docs/validation/PerformanceValidationPlan.md` | Reference | 성능 검증 계획 | 상세 기준 참고 |
| `docs/proposals/performance/PerformanceEngineSpec.md` | Proposal | PerformanceEngine 미래 설계 | current implementation 아님 |
| `docs/proposals/performance/PolicySpecification.md` | Proposal | policy 미래 설계 | current implementation 아님 |
| `docs/proposals/performance/GameProfileSpecification.md` | Proposal | GameProfile 미래 설계 | current implementation 아님 |
| `docs/design/AntiCheatFallbackDesign.md` | Proposal | Anti-Cheat fallback 설계 | lifecycle header 필요 |
| `docs/design/InputLatencyPhase2Design.md` | Proposal | Input latency phase 2 설계 | lifecycle header 필요 |
| `docs/design/IntegrationTestPlan_NextDesign.md` | Proposal | 다음 integration test 설계 | lifecycle header 필요 |
| `docs/design/ProcessorGroupPhase2Design.md` | Proposal | Processor Group phase 2 설계 | lifecycle header 필요 |
| `docs/design/ApplyGuardRegressionMatrix.md` | NeedsReview | ApplyGuard regression matrix | generation path not verified |
| `docs/engineering/Engineering_Handbook.md` | Reference | engineering guide | 상세 참고 |
| `docs/governance/Development_Roadmap.md` | Proposal | future planning | current 기준 아님 |
| `docs/governance/README.md` | Reference | governance 참고 | 상세 참고 |
| `docs/history/Build_Manifest.md` | Archived | build 기록 | generation path not verified |
| `docs/history/Patch_History.md` | Archived | patch 기록 | generation path not verified |
| `docs/Markdown_Collection_v3.1.md` | Superseded | 이전 catalog | `docs/README.md`와 이 register로 대체 |
| `GameOptimizer.UI/README.md` | Reference | UI 참고 문서 | 최종 UI 기준은 `docs/UI_SPEC.md` |
| `GameOptimizer/MIGRATION_NOTES.md` | Archived | migration 기록 | allowed edit scope 밖, 보존만 함 |
| `docs/architecture/Architecture_Decision_Record.md` at repo root | Superseded | non-canonical duplicate | allowed edit scope 밖 |
| `docs/architecture/Contract_Enforcement_Matrix.md` at repo root | Superseded | non-canonical duplicate | allowed edit scope 밖 |

## 5. Unresolved

- canonical SRS Markdown이 repo 내부에 없다.
- External SRS Rev 1.3 DOCX는 repo canonical 문서가 아니다.
- root `docs/architecture` 중복 문서는 정리 결정이 필요하다.
- `Generated` 여부는 generator path가 확인된 뒤에만 확정한다.
- Proposal 문서를 current implementation처럼 읽지 않도록 유지한다.
