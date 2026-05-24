# GameOptimizer Runtime Safety & Release Governance

## Purpose

This directory names the complete governance document system for GameOptimizer.
The Korean name is `GameOptimizer 거버넌스 문서 체계`.

Governance documents are the source of truth for decisions, contracts, validation, evidence, and release approval.
Code must satisfy these documents. When code and governance conflict, code changes are rejected unless the governance document is intentionally updated first.

## Document System

| Document | Role |
|---|---|
| `docs/architecture/Architecture_Decision_Record.md` | Architecture Decision Record: records why the architecture decisions exist. |
| `docs/engineering/Engineering_Handbook.md` | Defines coding and review rules. |
| `docs/contracts/Safety_Contract.md` | Defines safety contracts that must not be broken. |
| `docs/contracts/Runtime_Contract.md` | Defines runtime state flow, watchdog, validation, and shutdown contracts. |
| `docs/release/Release_Gate_Spec.md` | Defines release pass/fail gates. |
| `docs/release/Operational_Runbook.md` | Defines execution and validation procedure. |
| `docs/release/Known_Limitations.md` | Defines intentionally unsupported scope. |
| `docs/architecture/Contract_Enforcement_Matrix.md` | Connects contracts to static gates, runtime validation, and evidence. |

## Enforcement Chain

```text
Governance documents
-> Contract Enforcement Matrix
-> static gate
-> runtime validation
-> evidence
-> RC gate
-> release decision
```

The review question is not "is this good code?"
The review question is "does this violate the governance contract?"

## Status

- Architecture phase: started.
- Contract enforcement phase: in progress.
- Release-governed architecture phase: not complete until every governance contract is consumed by static gate, runtime validation, evidence, and `run_rc_gate.bat`.
