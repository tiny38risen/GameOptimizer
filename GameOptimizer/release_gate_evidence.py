from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import pathlib
import re
import subprocess
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parent
LOG_ROOT = ROOT / "release_gate_logs"
STATE_FILE_NAME = "evidence_state.json"
MAX_OBSERVATION_SAMPLES = 20
EXPECTED_SCHEMA = "gameoptimizer.rc_evidence.v1"
SEVERITY_POLICY = {
    "BLOCKER": [
        "runtime validation FAILED",
        "rollback failure or preserved rollback state",
        "hang detection failure",
        "timeline monotonicity failure",
        "missing evidence",
        "schema version, git commit, build hash, or exe SHA-256 mismatch",
        "unsafe rollback state discard regression",
        "ApplyGuard rollback failure",
    ],
    "WARN": [
        "IRQ unsupported monitoring-only",
        "Raw Input not detected or unsupported",
        "Access Denied fallback",
        "Processor Group 1+ background monitoring-only",
    ],
    "INFO": [
        "telemetry",
        "dry-run decision",
        "unsupported feature observation",
    ],
}


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


def file_size(path: pathlib.Path) -> int | None:
    if not path.exists() or not path.is_file():
        return None
    return path.stat().st_size


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
        "schema": EXPECTED_SCHEMA,
        "kind": args.kind,
        "run_id": run_dir.name,
        "status": "RUNNING",
        "started_utc": utc_now(),
        "finished_utc": None,
        "target": args.target,
        "exe_path": str(exe_path),
        "exe_sha256": sha256_file(exe_path),
        "binary_fingerprint": {
            "path": str(exe_path),
            "sha256": sha256_file(exe_path),
            "bytes": file_size(exe_path),
        },
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


def extract_log_lines(log_text: str, marker: str) -> list[str]:
    lines: list[str] = []
    for line in log_text.splitlines():
        if marker in line:
            lines.append(line.strip())
    return lines


def parse_bool_text(value: str | None) -> bool | None:
    if value is None:
        return None
    lowered = value.lower()
    if lowered in ("true", "1"):
        return True
    if lowered in ("false", "0"):
        return False
    return None


def extract_shutdown_failure_classification(log_text: str) -> dict[str, bool | None]:
    match = re.search(
        r"shutdown result: reason=([A-Za-z]+), "
        r"timerRollbackFailed=(true|false|0|1), "
        r"schedulerRollbackFailed=(true|false|0|1), "
        r"runtimeValidationFailed=(true|false|0|1), "
        r"rollbackStatePreserved=(true|false|0|1)",
        log_text,
        re.IGNORECASE,
    )
    return {
        "summary_present": match is not None,
        "shutdown_reason": match.group(1) if match else None,
        "timer_rollback_failed": parse_bool_text(match.group(2)) if match else None,
        "scheduler_rollback_failed": parse_bool_text(match.group(3)) if match else None,
        "runtime_validation_failed": parse_bool_text(match.group(4)) if match else None,
        "rollback_state_preserved": parse_bool_text(match.group(5)) if match else None,
    }


def extract_rollback_preserved_state_summary(log_text: str) -> dict[str, int | bool | None]:
    matches = re.findall(
        r"preserved rollback state count: thread=(\d+), process=(\d+)",
        log_text,
        re.IGNORECASE,
    )
    if not matches:
        return {
            "summary_present": False,
            "thread": None,
            "process": None,
            "total": None,
            "has_preserved_state": None,
        }

    thread_count = int(matches[-1][0])
    process_count = int(matches[-1][1])
    total = thread_count + process_count
    return {
        "summary_present": True,
        "thread": thread_count,
        "process": process_count,
        "total": total,
        "has_preserved_state": total > 0,
    }


def extract_processor_group_mode_summary(log_text: str) -> dict[str, Any]:
    selected_groups = sorted({
        int(group)
        for group in re.findall(r"selected_group=(\d+)", log_text, re.IGNORECASE)
    })
    fallback_groups = sorted({
        int(group)
        for group in re.findall(r"topology fallback policy selected from process affinity: group=(\d+)", log_text, re.IGNORECASE)
    })
    blocked_groups = sorted({
        int(group)
        for group in re.findall(r"background restriction blocked: processor group (\d+)", log_text, re.IGNORECASE)
    })
    provenance = sorted(set(re.findall(r"mask_provenance=([a-zA-Z0-9_]+)", log_text)))
    return {
        "selected_groups": selected_groups,
        "fallback_groups": fallback_groups,
        "background_blocked_groups": blocked_groups,
        "mask_provenance": provenance,
        "has_group_1_plus_evidence": any(group > 0 for group in selected_groups + fallback_groups + blocked_groups),
        "process_wide_group_1_plus_blocked": bool(blocked_groups),
    }


