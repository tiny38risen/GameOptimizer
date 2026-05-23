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
- Apply mode is limited, explicit, and allowed only after dry-run PASS plus soft-apply PASS evidence.

## Tag rule

Do not create `v3.0-rc1` until:

1. `run_rc_gate.bat <target.exe>` passes.
2. `rc_evidence_bundle_manifest.json` exists.
3. `rc_evidence_bundle_manifest.txt` exists.
4. `Release_Blocker_List.md` has no matching BLOCKER condition.
5. real game validation notes exist.
6. three real game records exist: Game A, Game B, and Game C.
7. any Apply record satisfies the fixed Apply Mode restriction policy.
