> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# MDS-004 SchedulerController

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Implemented
VerificationStatus: Static gate, regression, manual review
Owner: Thread-level scheduling mutation

Applicable invariants:
INV-02, INV-03, INV-04, INV-05, INV-06, INV-07, INV-08

## 1. Purpose

SchedulerController is the only owner of thread-level scheduling mutation. It must save rollback state, arm ApplyGuard, apply, verify, and commit or rollback.

## 2. Scope

- thread priority apply
- thread affinity apply
- thread group affinity apply
- post-apply and post-failure audit
- thread rollback handoff

## 3. Non-goals

- process background restriction
- policy scoring
- ThreadTracker sampling
- evidence schema ownership

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Thread-level scheduling mutation |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| validated policy command, target identity, topology facts, rollback manager | apply result, verify result, rollback context evidence | Validate identity -> save state -> arm guard -> mutate -> verify -> commit or rollback |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST save state before mutation | BLOCKER |
| MUST arm ApplyGuard before mutation | BLOCKER |
| MUST verify before commit | BLOCKER |
| MUST audit failed affinity apply before discard | BLOCKER |
| MUST preserve rollback failure state | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| identity mismatch | rollback or preserve | identity evidence | BLOCKER |
| apply failure | rollback affected state | apply/rollback result | BLOCKER |
| audit proves no side effect | discard saved state with evidence | audit evidence | INFO/WARN |
| rollback failure | preserve failed state | rollback summary | BLOCKER |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| ApplyGuard transaction static gate | save/arm/apply/verify ordering | static gate output |
| affinity failure selftests | audit and rollback behavior preserved | test_results |

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