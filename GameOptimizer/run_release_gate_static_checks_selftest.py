from __future__ import annotations

import json
import tempfile

import create_rc_evidence_bundle as bundle
import run_release_gate_static_checks as static_checks
import verify_rc_candidate


ORDERED_MARKERS = [
    "echo [RC-1] git diff whitespace gate",
    "echo [RC-2] Python syntax gate",
    "echo [RC-3] static release gate",
    "echo [RC-4] evidence self-test",
    "echo [RC-5] Release x64 MSVC build",
    "echo [RC-6] full regression",
    "echo [RC-7] release smoke",
]


def test_ordered_markers_pass() -> None:
    text = "\n".join(ORDERED_MARKERS)
    failures = static_checks.validate_ordered_markers("selftest", text, ORDERED_MARKERS)
    assert failures == []


def test_ordered_markers_reject_missing_marker() -> None:
    text = "\n".join([ORDERED_MARKERS[0], ORDERED_MARKERS[2]])
    failures = static_checks.validate_ordered_markers("selftest", text, ORDERED_MARKERS)
    assert any("missing ordered step marker" in failure for failure in failures)


def test_ordered_markers_reject_out_of_order_marker() -> None:
    text = "\n".join([ORDERED_MARKERS[1], ORDERED_MARKERS[0], ORDERED_MARKERS[2]])
    failures = static_checks.validate_ordered_markers("selftest", text, ORDERED_MARKERS)
    assert any("ordered step is out of sequence" in failure for failure in failures)


def test_markdown_section_contains_marker_is_section_scoped() -> None:
    text = "\n".join([
        "# Release Blocker List",
        "",
        "## BLOCKER",
        "- blocker item",
        "",
        "## WARN",
        "- warn item",
    ])
    assert static_checks.markdown_section_contains_marker(text, "BLOCKER", "blocker item")
    assert not static_checks.markdown_section_contains_marker(text, "BLOCKER", "warn item")
    assert static_checks.markdown_section_contains_marker(text, "WARN", "warn item")
    assert not static_checks.markdown_section_contains_marker(text, "MISSING", "blocker item")


def test_rc_gate_draft_excludes_long_soak_and_real_game_validation() -> None:
    rc_text = static_checks.ROOT.joinpath("run_rc_gate.bat").read_text(
        encoding="utf-8",
        errors="replace")
    assert "draft RC gate excludes 30m dry-run soak, 60m soft-apply soak" in rc_text
    assert "run_long_soak_presets.bat" not in rc_text
    assert "verify_real_game_validation.py --matrix docs\\release\\Game_Verification_Matrix.json" not in rc_text
    assert "verify_rc_candidate.py" not in rc_text
    assert "create_rc_evidence_bundle.py" not in rc_text


def test_standalone_soak_entrypoints_are_contract_checked() -> None:
    failures = static_checks.check_long_soak_automation_contract()
    assert failures == []
    dry_run_text = static_checks.DRY_RUN_SOAK_30M_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    soft_apply_text = static_checks.SOFT_APPLY_SOAK_60M_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    assert "run_long_soak_presets.bat \"%TARGET%\" 30m" in dry_run_text
    assert "run_long_soak_presets.bat \"%TARGET%\" 60m" in soft_apply_text
    assert "heartbeat progression" in dry_run_text
    assert "heartbeat progression" in soft_apply_text


def test_real_game_validation_order_for_full_candidate_flow() -> None:
    ordered_markers = [
        "echo [RC-9] verify RC candidate package inputs",
        "verify_real_game_validation.py --matrix docs\\release\\Game_Verification_Matrix.json",
        "verify_rc_candidate.py",
    ]
    text = "\n".join(ordered_markers)
    failures = static_checks.validate_ordered_markers("selftest", text, ordered_markers)
    assert failures == []


def test_bundle_creation_validates_real_game_matrix_before_writing_bundle() -> None:
    bundle_text = static_checks.CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    ordered_markers = [
        "verify_rc_candidate.validate_evidence_bundle(target)",
        "verify_rc_candidate.validate_real_game_matrix()",
        "create_bundle_dir(commit)",
    ]
    failures = static_checks.validate_ordered_markers("selftest", bundle_text, ordered_markers)
    assert failures == []


