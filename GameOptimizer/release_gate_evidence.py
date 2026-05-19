from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import pathlib
import subprocess
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parent
LOG_ROOT = ROOT / "release_gate_logs"
STATE_FILE_NAME = "evidence_state.json"


def utc_now() -> str:
    return dt.datetime.now(dt.UTC).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def sha256_file(path: pathlib.Path) -> str | None:
    if not path.exists() or not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_git(args: list[str]) -> str | None:
    try:
        result = subprocess.run(
            ["git", *args],
            cwd=ROOT.parent,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    except OSError:
        return None
    if result.returncode != 0:
        return None
    return result.stdout.strip() or None


def read_json(path: pathlib.Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def read_log_text(path: pathlib.Path) -> str:
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "utf-16", "utf-16-le"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def write_json(path: pathlib.Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True), encoding="utf-8")


def normalize_path(path: pathlib.Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT.resolve()))
    except ValueError:
        return str(path.resolve())


def load_state(run_dir: pathlib.Path) -> dict[str, Any]:
    return read_json(run_dir / STATE_FILE_NAME)


def save_state(run_dir: pathlib.Path, state: dict[str, Any]) -> None:
    write_json(run_dir / STATE_FILE_NAME, state)


def init_evidence(args: argparse.Namespace) -> int:
    LOG_ROOT.mkdir(parents=True, exist_ok=True)
    commit = run_git(["rev-parse", "HEAD"]) or "unknown"
    short_commit = commit[:12] if commit != "unknown" else "unknown"
    timestamp = dt.datetime.now(dt.UTC).strftime("%Y%m%dT%H%M%SZ")
    run_id = f"{timestamp}_{args.kind}_{short_commit}"
    run_dir = LOG_ROOT / run_id
    suffix = 1
    while run_dir.exists():
        run_dir = LOG_ROOT / f"{run_id}_{suffix}"
        suffix += 1
    logs_dir = run_dir / "logs"
    logs_dir.mkdir(parents=True, exist_ok=False)

    cwd_exe_path = (pathlib.Path.cwd() / args.exe).resolve()
    exe_path = cwd_exe_path if cwd_exe_path.exists() else (ROOT / args.exe).resolve()
    state: dict[str, Any] = {
        "schema": "gameoptimizer.rc_evidence.v1",
        "kind": args.kind,
        "run_id": run_dir.name,
        "status": "RUNNING",
        "started_utc": utc_now(),
        "finished_utc": None,
        "target": args.target,
        "exe_path": str(exe_path),
        "exe_sha256": sha256_file(exe_path),
        "git_commit": commit,
        "build_hash": run_git(["rev-parse", "HEAD^{tree}"]),
        "steps": [],
        "failures": [],
    }
    save_state(run_dir, state)
    print(str(run_dir))
    return 0


def detect_runtime_validation_failed(log_text: str) -> bool:
    return "runtime validation result: failed" in log_text.lower()


def record_step(args: argparse.Namespace) -> int:
    run_dir = pathlib.Path(args.run_dir)
    state = load_state(run_dir)
    log_path = pathlib.Path(args.log_file) if args.log_file else None
    log_text = ""
    if log_path and log_path.exists():
        log_text = read_log_text(log_path)

    step = {
        "step": args.step,
        "mode": args.mode,
        "command": args.command,
        "exit_code": args.exit_code,
        "assertion_exit_code": args.assertion_exit_code,
        "log_file": normalize_path(log_path) if log_path else None,
        "log_sha256": sha256_file(log_path) if log_path else None,
        "log_bytes": log_path.stat().st_size if log_path and log_path.exists() else None,
        "runtime_validation_failed": detect_runtime_validation_failed(log_text),
        "recorded_utc": utc_now(),
    }
    state["steps"].append(step)
    save_state(run_dir, state)
    return 0


