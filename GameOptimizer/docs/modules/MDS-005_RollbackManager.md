> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# MDS-005 RollbackManager

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Implemented
VerificationStatus: Static gate, regression, manual review
Owner: Rollback state

Applicable invariants:
INV-03, INV-05, INV-07, INV-08, INV-10

## 1. Purpose

RollbackManager owns saved rollback state and recovery outcomes. It preserves failed state for release evidence and must not downgrade rollback failure severity.

## 2. Scope

- save validated state
- restore saved state
- preserved-state accounting
- shutdown rollback transfer evidence

## 3. Non-goals

- policy selection
- observation sampling
- normal apply mutation
- release PASS generation

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Rollback state |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| validated current state, rollback scope, target identity, shutdown reason | saved state id, rollback result, preserved state count, failure evidence | Bind validated read -> save state -> restore on request -> preserve failure if restore fails |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST save only validated read state | BLOCKER |
| MUST revalidate identity during rollback | BLOCKER |
| MUST preserve failed rollback state | BLOCKER |
| MUST keep rollback failure BLOCKER | BLOCKER |
| MUST expose preserved state count | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| stale target | classify recoverable no-op | rollback evidence | WARN |
| living same identity restore failure | preserve state | rollback summary | BLOCKER |
| unsafe discard | block release | evidence selftest | BLOCKER |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| rollback manager tests | validated read and restore paths | test_results |
| release evidence selftest | rollback failures classify as BLOCKER | selftest output |

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