def test_bundle_manifest_preserves_real_game_matrix_artifact() -> None:
    bundle_text = static_checks.CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    for marker in [
        "real_game_matrix = verify_rc_candidate.verify_real_game_validation.DEFAULT_MATRIX",
        "real_game_validation_matrix_artifact = copy_file",
        "\"real_game_validation_matrix\"",
        "\"real_game_validation_matrix_sha256\"",
        "\"label\": \"real_game_validation_matrix\"",
    ]:
        assert marker in bundle_text


def test_bundle_creation_validates_manifest_artifact_hashes_before_pass() -> None:
    bundle_text = static_checks.CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    for marker in [
        "def validate_bundle_artifacts",
        "resolve_bundle_artifact_path",
        "SHA-256 mismatch",
        "byte size mismatch",
        "RC evidence bundle artifact validation",
    ]:
        assert marker in bundle_text

    ordered_markers = [
        "artifact_failures = validate_bundle_artifacts(manifest)",
        "json_manifest_path = bundle_dir / \"rc_evidence_bundle_manifest.json\"",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]
    failures = static_checks.validate_ordered_markers("selftest", bundle_text, ordered_markers)
    assert failures == []


def test_bundle_creation_validates_written_manifests_before_pass() -> None:
    bundle_text = static_checks.CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    for marker in [
        "def validate_written_manifests",
        "\"schema\"",
        "\"schema_hash\"",
        "JSON bundle manifest field mismatch",
        "Schema hash:",
        "Binary SHA-256:",
        "text bundle manifest missing marker",
        "RC evidence bundle manifest validation",
    ]:
        assert marker in bundle_text

    ordered_markers = [
        "write_json(json_manifest_path, manifest)",
        "write_text_manifest(text_manifest_path, manifest)",
        "manifest_failures = validate_written_manifests(",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]
    failures = static_checks.validate_ordered_markers("selftest", bundle_text, ordered_markers)
    assert failures == []


def test_bundle_creation_validates_source_reports_before_pass() -> None:
    bundle_text = static_checks.CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    for marker in [
        "def validate_bundle_source_reports",
        "source report path is missing",
        "RC evidence bundle source report validation",
        "\"smoke\", \"soak\", \"regression_log\", \"real_game_validation_matrix\"",
    ]:
        assert marker in bundle_text

    ordered_markers = [
        "source_report_failures = validate_bundle_source_reports(manifest)",
        "write_json(json_manifest_path, manifest)",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]
    failures = static_checks.validate_ordered_markers("selftest", bundle_text, ordered_markers)
    assert failures == []


def test_bundle_creation_validates_source_report_artifact_identity_before_pass() -> None:
    bundle_text = static_checks.CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    for marker in [
        "def validate_bundle_source_report_artifact_identity",
        "(\"smoke\", \"smoke_json\")",
        "(\"soak\", \"soak_json\")",
        "(\"regression_log\", \"final_regression_log\")",
        "(\"real_game_validation_matrix\", \"real_game_validation_matrix\")",
        "source report artifact SHA-256 mismatch",
        "RC evidence bundle source report artifact validation",
    ]:
        assert marker in bundle_text

    ordered_markers = [
        "source_identity_failures = validate_bundle_source_report_artifact_identity(manifest)",
        "regression_selftest_failures = validate_bundled_regression_selftest_summary(manifest)",
        "write_json(json_manifest_path, manifest)",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]
    failures = static_checks.validate_ordered_markers("selftest", bundle_text, ordered_markers)
    assert failures == []


def test_bundle_creation_validates_regression_selftest_summary_against_artifact_before_pass() -> None:
    bundle_text = static_checks.CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    for marker in [
        "def validate_bundled_regression_selftest_summary",
        "find_bundle_artifact(manifest, \"final_regression_log\")",
        "collect_regression_selftest_summary_from_text",
        "regression selftest summary mismatch",
        "RC evidence bundle regression selftest validation",
    ]:
        assert marker in bundle_text

    ordered_markers = [
        "regression_selftest_failures = validate_bundled_regression_selftest_summary(manifest)",
        "write_json(json_manifest_path, manifest)",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]
    failures = static_checks.validate_ordered_markers("selftest", bundle_text, ordered_markers)
    assert failures == []


