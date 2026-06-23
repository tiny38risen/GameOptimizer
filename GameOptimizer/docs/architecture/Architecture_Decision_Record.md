> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# GameOptimizer Architecture Decision Record

## Purpose

This document is the architecture contract index for GameOptimizer `v3.0-rc1`.
Each accepted ADR defines a runtime, release, or ownership rule that must be reflected in code, tests, static gates, and release evidence.

The ADRs below are not design notes. They are merge and release contracts.
If an implementation cannot satisfy an accepted ADR, the change must either update the ADR with a new accepted decision or remain blocked.

## Contract Language

- `MUST`: required for merge and release.
- `MUST NOT`: forbidden behavior.
- `WARN`: allowed limitation only when visible in evidence.
- `BLOCKER`: release candidate must not be tagged.
- `INFO`: tracking evidence only.

## Project Policy Alignment

GameOptimizer `v3.0-rc1` follows these project-wide operating policies:

- Non-invasive boundary: use public user-mode Windows APIs only; do not modify game memory, inject DLLs, patch drivers, patch the kernel, or patch anti-cheat.
- Conservative apply policy: default execution is soft-apply unless `--dry-run` or explicit `--apply` is passed.
- Evidence-before-mutation policy: dry-run and soft-apply evidence must precede limited apply-mode validation.
- Fallback-first policy: Access Denied, anti-cheat boundaries, unsupported IRQ paths, unavailable Raw Input, and group 1+ process-wide background restriction are WARN-only limitations when fallback or rollback evidence exists.
- BLOCKER-on-unsafe policy: missing fallback evidence, rollback failure, unsafe rollback-state discard, runtime validation failure, and identity/hash/schema mismatch block release.

## ADR Index

| ADR | Decision | Status | Primary enforcement |
|---|---|---|---|
| ADR-001 | Runtime mutation is transactional | Accepted | `ApplyGuard`, `RollbackManager`, static gate, release evidence |
| ADR-002 | `ThreadTracker` is observation-only | Accepted | static gate, runtime telemetry |
| ADR-003 | `SchedulerController` owns thread-level scheduling mutation | Accepted | static gate, regression tests |
| ADR-004 | Processor-group policy is explicit and group-aware | Accepted | HEDT tests, static gate, release evidence |
| ADR-005 | `SoftApply` is validation evidence, not rollback state | Accepted | rollback contract, evidence schema |
| ADR-006 | Release decisions are evidence-driven | Accepted | RC gate, evidence schema, blocker list |
| ADR-007 | `BackgroundController` owns process-level background restriction | Accepted | static gate, blocker list |
| ADR-008 | Access-boundary failures are fallback evidence unless mutation escaped | Accepted | anti-cheat fallback tests, static gate, evidence |
| ADR-009 | Input thread pinning requires high-confidence concrete TID evidence | Accepted | timer/input tests, known limitations, release gate |
| ADR-010 | Apply mode remains limited and explicit | Accepted | CLI policy, real-game validation, blocker list |

---

## ADR-001: Runtime Mutation Is Transactional

Status: Accepted

Owner: Runtime safety

Scope:
Thread affinity, thread priority, process affinity, and process priority mutations that are currently implemented by `SchedulerController` and `BackgroundController`.
Timer, network, and IRQ mutation backends remain outside this ADR until their original-state capture and rollback contracts are accepted.

Decision:
All supported runtime mutations MUST be guarded by an explicit query/save/apply/verify/commit-or-rollback transaction.

Rationale:
GameOptimizer modifies live Windows runtime state owned by external processes and threads.
Any partial mutation can leave the user system in a degraded state, especially when a target exits, access rights change, or anti-cheat boundaries deny handles.

Contract:
- Query the current state before mutation.
- Save rollback state before mutation.
- Create and arm an `ApplyGuard` before calling Win32 mutation APIs.
- Apply the Win32 mutation.
- Verify the post-apply state when the API supports verification.
- Call `ApplyGuard::commit()` only after verification succeeds.
- Call `ApplyGuard::rollbackNow()` on partial apply or verification failure.
- Preserve rollback state when rollback fails so shutdown can perform the final recovery attempt.

