from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent
SOURCE_EXTENSIONS = {".cpp", ".h", ".hpp", ".cxx", ".hxx"}
PROJECT_FILE = ROOT / "GameOptimizer.vcxproj"
BUILD_TESTS_FILE = ROOT / "build_decision_layer_tests.bat"
REGRESSION_TESTS_FILE = ROOT / "run_regression_tests.bat"
LONG_SOAK_PRESETS_FILE = ROOT / "run_long_soak_presets.bat"
SOAK_HANG_DETECTION_FILE = ROOT / "run_soak_with_hang_detection.py"
RELEASE_GATE_EVIDENCE_FILE = ROOT / "release_gate_evidence.py"

REQUIRED_MAIN_PATTERNS = [
    ("main.cpp", r"const\s+DWORD\s+targetProcessId\s*=\s*\*processId\s*;", "targetProcessId bind missing"),
    ("main.cpp", r"const\s+auto&\s+topology\s*=\s*\*topologyResult\s*;", "topologyResult bind missing"),
    ("main.cpp", r"const\s+auto&\s+observedThreads\s*=\s*\*topThreads\s*;", "topThreads bind missing"),
    ("main.cpp", r"const\s+auto&\s+commands\s*=\s*\*commandsResult\s*;", "commandsResult bind missing"),
    ("main.cpp", r"runtime timeout reached at watchdog cycle boundary", "runtime timeout safe-point log missing"),
]

ALLOWED_EXPECTED_BIND_LINES = [
    re.compile(r"\s*(?:const\s+)?(?:auto|auto&|const\s+auto&|DWORD|const\s+DWORD|ErrorCode|const\s+ErrorCode|WinHandle&|const\s+WinHandle&|std::uint64_t&|const\s+std::uint64_t&)\s+\w+\s*=\s*\*\w+\s*;\s*"),
]

CONTROL_KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "static_cast", "reinterpret_cast", "const_cast"}


def iter_source_files() -> list[pathlib.Path]:
    ignored_names = {
        "run_release_gate_static_checks.py",
        "run_release_gate_log_assertions.py",
    }
    files: list[pathlib.Path] = []
    for path in ROOT.iterdir():
        if path.name in ignored_names:
            continue
        if path.suffix.lower() in SOURCE_EXTENSIONS:
            files.append(path)
    return sorted(files)


def strip_comments_and_strings(text: str) -> str:
    # Lightweight scanner sufficient for release-gate pattern checks.
    result: list[str] = []
    i = 0
    state = "code"
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line_comment"
                result.append("  ")
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block_comment"
                result.append("  ")
                i += 2
                continue
            if ch == '"':
                state = "string"
                result.append('""')
                i += 1
                continue
            if ch == "'":
                state = "char"
                result.append("''")
                i += 1
                continue
            result.append(ch)
            i += 1
            continue
        if state == "line_comment":
            if ch == "\n":
                state = "code"
                result.append("\n")
            else:
                result.append(" ")
            i += 1
            continue
        if state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                result.append("  ")
                i += 2
            else:
                result.append("\n" if ch == "\n" else " ")
                i += 1
            continue
        if state == "string":
            if ch == "\\":
                result.append("  ")
                i += 2
                continue
            if ch == '"':
                state = "code"
            result.append("\n" if ch == "\n" else " ")
            i += 1
            continue
        if state == "char":
            if ch == "\\":
                result.append("  ")
                i += 2
                continue
            if ch == "'":
                state = "code"
            result.append("\n" if ch == "\n" else " ")
            i += 1
            continue
    return "".join(result)


def line_is_allowed_bind(line: str) -> bool:
    return any(pattern.fullmatch(line) for pattern in ALLOWED_EXPECTED_BIND_LINES)


def likely_expected_names(text: str) -> set[str]:
    names: set[str] = set()
    for match in re.finditer(r"(?:std::)?expected\s*<[^;\n]+?>\s+([A-Za-z_]\w*)", text):
        names.add(match.group(1))
    # Project convention: expected-return temporaries and results are named *Result/*Expected.
    for match in re.finditer(r"\b([A-Za-z_]\w*(?:Result|Expected))\b", text):
        names.add(match.group(1))
    # Broader regression guard: common project helpers returning std::expected are often
    # captured through auto. Keep this conservative and function-name driven so raw
    # pointer dereferences are not misclassified.
    expected_function_prefixes = (
        "open",
        "query",
        "save",
        "validate",
        "evaluate",
        "rollback",
        "collect",
        "find",
        "build",
    )
    assignment_pattern = re.compile(
        r"\b(?:const\s+)?auto(?:&)?\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*\(")
    for match in assignment_pattern.finditer(text):
        variable_name = match.group(1)
        function_name = match.group(2)
        if function_name.startswith(expected_function_prefixes):
            names.add(variable_name)
    names.update({"processId", "topologyResult", "topThreads", "commandsResult"})
    return names


