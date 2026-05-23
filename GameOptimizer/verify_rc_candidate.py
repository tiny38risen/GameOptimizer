from __future__ import annotations

import argparse
import pathlib
import re
from typing import Any

import release_gate_evidence as evidence
import verify_real_game_validation


ROOT = pathlib.Path(__file__).resolve().parent
RELEASE_DOCS = ROOT / "docs" / "release"


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def require_markers(path: pathlib.Path, markers: list[str], label: str) -> list[str]:
    failures: list[str] = []
    if not path.exists():
        return [f"{label} missing: {path.name}"]

    text = read_text(path)
    for marker in markers:
        if marker not in text:
            failures.append(f"{label} missing marker in {path.name}: {marker}")
    return failures


def validate_runbooks() -> list[str]:
    failures: list[str] = []
    failures.extend(require_markers(
        ROOT / "ReleaseGateRunbook.md",
        [
            "run_rc_gate.bat <target.exe>",
            "verify_rc_candidate.py --target <target.exe> --regression-log <log>",
            "v3.0-rc1",
            "BLOCKER",
            "evidence bundle",
            "RealGameValidationRunbook.md",
            "Evidence_Schema.md",
            "Release_Blocker_List.md",
        ],
        "RC runbook"))
    failures.extend(require_markers(
        RELEASE_DOCS / "Evidence_Schema.md",
        [
            "schema_version",
            "schema_hash",
            "commit_sha",
            "dirty_tree",
            "compiler_version",
            "runtime_validation_status",
            "rollback_preserved_state_count",
            "gameoptimizer.rc_evidence.v1",
            "gameoptimizer.rc_evidence_bundle.v1",
            "shutdown_failure_classification.shutdown_reason",
            "soft_apply_baseline_summary",
            "apply_guard_rollback_failure",
            "schema version mismatch",
            "schema_hash mismatch",
            "git commit mismatch",
            "build hash mismatch",
            "missing dirty tree flag",
            "missing dirty tree status",
            "exe SHA-256 mismatch",
            "Failure injection matrix",
            "missing binary SHA-256",
            "`SetThreadGroupAffinity` failure plus audit query failure",
            "`SetThreadGroupAffinity` failure plus audit mismatch",
            "heartbeat progression failure",
        ],
        "evidence schema"))
    failures.extend(require_markers(
        RELEASE_DOCS / "Release_Blocker_List.md",
        [
            "Runtime validation result is `FAILED`",
            "Dirty tree flag or status is missing from evidence",
            "Rollback preserved state remains after shutdown",
            "`ApplyGuard` explicit rollback failure is logged",
            "`SetThreadGroupAffinity` failure path discards rollback state without post-failure state audit",
            "`SetThreadGroupAffinity` failure plus post-failure audit query failure",
            "`SetThreadGroupAffinity` failure plus post-failure audit mismatch",
            "Missing binary SHA-256",
            "IRQ unsupported path is treated as ERROR/FAIL instead of WARN + monitoring-only",
            "Group 1+ process-wide background affinity mutation is enabled for `v3.0-rc1`",
            "Real game validation record is missing before `v3.0-rc1` tagging",
            "v3.0-rc1 intentional exclusions",
        ],
        "release blocker list"))
    failures.extend(require_markers(
        RELEASE_DOCS / "RC_Runbook.md",
        [
            "run_rc_gate.bat <target.exe>",
            "Python `py_compile`",
            "`git diff --check`",
            "Release x64 MSVC build",
            "30m dry-run soak",
            "60m soft-apply soak",
            "evidence bundle generation",
        ],
        "RC runbook"))
    failures.extend(require_markers(
        RELEASE_DOCS / "Known_Limitations.md",
        [
            "Processor Group / HEDT",
            "Group 1+ process-wide background affinity is `monitoring-only`",
            "Input Latency",
            "Raw Input not detected means input thread pinning is forbidden",
            "Network / IRQ",
            "IRQ repin is available only when the NIC/driver/runtime path supports interrupt affinity control",
            "Anti-cheat and Access Denied",
            "Anti-cheat Access Denied is a normal fallback path",
            "Non-invasive Boundary",
            "does not modify game memory",
            "does not use DLL injection",
            "does not use kernel patching",
            "does not use driver patching",
            "Apply Mode",
            "Apply mode remains in limited validation status",
        ],
        "known limitations"))
    failures.extend(require_markers(
        ROOT / "ReleaseRegressionMatrix.md",
        [
            "Merge blockers",
            "BLOCKER",
            "WARN",
            "INFO",
            "schema version, git commit, or exe SHA-256 mismatch",
            "Access Denied",
            "ConcreteTid",
            "IRQ",
            "ProcessorGroupHedtEvidenceTests",
        ],
        "blocker list"))
    failures.extend(require_markers(
        ROOT / "OperationalSafetyRunbook.md",
        [
            "Fatal background rollback cases",
            "Runtime timeout safe-point invariant",
            "release blocker",
            "Real game validation invariant",
        ],
        "operational runbook"))
    failures.extend(require_markers(
        ROOT / "RealGameValidationRunbook.md",
        [
            "Game A",
            "Game B",
            "Game C",
            "dry-run --max-runtime-seconds 1800",
            "max-runtime-seconds 3600",
            "Dry-run",
            "Soft-apply",
            "Access Denied / anti-cheat fallback",
            "DPC / IRQ evidence",
            "Raw Input / TID confidence",
            "Processor Group evidence",
            "Apply mode must stay disabled",
            "Game_Verification_Matrix.json",
            "verify_real_game_validation.py",
        ],
        "real game validation runbook"))
    return failures


