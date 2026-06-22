# MDS-006 BackgroundController

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Implemented/partial
VerificationStatus: Static gate, regression, manual review
Owner: Process-level background restriction

Applicable invariants:
INV-02, INV-03, INV-05, INV-06, INV-07, INV-08, INV-09

## 1. Purpose

BackgroundController owns process-level background restriction and process priority/affinity rollback state. It must treat access-boundary and processor-group limitations as evidenced fallback, not hidden success.

## 2. Scope

- process priority restriction
- process affinity/background restriction
- access-boundary fallback
- processor-group monitoring-only limits
- process rollback baseline

## 3. Non-goals

- thread-level scheduling
- main-thread detection
- anti-cheat bypass
- group 1+ process-wide mutation without backend

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Process-level background restriction |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| validated background policy, process identity, topology facts, rollback manager | background restriction mode, apply result, fallback evidence, rollback result | Validate target/filter/topology -> save state -> arm guard -> apply supported restriction -> audit -> commit or rollback |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST save process state before mutation | BLOCKER |
| MUST arm ApplyGuard before mutation | BLOCKER |
| MUST block unsupported group 1+ process-wide mutation | BLOCKER |
| MUST record access-boundary fallback | BLOCKER |
| MUST preserve rollback failures | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| access denied | monitoring-only or rollback evidence | access summary | WARN if contained |
| group 1+ process-wide restriction | monitoring-only | processor group mode | WARN |
| apply failure with side effect | rollback | apply/rollback result | BLOCKER |
| rollback failure | preserve state | rollback summary | BLOCKER |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| background ApplyGuard gate | save/arm/apply/commit order | static gate output |
| anti-cheat fallback tests | access boundary evidence preserved | test_results |
| processor group tests | group 1+ limitation remains WARN-only | test_results |

## 9. Open Decisions

| Decision | Blocking? |
|---|---|
| Additional implementation detail beyond this compact MDS | No, unless it changes ownership or severity |
| New mutation path or release-facing severity change | Yes, requires contract and gate update |

## 10. References

- docs/architecture/SAD_v1.0.md
- docs/architecture/RuntimeStateMachine.md
- docs/contracts/Safety_Contract.md
- docs/contracts/Runtime_Contract.md
- docs/architecture/Module_Ownership_Matrix.md
- docs/architecture/Contract_Enforcement_Matrix.md