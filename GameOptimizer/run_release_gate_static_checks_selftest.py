from __future__ import annotations

import run_release_gate_static_checks as static_checks


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


def main() -> int:
    test_ordered_markers_pass()
    test_ordered_markers_reject_missing_marker()
    test_ordered_markers_reject_out_of_order_marker()
    test_rc9_real_game_validation_runs_before_candidate_verification()
    test_bundle_creation_validates_real_game_matrix_before_writing_bundle()
    test_bundle_manifest_preserves_real_game_matrix_artifact()
    test_bundle_creation_validates_manifest_artifact_hashes_before_pass()
    test_bundle_creation_validates_written_manifests_before_pass()
    print("[PASS] static gate selftest passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
