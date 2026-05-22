from __future__ import annotations

import argparse
import hashlib
import pathlib
import shutil
import time

import release_gate_evidence as evidence


TARGET = f"synthetic-rc-evidence-selftest-{int(time.time())}.exe"


def make_step(name: str, mode: str) -> dict[str, object]:
    return {
        "step": name,
        "mode": mode,
        "command": "synthetic",
        "exit_code": 0,
        "assertion_exit_code": 0,
        "log_file": None,
        "log_sha256": None,
        "log_bytes": None,
        "runtime_validation_failed": False,
        "recorded_utc": evidence.utc_now(),
    }


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def make_report(
    kind: str,
    run_id: str,
    target: str,
    steps: list[dict[str, object]],
    exe_path: pathlib.Path,
    *,
    schema: str | None = None,
    commit: str | None = None,
    exe_sha256: str | None = None,
) -> None:
    resolved_commit = commit if commit is not None else evidence.run_git(["rev-parse", "HEAD"]) or "unknown"
    run_dir = evidence.LOG_ROOT / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    state = {
        "schema": schema or evidence.EXPECTED_SCHEMA,
        "kind": kind,
        "run_id": run_id,
        "status": "PASS",
        "started_utc": evidence.utc_now(),
        "finished_utc": evidence.utc_now(),
        "target": target,
        "exe_path": str(exe_path),
        "exe_sha256": exe_sha256 or sha256_file(exe_path),
        "binary_fingerprint": {
            "path": str(exe_path),
            "sha256": exe_sha256 or sha256_file(exe_path),
            "bytes": exe_path.stat().st_size,
        },
        "git_commit": resolved_commit,
        "build_hash": evidence.run_git(["rev-parse", "HEAD^{tree}"]),
        "steps": steps,
        "failures": [],
    }
    evidence.write_json(run_dir / "rc_evidence_report.json", state)


def make_running_state(kind: str, run_id: str, target: str, exe_path: pathlib.Path) -> None:
    commit = evidence.run_git(["rev-parse", "HEAD"]) or "unknown"
    run_dir = evidence.LOG_ROOT / run_id
    (run_dir / "logs").mkdir(parents=True, exist_ok=True)
    state = {
        "schema": evidence.EXPECTED_SCHEMA,
        "kind": kind,
        "run_id": run_id,
        "status": "RUNNING",
        "started_utc": evidence.utc_now(),
        "finished_utc": None,
        "target": target,
        "exe_path": str(exe_path),
        "exe_sha256": sha256_file(exe_path),
        "binary_fingerprint": {
            "path": str(exe_path),
            "sha256": sha256_file(exe_path),
            "bytes": exe_path.stat().st_size,
        },
        "git_commit": commit,
        "build_hash": evidence.run_git(["rev-parse", "HEAD^{tree}"]),
        "steps": [],
        "failures": [],
    }
    evidence.save_state(run_dir, state)


def run_verify(target: str) -> int:
    return evidence.verify_rc_evidence(argparse.Namespace(target=target))


def assert_warn_only_report_is_not_blocked(run_id: str, exe_path: pathlib.Path) -> bool:
    run_dir = evidence.LOG_ROOT / run_id
    make_running_state("smoke", run_id, f"{TARGET}.warn", exe_path)
    log_path = run_dir / "logs" / "warn_only.log"
    log_path.write_text("[WARN] synthetic warning path\n[INFO] synthetic info path\n", encoding="utf-8")
    record_result = evidence.record_step(argparse.Namespace(
        run_dir=str(run_dir),
        step="warn_only",
        mode="smoke",
        log_file=str(log_path),
        exit_code=0,
        assertion_exit_code=0,
        command="synthetic",
    ))
    finalize_result = evidence.finalize_evidence(argparse.Namespace(
        run_dir=str(run_dir),
        require_soak_both=False,
    ))
    report = evidence.read_json(run_dir / "rc_evidence_report.json")
    return (
        record_result == 0
        and finalize_result == 0
        and report["status"] == "PASS_WITH_WARNINGS"
        and report["severity_summary"]["BLOCKER"] == 0
        and report["severity_summary"]["WARN"] > 0
    )


