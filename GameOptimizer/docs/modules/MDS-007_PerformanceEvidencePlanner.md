> Status: Proposal  
> 이 문서는 제안 또는 미래 설계를 설명합니다. `docs/DOCUMENT_REGISTER.md`에서 승격되기 전까지 현재 구현 동작으로 간주하지 않습니다.

# MDS-007 PerformanceEvidencePlanner

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Proposal/partial
VerificationStatus: Static gate, regression, manual review
Owner: Evidence planning

Applicable invariants:
INV-08

## 1. Purpose

PerformanceEvidencePlanner defines which evidence is needed before a policy result may be interpreted. It plans evidence fields and detects gaps; it does not decide release PASS.

## 2. Scope

- before/after evidence requirements
- policy evidence field mapping
- evidence gap reporting
- classification input readiness

## 3. Non-goals

- Win32 mutation
- policy apply
- RC schema implementation
- performance marketing claims

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Evidence planning |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| policy type, confidence result, runtime state, rollback result, validation summary | evidence plan, required fields, gap list, classification readiness | Inspect policy/result -> derive required evidence -> compare available fields -> emit gaps |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST NOT mark PASS without required evidence | BLOCKER |
| MUST require rollback and runtime validation summaries for apply claims | BLOCKER |
| MUST distinguish missing optional fields from fatal gaps | WARN/BLOCKER |
| MUST keep RC schema authority in Evidence_Schema.md | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| missing required evidence | block success classification | gap list | BLOCKER |
| optional metric absent | allow warning only | gap list | WARN |
| schema mismatch | defer to release schema | schema evidence | BLOCKER |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| evidence completeness review | required fields mapped | manual/static review |
| release evidence selftest | schema and severity behavior preserved | selftest output |

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