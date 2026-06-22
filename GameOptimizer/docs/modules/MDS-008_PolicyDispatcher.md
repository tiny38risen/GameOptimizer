# MDS-008 PolicyDispatcher

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Implemented/partial
VerificationStatus: Static gate, regression, manual review
Owner: Policy validation and routing

Applicable invariants:
INV-02, INV-03, INV-05, INV-08, INV-10

## 1. Purpose

PolicyDispatcher validates policy commands and routes them to the owning controller. It must not become a direct Win32 mutation owner.

## 2. Scope

- policy command validation
- risk/cooldown/scope checks
- controller routing
- rejection evidence
- shutdown block enforcement

## 3. Non-goals

- direct affinity/priority calls
- rollback state storage
- confidence scoring
- release PASS decisions

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Policy validation and routing |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| policy candidate/command, runtime state, target identity, topology facts | dispatch result, rejection reason, controller result, evidence fields | Validate runtime state -> validate target/scope -> choose owner -> call owner -> record result |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST NOT call direct Win32 mutation APIs | BLOCKER |
| MUST reject missing rollback scope | BLOCKER |
| MUST reject shutdown-time new mutation | BLOCKER |
| MUST route thread mutation only to SchedulerController | BLOCKER |
| MUST route process restriction only to BackgroundController | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| invalid runtime state | reject policy | rejection reason | WARN/BLOCKER |
| missing rollback scope | reject policy | dispatch evidence | BLOCKER |
| controller failure | propagate result | controller evidence | BLOCKER if unresolved |
| shutdown requested | reject new mutation | shutdown reason | BLOCKER if bypassed |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| policy dispatcher static gate | no direct mutation APIs | static gate output |
| dispatch regression | owners and rejection reasons preserved | test_results |

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