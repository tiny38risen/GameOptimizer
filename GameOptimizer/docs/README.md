# GameOptimizer Documentation Map

## Architecture

Authoritative development contract.

- `architecture/Architecture_Decision_Record.md`: accepted ADRs. New development must satisfy this first.
- `architecture/Contract_Enforcement_Matrix.md`: contract-to-gate/evidence enforcement map.
- `architecture/Source_Bands.md`: source-file ownership bands.

## Design

Feature and subsystem design notes.

- `design/ApplyGuardDesign.md`: transaction guard design.
- `design/ApplyGuardRegressionMatrix.md`: ApplyGuard regression expectations.
- `design/AntiCheatFallbackDesign.md`: Access Denied and anti-cheat fallback policy.
- `design/InputLatencyPhase2Design.md`: Raw Input and input pinning eligibility.
- `design/ProcessorGroupPhase2Design.md`: processor-group and HEDT safety boundary.
- `design/RuntimeValidationPlan.md`: runtime validation architecture.
- `design/IntegrationTestPlan_NextDesign.md`: integration test planning.

## Operations

Runbooks, release operation, validation order, and performance checks.

- `operations/DevelopmentRoadmap.md`: stabilization and future architecture direction.
- `operations/OperationalSafetyRunbook.md`: dry-run, soft-apply, apply, rollback, and timeout safety.
- `operations/RealGameValidationRunbook.md`: real game validation requirements.
- `operations/ReleaseGateRunbook.md`: RC gate execution.
- `operations/ReleasePerformanceChecklist.md`: release performance checks.
- `operations/ReleaseRegressionMatrix.md`: regression scenarios and merge blockers.

## Release

Machine-checked release contracts and candidate data schemas.

- `release/Evidence_Schema.md`: RC evidence schema.
- `release/Release_Blocker_List.md`: authoritative blocker, WARN, and INFO list.
- `release/RC_Runbook.md`: concise RC runbook.
- `release/Known_Limitations.md`: accepted limitations.
- `release/Game_Verification_Matrix.example.json`: real game validation template.

## History

Historical context that is not a current source of policy.

- `history/Patch_History.md`: consolidated patch summaries grouped by function band.
- `history/Build_Manifest.md`: historical build/document manifest.
