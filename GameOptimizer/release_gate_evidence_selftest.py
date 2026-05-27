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
    schema_hash: str | None = None,
    commit: str | None = None,
    exe_sha256: str | None = None,
    omit_binary_sha256: bool = False,
) -> None:
    resolved_commit = commit if commit is not None else evidence.run_git(["rev-parse", "HEAD"]) or "unknown"
    exe_hash = exe_sha256 or sha256_file(exe_path)
    run_dir = evidence.LOG_ROOT / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    state = {
        "schema": schema or evidence.EXPECTED_SCHEMA,
        "schema_version": schema or evidence.EXPECTED_SCHEMA,
        "schema_hash": schema_hash or evidence.sha256_file(evidence.EVIDENCE_SCHEMA_FILE),
        "kind": kind,
        "run_id": run_id,
        "status": "PASS",
        "started_utc": evidence.utc_now(),
        "finished_utc": evidence.utc_now(),
        "target": target,
        "target_process": target,
        "exe_path": str(exe_path),
        "binary_path": str(exe_path),
        "exe_sha256": exe_hash,
        "binary_sha256": None if omit_binary_sha256 else exe_hash,
        "binary_fingerprint": {
            "path": str(exe_path),
            "sha256": exe_hash,
            "bytes": exe_path.stat().st_size,
        },
        "git_commit": resolved_commit,
        "commit_sha": resolved_commit,
        "branch": "synthetic",
        "build_hash": evidence.run_git(["rev-parse", "HEAD^{tree}"]),
        "build_configuration": evidence.DEFAULT_BUILD_CONFIGURATION,
        "compiler_version": "synthetic",
        "git_dirty": False,
        "dirty_tree": False,
        "git_status_short": [],
        "scheduler_mode": "synthetic",
        "shutdown_reason": "Unknown",
        "runtime_validation_status": "UNKNOWN",
        "rollback_preserved_state_count": 0,
        "apply_guard_rollback_failure_count": 0,
        "rollback_failure_transferred_to_shutdown_count": 0,
        "blocker_count": 0,
        "warn_count": 0,
        "info_count": 0,
        "processor_group_mode": [],
        "background_restriction_mode": [],
        "thread_tracker_telemetry": [],
        "test_results": evidence.test_results({"steps": steps}),
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
        "schema_version": evidence.EXPECTED_SCHEMA,
        "schema_hash": evidence.sha256_file(evidence.EVIDENCE_SCHEMA_FILE),
        "kind": kind,
        "run_id": run_id,
        "status": "RUNNING",
        "started_utc": evidence.utc_now(),
        "finished_utc": None,
        "target": target,
        "target_process": target,
        "exe_path": str(exe_path),
        "binary_path": str(exe_path),
        "exe_sha256": sha256_file(exe_path),
        "binary_sha256": sha256_file(exe_path),
        "binary_fingerprint": {
            "path": str(exe_path),
            "sha256": sha256_file(exe_path),
            "bytes": exe_path.stat().st_size,
        },
        "git_commit": commit,
        "commit_sha": commit,
        "branch": "synthetic",
        "build_hash": evidence.run_git(["rev-parse", "HEAD^{tree}"]),
        "build_configuration": evidence.DEFAULT_BUILD_CONFIGURATION,
        "compiler_version": "synthetic",
        "git_dirty": False,
        "dirty_tree": False,
        "git_status_short": [],
        "scheduler_mode": "synthetic",
        "shutdown_reason": "Unknown",
        "runtime_validation_status": "UNKNOWN",
        "rollback_preserved_state_count": 0,
        "apply_guard_rollback_failure_count": 0,
        "rollback_failure_transferred_to_shutdown_count": 0,
        "blocker_count": 0,
        "warn_count": 0,
        "info_count": 0,
        "processor_group_mode": [],
        "background_restriction_mode": [],
        "thread_tracker_telemetry": [],
        "test_results": [],
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


def assert_shutdown_reason_and_soft_apply_baseline_are_recorded(run_id: str, exe_path: pathlib.Path) -> bool:
    run_dir = evidence.LOG_ROOT / run_id
    make_running_state("smoke", run_id, f"{TARGET}.shutdown_reason", exe_path)
    log_path = run_dir / "logs" / "shutdown_reason.log"
    log_path.write_text(
        "\n".join([
            "[INFO] soft-apply validated scheduling baseline captured for TID 42 (audit-only, not stored as rollback state; affinity=0x3, group=0, priority=0, creationTime100ns=100)",
            "[INFO] runtime validation summary: cycles=1, minimum_required=1, minimum_satisfied=true, main_detected_cycles=1, main_policy_applied_cycles=1, decision_commands=0, feedback_commands=0, dispatch_failures=0, rollback_requests=0, thread_tracker_reset_events=0, high_rtt_cycles=0, high_dpc_cycles=0, high_migration_cycles=0, consecutive_no_main_cycles=0, critical_failure=false",
            "[INFO] shutdown result: reason=MaxRuntimeHardTimeout, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
        ]) + "\n",
        encoding="utf-8")
    record_result = evidence.record_step(argparse.Namespace(
        run_dir=str(run_dir),
        step="shutdown_reason",
        mode="timeout",
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
    text_report = (run_dir / "rc_evidence_report.txt").read_text(encoding="utf-8")
    step = report["steps"][0]
    return (
        record_result == 0
        and finalize_result == 0
        and step["shutdown_failure_classification"]["shutdown_reason"] == "MaxRuntimeHardTimeout"
        and step["soft_apply_baseline_summary"]["thread_baseline_count"] == 1
        and "shutdown failure classification:" in text_report
        and "shutdown_reason': 'MaxRuntimeHardTimeout" in text_report
        and "soft-apply baseline summary:" in text_report
    )


def assert_apply_guard_rollback_failure_is_blocked(run_id: str, exe_path: pathlib.Path) -> bool:
    run_dir = evidence.LOG_ROOT / run_id
    make_running_state("smoke", run_id, f"{TARGET}.apply_guard_failure", exe_path)
    log_path = run_dir / "logs" / "apply_guard_failure.log"
    log_path.write_text(
        "\n".join([
            "[ERROR] apply guard explicit rollback failed for target 42; rollback responsibility transferred to ShutdownPipeline/RollbackManager; rollback state remains preserved: rollback failed",
            "[INFO] shutdown result: reason=PolicyRollbackRequest, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
        ]) + "\n",
        encoding="utf-8")
    record_result = evidence.record_step(argparse.Namespace(
        run_dir=str(run_dir),
        step="apply_guard_failure",
        mode="apply",
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
    text_report = (run_dir / "rc_evidence_report.txt").read_text(encoding="utf-8")
    return (
        record_result == 0
        and finalize_result == 1
        and report["status"] == "FAIL"
        and report["apply_guard_rollback_failure_count"] == 1
        and report["rollback_failure_transferred_to_shutdown_count"] == 1
        and report["blocker_count"] > 0
        and any("ApplyGuard rollback failure" in blocker for blocker in report["blockers"])
        and "ApplyGuard rollback failure count: 1" in text_report
        and "Rollback failure transferred to shutdown count: 1" in text_report
    )


def assert_apply_guard_rollback_failure_without_transfer_is_blocked(
    run_id: str,
    exe_path: pathlib.Path) -> bool:
    finalize_result, report, text_report = finalize_single_step_report(
        run_id,
        "apply_guard_missing_transfer",
        exe_path,
        [
            "[ERROR] apply guard explicit rollback failed for target 42; rollback state remains preserved: rollback failed",
            "[INFO] shutdown result: reason=PolicyRollbackRequest, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
        ],
        step_name="apply_guard_missing_transfer",
        mode="apply",
    )
    blockers = report.get("blockers", [])
    return (
        finalize_result == 1
        and report.get("status") == "FAIL"
        and report.get("apply_guard_rollback_failure_count") == 1
        and report.get("rollback_failure_transferred_to_shutdown_count") == 0
        and report.get("blocker_count") == 2
        and sum("ApplyGuard rollback failure" in blocker for blocker in blockers) == 1
        and sum("ApplyGuard explicit rollback failure was not fully transferred" in blocker for blocker in blockers) == 1
        and "ApplyGuard rollback failure count: 1" in text_report
        and "Rollback failure transferred to shutdown count: 0" in text_report
    )


def assert_apply_guard_rollback_failure_does_not_duplicate_transfer_blocker(
    run_id: str,
    exe_path: pathlib.Path) -> bool:
    finalize_result, report, _text_report = finalize_single_step_report(
        run_id,
        "apply_guard_transfer_present",
        exe_path,
        [
            "[ERROR] apply guard explicit rollback failed for target 42; rollback responsibility transferred to ShutdownPipeline/RollbackManager; rollback state remains preserved: rollback failed",
            "[INFO] shutdown result: reason=PolicyRollbackRequest, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
        ],
        step_name="apply_guard_transfer_present",
        mode="apply",
    )
    blockers = report.get("blockers", [])
    return (
        finalize_result == 1
        and report.get("status") == "FAIL"
        and report.get("apply_guard_rollback_failure_count") == 1
        and report.get("rollback_failure_transferred_to_shutdown_count") == 1
        and report.get("blocker_count") == 1
        and sum("ApplyGuard rollback failure" in blocker for blocker in blockers) == 1
        and not any("ApplyGuard explicit rollback failure was not fully transferred" in blocker for blocker in blockers)
    )


def finalize_single_step_report(
    run_id: str,
    target_suffix: str,
    exe_path: pathlib.Path,
    log_lines: list[str],
    *,
    exit_code: int = 0,
    assertion_exit_code: int = 0,
    step_name: str = "failure_injection",
    mode: str = "synthetic",
) -> tuple[int, dict[str, object], str]:
    run_dir = evidence.LOG_ROOT / run_id
    make_running_state("smoke", run_id, f"{TARGET}.{target_suffix}", exe_path)
    log_path = run_dir / "logs" / f"{step_name}.log"
    log_path.write_text("\n".join(log_lines) + "\n", encoding="utf-8")
    record_result = evidence.record_step(argparse.Namespace(
        run_dir=str(run_dir),
        step=step_name,
        mode=mode,
        log_file=str(log_path),
        exit_code=exit_code,
        assertion_exit_code=assertion_exit_code,
        command="synthetic",
    ))
    finalize_result = evidence.finalize_evidence(argparse.Namespace(
        run_dir=str(run_dir),
        require_soak_both=False,
    ))
    report = evidence.read_json(run_dir / "rc_evidence_report.json")
    return finalize_result if record_result == 0 else 1, report, (run_dir / "rc_evidence_report.txt").read_text(
        encoding="utf-8")


def assert_blocker_injection(
    run_id: str,
    target_suffix: str,
    exe_path: pathlib.Path,
    log_lines: list[str],
    expected_marker: str,
    *,
    exit_code: int = 0,
    assertion_exit_code: int = 0,
    step_name: str = "failure_injection",
) -> bool:
    finalize_result, report, text_report = finalize_single_step_report(
        run_id,
        target_suffix,
        exe_path,
        log_lines,
        exit_code=exit_code,
        assertion_exit_code=assertion_exit_code,
        step_name=step_name,
    )
    blockers = report.get("blockers", [])
    return (
        finalize_result == 1
        and report.get("status") == "FAIL"
        and report.get("severity_summary", {}).get("BLOCKER", 0) > 0
        and any(expected_marker in blocker for blocker in blockers)
        and expected_marker in text_report
    )


def assert_warning_injection(
    run_id: str,
    target_suffix: str,
    exe_path: pathlib.Path,
    log_lines: list[str],
    expected_marker: str,
    *,
    step_name: str = "warning_injection",
) -> bool:
    finalize_result, report, text_report = finalize_single_step_report(
        run_id,
        target_suffix,
        exe_path,
        log_lines,
        step_name=step_name,
    )
    warnings = report.get("warnings", [])
    return (
        finalize_result == 0
        and report.get("status") == "PASS_WITH_WARNINGS"
        and report.get("severity_summary", {}).get("BLOCKER", 0) == 0
        and report.get("severity_summary", {}).get("WARN", 0) > 0
        and any(expected_marker in warning for warning in warnings)
        and expected_marker in text_report
    )


def assert_info_injection(
    run_id: str,
    target_suffix: str,
    exe_path: pathlib.Path,
    log_lines: list[str],
    expected_marker: str,
    *,
    step_name: str = "info_injection",
) -> bool:
    finalize_result, report, text_report = finalize_single_step_report(
        run_id,
        target_suffix,
        exe_path,
        log_lines,
        step_name=step_name,
    )
    return (
        finalize_result == 0
        and report.get("status") == "PASS"
        and report.get("severity_summary", {}).get("BLOCKER", 0) == 0
        and report.get("severity_summary", {}).get("WARN", 0) == 0
        and report.get("severity_summary", {}).get("INFO", 0) > 0
        and expected_marker in text_report
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
    schema_hash_prefix = f"synthetic_selftest_schema_hash_{unique_id}"
    commit_prefix = f"synthetic_selftest_commit_{unique_id}"
    hash_prefix = f"synthetic_selftest_hash_{unique_id}"
    missing_binary_sha_prefix = f"synthetic_selftest_missing_binary_sha_{unique_id}"
    reason_run_id = f"synthetic_selftest_reason_{unique_id}"
    apply_guard_failure_run_id = f"synthetic_selftest_apply_guard_failure_{unique_id}"
    apply_guard_missing_transfer_run_id = f"synthetic_selftest_apply_guard_missing_transfer_{unique_id}"
    apply_guard_transfer_present_run_id = f"synthetic_selftest_apply_guard_transfer_present_{unique_id}"
    runtime_validation_failure_run_id = f"synthetic_selftest_runtime_validation_failure_{unique_id}"
    rollback_failure_run_id = f"synthetic_selftest_rollback_failure_{unique_id}"
    thread_group_audit_query_run_id = f"synthetic_selftest_thread_group_audit_query_{unique_id}"
    thread_group_audit_mismatch_run_id = f"synthetic_selftest_thread_group_audit_mismatch_{unique_id}"
    access_denied_run_id = f"synthetic_selftest_access_denied_{unique_id}"
    group_one_run_id = f"synthetic_selftest_group_one_{unique_id}"
    timeline_run_id = f"synthetic_selftest_timeline_{unique_id}"
    heartbeat_run_id = f"synthetic_selftest_heartbeat_{unique_id}"
    unsafe_discard_run_id = f"synthetic_selftest_unsafe_discard_{unique_id}"
    info_run_id = f"synthetic_selftest_info_{unique_id}"
    synthetic_exe = evidence.LOG_ROOT / f"synthetic_selftest_exe_{unique_id}.bin"
    synthetic_exe.write_bytes(b"synthetic executable bytes")
    created_dirs = [
        evidence.LOG_ROOT / warn_run_id,
        evidence.LOG_ROOT / f"{valid_prefix}_smoke",
        evidence.LOG_ROOT / f"{valid_prefix}_soak",
        evidence.LOG_ROOT / f"{schema_prefix}_smoke",
        evidence.LOG_ROOT / f"{schema_prefix}_soak",
        evidence.LOG_ROOT / f"{schema_hash_prefix}_smoke",
        evidence.LOG_ROOT / f"{schema_hash_prefix}_soak",
        evidence.LOG_ROOT / f"{commit_prefix}_smoke",
        evidence.LOG_ROOT / f"{commit_prefix}_soak",
        evidence.LOG_ROOT / f"{hash_prefix}_smoke",
        evidence.LOG_ROOT / f"{hash_prefix}_soak",
        evidence.LOG_ROOT / f"{missing_binary_sha_prefix}_smoke",
        evidence.LOG_ROOT / f"{missing_binary_sha_prefix}_soak",
        evidence.LOG_ROOT / reason_run_id,
        evidence.LOG_ROOT / apply_guard_failure_run_id,
        evidence.LOG_ROOT / apply_guard_missing_transfer_run_id,
        evidence.LOG_ROOT / apply_guard_transfer_present_run_id,
        evidence.LOG_ROOT / runtime_validation_failure_run_id,
        evidence.LOG_ROOT / rollback_failure_run_id,
        evidence.LOG_ROOT / thread_group_audit_query_run_id,
        evidence.LOG_ROOT / thread_group_audit_mismatch_run_id,
        evidence.LOG_ROOT / access_denied_run_id,
        evidence.LOG_ROOT / group_one_run_id,
        evidence.LOG_ROOT / timeline_run_id,
        evidence.LOG_ROOT / heartbeat_run_id,
        evidence.LOG_ROOT / unsafe_discard_run_id,
        evidence.LOG_ROOT / info_run_id,
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

        schema_hash_target = f"{TARGET}.schema_hash_mismatch"
        make_complete_pair(schema_hash_target, schema_hash_prefix, synthetic_exe, schema_hash="bad-schema-hash")
        if run_verify(schema_hash_target) == 0:
            print("[FAIL] RC evidence self-test: schema hash mismatch unexpectedly passed")
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

        missing_binary_sha_target = f"{TARGET}.missing_binary_sha256"
        make_complete_pair(
            missing_binary_sha_target,
            missing_binary_sha_prefix,
            synthetic_exe,
            omit_binary_sha256=True,
        )
        if run_verify(missing_binary_sha_target) == 0:
            print("[FAIL] RC evidence self-test: missing binary SHA-256 unexpectedly passed")
            return 1

        if not assert_warn_only_report_is_not_blocked(warn_run_id, synthetic_exe):
            print("[FAIL] RC evidence self-test: WARN-only evidence did not produce PASS_WITH_WARNINGS")
            return 1

        if not assert_shutdown_reason_and_soft_apply_baseline_are_recorded(reason_run_id, synthetic_exe):
            print("[FAIL] RC evidence self-test: shutdown reason or SoftApply baseline evidence was not recorded")
            return 1

        if not assert_apply_guard_rollback_failure_is_blocked(apply_guard_failure_run_id, synthetic_exe):
            print("[FAIL] RC evidence self-test: ApplyGuard rollback failure did not become a BLOCKER")
            return 1

        if not assert_apply_guard_rollback_failure_without_transfer_is_blocked(
            apply_guard_missing_transfer_run_id,
            synthetic_exe):
            print("[FAIL] RC evidence self-test: ApplyGuard rollback failure without transfer did not add transfer BLOCKER")
            return 1

        if not assert_apply_guard_rollback_failure_does_not_duplicate_transfer_blocker(
            apply_guard_transfer_present_run_id,
            synthetic_exe):
            print("[FAIL] RC evidence self-test: ApplyGuard rollback failure with transfer duplicated transfer BLOCKER")
            return 1

        if not assert_blocker_injection(
            runtime_validation_failure_run_id,
            "runtime_validation_failure",
            synthetic_exe,
            [
                "[ERROR] runtime validation result: FAILED",
                "[INFO] shutdown result: reason=RuntimeValidationFailure, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=true, rollbackStatePreserved=false",
            ],
            "runtime validation FAILED",
            exit_code=1,
            step_name="runtime_validation_failed",
        ):
            print("[FAIL] RC evidence self-test: runtime validation FAILED did not become a BLOCKER with exit code 1")
            return 1

        if not assert_blocker_injection(
            rollback_failure_run_id,
            "rollback_failure",
            synthetic_exe,
            [
                "[ERROR] shutdown rollback failed",
                "[INFO] preserved rollback state count: thread=1, process=0",
                "[INFO] shutdown result: reason=PolicyRollbackRequest, timerRollbackFailed=false, schedulerRollbackFailed=true, runtimeValidationFailed=false, rollbackStatePreserved=true",
            ],
            "shutdown preserved rollback state",
            step_name="rollback_failure",
        ):
            print("[FAIL] RC evidence self-test: rollback failure did not become a BLOCKER")
            return 1

        if not assert_blocker_injection(
            thread_group_audit_query_run_id,
            "thread_group_audit_query_failure",
            synthetic_exe,
            [
                "[ERROR] SetThreadGroupAffinity failed; post-failure audit query failed for TID 42",
                "[INFO] shutdown result: reason=PolicyRollbackRequest, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
            ],
            "SetThreadGroupAffinity failure post-failure audit query failed",
            step_name="thread_group_audit_query_failure",
        ):
            print("[FAIL] RC evidence self-test: SetThreadGroupAffinity audit query failure did not become a BLOCKER")
            return 1

        if not assert_blocker_injection(
            thread_group_audit_mismatch_run_id,
            "thread_group_audit_mismatch",
            synthetic_exe,
            [
                "[ERROR] SetThreadGroupAffinity failed; post-failure audit found state drift for TID 42",
                "[INFO] shutdown result: reason=PolicyRollbackRequest, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
            ],
            "SetThreadGroupAffinity failure post-failure audit mismatch",
            step_name="thread_group_audit_mismatch",
        ):
            print("[FAIL] RC evidence self-test: SetThreadGroupAffinity audit mismatch did not become a BLOCKER")
            return 1

        if not assert_blocker_injection(
            timeline_run_id,
            "timeline_monotonicity_failure",
            synthetic_exe,
            [
                "[ERROR] runtime validation sample timeline is not monotonic: cycle 7 followed by 3",
                "[INFO] shutdown result: reason=RuntimeValidationFailure, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
            ],
            "timeline monotonicity failure",
            step_name="timeline_monotonicity_failure",
        ):
            print("[FAIL] RC evidence self-test: timeline monotonicity failure did not become a BLOCKER")
            return 1

        if not assert_blocker_injection(
            heartbeat_run_id,
            "heartbeat_progression_failure",
            synthetic_exe,
            [
                "[ERROR] heartbeat progression failure: watchdog heartbeat stopped advancing",
                "[INFO] shutdown result: reason=WatchdogFailure, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
            ],
            "heartbeat progression failure",
            step_name="heartbeat_progression_failure",
        ):
            print("[FAIL] RC evidence self-test: heartbeat progression failure did not become a BLOCKER")
            return 1

        if not assert_blocker_injection(
            unsafe_discard_run_id,
            "unsafe_rollback_state_discard",
            synthetic_exe,
            [
                "[ERROR] unsafe rollback state discard regression: discarded rollback state without post-failure audit",
                "[INFO] shutdown result: reason=PolicyRollbackRequest, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
            ],
            "unsafe rollback state discard",
            step_name="unsafe_rollback_state_discard",
        ):
            print("[FAIL] RC evidence self-test: unsafe rollback state discard did not become a BLOCKER")
            return 1

        if not assert_warning_injection(
            access_denied_run_id,
            "access_denied_fallback",
            synthetic_exe,
            [
                "[WARN] OpenProcess failure: Access Denied; recoverable access limitation, monitoring-only fallback remains active",
                "[INFO] rollback path was invoked when needed",
                "[INFO] shutdown result: reason=ConsoleControl, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
            ],
            "Access Denied/access-boundary fallback evidence lines=1",
            step_name="access_denied_fallback",
        ):
            print("[FAIL] RC evidence self-test: Access Denied fallback did not remain WARN-only")
            return 1

        if not assert_warning_injection(
            group_one_run_id,
            "group_one_mock",
            synthetic_exe,
            [
                "[WARN] background restriction blocked: processor group 1 is selected, but process-wide SetProcessAffinityMask is only safe for group 0 policies",
                "[WARN] background restriction evidence: background_restriction_mode=monitoring_only_due_to_processor_group, blocked_processor_group=1, process_wide_affinity_supported=false",
                "[INFO] topology fallback policy selected from process affinity: group=1, mask_provenance=process_affinity_fallback",
                "[INFO] shutdown result: reason=ConsoleControl, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
            ],
            "Processor Group 1+ background restriction recorded as monitoring-only",
            step_name="group_one_mock",
        ):
            print("[FAIL] RC evidence self-test: group 1+ mock did not remain WARN-only")
            return 1

        if not assert_info_injection(
            info_run_id,
            "telemetry_info",
            synthetic_exe,
            [
                "[INFO] thread tracker telemetry: open_thread_failures=0, open_thread_access_denied=0, get_thread_times_failures=0, get_thread_times_access_denied=0, reset_event=false, total_open_thread_failures=0, total_open_thread_access_denied=0, total_get_thread_times_failures=0, total_reset_events=0",
                "[INFO] shutdown result: reason=ConsoleControl, timerRollbackFailed=false, schedulerRollbackFailed=false, runtimeValidationFailed=false, rollbackStatePreserved=false",
            ],
            "thread_tracker_telemetry_summary",
            step_name="telemetry_info",
        ):
            print("[FAIL] RC evidence self-test: telemetry INFO did not remain INFO-only")
            return 1

        print("[PASS] RC evidence self-test passed")
        return 0
    finally:
        for run_dir in created_dirs:
            shutil.rmtree(run_dir, ignore_errors=True)
        synthetic_exe.unlink(missing_ok=True)


if __name__ == "__main__":
    raise SystemExit(main())