def extract_thread_tracker_telemetry_summary(log_text: str) -> dict[str, Any]:
    pattern = re.compile(
        r"thread tracker telemetry:\s*"
        r"open_thread_failures=(\d+),\s*"
        r"open_thread_access_denied=(\d+),\s*"
        r"get_thread_times_failures=(\d+),\s*"
        r"get_thread_times_access_denied=(\d+),\s*"
        r"reset_event=(true|false|0|1),\s*"
        r"total_open_thread_failures=(\d+),\s*"
        r"total_open_thread_access_denied=(\d+),\s*"
        r"total_get_thread_times_failures=(\d+),\s*"
        r"total_reset_events=(\d+)",
        re.IGNORECASE,
    )
    samples: list[dict[str, Any]] = []
    for match in pattern.finditer(log_text):
        samples.append({
            "open_thread_failures": int(match.group(1)),
            "open_thread_access_denied": int(match.group(2)),
            "get_thread_times_failures": int(match.group(3)),
            "get_thread_times_access_denied": int(match.group(4)),
            "reset_event": parse_bool_text(match.group(5)),
            "total_open_thread_failures": int(match.group(6)),
            "total_open_thread_access_denied": int(match.group(7)),
            "total_get_thread_times_failures": int(match.group(8)),
            "total_reset_events": int(match.group(9)),
        })

    last_sample = samples[-1] if samples else None
    return {
        "summary_present": bool(samples),
        "sample_count": len(samples),
        "last_sample": last_sample,
        "max_total_open_thread_failures": max((sample["total_open_thread_failures"] for sample in samples), default=0),
        "max_total_open_thread_access_denied": max((sample["total_open_thread_access_denied"] for sample in samples), default=0),
        "max_total_get_thread_times_failures": max((sample["total_get_thread_times_failures"] for sample in samples), default=0),
        "max_total_reset_events": max((sample["total_reset_events"] for sample in samples), default=0),
    }


def extract_input_latency_summary(log_text: str) -> dict[str, Any]:
    lowered = log_text.lower()
    return {
        "raw_input_detected": "raw input detected" in lowered,
        "raw_input_not_found": "raw input local-process registration not found" in lowered,
        "remote_detection_unsupported": "raw input remote-process detection is unavailable" in lowered,
        "fallback_monitoring_only": "fallback input policy" in lowered or "monitoring-only fallback" in lowered,
        "concrete_tid_seen": "concretetid" in lowered or "concrete input tid" in lowered,
        "pinning_skipped": "input thread pinning skipped" in lowered or "input thread pinning disabled" in lowered,
        "pinning_attempted": "would pin an input thread" in lowered or "input pinning is not wired" in lowered,
    }


def extract_network_irq_summary(log_text: str) -> dict[str, Any]:
    lowered = log_text.lower()
    unsupported_count = len(re.findall(r"irq_repin .*unsupported|interrupt affinity .*unsupported", lowered))
    suppressed_count = len(re.findall(r"irq_repin suppressed|suppressed monitoring-only", lowered))
    return {
        "irq_repin_requested": "irq_repin" in lowered,
        "interrupt_affinity_unsupported": "interrupt affinity" in lowered and "unsupported" in lowered,
        "unsupported_count": unsupported_count,
        "suppressed_monitoring_only_count": suppressed_count,
        "monitoring_only": "monitoring-only" in lowered and ("irq" in lowered or "interrupt" in lowered),
        "dispatch_path_reached": "irq_repin dispatch path reached" in lowered,
    }


def extract_access_denied_fallback_summary(log_text: str) -> dict[str, Any]:
    access_markers = (
        "access denied",
        "access boundary",
        "openthread failure",
        "openprocess failure",
    )
    fallback_markers = (
        "monitoring-only fallback remains active",
        "recoverable access limitation",
        "rollback path was invoked when needed",
        "saved state discarded before mutation",
        "blocked by an access boundary",
        "skipped",
    )
    access_lines: list[str] = []
    fallback_lines: list[str] = []
    for line in log_text.splitlines():
        lowered = line.lower()
        if any(marker in lowered for marker in access_markers):
            access_lines.append(line.strip())
            if any(marker in lowered for marker in fallback_markers):
                fallback_lines.append(line.strip())
    return {
        "access_boundary_count": len(access_lines),
        "fallback_evidence_count": len(fallback_lines),
        "samples": access_lines[:MAX_OBSERVATION_SAMPLES],
    }