def check_expected_access(path: pathlib.Path) -> list[str]:
    failures: list[str] = []
    raw_text = path.read_text(encoding="utf-8", errors="replace")
    text = strip_comments_and_strings(raw_text)
    names = likely_expected_names(text)

    for line_number, line in enumerate(text.splitlines(), start=1):
        if line_is_allowed_bind(line):
            continue
        for name in names:
            if re.search(rf"(?<![\w])\*\s*{re.escape(name)}\b", line):
                failures.append(f"[FAIL] {path.name}:{line_number}: B2 direct expected dereference: *{name}")
            if re.search(rf"(?<![\w]){re.escape(name)}\s*->", line):
                failures.append(f"[FAIL] {path.name}:{line_number}: B2 expected operator-> access: {name}->")
    return failures



# NOTE: This gate is intentionally conservative. It validates minimum lexical
# ordering inside a named function body, not the entire translation unit. This
# prevents unrelated helpers, dead code in other functions, or earlier global
# matches from satisfying RG-6. It is still not a full control-flow proof, so
# branch-local rollbackNow/discardSavedState paths require code review.
def extract_function_body(text: str, function_name: str) -> str | None:
    name_index = text.find(function_name)
    if name_index < 0:
        return None

    open_brace = text.find("{", name_index)
    if open_brace < 0:
        return None

    depth = 0
    for index in range(open_brace, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
            continue
        if char == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace + 1:index]
    return None


def first_match_index(text: str, pattern: str) -> int:
    match = re.search(pattern, text)
    return -1 if match is None else match.start()


def check_ordered_patterns_in_function(
    translation_unit_text: str,
    function_name: str,
    ordered_patterns: list[tuple[str, str]],
    label: str,
) -> list[str]:
    failures: list[str] = []
    body = extract_function_body(translation_unit_text, function_name)
    if body is None:
        return [f"[FAIL] ApplyGuard transaction sequencing: {label} function body not found: {function_name}"]

    previous_index = -1
    previous_name = "<start>"
    for name, pattern in ordered_patterns:
        index = first_match_index(body, pattern)
        if index < 0:
            failures.append(f"[FAIL] ApplyGuard transaction sequencing: {label} missing {name}")
            continue
        if previous_index >= 0 and index <= previous_index:
            failures.append(
                f"[FAIL] ApplyGuard transaction sequencing: {label} {name} appears before or at {previous_name}")
        previous_index = index
        previous_name = name
    return failures


