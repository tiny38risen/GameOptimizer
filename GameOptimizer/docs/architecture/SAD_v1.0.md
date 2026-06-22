# GameOptimizer v3.1 Software Architecture Design

Status: Active
Authority: Architecture contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Partial implementation
VerificationStatus: Static gate, runtime evidence, manual review
Owner: Architecture

Applicable invariants:
INV-01, INV-02, INV-03, INV-04, INV-05, INV-06, INV-07, INV-08, INV-09, INV-10

## 1. Purpose

This document defines the runtime architecture boundaries for GameOptimizer.
It explains which layer may observe, decide, dispatch, mutate, roll back, and produce release evidence.
It is a compact architecture contract, not a full implementation guide.

## 2. Scope

- Runtime ownership and layer boundaries
- Module responsibilities and mutation ownership
- Policy and evidence flow
- Runtime state integration
- Failure and fallback behavior

## 3. Non-goals

- C++ class signature freeze
- UI design
- Installer design
- Release gate script implementation
- Game-specific support certification

## 4. Source of Truth

| Area | Source |
|---|---|
| Product direction | `docs/vision/PVD_v1.0.md` |
| Runtime safety | `docs/contracts/Safety_Contract.md` |
| Runtime behavior | `docs/contracts/Runtime_Contract.md` |
| Release gate | `docs/release/Release_Gate_Spec.md` |
| RC evidence schema | `docs/release/Evidence_Schema.md` |
| Runtime states | `docs/architecture/RuntimeStateMachine.md` |
| Module details | `docs/modules/MDS-001` through `MDS-009` |
| Proposed v3.1 performance work | `docs/proposals/performance/` |

## 5. Architecture Goals

- Keep observation, decision, dispatch, mutation, rollback, and evidence separate.
- Keep all mutation behind module ownership, ApplyGuard, and RollbackManager.
- Prefer monitoring-only behavior when identity, topology, permission, or evidence is weak.
- Keep public user-mode Win32 boundaries.
- Preserve rollback state and release evidence on failures.
- Treat release PASS as evidence-driven, not impression-driven.

## 6. Layer Model

```text
CLI / App
  -> RuntimeOrchestrator
  -> StartupPipeline / RuntimeContext / RuntimeSignalState
  -> WatchdogCycleRunner
  -> Observation Layer
  -> Decision / Policy Layer
  -> PolicyDispatcher
  -> Mutation Controllers
  -> RollbackManager / ApplyGuard
  -> Evidence / RuntimeValidation / Release Gate
```

## 7. Layer Responsibilities

| Layer | Responsibility | Must Not |
|---|---|---|
| Application | parse CLI, assemble runtime, own lifecycle | mutate target state directly |
| Runtime | run cycles, safe points, shutdown signals | bypass rollback or evidence |
| Observation | collect process/thread/topology/latency data | apply policy |
| Decision | calculate confidence and candidates | call Win32 mutation APIs |
| Dispatch | validate policy handoff | become mutation owner |
| Mutation | perform owned Win32 changes | mutate without save/arm/verify |
| Rollback | preserve and restore state | discard failed rollback state |
| Evidence | record facts and release fields | mark PASS without evidence |

## 8. Module Ownership

| Module | Ownership | Implementation Status |
|---|---|---|
| `RuntimeOrchestrator` | runtime lifecycle and shutdown sequencing | implemented/partial |
| `StartupPipeline` | startup assembly and initial mutation boundary | implemented/partial |
| `ShutdownPipeline` | final rollback/evidence flush | implemented/partial |
| `ThreadTracker` | observation-only thread sampling | implemented |
| `MainThreadConfidenceAnalyzer` | proposed confidence scoring contract | proposal/partial |
| `TopologyAnalyzer` | CPU topology and processor group facts | implemented/partial |
| `SchedulerController` | thread-level scheduling mutation | implemented |
| `RollbackManager` | rollback state ownership | implemented |
| `BackgroundController` | process-level background restriction | implemented/partial |
| `PerformanceEvidencePlanner` | proposed evidence planning | proposal/partial |
| `PolicyDispatcher` | policy validation and routing | implemented/partial |
| `RuntimeValidationMonitor` | runtime safety summaries | implemented/partial |
| `release_gate_evidence.py` | RC evidence reports | implemented |
| `verify_rc_candidate.py` | RC candidate validation | implemented |
| `create_rc_evidence_bundle.py` | final RC bundle | implemented |

## 9. Mutation Boundary

| Mutation Type | Owner | Guard | Evidence |
|---|---|---|---|
| Thread affinity | `SchedulerController` | ApplyGuard + RollbackManager | apply, verify, rollback |
| Thread group affinity | `SchedulerController` | ApplyGuard + audit | group and identity evidence |
| Thread priority | `SchedulerController` | ApplyGuard + rollback state | priority evidence |
| Process background affinity | `BackgroundController` | ApplyGuard + processor-group guard | background mode evidence |
| Process priority | `BackgroundController` | ApplyGuard + rollback state | priority evidence |
| Timer resolution | owning controller path | rollback-capable path | timer evidence |
| Rollback mutation | `RollbackManager` plus owner controller | rollback-only | rollback summary |