def summarize_failures(state: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    for step in state["steps"]:
        label = step["step"]
        if step["exit_code"] != 0:
            failures.append(f"{label}: command exit code {step['exit_code']}")
        if step["assertion_exit_code"] not in (None, 0):
            failures.append(f"{label}: log assertion exit code {step['assertion_exit_code']}")
        if step["runtime_validation_failed"]:
            failures.append(f"{label}: runtime validation FAILED")
            if step["exit_code"] != 1:
                failures.append(
                    f"{label}: runtime validation FAILED must pair with process exit code 1, "
                    f"found {step['exit_code']}")
    return failures


def validate_required_soak_steps(state: dict[str, Any]) -> list[str]:
    if state["kind"] != "soak":
        return []

    completed_steps = {step["step"] for step in state["steps"]}
    required_steps = {
        "soak_30m_dry_run",
        "soak_60m_soft_apply",
    }
    missing_steps = sorted(required_steps - completed_steps)
    return [f"required RC soak step missing: {step}" for step in missing_steps]


def write_text_report(run_dir: pathlib.Path, state: dict[str, Any]) -> None:
    lines = [
        "GameOptimizer RC Evidence Report",
        "",
        f"Run ID: {state['run_id']}",
        f"Kind: {state['kind']}",
        f"Status: {state['status']}",
        f"Started UTC: {state['started_utc']}",
        f"Finished UTC: {state['finished_utc']}",
        f"Target: {state['target']}",
        f"Git commit: {state['git_commit']}",
        f"Build hash: {state['build_hash']}",
        f"EXE SHA-256: {state['exe_sha256']}",
        "",
        "Steps:",
    ]
    for step in state["steps"]:
        lines.extend([
            f"- {step['step']} ({step['mode']})",
            f"  command exit code: {step['exit_code']}",
            f"  assertion exit code: {step['assertion_exit_code']}",
            f"  runtime validation failed: {step['runtime_validation_failed']}",
            f"  log: {step['log_file']}",
            f"  log sha256: {step['log_sha256']}",
        ])
    lines.extend(["", "Failures:"])
    if state["failures"]:
        lines.extend(f"- {failure}" for failure in state["failures"])
    else:
        lines.append("- none")
    (run_dir / "rc_evidence_report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def finalize_evidence(args: argparse.Namespace) -> int:
    run_dir = pathlib.Path(args.run_dir)
    state = load_state(run_dir)
    state["finished_utc"] = utc_now()
    state["failures"] = summarize_failures(state)
    if args.require_soak_both:
        state["failures"].extend(validate_required_soak_steps(state))
    state["status"] = "PASS" if not state["failures"] else "FAIL"
    write_json(run_dir / "rc_evidence_report.json", state)
    write_text_report(run_dir, state)
    save_state(run_dir, state)
    print(f"[{state['status']}] evidence report: {run_dir / 'rc_evidence_report.txt'}")
    return 0 if state["status"] == "PASS" else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Create GameOptimizer release gate evidence reports.")
    subparsers = parser.add_subparsers(dest="command_name", required=True)

    init_parser = subparsers.add_parser("init")
    init_parser.add_argument("--kind", required=True, choices=["smoke", "soak"])
    init_parser.add_argument("--target", required=True)
    init_parser.add_argument("--exe", required=True)
    init_parser.set_defaults(func=init_evidence)

    record_parser = subparsers.add_parser("record")
    record_parser.add_argument("--run-dir", required=True)
    record_parser.add_argument("--step", required=True)
    record_parser.add_argument("--mode", required=True)
    record_parser.add_argument("--log-file")
    record_parser.add_argument("--exit-code", required=True, type=int)
    record_parser.add_argument("--assertion-exit-code", type=int)
    record_parser.add_argument("--command", required=True)
    record_parser.set_defaults(func=record_step)

    finalize_parser = subparsers.add_parser("finalize")
    finalize_parser.add_argument("--run-dir", required=True)
    finalize_parser.add_argument("--require-soak-both", action="store_true")
    finalize_parser.set_defaults(func=finalize_evidence)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
