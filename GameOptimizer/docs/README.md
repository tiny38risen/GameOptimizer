# GameOptimizer Documentation Map

## 1. 문서 루트 기준

이 프로젝트의 문서 기준 경로는 `GameOptimizer/docs/` 이다.
문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

이 README는 AI Agent와 개발자가 가장 먼저 읽는 문서 맵이다. 문서 이동이나 신규 문서 추가 시 이 파일과 repository root의 `../MIGRATION_NOTES.md`를 함께 확인한다.

## 2. 문서 읽는 순서

1. `docs/vision/PVD_v1.0.md`
2. External SRS: `GameOptimizer_v3_SRS_Rev1_3_CPP23_Safety_Update.docx` (not tracked in this repository)
3. `docs/architecture/SAD_v1.0.md`
4. `docs/architecture/RuntimeStateMachine.md`
5. `docs/modules/`
6. `docs/contracts/`
7. `docs/validation/`
8. `docs/release/`
9. `docs/implementation/`
10. `docs/proposals/performance/` (v3.1 performance proposal work only)

`docs/requirements/SRS_Rev1_3.md` is a planned Markdown conversion target for the external SRS DOCX. It is not a current in-repository source of truth.

## 3. 문서별 역할

| 위치 | 문서 역할 | Status | Authority | ImplementationStatus | VerificationStatus | TargetVersion |
|---|---|---|---|---|---|---|
| `docs/vision/` | 제품 방향 | Active | Product direction | Design source | Manual review | v3.x |
| `docs/requirements/` | 요구사항 | Planned | External SRS conversion target | External DOCX not tracked; Markdown not created | Not verified in repo | v3.0/v3.1 |
| `docs/proposals/performance/` | 미구현 성능 엔진/정책 제안 | Proposal | Proposed design, not current implementation contract | Not implemented | Not verified/manual design review | v3.1 proposal |
| `docs/architecture/` | 전체 구조 | Active | Architecture contract | Partial implementation | Static gate/manual review | v3.0-rc1/v3.1 |
| `docs/contracts/` | 안전 계약 | Active | Normative contract | Implemented/partial | Static gate/runtime evidence | v3.0-rc1/v3.1 |
| `docs/modules/` | 모듈 상세 설계 | Active | Module implementation contract | Partial implementation | Code review/static gate | v3.1 |
| `docs/validation/` | 검증 계획 | Active | Validation design | Partial implementation | Manual/gate evidence | v3.1 |
| `docs/evidence/` | 증거 구조/분류 기준 | Active | Future evidence design | Design only | Planned/manual review | v3.1 |
| `docs/release/` | 릴리즈 기준 | Active | Release gate contract | Implemented/partial | Static gate/evidence validator | v3.0-rc1 |
| `docs/implementation/` | 구현 계획 | Active | Execution plan | Partial implementation | Manual progress review | v3.1 |
| `docs/archive/patch-plans/` | 과거 Phase Patch Plan | Archived | Historical reference only; GitHub Issues own active work | Superseded | Not release evidence | v3.1 history |
| `docs/adr/` | 개별 ADR 분리 문서 | Planned | ADR split target | Not started | Not verified | TBD |
| `docs/design/` | 기존 설계 노트 | Active | Supporting design note | Historical/partial | Manual review | v3.x |
| `docs/governance/` | 기존 거버넌스 체계 | Active | Governance guide | Partial implementation | Manual/static review | v3.x |
| `docs/engineering/` | 엔지니어링 규칙 | Active | Engineering rulebook | Partial implementation | Static/manual review | v3.x |
| `docs/history/` | 히스토리 및 빌드 기록 | Active | Generated/history record | Generated/manual update | Artifact review | v3.x |

### 3.1 Evidence 문서 권위

| 문서 | Status | Authority | ImplementationStatus | VerificationStatus | TargetVersion |
|---|---|---|---|---|---|
| `docs/release/Evidence_Schema.md` | Active | Current machine contract for RC evidence | Implemented by `release_gate_evidence.py`, `verify_rc_candidate.py`, and `create_rc_evidence_bundle.py` | Static gate and RC evidence validators | v3.0-rc1 |
| `docs/evidence/EvidenceSpecification.md` | Active | Future v3.1 evidence design | Design only; not the current machine schema | Planned/manual design review | v3.1 |

## 4. 문서 상태 규칙

- Active: 현재 기준 문서
- Draft: 작성 중
- Proposal: 구현되지 않은 설계 후보이며 현재 기준 계약이 아님
- Planned: 아직 작성 예정
- Deprecated: 더 이상 사용하지 않음
- Superseded: 다른 문서로 대체됨

