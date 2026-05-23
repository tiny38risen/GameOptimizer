from __future__ import annotations

import argparse
import json
import pathlib
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parent
DEFAULT_MATRIX = ROOT / "docs" / "release" / "Game_Verification_Matrix.json"
REQUIRED_ROLES = {"Game A", "Game B", "Game C"}
REQUIRED_FIELDS = (
    "role",
    "process_name",
    "admin",
    "scheduler_mode",
    "access_denied",
    "fallback",
    "thread_tracker_confidence",
    "raw_input_tid_confidence",
    "migration_count",
    "dpc_spike_count",
    "rtt_jitter_summary",
    "raw_input_detection",
    "irq_support",
    "background_mode",
    "shutdown_reason",
    "rollback_result",
    "rollback_state_saved",
    "apply_guard_audit",
    "group_aware_thread_path_verified",
    "anti_cheat_suspected",
    "requires_group1_process_wide_background_restriction",
    "soft_apply_warn_count",
    "blocker_count",
    "warn_count",
    "info_count",
    "runs",
)
MAX_SOFT_APPLY_WARN_COUNT_FOR_APPLY = 3


def read_json(path: pathlib.Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def has_successful_run(game: dict[str, Any], mode: str, minimum_minutes: int) -> bool:
    for run in game.get("runs", []):
        if run.get("mode") != mode:
            continue
        if run.get("duration_minutes", 0) < minimum_minutes:
            continue
        if run.get("exit_code") != 0:
            continue
        if run.get("blocker_count") != 0:
            continue
        if run.get("rollback_preserved_state_count") != 0:
            continue
        if not run.get("completed"):
            continue
        return True
    return False


def has_policy_decision_telemetry(game: dict[str, Any]) -> bool:
    telemetry = game.get("policy_decision_telemetry")
    if isinstance(telemetry, bool):
        return telemetry
    for run in game.get("runs", []):
        if run.get("policy_decision_telemetry"):
            return True
    return False


def confidence_is_sufficient(value: Any) -> bool:
    return str(value).strip().lower() in {"high", "sufficient", "confirmed"}


def successful_runs(game: dict[str, Any], mode: str, minimum_minutes: int) -> list[dict[str, Any]]:
    return [
        run
        for run in game.get("runs", [])
        if run.get("mode") == mode
        and run.get("duration_minutes", 0) >= minimum_minutes
        and run.get("exit_code") == 0
        and run.get("blocker_count") == 0
        and run.get("rollback_preserved_state_count") == 0
        and run.get("completed")
    ]


def apply_runs(game: dict[str, Any]) -> list[dict[str, Any]]:
    return [
        run
        for run in game.get("runs", [])
        if run.get("mode") == "apply"
    ]


def validate_apply_policy(game: dict[str, Any]) -> list[str]:
    role = game.get("role", "<unknown>")
    failures: list[str] = []
    runs = apply_runs(game)
    if not runs:
        return failures

    if not has_successful_run(game, "dry-run", 30):
        failures.append(f"{role}: apply mode requires a prior PASS 30m dry-run")
    if not has_successful_run(game, "soft-apply", 60):
        failures.append(f"{role}: apply mode requires a prior PASS 60m soft-apply")
    if game.get("access_denied"):
        failures.append(f"{role}: apply mode is forbidden after Access Denied")
    if game.get("anti_cheat_suspected"):
        failures.append(f"{role}: apply mode is forbidden when anti-cheat is suspected")
    if game.get("fallback"):
        failures.append(f"{role}: apply mode is forbidden when fallback is active")
    if not game.get("rollback_state_saved"):
        failures.append(f"{role}: apply mode requires rollback state save success")
    if not confidence_is_sufficient(game.get("thread_tracker_confidence")):
        failures.append(f"{role}: apply mode requires sufficient ThreadTracker confidence")
    if str(game.get("raw_input_tid_confidence")).strip().lower() == "low":
        failures.append(f"{role}: apply mode is forbidden with low Raw Input TID confidence")
    if str(game.get("apply_guard_audit")).strip().upper() != "PASS":
        failures.append(f"{role}: apply mode requires ApplyGuard audit PASS")
    if not game.get("group_aware_thread_path_verified"):
        failures.append(f"{role}: apply mode requires verified group-aware thread path")
    if game.get("requires_group1_process_wide_background_restriction"):
        failures.append(f"{role}: apply mode is forbidden when group 1+ process-wide background restriction is required")
    if int(game.get("soft_apply_warn_count") or 0) > MAX_SOFT_APPLY_WARN_COUNT_FOR_APPLY:
        failures.append(f"{role}: apply mode is forbidden after many soft-apply WARN findings")

    for run in runs:
        if not run.get("limited"):
            failures.append(f"{role}: apply mode must be limited")
        if not run.get("explicit_option"):
            failures.append(f"{role}: apply mode must use an explicit --apply option")
        if run.get("blocker_count") != 0:
            failures.append(f"{role}: apply run has BLOCKER findings")
        if run.get("exit_code") != 0:
            failures.append(f"{role}: apply run exit code must be 0")
        if run.get("rollback_preserved_state_count") != 0:
            failures.append(f"{role}: apply run must not preserve rollback state")

    return failures


def validate_game(game: dict[str, Any]) -> list[str]:
    role = game.get("role", "<unknown>")
    failures: list[str] = []
    for field in REQUIRED_FIELDS:
        if field not in game:
            failures.append(f"{role}: required field missing: {field}")

    if game.get("blocker_count") != 0:
        failures.append(f"{role}: blocker_count must be 0")
    if game.get("abnormal_exit"):
        failures.append(f"{role}: abnormal_exit must be false")
    if game.get("rollback_preserved_state_count", 0) != 0:
        failures.append(f"{role}: rollback_preserved_state_count must be 0")
    if not has_successful_run(game, "dry-run", 30):
        failures.append(f"{role}: missing successful 30m dry-run")
    if not has_successful_run(game, "soft-apply", 60):
        failures.append(f"{role}: missing successful 60m soft-apply")

    failures.extend(validate_apply_policy(game))
    return failures


def validate_matrix(path: pathlib.Path) -> list[str]:
    failures: list[str] = []
    if not path.exists():
        return [f"real game validation matrix missing: {path}"]

    try:
        matrix = read_json(path)
    except (OSError, json.JSONDecodeError) as exc:
        return [f"real game validation matrix unreadable: {exc}"]

    games = matrix.get("games")
    if not isinstance(games, list):
        return ["real game validation matrix must contain a games array"]

    roles = {game.get("role") for game in games if isinstance(game, dict)}
    missing_roles = sorted(REQUIRED_ROLES - roles)
    if missing_roles:
        failures.append(f"real game validation missing required roles: {', '.join(missing_roles)}")
    if len(games) < 3:
        failures.append("real game validation requires at least 3 games")

    soft_apply_pass_count = 0
    apply_pass_count = 0
    policy_telemetry_count = 0
    for game in games:
        if not isinstance(game, dict):
            failures.append("real game validation game entry must be an object")
            continue
        failures.extend(validate_game(game))
        if has_successful_run(game, "soft-apply", 60):
            soft_apply_pass_count += 1
        if has_successful_run(game, "apply", 1):
            apply_pass_count += 1
        if has_policy_decision_telemetry(game):
            policy_telemetry_count += 1

    if soft_apply_pass_count < 2:
        failures.append("real game validation requires at least 2 successful 60m soft-apply runs")
    if apply_pass_count < 1:
        failures.append("real game validation requires 1 limited apply-mode validation")
    if policy_telemetry_count < 1:
        failures.append("real game validation requires policy decision telemetry from at least 1 game")

    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify GameOptimizer real game validation matrix.")
    parser.add_argument("--matrix", type=pathlib.Path, default=DEFAULT_MATRIX)
    args = parser.parse_args()

    failures = validate_matrix(args.matrix)
    if failures:
        for failure in failures:
            print(f"[BLOCKER] real game validation: {failure}")
        return 1

    print(f"[PASS] real game validation matrix passed: {args.matrix}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
