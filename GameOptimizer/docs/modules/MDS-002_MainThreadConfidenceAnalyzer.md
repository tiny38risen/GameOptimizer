> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# MDS-002 MainThreadConfidenceAnalyzer

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Proposal/partial
VerificationStatus: Static gate, regression, manual review
Owner: Decision layer

Applicable invariants:
INV-01, INV-08

## 1. Purpose

MainThreadConfidenceAnalyzer converts observation signals into confidence levels for main-thread policy eligibility. It does not apply policy and does not own target mutation.

## 2. Scope

- confidence scoring
- candidate thread ranking
- low-confidence rejection reasons
- evidence fields for confidence history

## 3. Non-goals

- thread pinning
- controller calls
- profile validation
- performance PASS claims

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Decision layer |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| ThreadTracker samples, runtime signals, topology hints | confidence level, candidate thread id when concrete, rejection reason | Collect signals -> score confidence -> classify -> emit evidence fields |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST NOT mutate target state | BLOCKER |
| MUST reject policy when confidence is insufficient | BLOCKER |
| MUST expose confidence evidence before policy handoff | BLOCKER |
| MUST NOT promote uncertain TID to concrete target | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| insufficient samples | return Low/Inconclusive | confidence evidence | WARN |
| conflicting signals | reject candidate | rejection reason | WARN |
| missing evidence | block policy success claim | evidence gap | BLOCKER |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| confidence unit tests | Low/High boundaries stable | test_results |
| policy handoff review | uncertain target rejected | manual/static review |

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