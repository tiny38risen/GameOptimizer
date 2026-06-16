# GameOptimizer v3.1 Markdown Collection

## 1. Purpose

이 문서는 2026-06-15 기준 `docs/` 하위 Markdown 문서를 한곳에서 추적하기 위한 컬렉션 인덱스다.

본문을 중복 복사하지 않고, 실제 기준 문서는 각 원본 파일에 둔다. 신규 Markdown 문서가 추가되면 이 파일과 `docs/README.md`를 함께 갱신한다.

## 2. Scope

- 기준 경로: `GameOptimizer/docs/`
- 문서 내부 참조 형식: `docs/...`
- 수집 대상: `docs/` 하위 Markdown 파일
- 수집 결과: Markdown 62개

## 3. v3.1 Core Authoring Set

v3.1 기준으로 새로 작성되었거나 직접 관리해야 하는 핵심 문서는 다음과 같다.

### 3.1 Architecture / Runtime

- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/performance/PolicySpecification.md`
- `docs/performance/GameProfileSpecification.md`
- `docs/evidence/EvidenceSpecification.md`
- `docs/validation/PerformanceValidationPlan.md`

### 3.2 Module Design Specifications

- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-005_RollbackManager.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/modules/MDS-009_RuntimeOrchestrator.md`

### 3.3 Implementation Plan / Patch Plans

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

### 3.4 Release / RC

- `docs/release/ReleaseChecklist_v3.1.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/release/RC_Report_Template_v3.1.md`

## 4. Full Markdown Catalog

### 4.1 Root

- `docs/README.md`
- `docs/Markdown_Collection_v3.1.md`

### 4.2 Architecture

- `docs/architecture/Architecture_Decision_Record.md`
- `docs/architecture/Contract_Enforcement_Matrix.md`
- `docs/architecture/Contract_Enforcement_Status.md`
- `docs/architecture/Module_Ownership_Matrix.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/architecture/Source_Bands.md`

### 4.3 Contracts

- `docs/contracts/Runtime_Contract.md`
- `docs/contracts/Safety_Contract.md`

### 4.4 Design

- `docs/design/AntiCheatFallbackDesign.md`
- `docs/design/ApplyGuardRegressionMatrix.md`
- `docs/design/InputLatencyPhase2Design.md`
- `docs/design/IntegrationTestPlan_NextDesign.md`
- `docs/design/ProcessorGroupPhase2Design.md`

### 4.5 Engineering

- `docs/engineering/Engineering_Handbook.md`

### 4.6 Evidence

- `docs/evidence/EvidenceSpecification.md`

### 4.7 Governance

- `docs/governance/Development_Roadmap.md`
- `docs/governance/README.md`

### 4.8 History

- `docs/history/Build_Manifest.md`
- `docs/history/Patch_History.md`

### 4.9 Implementation

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

### 4.10 Modules

- `docs/modules/README.md`
- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-005_RollbackManager.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/modules/MDS-009_RuntimeOrchestrator.md`

### 4.11 Performance

- `docs/performance/GameProfileSpecification.md`
- `docs/performance/PerformanceEngineSpec.md`
- `docs/performance/PolicySpecification.md`

### 4.12 Release

- `docs/release/Evidence_Schema.md`
- `docs/release/Game_Verification_Matrix.md`
- `docs/release/Known_Limitations.md`
- `docs/release/Operational_Runbook.md`
- `docs/release/RC_Report_Template_v3.1.md`
- `docs/release/RC_Runbook.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/release/Real_Game_Validation_Runbook.md`
- `docs/release/Release_Blocker_List.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/release/Release_Performance_Checklist.md`
- `docs/release/Release_Regression_Matrix.md`
- `docs/release/ReleaseChecklist_v3.1.md`

### 4.13 Validation

- `docs/validation/PerformanceValidationPlan.md`

### 4.14 Vision

- `docs/vision/PVD_v1.0.md`

## 5. Maintenance Rule

1. 새 Markdown 문서는 실제 소유 디렉터리에 먼저 생성한다.
2. `docs/README.md`의 Active 또는 Planned 상태를 갱신한다.
3. 이 컬렉션의 Full Markdown Catalog에 동일한 `docs/...` 경로로 추가한다.
4. v3.1 승인/릴리즈와 직접 관련된 문서는 Core Authoring Set에도 추가한다.