def check_apply_guard_transaction_patterns() -> list[str]:
    failures: list[str] = []
    scheduler_text = (ROOT / "SchedulerController.cpp").read_text(encoding="utf-8", errors="replace")
    background_text = (ROOT / "BackgroundController.cpp").read_text(encoding="utf-8", errors="replace")
    apply_guard_text = (ROOT / "ApplyGuard.h").read_text(encoding="utf-8", errors="replace")

    required_patterns = [
        (scheduler_text, r"saveAppliedState\s*\(", "SchedulerController must save rollback state before apply"),
        (scheduler_text, r"ApplyGuard::forThread", "SchedulerController must create thread ApplyGuard"),
        (scheduler_text, r"applyGuard\.arm\s*\(\s*\)", "SchedulerController ApplyGuard must be armed"),
        (scheduler_text, r"applyGuard\.commit\s*\(\s*\)", "SchedulerController ApplyGuard must commit after verification"),
        (scheduler_text, r"applyGuard\.rollbackNow\s*\(\s*\)", "SchedulerController failure path must rollback through ApplyGuard"),
        (background_text, r"saveProcessState\s*\(", "BackgroundController must save rollback state before process apply"),
        (background_text, r"ApplyGuard::forProcess", "BackgroundController must create process ApplyGuard"),
        (background_text, r"applyGuard\.arm\s*\(\s*\)", "BackgroundController ApplyGuard must be armed"),
        (background_text, r"applyGuard\.commit\s*\(\s*\)", "BackgroundController ApplyGuard must commit after successful apply"),
        (background_text, r"applyGuard\.rollbackNow\s*\(\s*\)", "BackgroundController priority failure path must rollback through ApplyGuard"),
        (apply_guard_text, r"enum class RollbackStateOwnership", "ApplyGuard must own rollback-state disposition internally"),
    ]
    for text, pattern, message in required_patterns:
        if not re.search(pattern, text):
            failures.append(f"[FAIL] ApplyGuard transaction gate: {message}")

    failures.extend(check_ordered_patterns_in_function(
        scheduler_text,
        "SchedulerController::applyMainThreadPolicy",
        [
            ("saveAppliedState", r"saveAppliedState\s*\("),
            ("ApplyGuard::forThread", r"ApplyGuard::forThread"),
            ("arm", r"applyGuard\.arm\s*\(\s*\)"),
            ("SetThreadGroupAffinity", r"SetThreadGroupAffinity\s*\("),
            ("SetThreadPriority", r"SetThreadPriority\s*\("),
            ("apply verification", r"const\s+auto\s+verifiedState\s*=\s*queryCurrentSchedulingState\s*\("),
            ("commit", r"applyGuard\.commit\s*\(\s*\)"),
        ],
        "SchedulerController::applyMainThreadPolicy"))

    failures.extend(check_ordered_patterns_in_function(
        background_text,
        "BackgroundController::applyRestriction",
        [
            ("saveProcessState", r"saveProcessState\s*\("),
            ("ApplyGuard::forProcess", r"ApplyGuard::forProcess"),
            ("arm", r"applyGuard\.arm\s*\(\s*\)"),
            ("SetProcessAffinityMask", r"SetProcessAffinityMask\s*\("),
            ("SetPriorityClass", r"SetPriorityClass\s*\("),
            ("commit", r"applyGuard\.commit\s*\(\s*\)"),
        ],
        "BackgroundController::applyRestriction"))

    forbidden_patterns = [
        (background_text, r"hasProcessState\s*\(", "BackgroundController must not use hasProcessState TOCTOU query"),
        (apply_guard_text, r"newlyCreated|preExisting|createdRollbackState", "ApplyGuard must not expose bool ownership naming"),
    ]
    for text, pattern, message in forbidden_patterns:
        if re.search(pattern, text):
            failures.append(f"[FAIL] ApplyGuard transaction gate: {message}")
    return failures


def check_dpc_monitoring_is_not_placeholder() -> list[str]:
    text = (ROOT / "LatencyMetricsCollector.cpp").read_text(encoding="utf-8", errors="replace")
    body = extract_function_body(text, "LatencyMetricsCollector::estimateDpcSpikeCount")
    if body is None:
        return ["[FAIL] DPC release gate: estimateDpcSpikeCount function body not found"]

    if re.search(r"Phase-2\s+placeholder|Returning\s+zero\s+keeps|return\s+0\s*;", body, re.IGNORECASE):
        return [
            "[FAIL] DPC release gate: estimateDpcSpikeCount is still a placeholder; "
            "implement real DPC monitoring before approving a release"
        ]

    return []


def check_background_processor_group_policy_is_explicit() -> list[str]:
    text = (ROOT / "BackgroundController.cpp").read_text(encoding="utf-8", errors="replace")
    required_markers = [
        "processorGroup",
        "blockedByUnsupportedProcessorGroup",
        "background restriction blocked: processor group",
    ]
    failures: list[str] = []
    for marker in required_markers:
        if marker not in text:
            failures.append(
                f"[FAIL] BackgroundController processor-group gate: missing explicit policy marker: {marker}")
    return failures


def check_cpp20_runtime_contracts() -> list[str]:
    failures: list[str] = []

    for path in iter_source_files():
        raw_text = path.read_text(encoding="utf-8", errors="replace")
        text = strip_comments_and_strings(raw_text)
        for line_number, line in enumerate(text.splitlines(), start=1):
            if re.search(r"\bstd::thread\b", line):
                failures.append(
                    f"[FAIL] C++ runtime gate: {path.name}:{line_number}: std::thread is forbidden; use std::jthread")
            if re.search(r"\bstd::this_thread\s*::\s*sleep_for\s*\(", line):
                failures.append(
                    f"[FAIL] C++ runtime gate: {path.name}:{line_number}: sleep_for polling is forbidden; use stop-aware wait")

    return failures


def check_project_language_contracts() -> list[str]:
    failures: list[str] = []
    text = PROJECT_FILE.read_text(encoding="utf-8", errors="replace")

    toolsets = re.findall(r"<PlatformToolset>([^<]+)</PlatformToolset>", text)
    if not toolsets:
        failures.append("[FAIL] MSVC toolset gate: no PlatformToolset entries found")
    for toolset in toolsets:
        if toolset != "v143":
            failures.append(
                f"[FAIL] MSVC toolset gate: PlatformToolset must be v143, found {toolset}")

    standards = re.findall(r"<LanguageStandard>([^<]+)</LanguageStandard>", text)
    if not standards:
        failures.append("[FAIL] MSVC language gate: no LanguageStandard entries found")
    for standard in standards:
        if standard != "stdcpplatest":
            failures.append(
                f"[FAIL] MSVC language gate: LanguageStandard must be stdcpplatest, found {standard}")

    return failures


