# Processor Group Phase-2 Design

## Goal

Processor Group Phase-2 makes the HEDT policy boundary explicit:

1. Thread-level `SetThreadGroupAffinity` remains supported through `SchedulerController`.
2. Process-wide `SetProcessAffinityMask` remains limited to processor group 0.
3. Group 1+ background restriction is a non-fatal monitoring-only limitation.
4. Topology fallback masks preserve the source processor group and record mask provenance.

## Safe Actions

1. Build main-thread scheduling masks with `processorGroup + KAFFINITY`.
2. Apply thread-level group affinity through `SchedulerController` and `ApplyGuard`.
3. Generate deterministic process-affinity fallback masks for group 1+ mock tests.
4. Save and log background rollback state with the processor group that authorized process-wide restriction.

## Blocked Actions

1. Do not call `SetProcessAffinityMask` for group 1+ policies.
2. Do not silently coerce group 1+ background restriction to group 0.
3. Do not drop processor group provenance from topology fallback or rollback logs.

## Release Gate Contract

The static gate must verify:

1. Background process-wide restriction is guarded by `supportsProcessWideRestrictionForGroup(policy.processorGroup)`.
2. `SetProcessAffinityMask` appears only after the processor-group guard in the background apply path.
3. Group 1+ blocked summaries record the blocked processor group.
4. Rollback save and restore logs include group.
5. Topology fallback records `TopologyMaskProvenance::ProcessAffinityFallback`.