Forbidden:
- Calling Win32 mutation APIs before rollback state is saved.
- Discarding rollback state without an audit proving no mutation occurred.
- Treating rollback failure as `WARN` or `INFO`.
- Releasing an armed `ApplyGuard` without either commit, rollback, audited discard, or responsibility transfer.

Enforced by:
- `ApplyGuard.h`
- `ApplyGuard.cpp`
- `RollbackManager.h`
- `RollbackManager.cpp`
- `SchedulerController.cpp`
- `BackgroundController.cpp`
- `run_release_gate_static_checks.py`
- `docs/release/Release_Regression_Matrix.md`
- `docs/release/Evidence_Schema.md`
- `docs/release/Release_Blocker_List.md`

Review trigger:
Any new Win32 mutation API, any new rollback state type, or any change to `ApplyGuard` ownership semantics.

---

## ADR-002: ThreadTracker Is Observation-Only

Status: Accepted

Owner: Runtime observation

Scope:
`ThreadTracker` sampling, EMA ranking, stickiness, migration observation, candidate selection, and telemetry.

Decision:
`ThreadTracker` MUST observe thread behavior only.
It MUST NOT mutate thread or process scheduling state.

Rationale:
Observation and mutation in the same module create hidden side effects, ambiguous rollback ownership, and false confidence in release evidence.

Allowed:
- Enumerating process threads.
- Opening threads for query-only access.
- Calling `GetThreadTimes`.
- Tracking CPU deltas, EMA, stickiness, migration count, reset events, and candidate confidence.
- Emitting telemetry and warnings for access-boundary limitations.

Forbidden:
- `SetThreadAffinityMask`
- `SetThreadGroupAffinity`
- `SetThreadPriority`
- `SetPriorityClass`
- `SetProcessAffinityMask`
- `RollbackManager` mutation or rollback calls
- Any direct ownership of rollback state

Enforced by:
- `ThreadTracker.cpp`
- `ThreadTracker.h`
- `WatchdogCycleRunner.cpp`
- `RuntimeValidationMonitor.cpp`
- `run_release_gate_static_checks.py`

Review trigger:
Any change that adds handles with set rights, scheduling APIs, rollback ownership, or policy dispatch behavior to `ThreadTracker`.

---

## ADR-003: SchedulerController Owns Thread-Level Scheduling Mutation

Status: Accepted

Owner: Scheduling mutation

Scope:
Thread-level affinity and priority mutation.

Decision:
`SchedulerController` is the only module allowed to apply thread-level scheduling mutations.

Rationale:
Thread scheduling mutation requires a single owner for rollback state, `ApplyGuard` lifetime, post-apply verification, and access-boundary fallback.

Contract:
- `SchedulerController` MUST use `RollbackManager` and `ApplyGuard`.
- `SchedulerController` MUST follow Assign -> Check -> Bind for `std::expected` values.
- `SchedulerController` MUST save rollback state before `SetThreadGroupAffinity` or `SetThreadPriority`.
- `SchedulerController` MUST verify final thread group, affinity mask, and priority before commit.
- `SetThreadGroupAffinity` failure MUST run a post-failure audit before rollback state is discarded.
- Recoverable access limitations MUST be logged as monitoring-only fallback with rollback evidence when needed.

Forbidden:
- Thread-level scheduling mutation outside `SchedulerController`.
- Direct expected dereference outside the approved bind pattern.
- Committing an `ApplyGuard` before post-apply verification.

Enforced by:
- `SchedulerController.cpp`
- `SchedulerController.h`
- `run_release_gate_static_checks.py`
- `docs/release/Release_Regression_Matrix.md`

Review trigger:
Any new thread-level affinity, group, priority, QoS, or scheduling policy.

---

## ADR-004: Processor-Group Policy Is Explicit And Group-Aware

Status: Accepted

Owner: Processor topology

Scope:
Thread-level processor-group affinity and process-wide background restriction behavior on multi-group systems.

Decision:
Thread-level affinity MUST carry processor-group identity explicitly.
Process-wide background affinity restriction for group 1+ is not supported in `v3.0-rc1` and MUST remain monitoring-only.

