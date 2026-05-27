from __future__ import annotations

import argparse
import json
import pathlib
import shutil
from typing import Any

import release_gate_evidence as evidence
import verify_rc_candidate


ROOT = pathlib.Path(__file__).resolve().parent
BUNDLE_SCHEMA = "gameoptimizer.rc_evidence_bundle.v1"


def relative(path: pathlib.Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT.resolve()))
    except ValueError:
        return str(path.resolve())


def copy_file(src: pathlib.Path, dst: pathlib.Path) -> dict[str, Any]:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return {
        "path": relative(dst),
        "source": relative(src),
        "sha256": evidence.sha256_file(dst),
        "bytes": dst.stat().st_size,
    }


def copy_directory(src: pathlib.Path, dst: pathlib.Path) -> list[dict[str, Any]]:
    copied: list[dict[str, Any]] = []
    if not src.exists():
        return copied

    for path in sorted(src.rglob("*")):
        if not path.is_file():
            continue
        copied.append(copy_file(path, dst / path.relative_to(src)))
    return copied


def latest_reports(target: str) -> tuple[tuple[pathlib.Path, dict[str, Any]], tuple[pathlib.Path, dict[str, Any]]]:
    smoke = evidence.find_latest_report("smoke", target)
    soak = evidence.find_latest_report("soak", target)
    if smoke is None or soak is None:
        raise RuntimeError("verified RC evidence disappeared before bundle creation")
    return smoke, soak


def create_bundle_dir(commit: str) -> pathlib.Path:
    evidence.LOG_ROOT.mkdir(parents=True, exist_ok=True)
    timestamp = evidence.utc_now().replace("-", "").replace(":", "")
    base_name = f"{timestamp}_rc_bundle_{commit[:12] if commit != 'unknown' else 'unknown'}"
    bundle_dir = evidence.LOG_ROOT / base_name
    suffix = 1
    while bundle_dir.exists():
        bundle_dir = evidence.LOG_ROOT / f"{base_name}_{suffix}"
        suffix += 1
    bundle_dir.mkdir(parents=True)
    return bundle_dir


def write_json(path: pathlib.Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True), encoding="utf-8")