def check_timer_input_module_registration() -> list[str]:
    failures: list[str] = []
    required_files = [
        ROOT / "TimerResolutionController.h",
        ROOT / "TimerResolutionController.cpp",
        ROOT / "InputLatencyController.h",
        ROOT / "InputLatencyController.cpp",
        ROOT / "TimerInputControllerTests.cpp",
    ]
    for path in required_files:
        if not path.exists():
            failures.append(f"[FAIL] Timer/Input gate: required file missing: {path.name}")

    project_text = PROJECT_FILE.read_text(encoding="utf-8", errors="replace")
    build_text = BUILD_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    regression_text = REGRESSION_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    main_text = (ROOT / "main.cpp").read_text(encoding="utf-8", errors="replace")

    required_project_entries = [
        "TimerResolutionController.cpp",
        "TimerResolutionController.h",
        "InputLatencyController.cpp",
        "InputLatencyController.h",
        "TimerInputControllerTests.cpp",
    ]
    for entry in required_project_entries:
        if entry not in project_text:
            failures.append(f"[FAIL] Timer/Input gate: project registration missing: {entry}")

    required_build_markers = [
        "building TimerInputControllerTests",
        "TimerInputControllerTests.cpp TimerResolutionController.cpp InputLatencyController.cpp",
        "TimerInputControllerTests.exe",
    ]
    for marker in required_build_markers:
        if marker not in build_text:
            failures.append(f"[FAIL] Timer/Input gate: build script registration missing: {marker}")

    required_regression_markers = [
        "running TimerInputControllerTests",
        "build_tests\\TimerInputControllerTests.exe",
        "TimerInputControllerTests passed",
    ]
    for marker in required_regression_markers:
        if marker not in regression_text:
            failures.append(f"[FAIL] Timer/Input gate: regression script registration missing: {marker}")

    required_main_markers = [
        '#include "TimerResolutionController.h"',
        '#include "InputLatencyController.h"',
        "TimerResolutionController timerResolutionController",
        "InputLatencyController inputLatencyController",
        "timerResolutionController.rollback()",
    ]
    for marker in required_main_markers:
        if marker not in main_text:
            failures.append(f"[FAIL] Timer/Input gate: runtime integration missing: {marker}")

    input_latency_text = (ROOT / "InputLatencyController.cpp").read_text(encoding="utf-8", errors="replace")
    required_input_markers = [
        "RawInputDetectionPath::LocalProcessRegisteredDevices",
        "RawInputDetectionPath::RemoteProcessUnsupported",
        "GetRegisteredRawInputDevices",
        "isInputThreadPinningAllowed",
        "status.rawInputDetected && status.inputThreadId != 0",
        "fallbackMonitoringOnly",
    ]
    for marker in required_input_markers:
        if marker not in input_latency_text:
            failures.append(f"[FAIL] Timer/Input gate: Raw Input fallback contract missing: {marker}")

    return failures


def check_long_soak_automation_contract() -> list[str]:
    failures: list[str] = []
    required_files = [
        LONG_SOAK_PRESETS_FILE,
        SOAK_HANG_DETECTION_FILE,
    ]
    for path in required_files:
        if not path.exists():
            failures.append(f"[FAIL] Long soak gate: required file missing: {path.name}")

    if LONG_SOAK_PRESETS_FILE.exists():
        soak_text = LONG_SOAK_PRESETS_FILE.read_text(encoding="utf-8", errors="replace")
        required_soak_markers = [
            "30m",
            "60m",
            "run_soak_with_hang_detection.py",
            "--mode soak",
            "--max-runtime-seconds 1800",
            "--max-runtime-seconds 3600",
        ]
        for marker in required_soak_markers:
            if marker not in soak_text:
                failures.append(f"[FAIL] Long soak gate: preset script missing marker: {marker}")

    if SOAK_HANG_DETECTION_FILE.exists():
        hang_text = SOAK_HANG_DETECTION_FILE.read_text(encoding="utf-8", errors="replace")
        required_hang_markers = [
            "--idle-timeout-seconds",
            "process.kill()",
            "no process output",
            "exceeded wall timeout",
        ]
        for marker in required_hang_markers:
            if marker not in hang_text:
                failures.append(f"[FAIL] Long soak gate: hang detection missing marker: {marker}")

    assertions_text = (ROOT / "run_release_gate_log_assertions.py").read_text(encoding="utf-8", errors="replace")
    required_assertion_markers = [
        "validate_timeline_monotonicity",
        "\"soak\"",
        "runtime validation sample:",
        "runtime validation summary:",
    ]
    for marker in required_assertion_markers:
        if marker not in assertions_text:
            failures.append(f"[FAIL] Long soak gate: log assertions missing marker: {marker}")

    return failures


