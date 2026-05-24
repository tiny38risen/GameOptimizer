# Input Latency Phase-2 Design

## Goal

Input Latency Phase-2 separates input-path detection from input-thread pinning eligibility.

Raw Input registration evidence alone is not enough to pin a thread. The controller must keep
pinning in monitoring-only mode until a concrete input thread ID is available with high confidence.

## Current Contract

1. Same-process Raw Input registration may be queried through `GetRegisteredRawInputDevices`.
2. Remote game-process Raw Input usage is not treated as confirmable through the current public Win32 path.
3. Input thread pinning requires:
   - Raw Input detection.
   - A non-zero concrete input TID.
   - `InputThreadTidConfidence::High`.
4. Low-confidence or missing TID evidence must log WARN and remain monitoring-only.

## TID Acquisition Candidates

1. ETW investigation path
   - Candidate source for high-confidence input dispatch attribution.
   - Not wired to pinning in this phase.

2. Foreground/message-queue observation path
   - Candidate source for exploratory diagnostics.
   - Must not be promoted to pinning eligibility without stronger evidence.

## Non-Goals

1. Do not infer a game input thread from process ID alone.
2. Do not pin a GUI or foreground thread based only on window ownership.
3. Do not claim remote Raw Input usage through unsupported public Win32 inspection.
4. Do not call SchedulerController for input pinning until the TID confidence is High.
