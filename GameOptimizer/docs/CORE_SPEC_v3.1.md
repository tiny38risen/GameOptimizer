> Status: Superseded  
> 이 문서는 현재 활성 기준 문서로 대체되었습니다. 현재 기준은 `docs/DOCUMENT_REGISTER.md`를 확인합니다.

# GameOptimizer Core Spec v3.1

## 1. Purpose

GameOptimizer Core Spec is the entry point for the documentation set.
This document does not replace detailed requirements, design, runbook, or evidence schema documents.
It summarizes document responsibilities and the core safety rules needed to navigate the current v3.1 documentation.

## 2. Product Definition

GameOptimizer is a Windows tool that observes runtime interference, thread movement, and latency variation in game processes, and applies limited optimization policy only when safety conditions are satisfied.

- Target OS: Windows 10 / 11 x64.
- Language / toolchain: C++23, Win32 API, MSVC v143, Visual Studio 2022, `/std:c++latest`.
- Core goals: safe apply, rollback capability, and verifiable evidence.
- Performance improvement is not guaranteed.
- External SRS Rev 1.3 DOCX is an external reference, not canonical inside this repo.

## 3. Scope

### Core

- Thread observation.
- Main thread candidate tracking.
- Processor topology awareness.
- SchedulerController-owned affinity / priority mutation.
- ApplyGuard transaction.
- RollbackManager state preservation.
- Runtime validation.
- RC evidence and release blocker classification.

### Optional

- Background restriction.
- Timer resolution policy.
- Network latency monitoring.
- Real-game validation.

### Experimental / Proposal

- Aggressive Single-Core Mode.
- PerformanceEngine extension.
- GameProfile expansion.
- Self-Tuning.

## 4. Non-Goals

- DLL injection.
- Kernel driver.
- Anti-cheat bypass.
- Direct game memory patch.
- Forced socket mutation inside game process.
- Performance improvement guarantee.
- Broad apply without evidence.
- UI exists as a product surface, but UI behavior is specified separately in UI Spec.
- UI must not own runtime mutation.

## 5. Core Runtime Flow

```text
Observe
→ Analyze
→ Decide
→ Save
→ Arm
→ Apply
→ Verify
→ Commit or Rollback
→ Evidence
```

- Observe: collect runtime facts without mutation.
- Analyze: evaluate samples, identity, topology, confidence, and risk.
- Decide: produce an allowed policy candidate or monitoring-only rejection.
- Save: capture original state before mutation.
- Arm: bind an ApplyGuard to the saved rollback state.
- Apply: perform mutation only through the documented owner.
- Verify: compare expected and actual target state before success.
- Commit or Rollback: commit verified apply or roll back and preserve failed state.
- Evidence: record what happened before claiming success.

## 6. Module Responsibility Map

| Area | Owner | Responsibility | Must Not Own |
|---|---|---|---|
| Thread observation | ThreadTracker | Query and rank thread behavior as observation-only telemetry | Win32 mutation, rollback state, ApplyGuard |
| Topology | TopologyAnalyzer | Report CPU topology and processor group facts | Silent group 0 correction or mutation ownership |
| Thread scheduling mutation | SchedulerController | Own thread affinity, group affinity, and priority mutation | Observation-only sampling or UI behavior |
| Background restriction | BackgroundController | Own process-level background restriction where supported | Thread observation or group 1+ unsafe process-wide mutation |
| Rollback state | RollbackManager | Preserve and restore original runtime state | Policy decisions or observation sampling |
| Transaction cleanup | ApplyGuard | Guard save, apply, verify, commit, rollback cleanup | Creating rollback state or hiding rollback failure |
| Runtime orchestration | RuntimeOrchestrator | Own lifecycle, cycles, shutdown, and safe sequencing | Direct mutation outside controller ownership |
| Runtime validation | RuntimeValidationMonitor | Observe validation status and report BLOCKER evidence | Policy generation, rollback execution, scheduler mutation |
| Release evidence | release evidence scripts | Produce and validate RC evidence and bundle identity | Production runtime mutation or false PASS |
| UI | WPF/UI layer | Launch modes and present WARN/BLOCKER/PASS status | Direct SchedulerController calls or runtime mutation |

- UI must not call SchedulerController directly.
- ThreadTracker must not call mutation APIs.
- Performance / policy proposal documents must not be treated as implemented runtime owners.

## 7. Core Invariants

- INV-01 Observation-only modules must not perform runtime mutation.
- INV-02 Affinity / priority mutation must be owned by SchedulerController or the documented mutation owner.
- INV-03 Runtime mutation must follow Save → Arm → Apply → Verify → Commit or Rollback.
- INV-04 `std::expected` values must follow Assign → Check → Bind before consumption.
- INV-05 Target process/thread identity must be revalidated before apply or rollback.
- INV-06 Processor Group identity must be explicit; silent group 0 coercion is forbidden.
- INV-07 Rollback failure is a BLOCKER and failed rollback state must be preserved.
- INV-08 PASS or performance success must not be claimed without required Evidence.
- INV-09 Game memory patching, DLL injection, kernel patching, and anti-cheat bypass are forbidden.
- INV-10 After ShutdownRequested, new runtime mutation is forbidden.

## 8. Release Blockers

The following conditions block release candidate approval:

- Mutation before rollback state save.
- ApplyGuard bypass.
- Commit before verify.
- Rollback failure hidden as WARN or INFO.
- Failed rollback state discarded.
- Target identity mismatch during apply or rollback.
- ThreadTracker mutation.
- Processor Group group 0 hardcoding in group-aware paths.
- RuntimeValidation BLOCKER.
- Required Evidence missing.
- False PASS in release gate.
- Binary or commit identity mismatch in RC evidence.

## 9. Documentation Map

| Responsibility | Canonical Document |
|---|---|
| Requirements | SRS markdown is currently missing inside repo; external SRS Rev 1.3 DOCX is not canonical |
| Safety rules | `docs/contracts/Safety_Contract.md` |
| Runtime rules | `docs/contracts/Runtime_Contract.md` |
| Architecture | `docs/architecture/SAD_v1.0.md` |
| Ownership | `docs/architecture/Module_Ownership_Matrix.md` |
| ADR / release contracts | `docs/architecture/Architecture_Decision_Record.md` |
| Implementation order | `docs/implementation/ImplementationPlan_v3.1.md` |
| RC execution | `docs/release/RC_Runbook_v3.1.md` |
| Evidence schema | `docs/release/Evidence_Schema.md` |
| UI behavior | `GameOptimizer.UI/README.md`, future `docs/ui/UI_SPEC_v3.1.md` |
| Document lifecycle | `docs/DOCUMENT_REGISTER.md` |

## 10. Current Unresolved Items

- Canonical SRS Markdown is missing inside repo.
- External SRS Rev 1.3 DOCX is not canonical repo documentation.
- UI README remains outside GameOptimizer/docs.
- Root docs/architecture duplicates require reconciliation.
- Generated/data/report/matrix/catalog/history documents need verified generator paths.
- Archived PatchPlan documents may still contain source-of-truth wording.
- Proposal and design documents need explicit lifecycle headers.

## 11. References

- `docs/DOCUMENT_REGISTER.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/architecture/Architecture_Decision_Record.md`
- `docs/architecture/Module_Ownership_Matrix.md`
- `docs/architecture/SAD_v1.0.md`
- `docs/release/Evidence_Schema.md`
- `docs/release/RC_Runbook_v3.1.md`
- `docs/release/Release_Blocker_List.md`
- `GameOptimizer.UI/README.md`
