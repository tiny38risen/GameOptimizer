> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# MDS-009 RuntimeOrchestrator

Status: Active
Authority: Module implementation contract
TargetVersion: v3.0-rc1/v3.1
ImplementationStatus: Implemented/partial
VerificationStatus: Static gate, regression, manual review
Owner: Runtime lifecycle

Applicable invariants:
INV-03, INV-08, INV-10

## 1. Purpose

RuntimeOrchestrator owns the application runtime lifecycle and composes startup, watchdog cycles, shutdown, context, and signal state. It must stop new mutation during shutdown and delegate rollback/evidence responsibilities to the proper owners.

## 2. Scope

- startup sequencing
- RuntimeContext ownership
- RuntimeSignalState ownership
- watchdog cycle delegation
- shutdown sequencing
- timeout safe-point behavior

## 3. Non-goals

- ThreadTracker internals
- controller mutation details
- release schema generation
- policy scoring

## 4. Ownership

| Field | Value |
|---|---|
| Module owner | Runtime lifecycle |
| Mutation owner | See contracts; none unless explicitly stated |
| Runtime state touchpoint | docs/architecture/RuntimeStateMachine.md |
| Evidence consumer | Runtime validation and release evidence |

## 5. Interface / Workflow

| Input | Output | Workflow |
|---|---|---|
| argc/argv, runtime options, shutdown signals, watchdog callbacks | exit code, shutdown reason, final evidence path, runtime validation snapshots | Run startup -> start watchdog cycles -> monitor signals -> request shutdown -> execute shutdown pipeline -> return exit code |

## 6. Contracts

| Rule | Severity |
|---|---|
| MUST own RuntimeContext and RuntimeSignalState | BLOCKER |
| MUST delegate startup to StartupPipeline | BLOCKER |
| MUST delegate cycles to WatchdogCycleRunner | BLOCKER |
| MUST delegate shutdown to ShutdownPipeline with reason | BLOCKER |
| MUST block new mutation after shutdown request | BLOCKER |

## 7. Failure and Fallback

| Failure | Action | Evidence | Severity |
|---|---|---|---|
| startup failure | safe exit or FatalError | startup result | BLOCKER if release run |
| timeout | request shutdown at safe point | shutdown reason | WARN/BLOCKER |
| hard-timeout grace exceeded | force clean shutdown request | timeout log | WARN/BLOCKER |
| shutdown rollback failure | preserve and report | rollback summary | BLOCKER |

## 8. Verification

| Test/Gate | Expected | Evidence |
|---|---|---|
| runtime orchestrator static gate | delegation and ownership markers present | static gate output |
| timeout safe-point tests | shutdown reason and final snapshots | test_results |
| RC smoke | clean shutdown and evidence flush | rc evidence report |

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