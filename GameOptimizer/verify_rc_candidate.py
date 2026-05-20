from __future__ import annotations

import argparse
import pathlib
import re
from typing import Any

import release_gate_evidence as evidence


ROOT = pathlib.Path(__file__).resolve().parent


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
        ],
        "RC runbook"))
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
        ],
        "operational runbook"))
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


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify GameOptimizer v3.0 RC candidate readiness.")
    parser.add_argument("--target", required=True)
    parser.add_argument("--regression-log", required=True, type=pathlib.Path)
    args = parser.parse_args()

    failures: list[str] = []
    failures.extend(validate_runbooks())
    failures.extend(validate_regression_log(args.regression_log))
    failures.extend(validate_evidence_bundle(args.target))

    if failures:
        for failure in failures:
            print(f"[BLOCKER] RC candidate verification: {failure}")
        return 1

    print(f"[PASS] RC candidate verification passed for target={args.target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
