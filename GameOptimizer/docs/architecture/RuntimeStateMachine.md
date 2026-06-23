> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# GameOptimizer v3.1 Runtime State Machine

Status: Active
Authority: Runtime architecture contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Partial implementation
VerificationStatus: Static gate, runtime evidence, manual review
Owner: Runtime architecture

Applicable invariants:
INV-02, INV-03, INV-05, INV-07, INV-08, INV-10

## 1. Purpose

This document defines the runtime states that constrain policy application, rollback, shutdown, and release evidence.
It is a compact state contract, not a C++ enum freeze.

## 2. Scope

- Runtime state names and meanings
- Allowed mutation boundaries
- Transition rules
- Failure and fallback behavior
- Evidence required for release decisions

## 3. Non-goals

- C++ enum finalization
- UI state model
- Logger field freeze beyond release evidence needs
- Release script implementation
- Full test case listing

## 4. State Groups

| Group | States | Mutation |
|---|---|---|
| Startup | Uninitialized, Initializing | none |
| Observation | Observing, Evaluating | none |
| Policy | PolicyPending, PolicyRejected | none |
| Mutation | Applying, Verifying, Running | guarded only |
| Fallback | MonitoringOnly | none |
| Recovery | RollbackPending, RollbackFailedPreserved | rollback only |
| Shutdown | ShutdownRequested, ShuttingDown | no new mutation |
| Terminal | Terminated, FatalError | none |

## 5. State Definitions

| State | Purpose | Entry | Allowed | Forbidden | Exit |
|---|---|---|---|---|---|
| Uninitialized | object exists but runtime is not ready | process start | parse config | sampling, policy, mutation | Initializing or FatalError |
| Initializing | discover target, topology, evidence session | startup ready | discovery, topology, context setup | policy apply | Observing, MonitoringOnly, FatalError |
| Observing | collect runtime facts | init success or rollback success | sampling, telemetry, validation | Win32 mutation | Evaluating, MonitoringOnly, ShutdownRequested |
| Evaluating | analyze observations | enough samples | confidence and candidate calculation | controller calls | PolicyPending, PolicyRejected, MonitoringOnly |
| PolicyPending | validate handoff | candidate exists | risk, cooldown, rollback scope validation | mutation before validation | Applying or PolicyRejected |
| PolicyRejected | record denied policy | failed validation | rejection evidence | partial apply, silent retry | Observing or MonitoringOnly |
| Applying | owner controller performs guarded mutation | validated policy | save, arm, apply | mutation before save/arm | Verifying or RollbackPending |
| Verifying | confirm applied state | apply success | query and compare state | commit without verify | Running or RollbackPending |
| Running | monitor active policy | verify success | collect evidence, watch deactivation | new mutation without cycle | Observing, Evaluating, RollbackPending |
| MonitoringOnly | continue observation without mutation | weak confidence/topology/access | observation and evidence | aggressive/apply/background mutation | Observing, ShutdownRequested, Terminated |
| RollbackPending | restore affected state | apply/verify/runtime/shutdown failure | rollback, preserve state | new policy apply | Observing, ShuttingDown, RollbackFailedPreserved |
| RollbackFailedPreserved | failed rollback state is retained | rollback failure with state | preserve, report, retry planning | success exit, state discard | ShuttingDown or FatalError |
| ShutdownRequested | shutdown signal received | user, timeout, target exit, fatal path | block new policy, check active mutation | new policy apply | RollbackPending or ShuttingDown |
| ShuttingDown | final rollback/evidence flush | shutdown path | final snapshot, flush, stop | new cycle or mutation | Terminated or FatalError |
| Terminated | runtime ended | clean shutdown or final fatal path | return code, final path print | sampling, apply, rollback retry | none |
| FatalError | cannot safely continue | fatal init, rollback, evidence, contract failure | fatal evidence best effort | retry policy, normal PASS | Terminated |

## 6. Primary Workflow

```text
Uninitialized
 -> Initializing
 -> Observing
 -> Evaluating
 -> PolicyPending
 -> Applying
 -> Verifying
 -> Running
 -> RollbackPending when failure or deactivation requires restore
 -> ShutdownRequested when runtime must stop
 -> ShuttingDown
 -> Terminated
```

## 7. Transition Rules

| From | Event | To | Required Action | Evidence |
|---|---|---|---|---|
| Initializing | target unavailable | MonitoringOnly | block mutation | target discovery result |
| Observing | enough samples | Evaluating | analyze facts | sample summary |
| Observing | access denied repeated | MonitoringOnly | fallback | access failure count |
| Evaluating | valid candidate | PolicyPending | validate candidate | candidate reason |
| Evaluating | unsafe candidate | PolicyRejected | record rejection | rejection reason |
| PolicyPending | validation success | Applying | hand off to owner | validation result |
| PolicyPending | validation failure | PolicyRejected | reject | validation result |
| Applying | apply success | Verifying | query state | apply result |
| Applying | apply failure | RollbackPending | rollback affected scope | apply failure |
| Applying | identity mismatch | RollbackPending | rollback or preserve | target identity |
| Verifying | verify success | Running | commit guarded transaction | verify result |
| Verifying | verify failure | RollbackPending | rollback | mismatch reason |
| Running | deactivation | RollbackPending | rollback active policy | deactivation reason |
| Running | runtime BLOCKER | RollbackPending | rollback active policy | validation summary |
| Any nonterminal | shutdown requested | ShutdownRequested | block new mutation | shutdown reason |
| ShutdownRequested | active mutation | RollbackPending | rollback | active policy count |
| ShutdownRequested | no active mutation | ShuttingDown | flush | shutdown reason |
| RollbackPending | rollback success and continue | Observing | resume observation | rollback result |
| RollbackPending | rollback success and stop | ShuttingDown | flush | rollback result |
| RollbackPending | rollback failed preserved | RollbackFailedPreserved | preserve state | preserved count |
| RollbackFailedPreserved | retry/flush | ShuttingDown | retry or report | retry eligibility |
| ShuttingDown | flush complete | Terminated | return exit | final status |
| FatalError | final evidence attempted | Terminated | return failure | fatal reason |