def extract_runtime_validation_summary(log_text: str) -> dict[str, Any]:
    match = re.search(
        r"runtime validation summary:\s*cycles=(\d+),\s*"
        r"minimum_required=(\d+),\s*"
        r"minimum_satisfied=(true|false|0|1).*?"
        r"thread_tracker_reset_events=(\d+).*?"
        r"critical_failure=(true|false|0|1)",
        log_text,
        re.IGNORECASE,
    )
    if not match:
        return {"summary_present": False}
    return {
        "summary_present": True,
        "cycles": int(match.group(1)),
        "minimum_required": int(match.group(2)),
        "minimum_satisfied": parse_bool_text(match.group(3)),
        "thread_tracker_reset_events": int(match.group(4)),
        "critical_failure": parse_bool_text(match.group(5)),
    }


def extract_soft_apply_baseline_summary(log_text: str) -> dict[str, Any]:
    thread_baseline_count = len(re.findall(
        r"soft-apply validated scheduling baseline captured",
        log_text,
        re.IGNORECASE,
    ))
    process_baseline_count = len(re.findall(
        r"background rollback baseline validated",
        log_text,
        re.IGNORECASE,
    ))
    not_stored_count = len(re.findall(
        r"validated baseline was not stored as rollback state|audit-only, not stored as rollback state",
        log_text,
        re.IGNORECASE,
    ))
    return {
        "thread_baseline_count": thread_baseline_count,
        "process_baseline_count": process_baseline_count,
        "not_stored_as_rollback_state_count": not_stored_count,
        "summary_present": thread_baseline_count > 0 or process_baseline_count > 0 or not_stored_count > 0,
    }


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
        "shutdown_failure_classification": extract_shutdown_failure_classification(log_text),
        "rollback_preserved_state_summary": extract_rollback_preserved_state_summary(log_text),
        "processor_group_mode_summary": extract_processor_group_mode_summary(log_text),
        "thread_tracker_telemetry_summary": extract_thread_tracker_telemetry_summary(log_text),
        "input_latency_summary": extract_input_latency_summary(log_text),
        "network_irq_summary": extract_network_irq_summary(log_text),
        "access_denied_fallback_summary": extract_access_denied_fallback_summary(log_text),
        "runtime_validation_summary": extract_runtime_validation_summary(log_text),
        "soft_apply_baseline_summary": extract_soft_apply_baseline_summary(log_text),
        "apply_guard_rollback_failure": "apply guard explicit rollback failed" in log_text.lower()
            or "apply guard destructor rollback failed" in log_text.lower(),
        "warn_count": len(extract_log_lines(log_text, "[WARN]")),
        "warn_samples": extract_log_lines(log_text, "[WARN]")[:MAX_OBSERVATION_SAMPLES],
        "info_count": len(extract_log_lines(log_text, "[INFO]")),
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
        shutdown = step.get("shutdown_failure_classification") or {}
        if shutdown.get("rollback_state_preserved"):
            failures.append(f"{label}: shutdown preserved rollback state")
        rollback = step.get("rollback_preserved_state_summary") or {}
        if rollback.get("has_preserved_state"):
            failures.append(
                f"{label}: preserved rollback state remains "
                f"(thread={rollback.get('thread')}, process={rollback.get('process')})")
        runtime_summary = step.get("runtime_validation_summary") or {}
        if runtime_summary.get("critical_failure"):
            failures.append(f"{label}: runtime validation summary reports critical failure")
        if step.get("apply_guard_rollback_failure"):
            failures.append(f"{label}: ApplyGuard rollback failure")
    return failures