def validate_regression_log(path: pathlib.Path) -> list[str]:
    if not path.exists():
        return [f"final regression result missing: {path}"]

    text = read_text(path)
    failures: list[str] = []
    if "[PASS] all regression tests passed" not in text:
        failures.append("final regression log missing PASS marker")
    summary = re.search(r"regression summary:\s*total=(\d+),\s*failed=(\d+)", text, re.IGNORECASE)
    if not summary:
        failures.append("final regression log missing total/failed summary")
    elif summary.group(2) != "0":
        failures.append(f"final regression log reports failed={summary.group(2)}")
    return failures


def validate_evidence_bundle(target: str) -> list[str]:
    commit = evidence.run_git(["rev-parse", "HEAD"]) or "unknown"
    smoke_report = evidence.find_latest_report("smoke", target)
    soak_report = evidence.find_latest_report("soak", target)
    failures: list[str] = []

    if smoke_report is None:
        failures.append(f"evidence bundle missing smoke report for target={target} commit={commit}")
    else:
        failures.extend(evidence.validate_smoke_report(smoke_report[1], commit))

    if soak_report is None:
        failures.append(f"evidence bundle missing soak report for target={target} commit={commit}")
    else:
        failures.extend(evidence.validate_soak_report(soak_report[1], commit))

    if smoke_report is not None and soak_report is not None:
        failures.extend(evidence.validate_evidence_pair(smoke_report[1], soak_report[1]))
        failures.extend(validate_evidence_report_files(smoke_report[0], smoke_report[1], "smoke"))
        failures.extend(validate_evidence_report_files(soak_report[0], soak_report[1], "soak"))

    return failures


def validate_evidence_report_files(report_path: pathlib.Path, state: dict[str, Any], label: str) -> list[str]:
    failures: list[str] = []
    run_dir = report_path.parent
    text_report = run_dir / "rc_evidence_report.txt"
    json_report = run_dir / "rc_evidence_report.json"
    state_file = run_dir / evidence.STATE_FILE_NAME
    logs_dir = run_dir / "logs"

    for required_path in (text_report, json_report, state_file, logs_dir):
        if not required_path.exists():
            failures.append(f"{label} evidence bundle missing required artifact: {required_path}")

    for step in state.get("steps", []):
        log_file = step.get("log_file")
        if not log_file:
            continue
        log_path = ROOT / str(log_file)
        if not log_path.exists():
            failures.append(f"{label} evidence bundle missing step log: {log_path}")

    return failures


def validate_real_game_matrix() -> list[str]:
    return verify_real_game_validation.validate_matrix(verify_real_game_validation.DEFAULT_MATRIX)


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify GameOptimizer v3.0 RC candidate readiness.")
    parser.add_argument("--target", required=True)
    parser.add_argument("--regression-log", required=True, type=pathlib.Path)
    args = parser.parse_args()

    failures: list[str] = []
    failures.extend(validate_runbooks())
    failures.extend(validate_regression_log(args.regression_log))
    failures.extend(validate_evidence_bundle(args.target))
    failures.extend(validate_real_game_matrix())

    if failures:
        for failure in failures:
            print(f"[BLOCKER] RC candidate verification: {failure}")
        return 1

    print(f"[PASS] RC candidate verification passed for target={args.target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