Rationale:
`SetThreadGroupAffinity` can express a thread group and `KAFFINITY` mask.
`SetProcessAffinityMask` cannot safely express and restore arbitrary group 1+ process-wide affinity in the current rollback model.

Contract:
- `SchedulerPolicy` MUST carry both `processorGroup` and `affinityMask`.
- Thread-level apply MUST convert `SchedulerPolicy.affinityMask` to `KAFFINITY` and set `GROUP_AFFINITY.Group`.
- Group 0 hardcoding is forbidden for thread-level policy.
- `BackgroundController::supportsProcessWideRestrictionForGroup()` MUST block group 1+ process-wide background affinity restriction.
- Group 1+ background process-wide restriction MUST be recorded as `WARN` plus monitoring-only evidence.

Forbidden:
- Silent fallback from group 1+ to group 0.
- Process-wide group 1+ background affinity mutation in `v3.0-rc1`.
- Treating group 1+ background process-wide limitation as release success without WARN evidence.

Enforced by:
- `SchedulerController.cpp`
- `BackgroundController.cpp`
- `TopologyAnalyzer.cpp`
- `ProcessorGroupHedtEvidenceTests.cpp`
- `run_release_gate_static_checks.py`
- `docs/release/Evidence_Schema.md`
- `docs/release/Release_Blocker_List.md`

Review trigger:
Any processor topology change, HEDT/hybrid scheduling change, or new process-wide group-aware rollback strategy.

---

## ADR-005: SoftApply Is Validation Evidence, Not Rollback State

Status: Accepted

Owner: Apply mode policy

Scope:
`SchedulerMode::SoftApply` and soft-apply release evidence.

Decision:
`SoftApply` validates rights, query paths, and rollback baseline shape, but MUST NOT be treated as an applied rollback state.

Rationale:
Soft-apply exists to prove that runtime access and evidence paths are viable without mutating user runtime state.
Counting soft-apply baseline checks as applied rollback state would make shutdown evidence misleading.

Contract:
- Soft-apply baseline is `INFO` or `WARN` evidence.
- Soft-apply baseline MUST NOT increase applied rollback preserved-state count.
- Soft-apply baseline MUST NOT be restored by `rollbackAll()` as if mutation happened.
- Apply mode MUST remain blocked unless prior dry-run and soft-apply evidence satisfy the release policy.

Forbidden:
- Treating validated read state as applied state.
- Restoring soft-apply baseline during shutdown rollback.
- Using soft-apply success as permission for broad apply mode.

Enforced by:
- `RollbackManager.cpp`
- `SchedulerController.cpp`
- `BackgroundController.cpp`
- `docs/release/Evidence_Schema.md`
- `docs/release/Release_Blocker_List.md`
- `verify_real_game_validation.py`

Review trigger:
Any new soft-apply path, baseline validator, apply-mode admission rule, or rollback preserved-state metric.

---

## ADR-006: Release Decisions Are Evidence-Driven

Status: Accepted

Owner: Release gate

Scope:
RC candidate approval, release evidence, blocker classification, and tagging.

Decision:
A release candidate is valid only when the RC gate produces a complete evidence bundle for the current commit and no `BLOCKER` findings exist.

Rationale:
GameOptimizer changes live runtime scheduling behavior.
Release approval must be based on reproducible evidence rather than manual confidence.

Contract:
- Missing required evidence field is `BLOCKER`.
- Schema mismatch is `BLOCKER`.
- `schema_hash` mismatch is `BLOCKER`.
- Commit mismatch is `BLOCKER`.
- Binary hash mismatch is `BLOCKER`.
- Dirty tree field or status omission is `BLOCKER`.
- Runtime validation `FAILED` is `BLOCKER` and MUST pair with process exit code `1`.
- Rollback failure is `BLOCKER`.
- `ApplyGuard` rollback failure is `BLOCKER`.
- `SetThreadGroupAffinity` failure plus audit query failure or audit mismatch is `BLOCKER`.
- WARN-only limitations may pass only as `PASS_WITH_WARNINGS`.