def write_text_manifest(path: pathlib.Path, manifest: dict[str, Any]) -> None:
    lines = [
        "GameOptimizer RC Evidence Bundle",
        "",
        f"Schema: {manifest['schema']}",
        f"Schema version: {manifest['schema_version']}",
        f"Schema hash: {manifest['schema_hash']}",
        f"Decision: {manifest['candidate_decision']}",
        f"Status: {manifest['status']}",
        f"Target: {manifest['target']}",
        f"Target process: {manifest['target_process']}",
        f"Git commit: {manifest['git_commit']}",
        f"Commit SHA: {manifest['commit_sha']}",
        f"Branch: {manifest['branch']}",
        f"Git dirty: {manifest['git_dirty']}",
        f"Dirty tree: {manifest['dirty_tree']}",
        f"Git status short: {manifest['git_status_short']}",
        f"Build configuration: {manifest['build_configuration']}",
        f"Compiler version: {manifest['compiler_version']}",
        f"Build hash: {manifest['build_hash']}",
        f"EXE SHA-256: {manifest['exe_sha256']}",
        f"Binary path: {manifest['binary_path']}",
        f"Binary SHA-256: {manifest['binary_sha256']}",
        f"Binary fingerprint: {manifest['binary_fingerprint']}",
        f"Scheduler mode: {manifest['scheduler_mode']}",
        f"Shutdown reason: {manifest['shutdown_reason']}",
        f"Runtime validation status: {manifest['runtime_validation_status']}",
        f"Rollback preserved state count: {manifest['rollback_preserved_state_count']}",
        f"ApplyGuard rollback failure count: {manifest['apply_guard_rollback_failure_count']}",
        f"Rollback failure transferred to shutdown count: {manifest['rollback_failure_transferred_to_shutdown_count']}",
        f"Severity counts: BLOCKER={manifest['blocker_count']}, WARN={manifest['warn_count']}, INFO={manifest['info_count']}",
        f"Rollback preserved state summary: {manifest['rollback_preserved_state_summary']}",
        f"Shutdown failure classification: {manifest['shutdown_failure_classification']}",
        f"Processor group mode summary: {manifest['processor_group_mode_summary']}",
        f"Processor group mode: {manifest['processor_group_mode']}",
        f"Background restriction mode: {manifest['background_restriction_mode']}",
        f"ThreadTracker telemetry summary: {manifest['thread_tracker_telemetry_summary']}",
        f"ThreadTracker telemetry: {manifest['thread_tracker_telemetry']}",
        f"Input latency summary: {manifest['input_latency_summary']}",
        f"Network IRQ summary: {manifest['network_irq_summary']}",
        f"Access Denied fallback summary: {manifest['access_denied_fallback_summary']}",
        f"Runtime validation summary: {manifest['runtime_validation_summary']}",
        f"SoftApply baseline summary: {manifest['soft_apply_baseline_summary']}",
        f"Real game validation matrix: {manifest['real_game_validation_matrix']}",
        f"Real game validation matrix SHA-256: {manifest['real_game_validation_matrix_sha256']}",
        f"Regression selftest summary: {manifest['regression_selftest_summary']}",
        f"Test results: {manifest['test_results']}",
        f"Created UTC: {manifest['created_utc']}",
        "",
        "Severity policy:",
    ]
    for severity, items in manifest["severity_policy"].items():
        lines.append(f"- {severity}: " + "; ".join(items))
    lines.extend([
        "",
        "Artifacts:",
    ])
    for artifact in manifest["artifacts"]:
        lines.append(f"- {artifact['label']}: {artifact['path']} sha256={artifact['sha256']}")
    lines.extend([
        "",
        "BLOCKER:",
        "- none",
        "",
        "WARN:",
    ])
    warnings = manifest.get("warnings", [])
    if warnings:
        lines.extend(f"- {warning}" for warning in warnings)
    else:
        lines.append("- none")
    lines.extend([
        "",
        "INFO:",
        f"- smoke report: {manifest['source_reports']['smoke']}",
        f"- soak report: {manifest['source_reports']['soak']}",
        f"- regression log: {manifest['source_reports']['regression_log']}",
    ])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def resolve_bundle_artifact_path(value: Any) -> pathlib.Path:
    path = pathlib.Path(str(value))
    if path.is_absolute():
        return path
    return ROOT / path


