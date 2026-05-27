# GameOptimizer v3.0-rc1 Runbook

## Single entry point

Run:

```bat
run_rc_gate.bat <target.exe>
```

This command is the only RC gate entry point.

Required order:

1. Python `py_compile`
2. `git diff --check`
3. static release gate
4. Release x64 MSVC build
5. full regression
6. release smoke
7. 30m dry-run soak
8. 60m soft-apply soak
9. `verify-rc`
10. evidence bundle generation

The static release gate verifies this order through the `[RC-1]` through `[RC-10]` markers in `run_rc_gate.bat`; within `[RC-3]`, `run_release_gate_static_checks_selftest.py` must run before `run_release_gate_static_checks.py`.
Within `[RC-9]`, `verify_real_game_validation.py --matrix docs\release\Game_Verification_Matrix.json` must run before `verify_rc_candidate.py`.

## Completion criteria

- `BLOCKER` count is `0`.
- binary SHA-256 exists.
- commit SHA exists.
- schema version matches.
- schema hash exists.
- dirty tree state is recorded.
- shutdown reason is recorded.
- rollback preserved state count is recorded.
- smoke and soak reports match the current commit and binary hash.
- final regression reports `failed=0`.
- `docs/release/Game_Verification_Matrix.json` passes `verify_real_game_validation.py`.
- `create_rc_evidence_bundle.py` repeats the real-game validation matrix check before writing the final bundle.
- final bundle manifest records `real_game_validation_matrix` and `real_game_validation_matrix_sha256`.
- final bundle artifact entries pass path, SHA-256, and byte-size validation before PASS.
- written JSON/TXT bundle manifests pass reload and marker validation before PASS.
- written JSON/TXT bundle manifests preserve schema, schema hash, and PASS status before PASS.
- written JSON/TXT bundle manifests preserve target process, git commit, build hash, and binary SHA-256 before PASS.
- final bundle `source_reports` entries point to existing smoke, soak, final regression, and real-game validation matrix files before PASS.
- final bundle `source_reports` entries SHA-256 match their copied bundle artifacts.
- static gate selftest exercises final bundle validators with real temporary files and mismatched-file failures.
- full regression runs `run_release_gate_static_checks_selftest.py`.
- final regression log contains PASS markers for `run_release_gate_static_checks_selftest.py` and `release_gate_evidence_selftest.py`.
- final bundle manifest records `regression_selftest_summary`.
- `regression_selftest_summary` entries are both `true`.
- `regression_selftest_summary` matches the bundled `final_regression_log` artifact.
- text bundle manifest includes the regression selftest summary and both selftest keys.
- every real-game validation run links to an existing `evidence_report` artifact.
- `docs/architecture/Architecture_Decision_Record.md` exists and matches the accepted runtime mutation, fallback, input pinning, limited apply, and evidence contracts.
- Apply mode is limited, explicit, and allowed only after dry-run PASS plus soft-apply PASS evidence.
- each development commit preserves the atomic governance change unit: document contract 1 -> static gate 1 -> validator/evidence coupling 1 -> selftest or regression 1 -> document/blocker/runbook sync -> verification -> commit 1.
- verification notes identify the static gate and validator/selftest or regression evidence used for the committed contract chain.
- `SchedulerController` affinity failure handling remains split into explicit audit, result, and rollback-log helpers.
- `StartupPipeline::prepare()` remains split into startup policy log, main-thread policy, background filter, and background policy helpers.
- `release_gate_evidence_selftest.py` covers ApplyGuard rollback failure with and without shutdown-transfer evidence.
- `release_gate_evidence_selftest.py` proves SoftApply baseline evidence does not increase rollback preserved-state count.
- `check_contract_enforcement_matrix` requires the recent helper/evidence selftest markers from CEM.

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
