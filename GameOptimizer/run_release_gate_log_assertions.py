from __future__ import annotations

import argparse
import pathlib
import re


def read_log_text(path: pathlib.Path) -> str:
    if not path.exists():
        raise FileNotFoundError(f"log file not found: {path}")
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "utf-16", "utf-16-le"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def contains(text: str, needle: str) -> bool:
    return needle.lower() in text.lower()


def ordered_indices(text: str, needles: list[str]) -> list[int] | None:
    lowered = text.lower()
    indices: list[int] = []
    search_from = 0
    for needle in needles:
        index = lowered.find(needle.lower(), search_from)
        if index < 0:
            return None
        indices.append(index)
        search_from = index + len(needle)
    return indices


def validate_timeout_sequence(log_text: str) -> list[str]:
    failures: list[str] = []
    required_sequence = [
        "shutdown will be requested at the next watchdog safe point",
        "runtime timeout reached at watchdog cycle boundary; requesting clean shutdown",
        "shutdown requested; stopping watchdog and latency sensor before rollback",
        "shutdown completed cleanly",
    ]
    indices = ordered_indices(log_text, required_sequence)
    if indices is None:
        for marker in required_sequence:
            if not contains(log_text, marker):
                failures.append(f"timeout safe-point sequence missing marker: {marker}")
        if not failures:
            failures.append("timeout safe-point sequence markers exist but are out of order")
        return failures

    if indices != sorted(indices):
        failures.append("timeout safe-point sequence is out of order")
    return failures


def extract_pid(line: str) -> str | None:
    match = re.search(r"\bPID\s+(\d+)\b|\bpid=(\d+)\b", line, re.IGNORECASE)
    if not match:
        return None
    return match.group(1) or match.group(2)


def validate_background_stale_matching(log_text: str) -> list[str]:
    failures: list[str] = []
    lines = log_text.splitlines()
    fatal_failure_pattern = re.compile(r"background rollback failed for PID\s+(\d+)", re.IGNORECASE)
    stale_patterns = [
        re.compile(r"background rollback skipped for PID\s+(\d+) because .*identity changed", re.IGNORECASE),
        re.compile(r"background rollback skipped for PID\s+(\d+) because .*no longer openable", re.IGNORECASE),
        re.compile(r"background rollback skipped for PID\s+(\d+) because .*identity became stale", re.IGNORECASE),
        re.compile(r"background rollback skipped for PID\s+(\d+) because .*process.*stale", re.IGNORECASE),
        re.compile(r"stale rollback entry.*PID\s+(\d+)", re.IGNORECASE),
        re.compile(r"creation time mismatch.*PID\s+(\d+)", re.IGNORECASE),
    ]

    failure_pids: set[str] = set()
    stale_pids: set[str] = set()

    for line in lines:
        failure_match = fatal_failure_pattern.search(line)
        if failure_match:
            failure_pids.add(failure_match.group(1))
        for pattern in stale_patterns:
            stale_match = pattern.search(line)
            if stale_match:
                stale_pids.add(stale_match.group(1))

    # A real fatal rollback failure must remain fatal. The log must not be accepted merely
    # because some unrelated stale PID exists elsewhere in the run.
    unmatched = sorted(failure_pids - stale_pids, key=int)
    for pid in unmatched:
        failures.append(f"background rollback failed for PID {pid} without same-PID stale identity evidence")

    return failures


def validate_access_denied_fallback_evidence(log_text: str) -> list[str]:
    failures: list[str] = []
    access_markers = [
        "access denied",
        "access boundary",
        "recoverable access limitation",
        "openthread failure",
        "openprocess failure",
    ]
    evidence_markers = [
        "monitoring-only fallback remains active",
        "skipped",
        "rollback path was invoked when needed",
        "saved state discarded before mutation",
        "no longer openable",
        "blocked by an access boundary",
    ]

    for line_number, line in enumerate(log_text.splitlines(), start=1):
        lowered = line.lower()
        if not any(marker in lowered for marker in access_markers):
            continue
        if any(marker in lowered for marker in evidence_markers):
            continue
        failures.append(
            f"access-denied/access-boundary log lacks fallback or rollback evidence at line {line_number}: {line}")
    return failures


def validate_timeline_monotonicity(log_text: str) -> list[str]:
    failures: list[str] = []
    cycles = [
        int(match.group(1))
        for match in re.finditer(r"runtime validation sample:\s*cycle=(\d+)", log_text, re.IGNORECASE)
    ]
    if not cycles:
        return ["runtime validation sample timeline missing"]

    for previous, current in zip(cycles, cycles[1:]):
        if current <= previous:
            failures.append(
                f"runtime validation sample timeline is not monotonic: cycle {previous} followed by {current}")
            break

    summary_match = re.search(r"runtime validation summary:\s*cycles=(\d+)", log_text, re.IGNORECASE)
    if not summary_match:
        failures.append("runtime validation summary missing")
        return failures

    summary_cycles = int(summary_match.group(1))
    if summary_cycles != cycles[-1]:
        failures.append(
            f"runtime validation summary cycle count {summary_cycles} does not match last sample cycle {cycles[-1]}")

    return failures


def validate(mode: str, log_text: str) -> list[str]:
    failures: list[str] = []

    if not contains(log_text, "shutdown completed cleanly"):
        failures.append("missing clean shutdown log")
    if not contains(log_text, "shutdown result: timerRollbackFailed="):
        failures.append("shutdown result summary missing")
    if contains(log_text, "shutdown rollback failed"):
        failures.append("shutdown rollback failed")
    if contains(log_text, "rollbackStatePreserved=true"):
        failures.append("rollback state preserved after rollback failure")
    if re.search(r"\[ERROR\].*rollback failed", log_text, re.IGNORECASE):
        failures.append("rollback error log found")
    if contains(log_text, "runtime validation result: FAILED"):
        failures.append("runtime validation failed")
    if re.search(r"\[ERROR\].*runtime validation", log_text, re.IGNORECASE):
        failures.append("runtime validation error log found")
    if contains(log_text, "continue statement"):
        failures.append("compiler-style continue error text found")
    failures.extend(validate_access_denied_fallback_evidence(log_text))

    if mode == "dry-run":
        forbidden_mutation_markers = [
            "background restricted:",
            "rollback state saved for TID",
            "background rollback state saved",
        ]
        for marker in forbidden_mutation_markers:
            if contains(log_text, marker):
                failures.append(f"dry-run mutation marker found: {marker}")

    if mode == "apply":
        if not contains(log_text, "rollback audit passed"):
            failures.append("apply mode missing rollback audit passed")
        failures.extend(validate_background_stale_matching(log_text))

    if mode == "timeout":
        failures.extend(validate_timeout_sequence(log_text))

    if mode == "soak":
        if not contains(log_text, "runtime validation result: PASSED_OR_INCONCLUSIVE"):
            failures.append("soak mode missing runtime validation pass/inconclusive summary")
        failures.extend(validate_timeline_monotonicity(log_text))

    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate GameOptimizer Release Gate logs.")
    parser.add_argument("--mode", required=True, choices=["dry-run", "soft-apply", "apply", "timeout", "soak"])
    parser.add_argument("log_file", type=pathlib.Path)
    args = parser.parse_args()

    try:
        log_text = read_log_text(args.log_file)
    except OSError as exc:
        print(f"[FAIL] {args.log_file}: {exc}")
        return 1
    failures = validate(args.mode, log_text)
    if failures:
        for failure in failures:
            print(f"[FAIL] {args.log_file}: {failure}")
        return 1

    print(f"[PASS] {args.mode} log assertions passed: {args.log_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