def validate_bundle_artifacts(manifest: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    artifacts = manifest.get("artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        return ["final bundle manifest has no artifact list"]

    for index, artifact in enumerate(artifacts):
        if not isinstance(artifact, dict):
            failures.append(f"artifact #{index + 1} is not an object")
            continue

        label = artifact.get("label") or f"artifact #{index + 1}"
        artifact_path = resolve_bundle_artifact_path(artifact.get("path"))
        if not artifact_path.exists() or not artifact_path.is_file():
            failures.append(f"{label}: bundled artifact is missing: {artifact_path}")
            continue

        expected_sha256 = artifact.get("sha256")
        actual_sha256 = evidence.sha256_file(artifact_path)
        if expected_sha256 != actual_sha256:
            failures.append(f"{label}: SHA-256 mismatch")

        expected_bytes = artifact.get("bytes")
        actual_bytes = artifact_path.stat().st_size
        if expected_bytes != actual_bytes:
            failures.append(f"{label}: byte size mismatch")

    return failures


def validate_written_manifests(
    json_manifest_path: pathlib.Path,
    text_manifest_path: pathlib.Path,
    manifest: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    if not json_manifest_path.exists() or not json_manifest_path.is_file():
        failures.append(f"JSON bundle manifest is missing: {json_manifest_path}")
    if not text_manifest_path.exists() or not text_manifest_path.is_file():
        failures.append(f"text bundle manifest is missing: {text_manifest_path}")
    if failures:
        return failures

    try:
        loaded_manifest = json.loads(json_manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"JSON bundle manifest is unreadable: {exc}"]

    for field in (
        "schema_version",
        "candidate_decision",
        "commit_sha",
        "real_game_validation_matrix_sha256",
        "regression_selftest_summary",
        "artifacts",
    ):
        if loaded_manifest.get(field) != manifest.get(field):
            failures.append(f"JSON bundle manifest field mismatch: {field}")

    text_manifest = text_manifest_path.read_text(encoding="utf-8", errors="replace")
    for marker in (
        "Decision: RC_CANDIDATE_PASS",
        f"Commit SHA: {manifest['commit_sha']}",
        f"Real game validation matrix SHA-256: {manifest['real_game_validation_matrix_sha256']}",
        "BLOCKER:",
        "- none",
    ):
        if marker not in text_manifest:
            failures.append(f"text bundle manifest missing marker: {marker}")

    return failures


def validate_bundle_source_reports(manifest: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    source_reports = manifest.get("source_reports")
    if not isinstance(source_reports, dict):
        return ["final bundle manifest source_reports is missing or invalid"]

    for key in ("smoke", "soak", "regression_log", "real_game_validation_matrix"):
        value = source_reports.get(key)
        if not isinstance(value, str) or not value:
            failures.append(f"source report is missing: {key}")
            continue
        source_path = resolve_bundle_artifact_path(value)
        if not source_path.exists() or not source_path.is_file():
            failures.append(f"source report path is missing: {key}: {source_path}")

    return failures


def collect_warnings(*states: dict[str, Any]) -> list[str]:
    warnings: list[str] = []
    for state in states:
        label = state.get("kind", "evidence")
        for warning in state.get("warnings", []):
            warnings.append(f"{label}: {warning}")
    return warnings


def collect_step_field(field_name: str, *states: dict[str, Any]) -> list[dict[str, Any]]:
    values: list[dict[str, Any]] = []
    for state in states:
        label = state.get("kind", "evidence")
        for step in state.get("steps", []):
            value = step.get(field_name)
            if value is not None:
                values.append({
                    "kind": label,
                    "step": step.get("step"),
                    "value": value,
                })
    return values


def collect_regression_selftest_summary(regression_log: pathlib.Path) -> dict[str, bool]:
    text = regression_log.read_text(encoding="utf-8", errors="replace")
    return {
        "run_release_gate_static_checks_selftest": (
            "[PASS] run_release_gate_static_checks_selftest passed" in text),
        "release_gate_evidence_selftest": (
            "[PASS] release_gate_evidence_selftest passed" in text),
    }


def create_bundle(target: str, regression_log: pathlib.Path) -> pathlib.Path:
    failures: list[str] = []
    failures.extend(verify_rc_candidate.validate_runbooks())
    failures.extend(verify_rc_candidate.validate_regression_log(regression_log))
    failures.extend(verify_rc_candidate.validate_evidence_bundle(target))
    failures.extend(verify_rc_candidate.validate_real_game_matrix())
    if failures:
        for failure in failures:
            print(f"[BLOCKER] RC evidence bundle: {failure}")
        raise SystemExit(1)

    commit = evidence.run_git(["rev-parse", "HEAD"]) or "unknown"
    smoke, soak = latest_reports(target)
    smoke_report, smoke_state = smoke
    soak_report, soak_state = soak
    bundle_dir = create_bundle_dir(commit)
    real_game_matrix = verify_rc_candidate.verify_real_game_validation.DEFAULT_MATRIX

    artifacts: list[dict[str, Any]] = []
    smoke_dir = smoke_report.parent
    soak_dir = soak_report.parent
    artifacts.append({"label": "smoke_json", **copy_file(smoke_report, bundle_dir / "smoke" / "rc_evidence_report.json")})
    artifacts.append({"label": "smoke_text", **copy_file(smoke_dir / "rc_evidence_report.txt", bundle_dir / "smoke" / "rc_evidence_report.txt")})
    artifacts.append({"label": "smoke_state", **copy_file(smoke_dir / evidence.STATE_FILE_NAME, bundle_dir / "smoke" / evidence.STATE_FILE_NAME)})
    artifacts.extend({"label": "smoke_log", **item} for item in copy_directory(smoke_dir / "logs", bundle_dir / "smoke" / "logs"))

    artifacts.append({"label": "soak_json", **copy_file(soak_report, bundle_dir / "soak" / "rc_evidence_report.json")})
    artifacts.append({"label": "soak_text", **copy_file(soak_dir / "rc_evidence_report.txt", bundle_dir / "soak" / "rc_evidence_report.txt")})
    artifacts.append({"label": "soak_state", **copy_file(soak_dir / evidence.STATE_FILE_NAME, bundle_dir / "soak" / evidence.STATE_FILE_NAME)})
    artifacts.extend({"label": "soak_log", **item} for item in copy_directory(soak_dir / "logs", bundle_dir / "soak" / "logs"))

    artifacts.append({"label": "final_regression_log", **copy_file(regression_log, bundle_dir / "final_regression.log")})
    real_game_validation_matrix_artifact = copy_file(
        real_game_matrix,
        bundle_dir / "real_game_validation" / "Game_Verification_Matrix.json")
    artifacts.append({"label": "real_game_validation_matrix", **real_game_validation_matrix_artifact})

    manifest = {
        "schema": BUNDLE_SCHEMA,
        "schema_version": BUNDLE_SCHEMA,
        "schema_hash": evidence.sha256_file(evidence.EVIDENCE_SCHEMA_FILE),
        "candidate_decision": "RC_CANDIDATE_PASS",
        "status": "PASS",
        "target": target,
        "target_process": target,
        "git_commit": commit,
        "commit_sha": commit,
        "branch": smoke_state.get("branch"),
        "git_dirty": smoke_state.get("git_dirty"),
        "dirty_tree": smoke_state.get("dirty_tree"),
        "git_status_short": smoke_state.get("git_status_short"),
        "build_configuration": smoke_state.get("build_configuration"),
        "compiler_version": smoke_state.get("compiler_version"),
        "build_hash": smoke_state.get("build_hash"),
        "exe_sha256": smoke_state.get("exe_sha256"),
        "binary_path": smoke_state.get("binary_path"),
        "binary_sha256": smoke_state.get("binary_sha256"),
        "binary_fingerprint": smoke_state.get("binary_fingerprint"),
        "scheduler_mode": "mixed",
        "shutdown_reason": soak_state.get("shutdown_reason") or smoke_state.get("shutdown_reason"),
        "runtime_validation_status": (
            "FAILED"
            if "FAILED" in (smoke_state.get("runtime_validation_status"), soak_state.get("runtime_validation_status"))
            else "PASSED_OR_INCONCLUSIVE"),
        "rollback_preserved_state_count": (
            int(smoke_state.get("rollback_preserved_state_count") or 0)
            + int(soak_state.get("rollback_preserved_state_count") or 0)),
        "apply_guard_rollback_failure_count": (
            int(smoke_state.get("apply_guard_rollback_failure_count") or 0)
            + int(soak_state.get("apply_guard_rollback_failure_count") or 0)),
        "rollback_failure_transferred_to_shutdown_count": (
            int(smoke_state.get("rollback_failure_transferred_to_shutdown_count") or 0)
            + int(soak_state.get("rollback_failure_transferred_to_shutdown_count") or 0)),
        "blocker_count": 0,
        "warn_count": len(collect_warnings(smoke_state, soak_state)),
        "info_count": len(smoke_state.get("info", [])) + len(soak_state.get("info", [])),
        "rollback_preserved_state_summary": collect_step_field(
            "rollback_preserved_state_summary",
            smoke_state,
            soak_state),
        "shutdown_failure_classification": collect_step_field(
            "shutdown_failure_classification",
            smoke_state,
            soak_state),
        "processor_group_mode_summary": collect_step_field(
            "processor_group_mode_summary",
            smoke_state,
            soak_state),
        "processor_group_mode": (
            smoke_state.get("processor_group_mode", [])
            + soak_state.get("processor_group_mode", [])),
        "background_restriction_mode": (
            smoke_state.get("background_restriction_mode", [])
            + soak_state.get("background_restriction_mode", [])),
        "thread_tracker_telemetry_summary": collect_step_field(
            "thread_tracker_telemetry_summary",
            smoke_state,
            soak_state),
        "thread_tracker_telemetry": (
            smoke_state.get("thread_tracker_telemetry", [])
            + soak_state.get("thread_tracker_telemetry", [])),
        "input_latency_summary": collect_step_field(
            "input_latency_summary",
            smoke_state,
            soak_state),
        "network_irq_summary": collect_step_field(
            "network_irq_summary",
            smoke_state,
            soak_state),
        "access_denied_fallback_summary": collect_step_field(
            "access_denied_fallback_summary",
            smoke_state,
            soak_state),
        "runtime_validation_summary": collect_step_field(
            "runtime_validation_summary",
            smoke_state,
            soak_state),
        "soft_apply_baseline_summary": collect_step_field(
            "soft_apply_baseline_summary",
            smoke_state,
            soak_state),
        "real_game_validation_matrix": real_game_validation_matrix_artifact["path"],
        "real_game_validation_matrix_sha256": real_game_validation_matrix_artifact["sha256"],
        "regression_selftest_summary": collect_regression_selftest_summary(regression_log),
        "severity_policy": evidence.SEVERITY_POLICY,
        "test_results": (
            smoke_state.get("test_results", [])
            + soak_state.get("test_results", [])),
        "created_utc": evidence.utc_now(),
        "warnings": collect_warnings(smoke_state, soak_state),
        "source_reports": {
            "smoke": relative(smoke_report),
            "soak": relative(soak_report),
            "regression_log": relative(regression_log),
            "real_game_validation_matrix": relative(real_game_matrix),
        },
        "artifacts": artifacts,
    }
    artifact_failures = validate_bundle_artifacts(manifest)
    if artifact_failures:
        for failure in artifact_failures:
            print(f"[BLOCKER] RC evidence bundle artifact validation: {failure}")
        raise SystemExit(1)

    source_report_failures = validate_bundle_source_reports(manifest)
    if source_report_failures:
        for failure in source_report_failures:
            print(f"[BLOCKER] RC evidence bundle source report validation: {failure}")
        raise SystemExit(1)

    json_manifest_path = bundle_dir / "rc_evidence_bundle_manifest.json"
    text_manifest_path = bundle_dir / "rc_evidence_bundle_manifest.txt"
    write_json(json_manifest_path, manifest)
    write_text_manifest(text_manifest_path, manifest)
    manifest_failures = validate_written_manifests(
        json_manifest_path,
        text_manifest_path,
        manifest)
    if manifest_failures:
        for failure in manifest_failures:
            print(f"[BLOCKER] RC evidence bundle manifest validation: {failure}")
        raise SystemExit(1)

    print(f"[PASS] RC evidence bundle created: {bundle_dir}")
    return bundle_dir


def main() -> int:
    parser = argparse.ArgumentParser(description="Create the final GameOptimizer v3.0 RC evidence bundle.")
    parser.add_argument("--target", required=True)
    parser.add_argument("--regression-log", required=True, type=pathlib.Path)
    args = parser.parse_args()

    create_bundle(args.target, args.regression_log)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