def test_bundle_validators_accept_real_files() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = bundle.pathlib.Path(tmp)
        artifact_path = root / "artifact.txt"
        artifact_path.write_text("artifact", encoding="utf-8")
        source_path = root / "source.txt"
        source_path.write_text("source", encoding="utf-8")
        json_manifest_path = root / "rc_evidence_bundle_manifest.json"
        text_manifest_path = root / "rc_evidence_bundle_manifest.txt"
        manifest = {
            "schema": "gameoptimizer.rc_evidence_bundle.v1",
            "schema_version": "gameoptimizer.rc_evidence_bundle.v1",
            "schema_hash": "schema-sha",
            "candidate_decision": "RC_CANDIDATE_PASS",
            "status": "PASS",
            "target": "target.exe",
            "target_process": "target.exe",
            "git_commit": "abc123",
            "commit_sha": "abc123",
            "build_hash": "build-sha",
            "binary_sha256": "binary-sha",
            "real_game_validation_matrix_sha256": "matrix-sha",
            "regression_selftest_summary": {
                "run_release_gate_static_checks_selftest": True,
                "release_gate_evidence_selftest": True,
            },
            "artifacts": [{
                "label": "artifact",
                "path": str(artifact_path),
                "sha256": bundle.evidence.sha256_file(artifact_path),
                "bytes": artifact_path.stat().st_size,
            }],
            "source_reports": {
                "smoke": str(source_path),
                "soak": str(source_path),
                "regression_log": str(source_path),
                "real_game_validation_matrix": str(source_path),
            },
        }
        json_manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        text_manifest_path.write_text(
            "\n".join([
                "Decision: RC_CANDIDATE_PASS",
                "Schema: gameoptimizer.rc_evidence_bundle.v1",
                "Schema hash: schema-sha",
                "Status: PASS",
                "Target: target.exe",
                "Target process: target.exe",
                "Git commit: abc123",
                "Commit SHA: abc123",
                "Build hash: build-sha",
                "Binary SHA-256: binary-sha",
                "Real game validation matrix SHA-256: matrix-sha",
                "Regression selftest summary:",
                "run_release_gate_static_checks_selftest",
                "release_gate_evidence_selftest",
                "BLOCKER:",
                "- none",
            ]),
            encoding="utf-8")

        assert bundle.validate_bundle_artifacts(manifest) == []
        assert bundle.validate_bundle_source_reports(manifest) == []
        assert bundle.validate_written_manifests(
            json_manifest_path,
            text_manifest_path,
            manifest) == []


def test_bundle_regression_selftest_summary_matches_bundled_log() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = bundle.pathlib.Path(tmp)
        regression_log = root / "final_regression.log"
        regression_log.write_text(
            "\n".join([
                "[PASS] run_release_gate_static_checks_selftest passed",
                "[PASS] release_gate_evidence_selftest passed",
                "[INFO] regression summary: total=11, failed=0",
                "[PASS] all regression tests passed",
            ]),
            encoding="utf-8")
        manifest = {
            "regression_selftest_summary": {
                "run_release_gate_static_checks_selftest": True,
                "release_gate_evidence_selftest": True,
            },
            "artifacts": [{
                "label": "final_regression_log",
                "path": str(regression_log),
                "sha256": bundle.evidence.sha256_file(regression_log),
                "bytes": regression_log.stat().st_size,
            }],
        }

        assert bundle.validate_bundled_regression_selftest_summary(manifest) == []

        mismatched_manifest = dict(manifest)
        mismatched_manifest["regression_selftest_summary"] = {
            "run_release_gate_static_checks_selftest": False,
            "release_gate_evidence_selftest": True,
        }
        failures = bundle.validate_bundled_regression_selftest_summary(mismatched_manifest)
        assert any("regression selftest summary mismatch" in failure for failure in failures)