def summarize_warnings(state: dict[str, Any]) -> list[str]:
    warnings: list[str] = []
    for step in state["steps"]:
        label = step["step"]
        warn_count = int(step.get("warn_count") or 0)
        if warn_count == 0:
            continue
        samples = step.get("warn_samples") or []
        warnings.append(f"{label}: {warn_count} warning log line(s) observed")
        warnings.extend(f"{label}: {sample}" for sample in samples)
        network = step.get("network_irq_summary") or {}
        if network.get("interrupt_affinity_unsupported"):
            warnings.append(f"{label}: IRQ unsupported path recorded as WARN + monitoring-only")
        input_summary = step.get("input_latency_summary") or {}
        if input_summary.get("raw_input_not_found") or input_summary.get("remote_detection_unsupported"):
            warnings.append(f"{label}: Raw Input detection unavailable; fallback input policy recorded")
        processor = step.get("processor_group_mode_summary") or {}
        if processor.get("process_wide_group_1_plus_blocked"):
            warnings.append(f"{label}: Processor Group 1+ background restriction recorded as monitoring-only")
        access = step.get("access_denied_fallback_summary") or {}
        if access.get("access_boundary_count"):
            warnings.append(
                f"{label}: Access Denied/access-boundary fallback evidence lines={access.get('fallback_evidence_count')}")
    return warnings


def summarize_info(state: dict[str, Any]) -> list[str]:
    info: list[str] = [
        f"target: {state['target']}",
        f"git_commit: {state['git_commit']}",
        f"build_hash: {state['build_hash']}",
        f"exe_sha256: {state['exe_sha256']}",
        f"binary_fingerprint: {state.get('binary_fingerprint')}",
    ]
    processor_group_summaries = [
        step.get("processor_group_mode_summary")
        for step in state["steps"]
        if step.get("processor_group_mode_summary")
    ]
    shutdown_summaries = [
        step.get("shutdown_failure_classification")
        for step in state["steps"]
        if step.get("shutdown_failure_classification")
    ]
    rollback_summaries = [
        step.get("rollback_preserved_state_summary")
        for step in state["steps"]
        if step.get("rollback_preserved_state_summary")
    ]
    info.append(f"processor_group_mode_summary: {processor_group_summaries}")
    info.append(f"shutdown_failure_classification: {shutdown_summaries}")
    info.append(f"rollback_preserved_state_summary: {rollback_summaries}")
    info.append(f"thread_tracker_telemetry_summary: {collect_step_summaries(state, 'thread_tracker_telemetry_summary')}")
    info.append(f"input_latency_summary: {collect_step_summaries(state, 'input_latency_summary')}")
    info.append(f"network_irq_summary: {collect_step_summaries(state, 'network_irq_summary')}")
    info.append(f"access_denied_fallback_summary: {collect_step_summaries(state, 'access_denied_fallback_summary')}")
    info.append(f"runtime_validation_summary: {collect_step_summaries(state, 'runtime_validation_summary')}")
    info.append(f"soft_apply_baseline_summary: {collect_step_summaries(state, 'soft_apply_baseline_summary')}")
    for step in state["steps"]:
        info.append(f"{step['step']}: {int(step.get('info_count') or 0)} info log line(s) observed")
    return info


def collect_step_summaries(state: dict[str, Any], field_name: str) -> list[dict[str, Any]]:
    return [
        {
            "step": step.get("step"),
            "value": step.get(field_name),
        }
        for step in state.get("steps", [])
        if step.get(field_name) is not None
    ]


def apply_severity_summary(state: dict[str, Any]) -> None:
    blockers = list(state.get("blockers") or state.get("failures") or [])
    warnings = list(state.get("warnings") or [])
    info = list(state.get("info") or [])
    state["blockers"] = blockers
    state["failures"] = blockers
    state["warnings"] = warnings
    state["info"] = info
    state["severity_summary"] = {
        "BLOCKER": len(blockers),
        "WARN": len(warnings),
        "INFO": len(info),
    }
    if blockers:
        state["status"] = "FAIL"
    elif warnings:
        state["status"] = "PASS_WITH_WARNINGS"
    else:
        state["status"] = "PASS"


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


def validate_required_smoke_steps(state: dict[str, Any]) -> list[str]:
    if state["kind"] != "smoke":
        return []

    completed_steps = {step["step"] for step in state["steps"]}
    required_steps = {
        "static_checks",
        "rg1_dry_run",
        "rg2_soft_apply",
        "rg3_apply",
        "rg4_timeout",
    }
    missing_steps = sorted(required_steps - completed_steps)
    return [f"required smoke step missing: {step}" for step in missing_steps]


def step_is_successful(step: dict[str, Any]) -> bool:
    return (
        step.get("exit_code") == 0
        and step.get("assertion_exit_code") in (None, 0)
        and not step.get("runtime_validation_failed", False)
    )


def report_is_successful(state: dict[str, Any]) -> bool:
    return (
        state.get("status") in ("PASS", "PASS_WITH_WARNINGS")
        and not state.get("blockers", state.get("failures"))
        and all(step_is_successful(step) for step in state.get("steps", []))
    )