def make_complete_pair(target: str, prefix: str, exe_path: pathlib.Path, **overrides: object) -> None:
    make_report(
        "smoke",
        f"{prefix}_smoke",
        target,
        [
            make_step("static_checks", "static"),
            make_step("rg1_dry_run", "dry-run"),
            make_step("rg2_soft_apply", "soft-apply"),
            make_step("rg3_apply", "apply"),
            make_step("rg4_timeout", "timeout"),
        ],
        exe_path,
        **overrides,
    )
    make_report(
        "soak",
        f"{prefix}_soak",
        target,
        [
            make_step("soak_30m_dry_run", "soak"),
            make_step("soak_60m_soft_apply", "soak"),
        ],
        exe_path,
        **overrides,
    )


def main() -> int:
    evidence.LOG_ROOT.mkdir(parents=True, exist_ok=True)
    unique_id = int(time.time())
    warn_run_id = f"synthetic_selftest_warn_{unique_id}"
    valid_prefix = f"synthetic_selftest_valid_{unique_id}"
    schema_prefix = f"synthetic_selftest_schema_{unique_id}"
    commit_prefix = f"synthetic_selftest_commit_{unique_id}"
    hash_prefix = f"synthetic_selftest_hash_{unique_id}"
    synthetic_exe = evidence.LOG_ROOT / f"synthetic_selftest_exe_{unique_id}.bin"
    synthetic_exe.write_bytes(b"synthetic executable bytes")
    created_dirs = [
        evidence.LOG_ROOT / warn_run_id,
        evidence.LOG_ROOT / f"{valid_prefix}_smoke",
        evidence.LOG_ROOT / f"{valid_prefix}_soak",
        evidence.LOG_ROOT / f"{schema_prefix}_smoke",
        evidence.LOG_ROOT / f"{schema_prefix}_soak",
        evidence.LOG_ROOT / f"{commit_prefix}_smoke",
        evidence.LOG_ROOT / f"{commit_prefix}_soak",
        evidence.LOG_ROOT / f"{hash_prefix}_smoke",
        evidence.LOG_ROOT / f"{hash_prefix}_soak",
    ]

    try:
        missing_target = f"{TARGET}.missing"
        if run_verify(missing_target) == 0:
            print("[FAIL] RC evidence self-test: missing evidence unexpectedly passed")
            return 1

        make_complete_pair(TARGET, valid_prefix, synthetic_exe)

        if run_verify(TARGET) != 0:
            print("[FAIL] RC evidence self-test: complete synthetic evidence did not pass")
            return 1

        schema_target = f"{TARGET}.schema_mismatch"
        make_complete_pair(schema_target, schema_prefix, synthetic_exe, schema="bad.schema")
        if run_verify(schema_target) == 0:
            print("[FAIL] RC evidence self-test: schema version mismatch unexpectedly passed")
            return 1

        commit_target = f"{TARGET}.commit_mismatch"
        make_complete_pair(commit_target, commit_prefix, synthetic_exe, commit="bad-commit")
        if run_verify(commit_target) == 0:
            print("[FAIL] RC evidence self-test: git commit mismatch unexpectedly passed")
            return 1

        hash_target = f"{TARGET}.hash_mismatch"
        make_complete_pair(hash_target, hash_prefix, synthetic_exe, exe_sha256="bad-hash")
        if run_verify(hash_target) == 0:
            print("[FAIL] RC evidence self-test: exe SHA-256 mismatch unexpectedly passed")
            return 1

        if not assert_warn_only_report_is_not_blocked(warn_run_id, synthetic_exe):
            print("[FAIL] RC evidence self-test: WARN-only evidence did not produce PASS_WITH_WARNINGS")
            return 1

        print("[PASS] RC evidence self-test passed")
        return 0
    finally:
        for run_dir in created_dirs:
            shutil.rmtree(run_dir, ignore_errors=True)
        synthetic_exe.unlink(missing_ok=True)


if __name__ == "__main__":
    raise SystemExit(main())