## 8. Mutation Permission Matrix

| State | Candidate | New Mutation | Rollback | Evidence |
|---|---:|---:|---:|---:|
| Uninitialized | No | No | No | Limited |
| Initializing | No | No | No | Yes |
| Observing | No | No | No | Yes |
| Evaluating | Yes | No | No | Yes |
| PolicyPending | No | No | No | Yes |
| PolicyRejected | No | No | No | Yes |
| Applying | No | Yes, guarded | On fail | Yes |
| Verifying | No | No | On fail | Yes |
| Running | Conditional | No direct mutation | On deactivation | Yes |
| MonitoringOnly | No | No | No | Yes |
| RollbackPending | No | Rollback only | Yes | Yes |
| RollbackFailedPreserved | No | Rollback retry only | Yes | Yes |
| ShutdownRequested | No | No new mutation | Conditional | Yes |
| ShuttingDown | No | Rollback retry only | Conditional | Yes |
| Terminated | No | No | No | No |
| FatalError | No | No new mutation | Best effort | Best effort |

## 9. Runtime Contracts

| ID | Rule | Severity |
|---|---|---|
| RSM-C01 | Only Applying may perform new guarded mutation. | BLOCKER |
| RSM-C02 | Applying requires rollback state and an armed ApplyGuard before mutation. | BLOCKER |
| RSM-C03 | Verifying must not commit success without state evidence. | BLOCKER |
| RSM-C04 | RuntimeValidation BLOCKER must stop new policy application. | BLOCKER |
| RSM-C05 | ShutdownRequested must block new mutation. | BLOCKER |
| RSM-C06 | Rollback failure must transition to preserved state or FatalError. | BLOCKER |
| RSM-C07 | MonitoringOnly is safe fallback, not hidden PASS. | WARN/BLOCKER |
| RSM-C08 | Evidence missing for a success claim blocks release PASS. | BLOCKER |

## 10. Monitoring-only Conditions

- Low or inconclusive main-thread confidence
- Incomplete topology or unsupported processor-group path
- Repeated access denied with safe fallback
- Anti-cheat boundary suspicion
- Missing rollback scope or evidence fields
- Unknown or candidate profile requiring observation only

## 11. Shutdown Rules

1. Record shutdown reason.
2. Stop new candidate generation.
3. Wait for watchdog safe point when available.
4. If active mutation exists, enter RollbackPending.
5. Flush final snapshot and runtime validation summary.
6. End in Terminated or FatalError.

## 12. Evidence Requirements

| Field | Purpose |
|---|---|
| stateBefore/stateAfter | transition traceability |
| transitionReason | why the state changed |
| policyType | affected policy when present |
| targetIdentity | stale/mismatch decisions |
| runtimeValidationState | release severity |
| rollbackRequired | recovery path |
| rollbackResult | restored or preserved outcome |
| monitoringOnlyReason | fallback explanation |
| shutdownReason | final runtime cause |
| fatalReason | unrecoverable condition |
| timestamp/cycleId | ordering |

## 13. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| access denied | MonitoringOnly or rollback | access summary | WARN if contained |
| target exited | ShutdownRequested | target identity | INFO/WARN |
| identity mismatch | RollbackPending | identity result | BLOCKER if mutation escaped |
| apply failure | RollbackPending | apply result | BLOCKER if unresolved |
| verify failure | RollbackPending | mismatch | BLOCKER |
| rollback failure | RollbackFailedPreserved | preserved state | BLOCKER |
| evidence fatal | FatalError or invalid run | write result | BLOCKER/FATAL |
| runtime BLOCKER | RollbackPending or policy block | validation summary | BLOCKER |

## 14. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| static release gate | no ownership drift | PASS output |
| evidence self-test | BLOCKER/WARN behavior preserved | PASS output |
| runtime smoke | final state reaches clean shutdown | logs and reports |
| RC gate | no BLOCKER and complete bundle | RC artifacts |
| manual review | state rules match SAD/MDS | review notes |

## 15. Open Decisions

| Decision | Blocking? |
|---|---|
| Concrete C++ enum for all states | No |
| RuntimeStateController split from RuntimeOrchestrator | No |
| Evidence recorder ownership for transitions | Yes before v3.1 schema freeze |
| FatalError exit code mapping | Yes before v3.1 RC |
| MonitoringOnly recovery strictness | No |

## 16. References

- `docs/architecture/SAD_v1.0.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Evidence_Schema.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/modules/MDS-001_ThreadTracker.md` through `docs/modules/MDS-009_RuntimeOrchestrator.md`