# MDS-001 ThreadTracker

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Implemented
VerificationStatus: Static gate, regression, manual review
Owner: Observation layer

Applicable invariants:
INV-01, INV-04, INV-08

## 1. Purpose

ThreadTracker observes target threads and produces thread samples, telemetry, and update disposition. It is observation-only and must never mutate thread or process state.

## 2. Scope

- Thread enumeration and sampling
- OpenThread/GetThreadTimes telemetry
- candidate eligibility flags
- reset disposition surfaced to runtime

## 3. Non-goals

- thread affinity or priority changes
- policy decisions
- rollback ownership
- release PASS decisions

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Observation layer |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| target process identity, sampling interval, stop token | thread samples, telemetry, update disposition, access/query failure counts | Enumerate -> sample -> mark eligibility -> record telemetry -> return disposition |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST remain observation-only | BLOCKER |
| MUST exclude failed samples from candidates | BLOCKER |
| MUST use checked expected handling | BLOCKER |
| MUST expose reset disposition to runtime | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| OpenThread failure | record telemetry and continue | telemetry summary | WARN |
| GetThreadTimes failure | exclude sample and continue | query failure count | WARN |
| invariant failure | return reset disposition | runtime validation | BLOCKER |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| static ThreadTracker gate | forbidden mutation APIs absent | static gate output |
| runtime telemetry regression | failures visible in summary | test_results |

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