## 10. Global Prohibitions

| ID | Rule | Severity |
|---|---|---|
| SAD-C01 | Observation-only modules MUST NOT mutate target state. | BLOCKER |
| SAD-C02 | Decision and dispatch layers MUST NOT call direct Win32 mutation APIs. | BLOCKER |
| SAD-C03 | Mutation MUST follow save, arm, apply, verify, commit or rollback. | BLOCKER |
| SAD-C04 | Target identity MUST be revalidated before success is claimed. | BLOCKER |
| SAD-C05 | Processor group fallback MUST NOT silently force group 0. | BLOCKER |
| SAD-C06 | Rollback failure MUST preserve failed state and remain BLOCKER. | BLOCKER |
| SAD-C07 | PASS MUST NOT be recorded without evidence. | BLOCKER |
| SAD-C08 | Anti-cheat bypass, injection, kernel driver, and memory patching are out of scope. | BLOCKER |
| SAD-C09 | ShutdownRequested MUST block new mutation. | BLOCKER |

## 11. Runtime Workflow

| Step | Producer | Consumer | Output |
|---|---|---|---|
| 1. Parse | CLI | StartupPipeline | validated options |
| 2. Discover | ProcessFinder | RuntimeContext | target identity |
| 3. Topology | TopologyAnalyzer | policy/runtime layers | group and CPU facts |
| 4. Observe | ThreadTracker, sensors | decision layer | samples and telemetry |
| 5. Decide | confidence/policy logic | PolicyDispatcher | candidate or rejection |
| 6. Dispatch | PolicyDispatcher | controllers | validated command |
| 7. Save | controller | RollbackManager | rollback state |
| 8. Guard | controller | ApplyGuard | armed transaction |
| 9. Apply | owner controller | target state | controlled mutation |
| 10. Verify | owner controller | ApplyGuard/evidence | success or rollback |
| 11. Monitor | RuntimeValidationMonitor | runtime loop | PASS/WARN/BLOCKER state |
| 12. Rollback | RollbackManager/controllers | target state | restored or preserved state |
| 13. Flush | evidence scripts/runtime | release gate | reports and bundle |

## 12. Runtime States

The detailed state contract is in `docs/architecture/RuntimeStateMachine.md`.
SAD owns only the architectural meaning of state groups.

| State Group | States | Architectural Meaning |
|---|---|---|
| Startup | Uninitialized, Initializing | assemble context, no policy mutation |
| Observation | Observing, Evaluating | collect and decide, no mutation |
| Policy | PolicyPending, PolicyRejected | validate or reject handoff |
| Mutation | Applying, Verifying, Running | guarded mutation and monitoring |
| Fallback | MonitoringOnly | observation continues, mutation blocked |
| Recovery | RollbackPending, RollbackFailedPreserved | restore or preserve state |
| Shutdown | ShutdownRequested, ShuttingDown | block new mutation and flush evidence |
| Terminal | Terminated, FatalError | exit with final evidence or failure |

## 13. Data Inputs

| Input | Required By | Notes |
|---|---|---|
| CLI options | RuntimeOrchestrator | target, mode, filters, limits |
| target process identity | controllers, rollback | PID plus identity facts |
| thread samples | confidence, evidence | observation-only |
| topology facts | policy and controllers | processor group explicitness required |
| background filter | BackgroundController | explicit apply boundary |
| latency and runtime signals | decision/evidence | observation and validation |
| previous rollback state | rollback path | cannot be fabricated |

## 14. Data Outputs

| Output | Consumer | Required Evidence |
|---|---|---|
| startup summary | release/debug | target and config facts |
| thread telemetry | validation/evidence | access/query/reset counts |
| topology summary | policy/evidence | group mode, capability |
| policy decision | dispatcher/evidence | reason, confidence, risk |
| apply result | evidence/validation | owner, target, result |
| verify result | evidence/guard | expected and actual state |
| rollback summary | release gate | restored/preserved state |
| runtime validation summary | release gate | BLOCKER count and status |
| RC evidence bundle | release decision | schema, hashes, reports |

## 15. Evidence Contract

| Evidence | Minimum Fields | Severity If Missing |
|---|---|---|
| identity | target, commit, binary hash | BLOCKER for RC |
| topology | processor group mode, CPU facts | WARN or BLOCKER by path |
| thread observation | sample count, failures, telemetry | WARN or BLOCKER by consumer |
| policy | candidate, rejection/apply reason | BLOCKER if success claimed |
| apply | owner, target, result | BLOCKER if mutation occurred |
| verify | expected, actual, result | BLOCKER if success claimed |
| rollback | saved state, result, preserved count | BLOCKER if missing after mutation |
| shutdown | reason, final status | BLOCKER for RC if missing |
| release bundle | schema, hashes, source reports | BLOCKER if invalid |

