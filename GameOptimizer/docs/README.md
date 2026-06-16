# GameOptimizer Documentation Map

## 1. 문서 루트 기준

이 프로젝트의 문서 기준 경로는 `GameOptimizer/docs/` 이다.
문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

이 README는 AI Agent와 개발자가 가장 먼저 읽는 문서 맵이다. 문서 이동이나 신규 문서 추가 시 이 파일과 `MIGRATION_NOTES.md`를 함께 확인한다.

## 2. 문서 읽는 순서

1. `docs/vision/PVD_v1.0.md`
2. `docs/requirements/SRS_Rev1_3.md`
3. `docs/performance/PerformanceEngineSpec.md`
4. `docs/performance/PolicySpecification.md`
5. `docs/architecture/SAD_v1.0.md`
6. `docs/architecture/RuntimeStateMachine.md`
7. `docs/modules/`
8. `docs/contracts/`
9. `docs/validation/`
10. `docs/release/`
11. `docs/implementation/`

## 3. 문서별 역할

| 위치 | 문서 역할 | 상태 |
|---|---|---|
| `docs/vision/` | 제품 방향 | Active |
| `docs/requirements/` | 요구사항 | Planned |
| `docs/performance/` | 성능 엔진/정책 | Active |
| `docs/architecture/` | 전체 구조 | Active |
| `docs/contracts/` | 안전 계약 | Active |
| `docs/modules/` | 모듈 상세 설계 | Active |
| `docs/validation/` | 검증 계획 | Active |
| `docs/evidence/` | 증거 구조/분류 기준 | Active |
| `docs/release/` | 릴리즈 기준 | Active |
| `docs/implementation/` | 구현 계획 | Active |
| `docs/adr/` | 개별 ADR 분리 문서 | Planned |
| `docs/design/` | 기존 설계 노트 | Active |
| `docs/governance/` | 기존 거버넌스 체계 | Active |
| `docs/engineering/` | 엔지니어링 규칙 | Active |
| `docs/history/` | 히스토리 및 빌드 기록 | Active |

## 4. 문서 상태 규칙

- Active: 현재 기준 문서
- Draft: 작성 중
- Planned: 아직 작성 예정
- Deprecated: 더 이상 사용하지 않음
- Superseded: 다른 문서로 대체됨

## 5. 문서 작성 규칙

1. 문서 경로는 `docs/...` 기준으로 쓴다.
2. 구현 전에는 관련 PVD/SRS/SPEC/MDS를 먼저 확인한다.
3. 안전 계약 문서는 코드보다 우선한다.
4. Performance 문서는 Policy 문서보다 상위 판단 기준이다.
5. Module Design 문서는 구현 직전 계약이다.
6. Release 문서는 배포 승인 기준이다.
7. 기존 Release Gate 스크립트가 참조하는 문서는 이동 전 반드시 참조 경로를 확인한다.
8. 경로 변경으로 스크립트가 깨질 가능성이 있으면 이동하지 않고 `MIGRATION_NOTES.md`에 기록한다.

## 6. Active Documents

- `docs/Markdown_Collection_v3.1.md`
- `docs/vision/PVD_v1.0.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/GameProfileSpecification.md`
- `docs/performance/PolicySpecification.md`
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
- `docs/implementation/PatchPlan_Phase1.md`
- `docs/implementation/PatchPlan_Phase2.md`
- `docs/implementation/PatchPlan_Phase3.md`
- `docs/implementation/PatchPlan_Phase4.md`
- `docs/implementation/PatchPlan_Phase5.md`
- `docs/implementation/PatchPlan_Phase6.md`
- `docs/implementation/PatchPlan_Phase7.md`
- `docs/implementation/PatchPlan_Phase8.md`
- `docs/implementation/PatchPlan_Phase9.md`
- `docs/implementation/PatchPlan_Phase10.md`

## 7. Planned Documents

- `docs/requirements/SRS_Rev1_3.md`
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

## 8. Migration Notes

문서 위치 정리 중 이동하지 않은 파일과 경로 충돌은 `MIGRATION_NOTES.md`에 기록한다.