def report_matches_target(state: dict[str, Any], target: str) -> bool:
    return state.get("target") == target


def load_evidence_reports() -> list[tuple[pathlib.Path, dict[str, Any]]]:
    reports: list[tuple[pathlib.Path, dict[str, Any]]] = []
    if not LOG_ROOT.exists():
        return reports

    for report_path in sorted(LOG_ROOT.glob("*/rc_evidence_report.json")):
        try:
            reports.append((report_path, read_json(report_path)))
        except (OSError, json.JSONDecodeError):
            continue
    return reports


def find_latest_report(kind: str, target: str) -> tuple[pathlib.Path, dict[str, Any]] | None:
    matches = [
        (path, state)
        for path, state in load_evidence_reports()
        if state.get("kind") == kind and report_matches_target(state, target)
    ]
    if not matches:
        return None
    return max(matches, key=lambda item: item[1].get("finished_utc") or item[1].get("started_utc") or "")


def validate_report_identity(state: dict[str, Any], label: str, git_commit: str) -> list[str]:
    failures: list[str] = []
    if state.get("schema") != EXPECTED_SCHEMA:
        failures.append(
            f"{label} evidence schema version mismatch: expected {EXPECTED_SCHEMA}, found {state.get('schema')}")
    if state.get("git_commit") != git_commit:
        failures.append(
            f"{label} evidence git commit mismatch: expected {git_commit}, found {state.get('git_commit')}")
    if not state.get("build_hash"):
        failures.append(f"{label} evidence build hash is missing")
    if not state.get("binary_fingerprint"):
        failures.append(f"{label} evidence binary fingerprint is missing")

    recorded_hash = state.get("exe_sha256")
    exe_path_text = state.get("exe_path")
    if not recorded_hash:
        failures.append(f"{label} evidence exe SHA-256 is missing")
    if not exe_path_text:
        failures.append(f"{label} evidence exe path is missing")
    elif recorded_hash:
        current_hash = sha256_file(pathlib.Path(exe_path_text))
        if current_hash is None:
            failures.append(f"{label} evidence exe path is not readable: {exe_path_text}")
        elif current_hash != recorded_hash:
            failures.append(
                f"{label} evidence exe SHA-256 mismatch: expected {current_hash}, found {recorded_hash}")
    return failures


def validate_smoke_report(state: dict[str, Any], git_commit: str) -> list[str]:
    failures: list[str] = []
    failures.extend(validate_report_identity(state, "smoke", git_commit))
    if not report_is_successful(state):
        failures.append(f"smoke evidence report is not PASS: {state.get('run_id')}")
    failures.extend(validate_required_smoke_steps(state))
    return failures


def validate_soak_report(state: dict[str, Any], git_commit: str) -> list[str]:
    failures: list[str] = []
    failures.extend(validate_report_identity(state, "soak", git_commit))
    if not report_is_successful(state):
        failures.append(f"soak evidence report is not PASS: {state.get('run_id')}")
    failures.extend(validate_required_soak_steps(state))
    return failures


def validate_evidence_pair(
    smoke_state: dict[str, Any],
    soak_state: dict[str, Any],
) -> list[str]:
    failures: list[str] = []
    if smoke_state.get("exe_sha256") != soak_state.get("exe_sha256"):
        failures.append(
            "smoke/soak evidence exe SHA-256 mismatch: "
            f"smoke={smoke_state.get('exe_sha256')}, soak={soak_state.get('exe_sha256')}")
    if smoke_state.get("build_hash") != soak_state.get("build_hash"):
        failures.append(
            "smoke/soak evidence build hash mismatch: "
            f"smoke={smoke_state.get('build_hash')}, soak={soak_state.get('build_hash')}")
    return failures


def verify_rc_evidence(args: argparse.Namespace) -> int:
    git_commit = run_git(["rev-parse", "HEAD"]) or "unknown"
    smoke_report = find_latest_report("smoke", args.target)
    soak_report = find_latest_report("soak", args.target)
    failures: list[str] = []

    if smoke_report is None:
        failures.append(
            f"required smoke evidence report missing for target={args.target} commit={git_commit}")
    else:
        failures.extend(validate_smoke_report(smoke_report[1], git_commit))

    if soak_report is None:
        failures.append(
            f"required soak evidence report missing for target={args.target} commit={git_commit}")
    else:
        failures.extend(validate_soak_report(soak_report[1], git_commit))

    if smoke_report is not None and soak_report is not None:
        failures.extend(validate_evidence_pair(smoke_report[1], soak_report[1]))

    if failures:
        for failure in failures:
            print(f"[BLOCKER] RC evidence verification: {failure}")
        return 1

    assert smoke_report is not None
    assert soak_report is not None
    print(f"[PASS] RC evidence verification passed for target={args.target} commit={git_commit}")
    print(f"[INFO] smoke evidence: {smoke_report[0]}")
    print(f"[INFO] soak evidence: {soak_report[0]}")
    return 0


