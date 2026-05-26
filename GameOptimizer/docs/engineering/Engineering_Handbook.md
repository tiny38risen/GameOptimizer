# Engineering Handbook

## Purpose

This handbook defines coding and review rules for GameOptimizer.
It belongs to `GameOptimizer Runtime Safety & Release Governance`.

## Core Rule

Code must satisfy governance contracts before it can be merged or released.

When implementation convenience conflicts with:

- `Architecture_Decision_Record.md`
- `Contract_Enforcement_Matrix.md`
- `Safety_Contract.md`
- `Runtime_Contract.md`
- `Release_Gate_Spec.md`

the implementation must change, or the relevant governance document must be updated first.

## Expected Values

Production code must use Assign -> Check -> Bind for `std::expected`.

Forbidden in runtime code:

- direct unchecked dereference,
- unchecked `operator->`,
- `value()` as a shortcut,
- tail-return propagation that hides an expected value check.

Tests may use simpler assertions when the value is already part of the assertion.

## Mutation Boundaries

Win32 runtime mutation must stay in the owning module:

- thread-level scheduling mutation: `SchedulerController`
- process-level background restriction: `BackgroundController`
- rollback state ownership: `RollbackManager`
- transaction cleanup: `ApplyGuard`

Observation-only modules must not mutate runtime state.

## Review Checklist

- Does the change violate an ADR?
- Does the CEM define a static gate, runtime validation, and evidence field for the touched contract?
- Does the change preserve Assign -> Check -> Bind?
- Does any new mutation have rollback state before apply?
- Does every failure path produce BLOCKER/WARN/INFO evidence?
- Does apply mode remain explicit, limited, and evidence-gated?
- Does Access Denied remain fallback evidence instead of unsafe success?

## Atomic Governance Change Unit

Every development commit must preserve this unit:

```text
document contract 1
-> static gate 1
-> validator/evidence coupling 1
-> selftest or regression 1
-> document/blocker/runbook sync
-> verification
-> commit 1
```

Do not mix unrelated governance chains in one commit. A change may touch multiple files only when those files are required to complete the same contract chain.

Required commit evidence:

- the changed governance contract,
- the static gate that rejects drift from that contract,
- the validator or evidence field that proves runtime/release compliance,
- the selftest or regression command that exercised the gate,
- synchronized release blocker and runbook text.

## Documentation Rule

Feature notes may live outside governance documents, but they cannot override governance documents.
If a feature note conflicts with governance, governance wins.