Forbidden:
- Tagging `v3.0-rc1` without a current-commit evidence bundle.
- Treating missing evidence as inconclusive success.
- Releasing with preserved rollback state after shutdown.

Enforced by:
- `run_rc_gate.bat`
- `release_gate_evidence.py`
- `release_gate_evidence_selftest.py`
- `verify_rc_candidate.py`
- `create_rc_evidence_bundle.py`
- `docs/release/Evidence_Schema.md`
- `docs/release/Release_Blocker_List.md`

Review trigger:
Any schema field change, severity policy change, RC gate sequence change, or release-blocker classification change.

---

## ADR-007: BackgroundController Owns Process-Level Background Restriction

Status: Accepted

Owner: Background policy

Scope:
Process-level background affinity and priority restriction.

Decision:
`BackgroundController` is the only module allowed to apply process-level background restriction.

Rationale:
Background restriction touches unrelated user processes.
It needs explicit filtering, protected-process exclusions, access-boundary fallback, rollback ownership, and clear group 1+ limitations.

Contract:
- `BackgroundController` MUST skip protected and explicitly allowed processes.
- Apply mode MUST require explicit deny/restrict configuration before broad background restriction.
- `BackgroundController` MUST use `RollbackManager` and `ApplyGuard` for process-level apply.
- `BackgroundController` MUST save process rollback state before `SetProcessAffinityMask` or `SetPriorityClass`.
- Process affinity apply failure MUST run a post-failure audit before rollback state is discarded.
- Recoverable access limitations MUST remain WARN plus fallback evidence.
- Group 1+ process-wide background restriction MUST remain monitoring-only in `v3.0-rc1`.

Forbidden:
- Background restriction without protected-process filtering.
- Broad apply-mode background restriction without explicit deny/restrict configuration.
- Process-wide group 1+ affinity mutation in `v3.0-rc1`.
- Discarding process rollback state without post-failure audit.

Enforced by:
- `BackgroundController.cpp`
- `BackgroundController.h`
- `BackgroundControllerSafetyTests.cpp`
- `AntiCheatFallbackTests.cpp`
- `run_release_gate_static_checks.py`
- `docs/release/Release_Blocker_List.md`

Review trigger:
Any change to background process filtering, process-wide affinity policy, process priority policy, or apply-mode admission.

---

## ADR-008: Access-Boundary Failures Are Fallback Evidence Unless Mutation Escaped

Status: Accepted

Owner: Anti-cheat and access-boundary policy

Scope:
Win32 access denial, protected process/thread boundaries, anti-cheat blocked scheduling APIs, and rollback open failures caused by access boundaries.

Decision:
Access-boundary failures are recoverable WARN conditions only when no verified partial mutation escaped or when rollback/fallback evidence exists.
They become `BLOCKER` findings when the runtime cannot prove fallback, rollback, or no-mutation safety.

Rationale:
Anti-cheat and protected process boundaries are normal runtime conditions for a user-mode optimizer.
The product must stay non-invasive and must not escalate privileges, inject code, patch anti-cheat, patch drivers, or treat access denial as permission to continue unsafe mutation.

Contract:
- `ERROR_ACCESS_DENIED` MUST map to `ErrorCode::AccessDenied`.
- `SchedulerController` and `BackgroundController` MUST classify recoverable access limitations explicitly.
- Main-thread scheduling MUST remain monitoring-only when thread access is denied before mutation.
- Background restriction MUST skip inaccessible processes and continue scanning other eligible targets.
- Partial apply paths MUST invoke rollback before returning unless post-failure audit proves no mutation occurred.
- Access Denied is WARN-only when fallback or rollback evidence exists.

Forbidden:
- Escalating beyond public user-mode Windows APIs.
- Treating Access Denied as success without fallback evidence.
- Continuing apply mode after anti-cheat suspicion without explicit fallback evidence.
- Suppressing rollback evidence after a partial mutation path.

Enforced by:
- `docs/design/AntiCheatFallbackDesign.md`
- `AntiCheatFallbackTests.cpp`
- `SchedulerController.cpp`
- `BackgroundController.cpp`
- `RollbackManager.cpp`
- `docs/release/Known_Limitations.md`
- `docs/release/Release_Blocker_List.md`
- `run_release_gate_static_checks.py`