def write_text_report(run_dir: pathlib.Path, state: dict[str, Any]) -> None:
    lines = [
        "GameOptimizer RC Evidence Report",
        "",
        f"Run ID: {state['run_id']}",
        f"Kind: {state['kind']}",
        f"Status: {state['status']}",
        f"Severity: BLOCKER={state['severity_summary']['BLOCKER']}, "
        f"WARN={state['severity_summary']['WARN']}, INFO={state['severity_summary']['INFO']}",
        f"Started UTC: {state['started_utc']}",
        f"Finished UTC: {state['finished_utc']}",
        f"Target: {state['target']}",
        f"Git commit: {state['git_commit']}",
        f"Build hash: {state['build_hash']}",
        f"EXE SHA-256: {state['exe_sha256']}",
        f"Binary fingerprint: {state.get('binary_fingerprint')}",
        "",
        "Steps:",
    ]
    for step in state["steps"]:
        lines.extend([
            f"- {step['step']} ({step['mode']})",
            f"  command exit code: {step['exit_code']}",
            f"  assertion exit code: {step['assertion_exit_code']}",
            f"  runtime validation failed: {step['runtime_validation_failed']}",
            f"  runtime validation summary: {step.get('runtime_validation_summary')}",
            f"  shutdown failure classification: {step.get('shutdown_failure_classification')}",
            f"  rollback preserved state summary: {step.get('rollback_preserved_state_summary')}",
            f"  processor group mode summary: {step.get('processor_group_mode_summary')}",
            f"  thread tracker telemetry summary: {step.get('thread_tracker_telemetry_summary')}",
            f"  input latency summary: {step.get('input_latency_summary')}",
            f"  network IRQ summary: {step.get('network_irq_summary')}",
            f"  access denied fallback summary: {step.get('access_denied_fallback_summary')}",
            f"  soft-apply baseline summary: {step.get('soft_apply_baseline_summary')}",
            f"  ApplyGuard rollback failure: {step.get('apply_guard_rollback_failure')}",
            f"  warn count: {step.get('warn_count', 0)}",
            f"  info count: {step.get('info_count', 0)}",
            f"  log: {step['log_file']}",
            f"  log sha256: {step['log_sha256']}",
        ])
    lines.extend(["", "Severity Policy:"])
    for severity, items in SEVERITY_POLICY.items():
        lines.append(f"- {severity}: " + "; ".join(items))
    lines.extend(["", "BLOCKER:"])
    if state["blockers"]:
        lines.extend(f"- {blocker}" for blocker in state["blockers"])
    else:
        lines.append("- none")
    lines.extend(["", "WARN:"])
    if state["warnings"]:
        lines.extend(f"- {warning}" for warning in state["warnings"])
    else:
        lines.append("- none")
    lines.extend(["", "INFO:"])
    if state["info"]:
        lines.extend(f"- {item}" for item in state["info"])
    else:
        lines.append("- none")
    (run_dir / "rc_evidence_report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def finalize_evidence(args: argparse.Namespace) -> int:
    run_dir = pathlib.Path(args.run_dir)
    state = load_state(run_dir)
    state["finished_utc"] = utc_now()
    state["blockers"] = summarize_failures(state)
    if args.require_soak_both:
        state["blockers"].extend(validate_required_soak_steps(state))
    state["warnings"] = summarize_warnings(state)
    state["info"] = summarize_info(state)
    apply_severity_summary(state)
    write_json(run_dir / "rc_evidence_report.json", state)
    write_text_report(run_dir, state)
    save_state(run_dir, state)
    print(f"[{state['status']}] evidence report: {run_dir / 'rc_evidence_report.txt'}")
    return 0 if state["status"] in ("PASS", "PASS_WITH_WARNINGS") else 1


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

    verify_parser = subparsers.add_parser("verify-rc")
    verify_parser.add_argument("--target", required=True)
    verify_parser.set_defaults(func=verify_rc_evidence)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