def check_release_evidence_contract() -> list[str]:
    failures: list[str] = []
    if not RELEASE_GATE_EVIDENCE_FILE.exists():
        return ["[FAIL] RC evidence gate: release_gate_evidence.py is missing"]

    evidence_text = RELEASE_GATE_EVIDENCE_FILE.read_text(encoding="utf-8", errors="replace")
    required_evidence_markers = [
        "rc_evidence_report.json",
        "rc_evidence_report.txt",
        "exe_sha256",
        "git_commit",
        "build_hash",
        "runtime validation FAILED must pair with process exit code 1",
        "required RC soak step missing",
        "release_gate_logs",
    ]
    for marker in required_evidence_markers:
        if marker not in evidence_text:
            failures.append(f"[FAIL] RC evidence gate: evidence writer missing marker: {marker}")

    script_requirements = [
        (ROOT / "run_release_gate_smoke.bat", "smoke"),
        (ROOT / "run_long_soak_presets.bat", "soak"),
    ]
    for script_path, kind in script_requirements:
        script_text = script_path.read_text(encoding="utf-8", errors="replace")
        required_script_markers = [
            f"release_gate_evidence.py init --kind {kind}",
            "release_gate_evidence.py record",
            "release_gate_evidence.py finalize",
            "%RUN_DIR%\\logs",
        ]
        for marker in required_script_markers:
            if marker not in script_text:
                failures.append(
                    f"[FAIL] RC evidence gate: {script_path.name} missing marker: {marker}")

    soak_text = (ROOT / "run_long_soak_presets.bat").read_text(encoding="utf-8", errors="replace")
    required_rc_soak_markers = [
        "set PRESET=both",
        "--require-soak-both",
        "soak_30m_dry_run",
        "soak_60m_soft_apply",
    ]
    for marker in required_rc_soak_markers:
        if marker not in soak_text:
            failures.append(f"[FAIL] RC soak gate: missing marker: {marker}")

    return failures


def check_runtime_validation_failure_exit_code_contract() -> list[str]:
    main_text = (ROOT / "main.cpp").read_text(encoding="utf-8", errors="replace")
    ordered_markers = [
        "const bool runtimeValidationFailed = runtimeValidationMonitor.hasCriticalFailure();",
        "if (runtimeValidationFailed)",
        'Logger::error("shutdown completed with runtime validation failure");',
        "return 1;",
    ]
    failures: list[str] = []
    search_from = 0
    for marker in ordered_markers:
        index = main_text.find(marker, search_from)
        if index < 0:
            failures.append(f"[FAIL] Runtime validation exit-code gate: missing ordered marker: {marker}")
            continue
        search_from = index + len(marker)

    assertions_text = (ROOT / "run_release_gate_log_assertions.py").read_text(encoding="utf-8", errors="replace")
    if "runtime validation result: FAILED" not in assertions_text:
        failures.append("[FAIL] Runtime validation exit-code gate: log assertion must reject FAILED result")

    return failures


def main() -> int:
    failures: list[str] = []

    for file_name, pattern, message in REQUIRED_MAIN_PATTERNS:
        text = (ROOT / file_name).read_text(encoding="utf-8", errors="replace")
        if not re.search(pattern, text):
            failures.append(f"[FAIL] {file_name}: {message}")

    for path in iter_source_files():
        failures.extend(check_expected_access(path))

    failures.extend(check_apply_guard_transaction_patterns())
    failures.extend(check_dpc_monitoring_is_not_placeholder())
    failures.extend(check_background_processor_group_policy_is_explicit())
    failures.extend(check_cpp20_runtime_contracts())
    failures.extend(check_project_language_contracts())
    failures.extend(check_timer_input_module_registration())
    failures.extend(check_long_soak_automation_contract())
    failures.extend(check_release_evidence_contract())
    failures.extend(check_runtime_validation_failure_exit_code_contract())

    if failures:
        for failure in failures:
            print(failure)
        return 1

    print("[PASS] release gate static checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