Review trigger:
Any change to access-right masks, protected process handling, access-denied mapping, anti-cheat fallback logging, or rollback failure classification.

---

## ADR-009: Input Thread Pinning Requires High-Confidence Concrete TID Evidence

Status: Accepted

Owner: Input latency policy

Scope:
Raw Input detection, input-thread attribution, and any future input-thread scheduling mutation.

Decision:
Input thread pinning MUST remain disabled unless Raw Input detection and High confidence `ConcreteTid` evidence both exist.
Low-confidence, missing, remote, or inferred input evidence is monitoring-only.

Rationale:
Pinning the wrong GUI, foreground, or message-loop thread can degrade input behavior and create misleading release evidence.
The current public Win32 path cannot prove remote game-process Raw Input usage with enough confidence for mutation.

Contract:
- Missing Raw Input detection MUST keep input policy monitoring-only.
- Missing concrete input TID MUST keep input policy monitoring-only.
- `InputThreadTidConfidence::High` is required before any input-thread pinning path can call scheduling mutation.
- Low-confidence input observations MUST be WARN plus evidence, not mutation.
- Future ETW or message-queue attribution paths MUST remain evidence-only until a new ADR accepts pinning eligibility.

Forbidden:
- Inferring an input thread from process ID alone.
- Pinning a GUI or foreground thread based only on window ownership.
- Claiming remote Raw Input usage through unsupported public Win32 inspection.
- Calling `SchedulerController` for input pinning before High confidence `ConcreteTid` evidence.

Enforced by:
- `docs/design/InputLatencyPhase2Design.md`
- `TimerResolutionController.cpp`
- `InputLatencyController.cpp`
- `TimerInputControllerTests.cpp`
- `docs/release/Known_Limitations.md`
- `docs/release/Release_Blocker_List.md`
- `run_release_gate_static_checks.py`

Review trigger:
Any new input attribution source, Raw Input detector, ETW integration, or input-thread scheduling policy.

---

## ADR-010: Apply Mode Remains Limited And Explicit

Status: Accepted

Owner: Apply mode policy

Scope:
CLI mode selection, background restriction admission, real-game validation, and release tagging.

Decision:
Apply mode MUST remain limited, explicit, evidence-gated, and conservative for `v3.0-rc1`.
Soft-apply remains the default operational posture unless `--dry-run` or explicit `--apply` is passed.

Rationale:
GameOptimizer operates on live external processes.
Broad mutation without prior evidence, explicit user configuration, and rollback confidence would conflict with the project safety model.

Contract:
- Default mode MUST be soft-apply.
- Apply mode MUST require explicit `--apply`.
- Broad background restriction in apply mode MUST require explicit deny/restrict configuration.
- Apply mode MUST be preceded by clean dry-run and soft-apply evidence for the same target class.
- Apply mode requires rollback state save success, sufficient `ThreadTracker` confidence, `ApplyGuard` audit PASS, and verified group-aware thread path.
- Real-game validation MUST document Access Denied/fallback, DPC/IRQ, Raw Input confidence, processor-group behavior, rollback evidence, and policy decision telemetry before RC tagging.

Forbidden:
- Aggressive self-tuning as default behavior in `v3.0-rc1`.
- Apply mode after anti-cheat suspicion, repeated Access Denied, rollback state save failure, Low Raw Input TID confidence, required group 1+ process-wide background restriction, or excessive soft-apply WARN findings.
- Broad background restriction without explicit user deny/restrict configuration.

Enforced by:
- `CliOptions.cpp`
- `StartupPipeline.cpp`
- `docs/release/Operational_Runbook.md`
- `docs/release/Real_Game_Validation_Runbook.md`
- `verify_real_game_validation.py`
- `docs/release/Known_Limitations.md`
- `docs/release/Release_Blocker_List.md`
- `run_release_gate_static_checks.py`

Review trigger:
Any CLI mode default change, apply-mode admission change, automatic tuning policy, real-game validation rule, or background restriction default.