def test_bundle_source_report_artifact_identity_matches_real_files() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = bundle.pathlib.Path(tmp)
        source_paths = {
            "smoke": root / "source-smoke.json",
            "soak": root / "source-soak.json",
            "regression_log": root / "source-final-regression.log",
            "real_game_validation_matrix": root / "source-matrix.json",
        }
        artifact_labels = {
            "smoke": "smoke_json",
            "soak": "soak_json",
            "regression_log": "final_regression_log",
            "real_game_validation_matrix": "real_game_validation_matrix",
        }
        artifacts = []
        for key, source_path in source_paths.items():
            source_path.write_text(f"{key} source content", encoding="utf-8")
            artifact_path = root / f"artifact-{source_path.name}"
            artifact_path.write_text(source_path.read_text(encoding="utf-8"), encoding="utf-8")
            artifacts.append({
                "label": artifact_labels[key],
                "path": str(artifact_path),
                "sha256": bundle.evidence.sha256_file(artifact_path),
                "bytes": artifact_path.stat().st_size,
            })

        manifest = {
            "source_reports": {key: str(path) for key, path in source_paths.items()},
            "artifacts": artifacts,
        }
        assert bundle.validate_bundle_source_report_artifact_identity(manifest) == []

        source_paths["soak"].write_text("mutated source content", encoding="utf-8")
        failures = bundle.validate_bundle_source_report_artifact_identity(manifest)
        assert any("source report artifact SHA-256 mismatch" in failure for failure in failures)


def test_bundle_validators_reject_missing_or_mismatched_files() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = bundle.pathlib.Path(tmp)
        artifact_path = root / "artifact.txt"
        artifact_path.write_text("artifact", encoding="utf-8")
        source_path = root / "source.txt"
        source_path.write_text("source", encoding="utf-8")
        json_manifest_path = root / "rc_evidence_bundle_manifest.json"
        text_manifest_path = root / "rc_evidence_bundle_manifest.txt"
        manifest = {
            "schema": "gameoptimizer.rc_evidence_bundle.v1",
            "schema_version": "gameoptimizer.rc_evidence_bundle.v1",
            "schema_hash": "schema-sha",
            "candidate_decision": "RC_CANDIDATE_PASS",
            "status": "PASS",
            "target": "target.exe",
            "target_process": "target.exe",
            "git_commit": "abc123",
            "commit_sha": "abc123",
            "build_hash": "build-sha",
            "binary_sha256": "binary-sha",
            "real_game_validation_matrix_sha256": "matrix-sha",
            "regression_selftest_summary": {
                "run_release_gate_static_checks_selftest": False,
                "release_gate_evidence_selftest": True,
            },
            "artifacts": [{
                "label": "artifact",
                "path": str(artifact_path),
                "sha256": "bad-sha",
                "bytes": artifact_path.stat().st_size + 1,
            }],
            "source_reports": {
                "smoke": str(source_path),
                "soak": str(root / "missing-soak.json"),
                "regression_log": str(source_path),
                "real_game_validation_matrix": str(source_path),
            },
        }
        wrong_manifest = dict(manifest)
        wrong_manifest["binary_sha256"] = "different"
        json_manifest_path.write_text(json.dumps(wrong_manifest), encoding="utf-8")
        text_manifest_path.write_text("Decision: RC_CANDIDATE_PASS\n", encoding="utf-8")

        artifact_failures = bundle.validate_bundle_artifacts(manifest)
        source_failures = bundle.validate_bundle_source_reports(manifest)
        manifest_failures = bundle.validate_written_manifests(
            json_manifest_path,
            text_manifest_path,
            manifest)

        assert any("SHA-256 mismatch" in failure for failure in artifact_failures)
        assert any("byte size mismatch" in failure for failure in artifact_failures)
        assert any("source report path is missing" in failure for failure in source_failures)
        assert any("JSON bundle manifest field mismatch: binary_sha256" in failure for failure in manifest_failures)
        assert any("regression selftest did not pass" in failure for failure in manifest_failures)
        assert any("text bundle manifest missing marker" in failure for failure in manifest_failures)


def test_rc_candidate_regression_log_requires_selftest_pass_markers() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = bundle.pathlib.Path(tmp)
        good_log = root / "good-regression.log"
        good_log.write_text(
            "\n".join([
                "[PASS] run_release_gate_static_checks_selftest passed",
                "[PASS] release_gate_evidence_selftest passed",
                "[INFO] regression summary: total=11, failed=0",
                "[PASS] all regression tests passed",
            ]),
            encoding="utf-8")
        assert verify_rc_candidate.validate_regression_log(good_log) == []

        bad_log = root / "bad-regression.log"
        bad_log.write_text(
            "\n".join([
                "[INFO] regression summary: total=11, failed=0",
                "[PASS] all regression tests passed",
            ]),
            encoding="utf-8")
        failures = verify_rc_candidate.validate_regression_log(bad_log)
        assert any("run_release_gate_static_checks_selftest" in failure for failure in failures)
        assert any("release_gate_evidence_selftest" in failure for failure in failures)


