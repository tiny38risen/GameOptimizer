# GameOptimizer v3.0-rc1 Runbook

## Single entry point

Run:

```bat
run_rc_gate.bat <target.exe>
```

This command is the only RC gate entry point.

Required order:

1. `git diff --check`
2. Python `py_compile`
3. static release gate
4. evidence self-test
5. Release x64 MSVC build
6. full regression
7. release smoke

The static release gate verifies this draft order through the `[RC-1]` through `[RC-7]` markers in `run_rc_gate.bat`; within `[RC-3]`, `run_release_gate_static_checks_selftest.py` must run before `run_release_gate_static_checks.py`.

Draft exclusions:

- 30m dry-run soak
- 60m soft-apply soak
- `verify-rc`
- real-game validation
- RC candidate verification
- final evidence bundle generation

The excluded steps remain required before tagging `v3.0-rc1`; they are intentionally outside the repeatable draft entry point.

## Completion criteria

- `BLOCKER` count is `0`.
- binary SHA-256 exists.
- commit SHA exists.
- schema version matches.
- schema hash exists.
- dirty tree state is recorded.
- shutdown reason is recorded.
- rollback preserved state count is recorded.
- smoke reports match the current commit and binary hash.
- final regression reports `failed=0`.
- static gate selftest exercises final bundle validators with real temporary files and mismatched-file failures.
- full regression runs `run_release_gate_static_checks_selftest.py`.
- final regression log contains PASS markers for `run_release_gate_static_checks_selftest.py` and `release_gate_evidence_selftest.py`.
- step logs are saved under `artifacts/rc/<timestamp>/`.
- text bundle manifest includes the regression selftest summary and both selftest keys.
- every real-game validation run links to an existing `evidence_report` artifact.
- `docs/architecture/Architecture_Decision_Record.md` exists and matches the accepted runtime mutation, fallback, input pinning, limited apply, and evidence contracts.
- Apply mode is limited, explicit, and allowed only after dry-run PASS plus soft-apply PASS evidence.
- each development commit preserves the atomic governance change unit: document contract 1 -> static gate 1 -> validator/evidence coupling 1 -> selftest or regression 1 -> document/blocker/runbook sync -> verification -> commit 1.
- verification notes identify the static gate and validator/selftest or regression evidence used for the committed contract chain.
- `SchedulerController` affinity failure handling remains split into explicit audit, result, and rollback-log helpers.
- `StartupPipeline::run()` remains split into core runtime component assembly, startup mutation, runtime service assembly, and runtime sensor activation helpers.
- `StartupPipeline::prepare()` remains split into startup policy log, main-thread policy, background filter, and background policy helpers.
- `release_gate_evidence_selftest.py` covers ApplyGuard explicit rollback failure with and without shutdown-transfer evidence, plus destructor rollback failure classification.
- `release_gate_evidence_selftest.py` proves SchedulerController affinity rollback context logging does not duplicate ApplyGuard rollback BLOCKER evidence.
- `release_gate_evidence_selftest.py` proves audited affinity no-side-effect discard remains non-BLOCKER evidence.
- `run_release_gate_static_checks.py` verifies ApplyGuard rollback markers remain under `BLOCKER`, not `WARN`.
- `release_gate_evidence_selftest.py` proves SoftApply baseline evidence does not increase rollback preserved-state count.
- `check_contract_enforcement_matrix` requires the recent helper/evidence selftest markers from CEM.
- `Release_Blocker_List.md` classifies SoftApply baseline preserved-state/severity drift as BLOCKER, not WARN.
- `Release_Blocker_List.md` classifies Processor Group 1+ monitoring-only background restriction as WARN, not BLOCKER.
- `Release_Blocker_List.md` classifies Access Denied with fallback evidence as WARN, not BLOCKER.
- `Release_Blocker_List.md` classifies unsupported IRQ affinity with monitoring-only evidence as WARN, not BLOCKER.
- `Release_Blocker_List.md` classifies missing Raw Input detection with fallback input policy as WARN, not BLOCKER.
- `Release_Blocker_List.md` classifies remote Raw Input detection unsupported through public Win32 APIs as WARN, not BLOCKER.
- `Release_Blocker_List.md` classifies DPC spike observation while IRQ mutation backend is unavailable as WARN, not BLOCKER.
- `Release_Blocker_List.md` classifies SoftApply baseline validation limitation without mutation as WARN, not BLOCKER.
- `run_release_gate_static_checks_selftest.py` iterates `WARN_ONLY_RELEASE_BLOCKER_MARKERS` so WARN-only release classifications cannot drift from `Release_Blocker_List.md`.
- `run_release_gate_static_checks_selftest.py` iterates `APPLY_GUARD_BLOCKER_RELEASE_MARKERS` so ApplyGuard rollback classifications cannot drift from `Release_Blocker_List.md`.
- `run_release_gate_static_checks_selftest.py` iterates `SOFT_APPLY_BLOCKER_RELEASE_MARKERS` so SoftApply preserved-state classifications cannot drift from `Release_Blocker_List.md`.

## Tag rule

Do not create `v3.0-rc1` until:

1. `run_rc_gate.bat <target.exe>` passes.
2. `rc_evidence_bundle_manifest.json` exists.
3. `rc_evidence_bundle_manifest.txt` exists.
4. `Release_Blocker_List.md` has no matching BLOCKER condition.
5. real game validation notes exist.
6. three real game records exist: Game A, Game B, and Game C.
7. any Apply record satisfies the fixed Apply Mode restriction policy.
8. every real-game validation run has a non-placeholder evidence artifact path.
9. the ADR contract index remains current with the release gate behavior.
