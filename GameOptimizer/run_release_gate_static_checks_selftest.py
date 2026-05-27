from __future__ import annotations

import json
import tempfile

import create_rc_evidence_bundle as bundle
import run_release_gate_static_checks as static_checks
import verify_rc_candidate


ORDERED_MARKERS = [
    "echo [RC-1] Python syntax gate",
    "echo [RC-2] git diff whitespace gate",
    "echo [RC-3] static release gate",
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


def test_rc9_real_game_validation_runs_before_candidate_verification() -> None:
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
        "JSON bundle manifest field mismatch",
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
            "schema_version": "gameoptimizer.rc_evidence_bundle.v1",
            "candidate_decision": "RC_CANDIDATE_PASS",
            "commit_sha": "abc123",
            "real_game_validation_matrix_sha256": "matrix-sha",
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
                "Commit SHA: abc123",
                "Real game validation matrix SHA-256: matrix-sha",
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
            "schema_version": "gameoptimizer.rc_evidence_bundle.v1",
            "candidate_decision": "RC_CANDIDATE_PASS",
            "commit_sha": "abc123",
            "real_game_validation_matrix_sha256": "matrix-sha",
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
        wrong_manifest["commit_sha"] = "different"
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
        assert any("JSON bundle manifest field mismatch" in failure for failure in manifest_failures)
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


def main() -> int:
    test_ordered_markers_pass()
    test_ordered_markers_reject_missing_marker()
    test_ordered_markers_reject_out_of_order_marker()
    test_rc9_real_game_validation_runs_before_candidate_verification()
    test_bundle_creation_validates_real_game_matrix_before_writing_bundle()
    test_bundle_manifest_preserves_real_game_matrix_artifact()
    test_bundle_creation_validates_manifest_artifact_hashes_before_pass()
    test_bundle_creation_validates_written_manifests_before_pass()
    test_bundle_creation_validates_source_reports_before_pass()
    test_bundle_validators_accept_real_files()
    test_bundle_validators_reject_missing_or_mismatched_files()
    test_rc_candidate_regression_log_requires_selftest_pass_markers()
    test_bundle_manifest_records_regression_selftest_summary()
    print("[PASS] static gate selftest passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