문서 등록부는 `Status`만으로 문서의 권위를 판단하지 않는다. 각 문서는 다음 필드를 함께 가진다.

- `Authority`: 해당 문서가 기준 계약인지, 설계 기준인지, 실행 절차인지, 생성 기록인지 구분한다.
- `ImplementationStatus`: 문서 내용이 코드/스크립트/자동화에 반영된 정도를 기록한다.
- `VerificationStatus`: 정적 게이트, 런타임 evidence, 수동 리뷰, 미검증 중 어떤 방식으로 확인되는지 기록한다.
- `TargetVersion`: 문서가 겨냥하는 릴리즈 또는 설계 버전을 기록한다.

## 5. 문서 작성 규칙

1. 문서 경로는 `docs/...` 기준으로 쓴다.
2. 구현 전에는 관련 PVD/SRS/SPEC/MDS를 먼저 확인한다.
3. 안전 계약 문서는 코드보다 우선한다.
4. Performance 문서는 Policy 문서보다 상위 판단 기준이다.
5. Module Design 문서는 구현 직전 계약이다.
6. Release 문서는 배포 승인 기준이다.
7. 기존 Release Gate 스크립트가 참조하는 문서는 이동 전 반드시 참조 경로를 확인한다.
8. 경로 변경으로 스크립트가 깨질 가능성이 있으면 이동하지 않고 `../MIGRATION_NOTES.md`에 기록한다.
9. `docs/proposals/` 문서는 구현 완료 전까지 기준 계약 또는 release PASS 근거로 사용하지 않는다.

## 6. Active Documents

- `docs/Markdown_Collection_v3.1.md`
- `docs/vision/PVD_v1.0.md`
- `docs/architecture/Architecture_Decision_Record.md`
- `docs/architecture/Contract_Enforcement_Matrix.md`
- `docs/architecture/Contract_Enforcement_Status.md`
- `docs/architecture/Module_Ownership_Matrix.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/architecture/Source_Bands.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/Evidence_Schema.md`
- `docs/release/RC_Runbook.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/release/RC_Report_Template_v3.1.md`
- `docs/release/Release_Blocker_List.md`
- `docs/release/Release_Regression_Matrix.md`
- `docs/release/Release_Performance_Checklist.md`
- `docs/release/Operational_Runbook.md`
- `docs/release/Real_Game_Validation_Runbook.md`
- `docs/release/Game_Verification_Matrix.md`
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
- `docs/evidence/EvidenceSpecification.md`
- `docs/implementation/ImplementationPlan_v3.1.md`
- `docs/implementation/DevelopmentExecutionPlan_v3.1.md`

## 7. Archived Documents

The archived Patch Plans are historical references only. Active execution work is tracked in GitHub Issues.

- `docs/archive/patch-plans/PatchPlan_Phase1.md`
- `docs/archive/patch-plans/PatchPlan_Phase2.md`
- `docs/archive/patch-plans/PatchPlan_Phase3.md`
- `docs/archive/patch-plans/PatchPlan_Phase4.md`
- `docs/archive/patch-plans/PatchPlan_Phase5.md`
- `docs/archive/patch-plans/PatchPlan_Phase6.md`
- `docs/archive/patch-plans/PatchPlan_Phase7.md`
- `docs/archive/patch-plans/PatchPlan_Phase8.md`
- `docs/archive/patch-plans/PatchPlan_Phase9.md`
- `docs/archive/patch-plans/PatchPlan_Phase10.md`

## 8. Proposal Documents

- `docs/proposals/performance/PerformanceEngineSpec.md`
- `docs/proposals/performance/GameProfileSpecification.md`
- `docs/proposals/performance/PolicySpecification.md`

## 9. Planned Documents

- `docs/requirements/SRS_Rev1_3.md` (Markdown conversion target for external `GameOptimizer_v3_SRS_Rev1_3_CPP23_Safety_Update.docx`)
- `docs/contracts/Rollback_Contract.md`
- `docs/contracts/Expected_Usage_Contract.md`
- `docs/validation/TestPlan.md`
- `docs/validation/IntegrationTestPlan.md`
- `docs/validation/RegressionMatrix.md`
- `docs/validation/RealGameValidationPlan.md`
- `docs/adr/ADR-001_TransactionalMutation.md`
- `docs/adr/ADR-002_ThreadTrackerObservationOnly.md`
- `docs/adr/ADR-003_SchedulerControllerOwnership.md`
- `docs/adr/ADR-004_ProcessorGroupPolicy.md`

## 10. Root Support Documents

- `../MIGRATION_NOTES.md`

## 11. Migration Notes

문서 위치 정리 중 이동하지 않은 파일과 경로 충돌은 repository root의 `../MIGRATION_NOTES.md`에 기록한다.