def test_bundle_manifest_records_regression_selftest_summary() -> None:
    bundle_text = static_checks.CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    for marker in [
        "def collect_regression_selftest_summary",
        "\"regression_selftest_summary\"",
        "\"run_release_gate_static_checks_selftest\"",
        "\"release_gate_evidence_selftest\"",
        "Regression selftest summary:",
    ]:
        assert marker in bundle_text


def test_cem_gate_requires_recent_contract_markers() -> None:
    static_gate_text = static_checks.pathlib.Path("run_release_gate_static_checks.py").read_text(
        encoding="utf-8",
        errors="replace")
    cem_text = static_checks.CONTRACT_ENFORCEMENT_MATRIX_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    status_text = static_checks.CONTRACT_ENFORCEMENT_STATUS_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    for marker in [
        "FailedAffinityApplyDisposition",
        "NoSideEffectDiscardAllowed",
        "auditFailedAffinityApply",
        "logAndReturnAffinityApplyFailure",
        "logRollbackFailureAfterAffinityApply",
        "buildCoreRuntimeComponents",
        "applyStartupMutations",
        "buildRuntimeServices",
        "startRuntimeSensors",
        "logStartupPolicySummary",
        "buildMainThreadPolicy",
        "loadAndPrepareBackgroundFilterConfig",
        "buildBackgroundRestrictionPolicy",
        "ApplyGuard rollback failure fixtures",
        "not duplicate the transfer-missing BLOCKER",
        "SchedulerController affinity rollback context logging must not create a second ApplyGuard rollback BLOCKER",
        "run_release_gate_static_checks.py` must require the ApplyGuard rollback fixture function names",
        "audited affinity no-side-effect discard is not a BLOCKER",
        "the report must not contain any `SetThreadGroupAffinity failure` BLOCKER",
        "run_release_gate_static_checks.py` must require the audited affinity no-side-effect discard fixture name",
        "ApplyGuard rollback evidence markers must stay in `BLOCKER`, not `WARN`",
        "ApplyGuard release blocker markers must be centralized in `APPLY_GUARD_BLOCKER_RELEASE_MARKERS`",
        "SoftApply baseline evidence stays separate",
        "run_release_gate_static_checks.py` must require the SoftApply baseline fixture function name",
        "SoftApply release blocker markers must be centralized in `SOFT_APPLY_BLOCKER_RELEASE_MARKERS`",
        "WARN-only release blocker markers must be centralized in `WARN_ONLY_RELEASE_BLOCKER_MARKERS`",
        "Processor Group 1+ monitoring-only marker must stay in `WARN`, not `BLOCKER`",
        "Access Denied fallback marker must stay in `WARN`, not `BLOCKER`",
        "IRQ unsupported monitoring-only marker must stay in `WARN`, not `BLOCKER`",
        "Raw Input fallback marker must stay in `WARN`, not `BLOCKER`",
        "Remote Raw Input unsupported marker must stay in `WARN`, not `BLOCKER`",
        "DPC spike monitoring-only marker must stay in `WARN`, not `BLOCKER`",
        "SoftApply limitation marker must stay in `WARN`, not `BLOCKER`",
        "rollback_preserved_state_summary.has_preserved_state",
        "report must remain `PASS`",
    ]:
        assert marker in static_gate_text
        assert marker in cem_text
    for marker in [
        "CEM ID",
        "Contract name",
        "Static gate status",
        "Runtime validation status",
        "Evidence status",
        "Self-test status",
        "Remaining work",
        "TODO",
    ]:
        assert marker in static_gate_text
        assert marker in status_text
    for index in range(1, 9):
        assert f"CEM-{index:03d}" in static_gate_text
        assert f"CEM-{index:03d}" in status_text


def test_soft_apply_preserved_state_drift_is_blocker_not_warn() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    assert len(static_checks.SOFT_APPLY_BLOCKER_RELEASE_MARKERS) == 1
    for marker in static_checks.SOFT_APPLY_BLOCKER_RELEASE_MARKERS:
        assert static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)
        assert not static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)


