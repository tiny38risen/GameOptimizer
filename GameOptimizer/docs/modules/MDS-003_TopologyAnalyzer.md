> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# MDS-003 TopologyAnalyzer

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Implemented/partial
VerificationStatus: Static gate, regression, manual review
Owner: Topology observation

Applicable invariants:
INV-01, INV-06, INV-08

## 1. Purpose

TopologyAnalyzer records CPU topology, processor-group facts, and capability limits used by policy and controllers. It is an observation module and must keep processor-group behavior explicit.

## 2. Scope

- logical processor facts
- processor group detection
- SMT/CCX hints when available
- topology evidence for policy

## 3. Non-goals

- thread or process mutation
- group 0 correction
- policy activation
- rollback

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Topology observation |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| Windows topology APIs, CPU/group information | topology summary, processor group mode, capability flags | Query topology -> normalize facts -> expose explicit group/capability summary |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST NOT mutate target state | BLOCKER |
| MUST NOT force group 0 when group data is missing | BLOCKER |
| MUST record unsupported topology as evidence | BLOCKER |
| MUST keep group 1+ restrictions explicit | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| topology query failure | degrade to monitoring-only policy | topology summary | WARN |
| group data missing | block group-sensitive mutation | processor group evidence | BLOCKER |
| unsupported group path | monitoring-only | background restriction mode | WARN |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| processor group evidence tests | group 1+ remains explicit | test_results |
| static C++ bit gate | popcount/countr_zero usage preserved | static gate output |

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