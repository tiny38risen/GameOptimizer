# GameOptimizer Runtime Safety & Release Governance

## Purpose

This is the documentation map for the GameOptimizer governance document system.
The Korean name is `GameOptimizer 거버넌스 문서 체계`.

The governance system controls code decisions, safety contracts, runtime validation, evidence, and release approval.

## Governance

- `governance/README.md`: system name, purpose, and enforcement chain.
- `governance/Development_Roadmap.md`: stabilization and future architecture direction.

## Architecture

- `architecture/Architecture_Decision_Record.md`: accepted ADRs.
- `architecture/Contract_Enforcement_Matrix.md`: contract-to-gate/evidence enforcement map.
- `architecture/Source_Bands.md`: source-file ownership bands.

## Engineering

- `engineering/Engineering_Handbook.md`: coding and review rules.

## Contracts

- `contracts/Safety_Contract.md`: safety contracts that must not be broken.
- `contracts/Runtime_Contract.md`: runtime state flow, watchdog, validation, and shutdown contracts.

## Release

- `release/Release_Gate_Spec.md`: release pass/fail gates.
- `release/Operational_Runbook.md`: execution and validation procedures.
- `release/Known_Limitations.md`: intentionally unsupported scope.
- `release/Evidence_Schema.md`: RC evidence schema.
- `release/Release_Blocker_List.md`: authoritative blocker, WARN, and INFO list.
- `release/RC_Runbook.md`: concise RC runbook.
- `release/Release_Regression_Matrix.md`: regression scenarios and merge blockers.
- `release/Release_Performance_Checklist.md`: release performance checks.
- `release/Real_Game_Validation_Runbook.md`: real-game validation requirements.
- `release/Game_Verification_Matrix.example.json`: real-game validation template.

## Design Notes

These are supporting notes. They do not override governance contracts.

- `design/AntiCheatFallbackDesign.md`
- `design/ApplyGuardRegressionMatrix.md`
- `design/InputLatencyPhase2Design.md`
- `design/IntegrationTestPlan_NextDesign.md`
- `design/ProcessorGroupPhase2Design.md`

## History

Historical context that is not a current source of policy.

- `history/Patch_History.md`
- `history/Build_Manifest.md`