def test_apply_guard_rollback_markers_are_blocker_not_warn() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    assert len(static_checks.APPLY_GUARD_BLOCKER_RELEASE_MARKERS) == 3
    for marker in static_checks.APPLY_GUARD_BLOCKER_RELEASE_MARKERS:
        assert static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)
        assert not static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)


def test_group_one_monitoring_only_marker_is_warn_not_blocker() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    marker = "Processor Group 1+ background process-wide restriction blocked as monitoring-only."
    assert static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)
    assert not static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)


def test_access_denied_fallback_marker_is_warn_not_blocker() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    marker = "Access Denied or access boundary encountered with fallback evidence."
    assert static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)
    assert not static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)


def test_irq_unsupported_marker_is_warn_not_blocker() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    marker = "IRQ affinity unsupported, recorded as monitoring-only."
    assert static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)
    assert not static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)


def test_raw_input_fallback_marker_is_warn_not_blocker() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    marker = "Raw Input not detected, fallback input policy active."
    assert static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)
    assert not static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)


def test_remote_raw_input_unsupported_marker_is_warn_not_blocker() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    marker = "Remote Raw Input detection unsupported through public Win32 APIs."
    assert static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)
    assert not static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)


def test_dpc_spike_marker_is_warn_not_blocker() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    marker = "DPC spike observed while IRQ mutation backend is unavailable."
    assert static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)
    assert not static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)


def test_soft_apply_limitation_marker_is_warn_not_blocker() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    marker = "SoftApply baseline validation records a limitation without mutation."
    assert static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)
    assert not static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)


def test_warn_only_release_markers_are_warn_not_blocker() -> None:
    blocker_text = static_checks.RELEASE_BLOCKER_LIST_FILE.read_text(
        encoding="utf-8",
        errors="replace")
    assert len(static_checks.WARN_ONLY_RELEASE_BLOCKER_MARKERS) >= 7
    for marker in static_checks.WARN_ONLY_RELEASE_BLOCKER_MARKERS:
        assert static_checks.markdown_section_contains_marker(blocker_text, "WARN", marker)
        assert not static_checks.markdown_section_contains_marker(blocker_text, "BLOCKER", marker)


def main() -> int:
    test_ordered_markers_pass()
    test_ordered_markers_reject_missing_marker()
    test_ordered_markers_reject_out_of_order_marker()
    test_markdown_section_contains_marker_is_section_scoped()
    test_rc_gate_draft_excludes_long_soak_and_real_game_validation()
    test_standalone_soak_entrypoints_are_contract_checked()
    test_real_game_validation_order_for_full_candidate_flow()
    test_bundle_creation_validates_real_game_matrix_before_writing_bundle()
    test_bundle_manifest_preserves_real_game_matrix_artifact()
    test_bundle_creation_validates_manifest_artifact_hashes_before_pass()
    test_bundle_creation_validates_written_manifests_before_pass()
    test_bundle_creation_validates_source_reports_before_pass()
    test_bundle_creation_validates_source_report_artifact_identity_before_pass()
    test_bundle_creation_validates_regression_selftest_summary_against_artifact_before_pass()
    test_bundle_validators_accept_real_files()
    test_bundle_regression_selftest_summary_matches_bundled_log()
    test_bundle_source_report_artifact_identity_matches_real_files()
    test_bundle_validators_reject_missing_or_mismatched_files()
    test_rc_candidate_regression_log_requires_selftest_pass_markers()
    test_bundle_manifest_records_regression_selftest_summary()
    test_cem_gate_requires_recent_contract_markers()
    test_soft_apply_preserved_state_drift_is_blocker_not_warn()
    test_apply_guard_rollback_markers_are_blocker_not_warn()
    test_group_one_monitoring_only_marker_is_warn_not_blocker()
    test_access_denied_fallback_marker_is_warn_not_blocker()
    test_irq_unsupported_marker_is_warn_not_blocker()
    test_raw_input_fallback_marker_is_warn_not_blocker()
    test_remote_raw_input_unsupported_marker_is_warn_not_blocker()
    test_dpc_spike_marker_is_warn_not_blocker()
    test_soft_apply_limitation_marker_is_warn_not_blocker()
    test_warn_only_release_markers_are_warn_not_blocker()
    print("[PASS] static gate selftest passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