## 16. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| target missing | monitoring-only or invalid run | target discovery result | WARN/BLOCKER |
| access denied | fallback or monitoring-only | access boundary summary | WARN if contained |
| identity mismatch | stop apply and rollback/preserve | identity validation | BLOCKER if mutation escaped |
| topology incomplete | block unsafe policy | topology summary | WARN/BLOCKER |
| processor group 1+ process-wide restriction | monitoring-only | group mode | WARN if contained |
| apply failure | rollback affected scope | apply and rollback result | BLOCKER if not restored/preserved |
| verify failure | rollback affected scope | verify mismatch | BLOCKER |
| rollback failure | preserve state and block release | rollback summary | BLOCKER |
| runtime validation failure | stop new policy, rollback if active | runtime validation summary | BLOCKER |
| evidence write fatal | invalid run or release block | evidence completeness | BLOCKER/FATAL |
| shutdown requested | block new mutation, flush final evidence | shutdown reason | BLOCKER if bypassed |

## 17. Processor Group Policy

- Processor group is an explicit input to policy and evidence.
- Group 0 MUST NOT be used as a silent correction for missing group data.
- Thread-level group-aware mutation belongs to `SchedulerController`.
- Process-wide background affinity on group 1+ remains monitoring-only unless a dedicated safe backend exists.
- WARN is allowed only when the unsupported path is contained and evidenced.
- BLOCKER is required when unsupported topology is treated as success or mutation escapes.

## 18. Anti-Cheat and Access Boundary

- The architecture stays inside public user-mode Win32 APIs.
- It does not bypass anti-cheat, inject code, patch game memory, or install drivers.
- Access denied is not automatically a release blocker when fallback evidence exists.
- Access denied becomes BLOCKER if mutation escaped, rollback evidence is missing, or the runtime reports success without proof.

## 19. Release Gate Integration

| Gate | Architecture Dependency |
|---|---|
| static release gate | ownership and forbidden API boundaries |
| evidence self-test | severity and schema behavior |
| Release x64 build | compile-time contract preservation |
| full regression | safety and helper marker coverage |
| release smoke | runtime evidence generation |
| evidence bundle generation | current schema and source reports |
| verify-rc | final evidence-driven decision |

## 20. Implementation Status

| Area | Status | Notes |
|---|---|---|
| v3.0 safety gate | implemented | release scripts enforce key contracts |
| transactional mutation | implemented/partial | static and runtime evidence connected |
| observation-only ThreadTracker | implemented | static gate connected |
| processor group policy | implemented/partial | group 1+ background limitation remains |
| v3.1 performance proposals | proposal | under `docs/proposals/performance/` |
| full runtime state enum | not finalized | document-level state contract only |
| evidence schema v3.1 | proposal | current machine schema is v3.0 RC schema |
| GitHub Issues execution | active process | archived Patch Plans are historical |

## 21. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| `run_release_gate_static_checks.py` | PASS | static ownership and release markers |
| `run_release_gate_static_checks_selftest.py` | PASS | selftest proves gate failures |
| `release_gate_evidence_selftest.py` | PASS | severity behavior |
| `run_rc_gate.bat <target.exe>` | PASS before RC tag | step logs under `artifacts/rc/` |
| real-game validation | current evidence | matrix and report artifacts |
| manual architecture review | no ownership drift | this SAD plus MDS |

## 22. Open Decisions

| Decision | Blocking? | Owner |
|---|---|---|
| Whether to implement a concrete runtime state enum | No | Runtime architecture |
| Whether v3.1 performance proposal becomes active contract | Yes, before implementation | Product/architecture |
| Whether a group-aware process background backend exists | Yes, before enabling mutation | Architecture/controllers |
| Whether EvidenceSpecification becomes machine schema | Yes, before v3.1 RC | Release/evidence |
| GitHub Issue taxonomy for archived Patch Plan replacement | No | Engineering management |

## 23. Interface Map

| Interface | Producer | Consumer |
|---|---|---|
| `RuntimeContext` | StartupPipeline | RuntimeOrchestrator, controllers |
| `RuntimeSignalState` | RuntimeOrchestrator | WatchdogCycleRunner, ShutdownPipeline |
| thread samples | ThreadTracker | confidence and evidence layers |
| topology summary | TopologyAnalyzer | PolicyDispatcher, controllers |
| policy command | decision layer | PolicyDispatcher |
| controller result | SchedulerController/BackgroundController | evidence and runtime validation |
| rollback result | RollbackManager | shutdown and release gate |
| RC bundle manifest | create_rc_evidence_bundle.py | verify-rc and release review |

## 24. References

- `docs/vision/PVD_v1.0.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/architecture/RuntimeStateMachine.md`
- `docs/architecture/Module_Ownership_Matrix.md`
- `docs/architecture/Contract_Enforcement_Matrix.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/release/Evidence_Schema.md`
- `docs/modules/README.md`
