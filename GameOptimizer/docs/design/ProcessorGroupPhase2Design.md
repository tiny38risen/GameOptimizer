> Status: Proposal  
> 이 문서는 제안 또는 미래 설계를 설명합니다. `docs/DOCUMENT_REGISTER.md`에서 승격되기 전까지 현재 구현 동작으로 간주하지 않습니다.

# Processor Group Phase-2 Design

## Goal

Processor Group Phase-2 makes the HEDT policy boundary explicit:

1. Thread-level `SetThreadGroupAffinity` remains supported through `SchedulerController`.
2. Process-wide `SetProcessAffinityMask` remains limited to processor group 0.
3. Group 1+ background restriction is a non-fatal monitoring-only limitation.
4. Topology fallback masks preserve the source processor group and record mask provenance.

This is an explicit safety limitation, not an incomplete implementation defect. Group 1+ process-wide background restriction must be reported as WARN/monitoring-only evidence and must not block RC by itself. Phase-2 does not apply priority-class-only background restriction for group 1+ processes because process affinity and process priority currently share one rollback state. Priority-only support requires a future split between affinity rollback and priority rollback ownership.

## Safe Actions

1. Build main-thread scheduling masks with `processorGroup + KAFFINITY`.
2. Apply thread-level group affinity through `SchedulerController` and `ApplyGuard`.
3. Generate deterministic process-affinity fallback masks for group 1+ mock tests.
4. Save and log background rollback state with the processor group that authorized process-wide restriction.
5. Keep background priority-class changes blocked for group 1+ until process affinity and priority rollback state are split.

## Blocked Actions

1. Do not call `SetProcessAffinityMask` for group 1+ policies.
2. Do not silently coerce group 1+ background restriction to group 0.
3. Do not drop processor group provenance from topology fallback or rollback logs.
4. Do not apply priority-class-only background restriction for group 1+ while it still shares rollback ownership with process affinity.

## Future Phase

Per-thread group-aware background restriction may be implemented later for HEDT and hybrid CPU systems. A separate priority-only rollback state may also be implemented later. Until those backends exist, group 1+ background process restriction remains LOW priority and must stay limited to evidence logging plus monitoring-only behavior.

## Release Gate Contract

The static gate must verify:

1. Background process-wide restriction is guarded by `supportsProcessWideRestrictionForGroup(policy.processorGroup)`.
2. `SetProcessAffinityMask` appears only after the processor-group guard in the background apply path.
3. Group 1+ blocked summaries record the blocked processor group.
4. Rollback save and restore logs include group.
5. Topology fallback records `TopologyMaskProvenance::ProcessAffinityFallback`.
6. Documentation classifies group 1+ process-wide background restriction as an explicit safety limitation and Future Phase item.
7. Runtime logs state that priority-class-only background restriction is blocked until affinity and priority rollback state are split.
