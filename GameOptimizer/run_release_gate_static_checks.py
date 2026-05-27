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
RELEASE_GATE_EVIDENCE_SELFTEST_FILE = ROOT / "release_gate_evidence_selftest.py"
VERIFY_RC_CANDIDATE_FILE = ROOT / "verify_rc_candidate.py"
VERIFY_REAL_GAME_VALIDATION_FILE = ROOT / "verify_real_game_validation.py"
CREATE_RC_EVIDENCE_BUNDLE_FILE = ROOT / "create_rc_evidence_bundle.py"
STATIC_CHECKS_SELFTEST_FILE = ROOT / "run_release_gate_static_checks_selftest.py"
RELEASE_DOCS = ROOT / "docs" / "release"
ARCHITECTURE_DOCS = ROOT / "docs" / "architecture"
DESIGN_DOCS = ROOT / "docs" / "design"
GOVERNANCE_DOCS = ROOT / "docs" / "governance"
ENGINEERING_DOCS = ROOT / "docs" / "engineering"
CONTRACTS_DOCS = ROOT / "docs" / "contracts"
HISTORY_DOCS = ROOT / "docs" / "history"
EVIDENCE_SCHEMA_FILE = RELEASE_DOCS / "Evidence_Schema.md"
RELEASE_BLOCKER_LIST_FILE = RELEASE_DOCS / "Release_Blocker_List.md"
ARCHITECTURE_DECISION_RECORD_FILE = ARCHITECTURE_DOCS / "Architecture_Decision_Record.md"
CONTRACT_ENFORCEMENT_MATRIX_FILE = ARCHITECTURE_DOCS / "Contract_Enforcement_Matrix.md"
MODULE_OWNERSHIP_MATRIX_FILE = ARCHITECTURE_DOCS / "Module_Ownership_Matrix.md"

REQUIRED_MAIN_PATTERNS = [
    ("main.cpp", r"RuntimeOrchestrator\s+orchestrator\s*\(\s*argc\s*,\s*argv\s*\)", "main must delegate to RuntimeOrchestrator"),
    ("RuntimeOrchestrator.cpp", r"RuntimeOrchestrator::run\s*\(\s*\)", "RuntimeOrchestrator run entry missing"),
    ("RuntimeOrchestrator.cpp", r"StartupPipeline::run\s*\(\s*argc_\s*,\s*argv_\s*\)", "RuntimeOrchestrator must delegate startup execution"),
    ("RuntimeOrchestrator.cpp", r"WatchdogCycleRunner\s+cycleRunner", "RuntimeOrchestrator must delegate watchdog cycles"),
    ("RuntimeOrchestrator.cpp", r"ShutdownPipeline::execute\s*\(\s*context_\s*,\s*watchdog\s*,\s*signalState_\.currentShutdownReason\s*\(\s*\)\s*\)", "RuntimeOrchestrator must delegate shutdown sequencing with shutdown reason"),
    ("RuntimeContext.h", r"struct\s+RuntimeContext", "RuntimeContext contract missing"),
    ("RuntimeContext.h", r"CliOptions\s+options", "RuntimeContext must retain parsed options state"),
    ("RuntimeContext.h", r"StartupPlan\s+startupPlan", "RuntimeContext must retain startup plan state"),
    ("RuntimeOrchestrator.h", r"RuntimeContext\s+context_", "RuntimeOrchestrator must own RuntimeContext"),
    ("RuntimeOrchestrator.h", r"RuntimeSignalState\s+signalState_", "RuntimeOrchestrator must own RuntimeSignalState"),
    ("RuntimeSignalState.h", r"struct\s+RuntimeSignalState", "RuntimeSignalState contract missing"),
    ("RuntimeContext.h", r"struct\s+StartupPlan", "StartupPlan contract missing"),
    ("StartupPipeline.cpp", r"std::expected\s*<\s*RuntimeContext\s*,\s*ErrorCode\s*>\s+StartupPipeline::run", "StartupPipeline run missing"),
    ("StartupPipeline.cpp", r"std::expected\s*<\s*StartupPlan\s*,\s*ErrorCode\s*>\s+StartupPipeline::prepare", "StartupPipeline prepare missing"),
    ("StartupPipeline.cpp", r"logStartupPolicySummary\s*\(", "StartupPipeline startup policy logging helper missing"),
    ("StartupPipeline.cpp", r"buildMainThreadPolicy\s*\(", "StartupPipeline main-thread policy helper missing"),
    ("StartupPipeline.cpp", r"loadAndPrepareBackgroundFilterConfig\s*\(", "StartupPipeline background filter helper missing"),
    ("StartupPipeline.cpp", r"buildBackgroundRestrictionPolicy\s*\(", "StartupPipeline background policy helper missing"),
    ("WatchdogCycleRunner.h", r"class\s+WatchdogCycleRunner", "WatchdogCycleRunner contract missing"),
    ("WatchdogCycleRunner.cpp", r"void\s+WatchdogCycleRunner::runCycle\s*\(", "WatchdogCycleRunner cycle entry missing"),
    ("ShutdownPipeline.h", r"struct\s+ShutdownResult", "shutdown result classification missing"),
    ("ShutdownPipeline.cpp", r"ShutdownPipeline::execute\s*\(", "ShutdownPipeline execute missing"),
    ("CliOptions.h", r"struct\s+CliOptions", "CliOptions contract missing"),
    ("CliOptions.cpp", r"std::expected\s*<\s*CliOptions\s*,\s*ErrorCode\s*>\s+parseCliOptions", "CliOptions parser missing"),
    ("StartupPipeline.cpp", r"const\s+auto&\s+topology\s*=\s*\*topologyResult\s*;", "topologyResult bind missing"),
    ("StartupPipeline.cpp", r"context\.startupPlan\.targetProcessId", "targetProcessId context bind missing"),
    ("WatchdogCycleRunner.cpp", r"const\s+auto&\s+observedThreads\s*=\s*\*topThreads\s*;", "topThreads bind missing"),
    ("WatchdogCycleRunner.cpp", r"const\s+auto&\s+commands\s*=\s*\*commandsResult\s*;", "commandsResult bind missing"),
    ("WatchdogCycleRunner.cpp", r"runtime timeout reached at watchdog cycle boundary", "runtime timeout safe-point log missing"),
    ("RuntimeOrchestrator.cpp", r"calculateRuntimeTimeoutHardGrace", "runtime timeout hard grace calculation missing"),
    ("RuntimeOrchestrator.cpp", r"watchdogInterval\s*\*\s*2", "runtime timeout hard grace must be based on watchdog interval"),
    ("RuntimeOrchestrator.cpp", r"kMinimumRuntimeTimeoutHardGrace", "runtime timeout hard grace minimum missing"),
    ("RuntimeOrchestrator.cpp", r"kMaximumRuntimeTimeoutHardGrace", "runtime timeout hard grace maximum missing"),
    ("RuntimeOrchestrator.cpp", r"max runtime hard-timeout grace exceeded", "runtime timeout hard-grace log missing"),
    ("ShutdownPipeline.cpp", r"pre-rollback", "shutdown pre-rollback evidence snapshot missing"),
    ("ShutdownPipeline.cpp", r"post-rollback", "shutdown post-rollback evidence snapshot missing"),
    ("ShutdownPipeline.cpp", r"shutdown result: reason=\{\}, timerRollbackFailed=\{\}, schedulerRollbackFailed=\{\}, runtimeValidationFailed=\{\}, rollbackStatePreserved=\{\}", "shutdown result summary log missing"),
    ("ThreadTracker.h", r"enum class\s+ThreadTrackerUpdateDisposition", "ThreadTracker update disposition contract missing"),
    ("ThreadTracker.h", r"struct\s+ThreadTrackerTelemetry", "ThreadTracker telemetry contract missing"),
    ("ThreadTracker.cpp", r"std::expected\s*<\s*ThreadTrackerUpdateDisposition\s*,\s*ErrorCode\s*>\s+ThreadTracker::update", "ThreadTracker update must return disposition"),
    ("ThreadTracker.cpp", r"recordOpenThreadFailure\s*\(", "ThreadTracker OpenThread telemetry missing"),
    ("ThreadTracker.cpp", r"recordThreadTimeQueryFailure\s*\(", "ThreadTracker GetThreadTimes telemetry missing"),
    ("ThreadTracker.cpp", r"sample\.eligibleForCandidate\s*=\s*false", "ThreadTracker failed samples must be excluded from candidates"),
    ("ThreadTracker.cpp", r"sampleWaitCondition_\.wait_for", "ThreadTracker sample wait must use persistent wait condition"),
    ("WatchdogCycleRunner.cpp", r"ResetAfterInvariantFailure", "WatchdogCycleRunner must surface ThreadTracker reset disposition"),
    ("RuntimeValidationMonitor.h", r"threadTrackerResetEvent", "RuntimeValidationMonitor must record ThreadTracker reset events"),
    ("RuntimeValidationMonitor.cpp", r"thread_tracker_reset_events", "RuntimeValidationMonitor summary must include ThreadTracker reset events"),
]

ALLOWED_EXPECTED_BIND_LINES = [
    re.compile(r"\s*(?:const\s+)?(?:auto|auto&|const\s+auto&|DWORD|const\s+DWORD|WORD|const\s+WORD|ErrorCode|const\s+ErrorCode|WinHandle&|const\s+WinHandle&|std::uint64_t&|const\s+std::uint64_t&)\s+\w+\s*=\s*\*\w+\s*;\s*"),
    re.compile(r"\s*auto\s+\w+\s*=\s*std::move\s*\(\s*\*\w+\s*\)\s*;\s*"),
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
            if not path.name.endswith("Tests.cpp") and re.search(rf"(?<![\w]){re.escape(name)}\s*\.value\s*\(", line):
                failures.append(f"[FAIL] {path.name}:{line_number}: B2 expected value() access: {name}.value()")
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
        (scheduler_text, r"stateAfterFailedAffinityApply\s*=\s*queryCurrentSchedulingState", "SetThreadGroupAffinity failure must query current state before discarding rollback state"),
        (scheduler_text, r"const\s+auto&\s+auditedState\s*=\s*\*stateAfterFailedAffinityApply\s*;", "SetThreadGroupAffinity failure audit must bind expected state before use"),
        (scheduler_text, r"matchesOriginalState\s*\(\s*auditedState\s*,\s*original\s*\)", "SetThreadGroupAffinity failure must compare post-failure state to original before discard"),
        (scheduler_text, r"main-thread affinity apply failed.*post-failure audit found state drift", "SetThreadGroupAffinity drift must be logged before rollback"),
        (scheduler_text, r"enum\s+class\s+FailedAffinityApplyDisposition", "SchedulerController affinity failure path must use an explicit disposition enum"),
        (scheduler_text, r"auditFailedAffinityApply\s*\(", "SchedulerController affinity failure audit must be isolated in a helper"),
        (scheduler_text, r"makeAffinityApplyFailureResult\s*\(", "SchedulerController affinity failure return mapping must be isolated in a helper"),
        (scheduler_text, r"logAffinityRollbackFailure\s*\(", "SchedulerController affinity rollback failure log must be isolated in a helper"),
        (background_text, r"saveProcessState\s*\(", "BackgroundController must save rollback state before process apply"),
        (background_text, r"ApplyGuard::forProcess", "BackgroundController must create process ApplyGuard"),
        (background_text, r"applyGuard\.arm\s*\(\s*\)", "BackgroundController ApplyGuard must be armed"),
        (background_text, r"applyGuard\.commit\s*\(\s*\)", "BackgroundController ApplyGuard must commit after successful apply"),
        (background_text, r"applyGuard\.rollbackNow\s*\(\s*\)", "BackgroundController priority failure path must rollback through ApplyGuard"),
        (background_text, r"validateProcessRollbackState\s*\(", "BackgroundController SoftApply must validate process rollback baseline"),
        (background_text, r"post-failure audit", "BackgroundController apply failure must audit current state before discarding rollback state"),
        (background_text, r"const\s+auto&\s+auditedAffinityState\s*=\s*\*stateAfterFailedAffinityApply\s*;", "BackgroundController affinity failure audit must bind expected state before use"),
        (background_text, r"matchesOriginalAffinity\s*\(\s*auditedAffinityState\s*,\s*processMask\s*\)", "BackgroundController affinity failure must compare bound post-failure state to original before discard"),
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
        (background_text, r"saved state discarded before mutation", "BackgroundController must not discard saved state without a post-failure audit"),
        (apply_guard_text, r"newlyCreated|preExisting|createdRollbackState", "ApplyGuard must not expose bool ownership naming"),
        (scheduler_text, r"stateAfterFailedAffinityApply\.value\s*\(\s*\)", "SchedulerController must not call expected.value() on the SetThreadGroupAffinity failure audit path"),
        (background_text, r"stateAfterFailedAffinityApply\.value\s*\(\s*\)", "BackgroundController must not call expected.value() on the SetProcessAffinityMask failure audit path"),
    ]
    for text, pattern, message in forbidden_patterns:
        if re.search(pattern, text):
            failures.append(f"[FAIL] ApplyGuard transaction gate: {message}")
    return failures


def check_thread_tracker_runtime_contracts() -> list[str]:
    failures: list[str] = []
    header_text = (ROOT / "ThreadTracker.h").read_text(encoding="utf-8", errors="replace")
    source_text = (ROOT / "ThreadTracker.cpp").read_text(encoding="utf-8", errors="replace")
    watchdog_text = (ROOT / "WatchdogCycleRunner.cpp").read_text(encoding="utf-8", errors="replace")

    forbidden_patterns = [
        (source_text, r"return\s+update\s*\(\s*std::stop_token\s*\{\s*\}\s*\)\s*;", "ThreadTracker::update() must not use unchecked return update(...) propagation"),
        (source_text, r"std::condition_variable_any\s+\w+\s*;", "ThreadTracker must not create a temporary condition_variable_any inside waitForNextSample"),
        (source_text, r"std::mutex\s+\w+\s*;\s*\n\s*std::condition_variable_any", "ThreadTracker must not allocate a temporary mutex/condition pair per sample wait"),
    ]
    for text, pattern, message in forbidden_patterns:
        if re.search(pattern, text):
            failures.append(f"[FAIL] ThreadTracker runtime gate: {message}")

    required_patterns = [
        (header_text, r"ThreadTrackerUpdateDisposition::ResetAfterInvariantFailure|ResetAfterInvariantFailure", "reset disposition must be declared"),
        (header_text, r"ThreadTrackerTelemetry", "telemetry struct must be declared"),
        (source_text, r"recordOpenThreadFailure\s*\(\s*openedHandle\.error\s*\(\s*\)\s*\)", "OpenThread failures must be recorded"),
        (source_text, r"recordThreadTimeQueryFailure\s*\(\s*currentCpuTime\.error\s*\(\s*\)\s*\)", "GetThreadTimes failures must be recorded"),
        (source_text, r"sample->eligibleForCandidate\s*=\s*false", "failed GetThreadTimes sample must be excluded"),
        (source_text, r"if\s*\(\s*!sample\.hasEma\s*\|\|\s*!sample\.eligibleForCandidate\s*\)", "candidate calculation must skip ineligible samples"),
        (source_text, r"sampleWaitCondition_\.wait_for", "waitForNextSample must use a persistent condition_variable"),
        (watchdog_text, r"ThreadTrackerUpdateDisposition::ResetAfterInvariantFailure", "upper layer must observe reset disposition"),
    ]
    for text, pattern, message in required_patterns:
        if not re.search(pattern, text):
            failures.append(f"[FAIL] ThreadTracker runtime gate: {message}")

    return failures


def check_runtime_orchestrator_signal_contracts() -> list[str]:
    failures: list[str] = []
    runtime_text = (ROOT / "RuntimeOrchestrator.cpp").read_text(encoding="utf-8", errors="replace")
    header_text = (ROOT / "RuntimeOrchestrator.h").read_text(encoding="utf-8", errors="replace")
    signal_text = (ROOT / "RuntimeSignalState.h").read_text(encoding="utf-8", errors="replace")

    required_patterns = [
        (header_text, r"RuntimeSignalState\s+signalState_", "RuntimeOrchestrator must own signal state"),
        (signal_text, r"enum class\s+ShutdownReason", "RuntimeSignalState must define structured shutdown reasons"),
        (signal_text, r"MaxRuntimeSoftTimeout", "shutdown reason must include max-runtime soft timeout"),
        (signal_text, r"MaxRuntimeHardTimeout", "shutdown reason must include max-runtime hard timeout"),
        (signal_text, r"ConsoleControl", "shutdown reason must include console control"),
        (signal_text, r"WatchdogFailure", "shutdown reason must include watchdog failure"),
        (runtime_text, r"std::atomic\s*<\s*RuntimeSignalState\s*\*\s*>\s+gActiveSignalState", "console handler bridge must only hold an active signal-state pointer"),
        (runtime_text, r"class\s+ConsoleControlRegistration", "console control registration must be RAII-owned"),
        (runtime_text, r"SetConsoleCtrlHandler\s*\(\s*handleConsoleControl\s*,\s*FALSE\s*\)", "console control handler must be unregistered"),
        (runtime_text, r"console control handler unregister failed", "console control handler unregister failure must be logged"),
        (runtime_text, r"calculateRuntimeTimeoutHardGrace\s*\(\s*context_\.startupPlan\.watchdogInterval\s*\)", "max-runtime hard timeout grace must be based on watchdog interval"),
        (runtime_text, r"hard_grace_ms=\{\}, watchdog_interval_ms=\{\}", "hard timeout log must include grace and watchdog interval evidence"),
        (runtime_text, r"signalState_\.requestShutdown\s*\(\s*ShutdownReason::MaxRuntimeHardTimeout\s*\)", "hard timeout must request shutdown with a structured reason"),
        (runtime_text, r"ShutdownPipeline::execute\s*\(\s*context_\s*,\s*watchdog\s*,\s*signalState_\.currentShutdownReason\s*\(\s*\)\s*\)", "ShutdownPipeline must receive shutdown reason"),
        (signal_text, r"void\s+requestShutdown\s*\(\s*ShutdownReason\s+reason\s*\)\s+noexcept", "RuntimeSignalState must expose reasoned requestShutdown"),
        (signal_text, r"currentShutdownReason", "RuntimeSignalState must expose current shutdown reason"),
        (signal_text, r"runtimeTimeoutRequested", "RuntimeSignalState must own timeout request state"),
    ]
    for text, pattern, message in required_patterns:
        if not re.search(pattern, text):
            failures.append(f"[FAIL] RuntimeOrchestrator signal gate: {message}")

    forbidden_patterns = [
        (runtime_text, r"std::atomic_bool\s+gRunning", "RuntimeOrchestrator must not reintroduce global gRunning ownership"),
        (runtime_text, r"std::atomic_bool\s+gRuntimeTimeoutRequested", "RuntimeOrchestrator must not reintroduce global timeout flag ownership"),
        (runtime_text, r"std::condition_variable\s+gRunStateChanged", "RuntimeOrchestrator must not reintroduce global condition_variable ownership"),
        (runtime_text, r"SetConsoleCtrlHandler\s*\(\s*handleConsoleControl\s*,\s*TRUE\s*\)\s*;\s*(?:\r?\n|\s)*auto\s+contextResult", "console handler registration must be guarded before StartupPipeline returns"),
    ]
    for text, pattern, message in forbidden_patterns:
        if re.search(pattern, text):
            failures.append(f"[FAIL] RuntimeOrchestrator signal gate: {message}")

    return failures


def check_apply_guard_rollback_failure_ownership() -> list[str]:
    failures: list[str] = []
    apply_guard_header = (ROOT / "ApplyGuard.h").read_text(encoding="utf-8", errors="replace")
    apply_guard_text = (ROOT / "ApplyGuard.cpp").read_text(encoding="utf-8", errors="replace")
    rollback_body = extract_function_body(apply_guard_text, "ApplyGuard::rollbackNow")
    if rollback_body is None:
        return ["[FAIL] ApplyGuard ownership gate: rollbackNow body not found"]

    for marker in [
        "rollback responsibility transferred to ShutdownPipeline/RollbackManager",
        "rollback state remains preserved",
        "transferRollbackFailureToShutdown()",
        "return std::unexpected(rollbackResult.error())",
    ]:
        if marker not in rollback_body:
            failures.append(f"[FAIL] ApplyGuard ownership gate: rollback failure path missing marker: {marker}")

    for marker in [
        "ApplyGuard& operator=(ApplyGuard&& other) = delete;",
        "rollbackFailureTransferredToShutdown_",
        "transferRollbackFailureToShutdown",
    ]:
        if marker not in apply_guard_header:
            failures.append(f"[FAIL] ApplyGuard ownership gate: header missing marker: {marker}")

    if "ApplyGuard& ApplyGuard::operator=(ApplyGuard&& other)" in apply_guard_text:
        failures.append("[FAIL] ApplyGuard ownership gate: move assignment implementation must remain deleted")

    destructor_body = extract_function_body(apply_guard_text, "ApplyGuard::~ApplyGuard")
    if destructor_body is None:
        failures.append("[FAIL] ApplyGuard ownership gate: destructor body not found")
    elif "rollbackFailureTransferredToShutdown_" not in destructor_body:
        failures.append("[FAIL] ApplyGuard ownership gate: destructor must suppress duplicate rollback after responsibility transfer")

    failure_index = rollback_body.find("if (!rollbackResult)")
    unexpected_index = rollback_body.find("return std::unexpected", failure_index)
    release_index = rollback_body.find("releaseWithoutAction()", failure_index)
    transfer_index = rollback_body.find("transferRollbackFailureToShutdown()", failure_index)
    if failure_index < 0:
        failures.append("[FAIL] ApplyGuard ownership gate: rollbackNow failure branch missing")
    if unexpected_index < 0:
        failures.append("[FAIL] ApplyGuard ownership gate: rollbackNow must return explicit failure")
    if release_index >= 0 and unexpected_index >= 0 and release_index < unexpected_index:
        failures.append("[FAIL] ApplyGuard ownership gate: rollbackNow must not release guard before returning rollback failure")
    if transfer_index < 0 or (unexpected_index >= 0 and transfer_index > unexpected_index):
        failures.append("[FAIL] ApplyGuard ownership gate: rollbackNow must transfer responsibility before returning rollback failure")

    return failures


def check_soft_apply_baseline_storage_contract() -> list[str]:
    failures: list[str] = []
    rollback_text = (ROOT / "RollbackManager.cpp").read_text(encoding="utf-8", errors="replace")
    evidence_text = RELEASE_GATE_EVIDENCE_FILE.read_text(encoding="utf-8", errors="replace")
    schema_text = EVIDENCE_SCHEMA_FILE.read_text(encoding="utf-8", errors="replace")

    body = extract_function_body(rollback_text, "RollbackManager::saveValidatedReadState")
    if body is None:
        return ["[FAIL] SoftApply baseline gate: saveValidatedReadState body not found"]

    for marker in ["states_", "insert_or_assign", "RollbackStateKind::Applied"]:
        if marker in body:
            failures.append(
                f"[FAIL] SoftApply baseline gate: saveValidatedReadState must not store rollback state marker: {marker}")

    required_markers = [
        (body, "audit-only, not stored as rollback state", "thread baseline must be logged as audit-only"),
        (body, "validated baseline was not stored as rollback state", "identity failure must not store baseline as rollback state"),
        (evidence_text, "soft_apply_baseline_summary", "evidence must extract soft-apply baseline as audit data"),
        (evidence_text, "rollback_preserved_state_count", "evidence must keep rollback preserved count separate"),
        (schema_text, "soft_apply_baseline_summary", "schema must document step-level soft-apply baseline evidence"),
    ]
    for text, marker, message in required_markers:
        if marker not in text:
            failures.append(f"[FAIL] SoftApply baseline gate: {message}")

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


def check_network_irq_unsupported_policy_is_warn_only() -> list[str]:
    failures: list[str] = []
    controller_text = (ROOT / "NetworkInterruptController.cpp").read_text(encoding="utf-8", errors="replace")
    controller_header_text = (ROOT / "NetworkInterruptController.h").read_text(encoding="utf-8", errors="replace")
    tests_text = (ROOT / "NetworkInterruptControllerTests.cpp").read_text(encoding="utf-8", errors="replace")
    dispatcher_text = (ROOT / "PolicyDispatcher.cpp").read_text(encoding="utf-8", errors="replace")
    build_text = BUILD_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    regression_text = REGRESSION_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    combined_text = "\n".join([
        controller_text,
        controller_header_text,
        tests_text,
        dispatcher_text,
        build_text,
        regression_text,
    ])

    required_markers = [
        "IRQ_REPIN ignored: interrupt affinity control is unsupported; continuing DPC monitoring only",
        "return {};",
        "irqRepinRequestCount",
        "irqRepinSuppressedCount",
        "unsupported IRQ_REPIN must be ignored as a non-fatal fallback",
        "unsupported IRQ_REPIN must be recorded as suppressed monitoring-only evidence",
        "IT-2: unsupported dispatcher IRQ_REPIN must be suppressed as WARN + monitoring-only evidence",
        "forced-support IRQ_REPIN path must remain non-fatal while no mutation backend exists",
        "NetworkInterruptControllerTests.cpp",
        "NetworkInterruptControllerTests.exe",
        "running NetworkInterruptControllerTests",
        "networkInterruptController_->handleIrqRepin()",
    ]
    for marker in required_markers:
        if marker not in combined_text:
            failures.append(f"[FAIL] Network IRQ gate: missing WARN/monitoring-only marker: {marker}")

    unsupported_body = extract_function_body(controller_text, "NetworkInterruptController::handleIrqRepin")
    if unsupported_body is None:
        failures.append("[FAIL] Network IRQ gate: handleIrqRepin body not found")
    else:
        unsupported_index = unsupported_body.find("!status_.interruptAffinitySupported")
        warn_index = unsupported_body.find("Logger::warn", unsupported_index)
        success_index = unsupported_body.find("return {};", unsupported_index)
        unexpected_index = unsupported_body.find("std::unexpected", unsupported_index)
        if unsupported_index < 0:
            failures.append("[FAIL] Network IRQ gate: unsupported interrupt-affinity branch missing")
        if warn_index < 0:
            failures.append("[FAIL] Network IRQ gate: unsupported IRQ_REPIN branch must log WARN")
        if success_index < 0:
            failures.append("[FAIL] Network IRQ gate: unsupported IRQ_REPIN branch must return success")
        if unexpected_index >= 0 and (success_index < 0 or unexpected_index < success_index):
            failures.append("[FAIL] Network IRQ gate: unsupported IRQ_REPIN must not return std::unexpected")
    return failures


def check_background_processor_group_policy_is_explicit() -> list[str]:
    background_text = (ROOT / "BackgroundController.cpp").read_text(encoding="utf-8", errors="replace")
    background_header_text = (ROOT / "BackgroundController.h").read_text(encoding="utf-8", errors="replace")
    rollback_text = (ROOT / "RollbackManager.cpp").read_text(encoding="utf-8", errors="replace")
    topology_text = (ROOT / "TopologyAnalyzer.cpp").read_text(encoding="utf-8", errors="replace")
    topology_tests_text = (ROOT / "TopologyAnalyzerTests.cpp").read_text(encoding="utf-8", errors="replace")
    processor_group_tests_text = (ROOT / "ProcessorGroupHedtEvidenceTests.cpp").read_text(encoding="utf-8", errors="replace")
    scheduler_text = (ROOT / "SchedulerController.cpp").read_text(encoding="utf-8", errors="replace")
    runtime_text = (ROOT / "RuntimeOrchestrator.cpp").read_text(encoding="utf-8", errors="replace")
    startup_text = (ROOT / "StartupPipeline.cpp").read_text(encoding="utf-8", errors="replace")
    shutdown_text = (ROOT / "ShutdownPipeline.cpp").read_text(encoding="utf-8", errors="replace")
    runtime_context_text = (ROOT / "RuntimeContext.h").read_text(encoding="utf-8", errors="replace")
    runtime_integration_text = "\n".join([runtime_text, startup_text, shutdown_text, runtime_context_text])
    watchdog_text = (ROOT / "WatchdogCycleRunner.cpp").read_text(encoding="utf-8", errors="replace")
    processor_group_design_text = (DESIGN_DOCS / "ProcessorGroupPhase2Design.md").read_text(encoding="utf-8", errors="replace")
    safety_runbook_text = (RELEASE_DOCS / "Operational_Runbook.md").read_text(encoding="utf-8", errors="replace")
    readme_text = (ROOT / "README.txt").read_text(encoding="utf-8", errors="replace")
    roadmap_text = (GOVERNANCE_DOCS / "Development_Roadmap.md").read_text(encoding="utf-8", errors="replace")
    project_text = PROJECT_FILE.read_text(encoding="utf-8", errors="replace")
    build_text = BUILD_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    regression_text = REGRESSION_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    combined_text = "\n".join([
        background_text,
        background_header_text,
        rollback_text,
        topology_text,
        topology_tests_text,
        processor_group_tests_text,
        scheduler_text,
        runtime_text,
        processor_group_design_text,
        safety_runbook_text,
        readme_text,
        roadmap_text,
        project_text,
        build_text,
        regression_text,
    ])
    required_markers = [
        "return processorGroup == 0;",
        "blockedByUnsupportedProcessorGroup",
        "blockedProcessorGroup",
        "background restriction blocked: processor group",
        "priority-class-only background restriction is also blocked until affinity and priority rollback state are split",
        "background_restriction_mode=monitoring_only_due_to_processor_group",
        "process_wide_affinity_supported=false",
        "priority-only process rollback",
        "priority rollback state",
        "thread-level SetThreadGroupAffinity remains supported",
        "SetThreadGroupAffinity",
        "saveProcessState(",
        "policy.processorGroup",
        "background rollback state saved for PID {} (observedGroup={}, rollbackMode={}",
        "background rollback restored PID {} (observedGroup={}, rollbackMode={}",
        "RollbackMode::LegacyProcessAffinityMask",
        "RollbackMode::GroupAwareUnsupported",
        "UnsupportedProcessorGroupRollback",
        "return std::unexpected(ErrorCode::UnsupportedProcessorGroupRollback);",
        "background rollback state save blocked for PID {} observedGroup={}",
        "SetProcessAffinityMask cannot restore processor-group-specific process affinity",
        "explicit safety limitation",
        "WARN/monitoring-only evidence",
        "Per-thread group-aware background restriction",
        "LOW priority",
        "Future Phase",
        "Release Candidate Stabilization",
        "Release Hardening",
        "Future Architecture",
        "preserved rollback state count: thread={}, process={}",
        "mask_provenance",
        "TopologyMaskProvenance::ProcessAffinityFallback",
        "topology fallback policy selected from process affinity: group={}",
        "ProcessorGroupHedtEvidenceTests.cpp",
        "ProcessorGroupHedtEvidenceTests.exe",
        "running ProcessorGroupHedtEvidenceTests",
        "ProcessorGroup/HEDT evidence tests completed",
        "processorGroup == 1",
        "targetAffinity.Group = state.originalProcessorGroup.value_or(0);",
        "rollback audit passed for TID {} (group={}",
        "expected(group={}",
    ]
    failures: list[str] = []
    for marker in required_markers:
        if marker not in combined_text:
            failures.append(
                f"[FAIL] BackgroundController processor-group gate: missing explicit policy marker: {marker}")

    apply_body = extract_function_body(background_text, "BackgroundController::applyRestriction")
    if apply_body is None:
        failures.append("[FAIL] BackgroundController processor-group gate: applyRestriction body not found")
    else:
        support_guard_index = apply_body.find("supportsProcessWideRestrictionForGroup(policy.processorGroup)")
        affinity_apply_index = apply_body.find("SetProcessAffinityMask")
        if support_guard_index < 0:
            failures.append("[FAIL] BackgroundController processor-group gate: missing processor group support guard")
        if affinity_apply_index < 0:
            failures.append("[FAIL] BackgroundController processor-group gate: missing SetProcessAffinityMask marker")
        if support_guard_index >= 0 and affinity_apply_index >= 0 and support_guard_index > affinity_apply_index:
            failures.append(
                "[FAIL] BackgroundController processor-group gate: SetProcessAffinityMask appears before group support guard")
    return failures


def check_cpp20_runtime_contracts() -> list[str]:
    failures: list[str] = []
    topology_text = (ROOT / "TopologyAnalyzer.cpp").read_text(encoding="utf-8", errors="replace")
    topology_without_comments = strip_comments_and_strings(topology_text)
    logger_text = (ROOT / "Logger.h").read_text(encoding="utf-8", errors="replace")
    if "#include <bit>" not in topology_text:
        failures.append("[FAIL] C++20 bit gate: TopologyAnalyzer must include <bit>")
    if "std::popcount" not in topology_without_comments:
        failures.append("[FAIL] C++20 bit gate: TopologyAnalyzer must use std::popcount")
    if "std::countr_zero" not in topology_without_comments:
        failures.append("[FAIL] C++20 bit gate: TopologyAnalyzer must use std::countr_zero")
    for marker in [
        "std::chrono::system_clock::now",
        "localtime_s",
        "sequenceCounter",
        "[{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}][#{:06}][{}]",
    ]:
        if marker not in logger_text:
            failures.append(f"[FAIL] Logger observability gate: missing marker: {marker}")

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

    warning_levels = re.findall(r"<WarningLevel>([^<]+)</WarningLevel>", text)
    if not warning_levels:
        failures.append("[FAIL] MSVC warning gate: no WarningLevel entries found")
    for warning_level in warning_levels:
        if warning_level != "Level4":
            failures.append(
                f"[FAIL] MSVC warning gate: WarningLevel must be Level4, found {warning_level}")

    treat_warning_as_error = re.findall(r"<TreatWarningAsError>([^<]+)</TreatWarningAsError>", text)
    if not treat_warning_as_error:
        failures.append("[FAIL] MSVC warning gate: TreatWarningAsError entries missing")
    for setting in treat_warning_as_error:
        if setting != "true":
            failures.append(
                f"[FAIL] MSVC warning gate: TreatWarningAsError must be true, found {setting}")

    conformance_modes = re.findall(r"<ConformanceMode>([^<]+)</ConformanceMode>", text)
    if not conformance_modes:
        failures.append("[FAIL] MSVC conformance gate: no ConformanceMode entries found")
    for conformance_mode in conformance_modes:
        if conformance_mode != "true":
            failures.append(
                f"[FAIL] MSVC conformance gate: ConformanceMode must be true, found {conformance_mode}")

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
    runtime_text = (ROOT / "RuntimeOrchestrator.cpp").read_text(encoding="utf-8", errors="replace")
    startup_text = (ROOT / "StartupPipeline.cpp").read_text(encoding="utf-8", errors="replace")
    shutdown_text = (ROOT / "ShutdownPipeline.cpp").read_text(encoding="utf-8", errors="replace")
    runtime_context_text = (ROOT / "RuntimeContext.h").read_text(encoding="utf-8", errors="replace")
    runtime_integration_text = "\n".join([runtime_text, startup_text, shutdown_text, runtime_context_text])

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
        "std::unique_ptr<TimerResolutionController> timerResolutionController",
        "std::unique_ptr<InputLatencyController> inputLatencyController",
        "context.timerResolutionController->rollback()",
    ]
    for marker in required_main_markers:
        if marker not in runtime_integration_text:
            failures.append(f"[FAIL] Timer/Input gate: runtime integration missing: {marker}")

    input_latency_text = (ROOT / "InputLatencyController.cpp").read_text(encoding="utf-8", errors="replace")
    required_input_markers = [
        "RawInputDetectionPath::LocalProcessRegisteredDevices",
        "RawInputDetectionPath::RemoteProcessUnsupported",
        "InputThreadTidConfidence::Low",
        "InputThreadTidConfidence::High",
        "InputThreadTidSource::EtwInvestigationPending",
        "InputThreadTidSource::ConcreteTid",
        "GetRegisteredRawInputDevices",
        "isInputThreadPinningAllowed",
        "status.tidConfidence == InputThreadTidConfidence::High",
        "status.tidSource == InputThreadTidSource::ConcreteTid",
        "pinningEligible",
        "fallbackMonitoringOnly",
        "TID source is not ConcreteTid",
    ]
    for marker in required_input_markers:
        if marker not in input_latency_text:
            failures.append(f"[FAIL] Timer/Input gate: Raw Input fallback contract missing: {marker}")

    tests_text = (ROOT / "TimerInputControllerTests.cpp").read_text(encoding="utf-8", errors="replace")
    required_input_test_markers = [
        "pinning must reject medium-confidence non-concrete TID candidates",
        "pinning must reject high-confidence candidates until the TID source is ConcreteTid",
        "High confidence, and ConcreteTid source",
    ]
    for marker in required_input_test_markers:
        if marker not in tests_text:
            failures.append(f"[FAIL] Timer/Input gate: concrete TID pinning test missing: {marker}")

    return failures


def check_anti_cheat_fallback_contract() -> list[str]:
    failures: list[str] = []
    required_files = [
        DESIGN_DOCS / "AntiCheatFallbackDesign.md",
        ROOT / "AntiCheatFallbackTests.cpp",
    ]
    for path in required_files:
        if not path.exists():
            failures.append(f"[FAIL] Anti-cheat fallback gate: required file missing: {path.name}")

    scheduler_text = (ROOT / "SchedulerController.cpp").read_text(encoding="utf-8", errors="replace")
    scheduler_header_text = (ROOT / "SchedulerController.h").read_text(encoding="utf-8", errors="replace")
    background_text = (ROOT / "BackgroundController.cpp").read_text(encoding="utf-8", errors="replace")
    background_header_text = (ROOT / "BackgroundController.h").read_text(encoding="utf-8", errors="replace")
    rollback_text = (ROOT / "RollbackManager.cpp").read_text(encoding="utf-8", errors="replace")
    runtime_text = (ROOT / "RuntimeOrchestrator.cpp").read_text(encoding="utf-8", errors="replace")
    watchdog_text = (ROOT / "WatchdogCycleRunner.cpp").read_text(encoding="utf-8", errors="replace")
    tests_text = (ROOT / "AntiCheatFallbackTests.cpp").read_text(encoding="utf-8", errors="replace")
    assertions_text = (ROOT / "run_release_gate_log_assertions.py").read_text(encoding="utf-8", errors="replace")
    winapi_text = (ROOT / "WinApiError.h").read_text(encoding="utf-8", errors="replace")
    build_text = BUILD_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    regression_text = REGRESSION_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    combined_text = "\n".join([
        scheduler_text,
        scheduler_header_text,
        background_text,
        background_header_text,
        rollback_text,
        runtime_text,
        watchdog_text,
        tests_text,
        assertions_text,
        winapi_text,
        build_text,
        regression_text,
    ])

    required_markers = [
        "ERROR_ACCESS_DENIED",
        "ErrorCode::AccessDenied",
        "SchedulerController::isRecoverableAccessLimitation",
        "BackgroundController::isRecoverableAccessLimitation",
        "monitoring-only fallback remains active",
        "recoverable access limitation",
        "rollback path was invoked when needed",
        "saved state discarded before mutation",
        "validate_access_denied_fallback_evidence",
        "access-denied/access-boundary log lacks fallback or rollback evidence",
        "background rollback skipped for PID {} because the original process is no longer openable or is blocked by an access boundary",
        "AntiCheatFallbackTests.cpp",
        "AntiCheatFallbackTests.exe",
        "running AntiCheatFallbackTests",
        "testAccessDeniedFallbackEvidenceMarkersArePresent",
    ]
    for marker in required_markers:
        if marker not in combined_text:
            failures.append(f"[FAIL] Anti-cheat fallback gate: missing marker: {marker}")

    return failures


def check_rc_gate_contract() -> list[str]:
    failures: list[str] = []
    rc_gate_path = ROOT / "run_rc_gate.bat"
    if not rc_gate_path.exists():
        return ["[FAIL] RC gate: run_rc_gate.bat is missing"]

    rc_text = rc_gate_path.read_text(encoding="utf-8", errors="replace")
    required_markers = [
        "py_compile",
        "git diff --check",
        "run_release_gate_static_checks_selftest.py",
        "run_release_gate_static_checks.py",
        "msbuild GameOptimizer.slnx /p:Configuration=Release /p:Platform=x64 /m",
        "run_regression_tests.bat",
        "run_release_gate_smoke.bat",
        "run_long_soak_presets.bat",
        "release_gate_evidence.py verify-rc",
        "verify_real_game_validation.py --matrix docs\\release\\Game_Verification_Matrix.json",
        "verify_rc_candidate.py",
        "create_rc_evidence_bundle.py",
        "RC_BLOCKER",
        "[BLOCKER] RC gate failed",
        "release smoke failed",
        "30m or 60m soak failed",
        "verify-rc failed",
        "real game validation failed",
        "py_compile failed",
        "git diff --check failed",
        "static gate selftest failed",
        "Release x64 MSVC build failed",
        "RC candidate verification failed",
        "RC evidence bundle creation failed",
        "both",
        "release_gate_logs",
        "[PASS] RC gate passed",
        "[FAIL] RC gate failed",
    ]
    for marker in required_markers:
        if marker not in rc_text:
            failures.append(f"[FAIL] RC gate: missing marker: {marker}")

    ordered_steps = [
        "echo [RC-1] Python syntax gate",
        "echo [RC-2] git diff whitespace gate",
        "echo [RC-3] static release gate",
        "echo [RC-4] Release x64 MSVC build",
        "echo [RC-5] full regression",
        "echo [RC-6] release smoke",
        "echo [RC-7] long soak evidence gate: 30m dry-run + 60m soft-apply",
        "echo [RC-8] verify RC evidence set",
        "echo [RC-9] verify RC candidate package inputs",
        "echo [RC-10] create final RC evidence bundle",
    ]
    failures.extend(validate_ordered_markers("RC gate", rc_text, ordered_steps))
    failures.extend(validate_ordered_markers("RC static gate selftest", rc_text, [
        "echo [RC-3] static release gate",
        "run_release_gate_static_checks_selftest.py",
        "run_release_gate_static_checks.py",
    ]))
    failures.extend(validate_ordered_markers("RC real game validation", rc_text, [
        "echo [RC-9] verify RC candidate package inputs",
        "verify_real_game_validation.py --matrix docs\\release\\Game_Verification_Matrix.json",
        "verify_rc_candidate.py",
    ]))

    return failures


def validate_ordered_markers(label: str, text: str, ordered_markers: list[str]) -> list[str]:
    failures: list[str] = []
    previous_position = -1
    for marker in ordered_markers:
        position = text.find(marker)
        if position == -1:
            failures.append(f"[FAIL] {label}: missing ordered step marker: {marker}")
            continue
        if position <= previous_position:
            failures.append(f"[FAIL] {label}: ordered step is out of sequence: {marker}")
        previous_position = position
    return failures


def markdown_section_contains_marker(text: str, heading: str, marker: str) -> bool:
    heading_marker = f"## {heading}"
    heading_index = text.find(heading_marker)
    if heading_index < 0:
        return False

    next_heading_index = text.find("\n## ", heading_index + len(heading_marker))
    section = text[heading_index:] if next_heading_index < 0 else text[heading_index:next_heading_index]
    return marker in section


def check_static_gate_selftest_contract() -> list[str]:
    failures: list[str] = []
    if not STATIC_CHECKS_SELFTEST_FILE.exists():
        return ["[FAIL] static gate selftest: run_release_gate_static_checks_selftest.py is missing"]

    text = STATIC_CHECKS_SELFTEST_FILE.read_text(encoding="utf-8", errors="replace")
    required_markers = [
        "validate_ordered_markers",
        "test_ordered_markers_pass",
        "test_ordered_markers_reject_missing_marker",
        "test_ordered_markers_reject_out_of_order_marker",
        "test_rc9_real_game_validation_runs_before_candidate_verification",
        "test_bundle_creation_validates_real_game_matrix_before_writing_bundle",
        "test_bundle_manifest_preserves_real_game_matrix_artifact",
        "test_bundle_creation_validates_manifest_artifact_hashes_before_pass",
        "test_bundle_creation_validates_written_manifests_before_pass",
        "test_bundle_creation_validates_source_reports_before_pass",
        "test_bundle_creation_validates_source_report_artifact_identity_before_pass",
        "test_bundle_creation_validates_regression_selftest_summary_against_artifact_before_pass",
        "test_bundle_validators_accept_real_files",
        "test_bundle_regression_selftest_summary_matches_bundled_log",
        "test_bundle_source_report_artifact_identity_matches_real_files",
        "test_bundle_validators_reject_missing_or_mismatched_files",
        "test_rc_candidate_regression_log_requires_selftest_pass_markers",
        "test_bundle_manifest_records_regression_selftest_summary",
        "test_cem_gate_requires_recent_contract_markers",
        "[PASS] static gate selftest passed",
    ]
    for marker in required_markers:
        if marker not in text:
            failures.append(f"[FAIL] static gate selftest: missing marker: {marker}")

    regression_text = REGRESSION_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    required_regression_markers = [
        "running run_release_gate_static_checks_selftest",
        "run_release_gate_static_checks_selftest.py",
        "run_release_gate_static_checks_selftest failed",
        "run_release_gate_static_checks_selftest passed",
    ]
    for marker in required_regression_markers:
        if marker not in regression_text:
            failures.append(f"[FAIL] static gate selftest: regression script missing marker: {marker}")

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
            "[BLOCKER] SOAK-30M command failed",
            "[BLOCKER] SOAK-60M command failed",
            "[BLOCKER] SOAK-30M log assertions failed",
            "[BLOCKER] SOAK-60M log assertions failed",
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
    if not RELEASE_GATE_EVIDENCE_SELFTEST_FILE.exists():
        return ["[FAIL] RC evidence gate: release_gate_evidence_selftest.py is missing"]

    evidence_text = RELEASE_GATE_EVIDENCE_FILE.read_text(encoding="utf-8", errors="replace")
    evidence_selftest_text = RELEASE_GATE_EVIDENCE_SELFTEST_FILE.read_text(encoding="utf-8", errors="replace")
    regression_text = REGRESSION_TESTS_FILE.read_text(encoding="utf-8", errors="replace")
    required_evidence_markers = [
        "rc_evidence_report.json",
        "rc_evidence_report.txt",
        "exe_sha256",
        "binary_fingerprint",
        "git_dirty",
        "git_status_short",
        "git_commit",
        "build_hash",
        "shutdown_failure_classification",
        "rollback_preserved_state_summary",
        "apply_guard_rollback_failure_count",
        "rollback_failure_transferred_to_shutdown_count",
        "processor_group_mode_summary",
        "thread_tracker_telemetry_summary",
        "input_latency_summary",
        "network_irq_summary",
        "access_denied_fallback_summary",
        "runtime_validation_summary",
        "SEVERITY_POLICY",
        "not_stored_as_rollback_state_count",
        "runtime validation FAILED must pair with process exit code 1",
        "SetThreadGroupAffinity failure post-failure audit query failed",
        "SetThreadGroupAffinity failure post-failure audit mismatch",
        "timeline monotonicity failure",
        "heartbeat progression failure",
        "unsafe rollback state discard",
        "ApplyGuard explicit rollback failure was not fully transferred",
        "required RC soak step missing",
        "required smoke step missing",
        "verify-rc",
        "EXPECTED_SCHEMA",
        "schema version mismatch",
        "schema_hash mismatch",
        "git commit mismatch",
        "dirty tree flag is missing",
        "dirty tree status is missing",
        "exe SHA-256 mismatch",
        "BLOCKER",
        "PASS_WITH_WARNINGS",
        "severity_summary",
        "release_gate_logs",
    ]
    for marker in required_evidence_markers:
        if marker not in evidence_text:
            failures.append(f"[FAIL] RC evidence gate: evidence writer missing marker: {marker}")

    required_selftest_markers = [
        "release_gate_evidence_selftest.py",
        "missing evidence unexpectedly passed",
        "complete synthetic evidence did not pass",
        "WARN-only evidence did not produce PASS_WITH_WARNINGS",
        "schema version mismatch unexpectedly passed",
        "schema hash mismatch unexpectedly passed",
        "git commit mismatch unexpectedly passed",
        "exe SHA-256 mismatch unexpectedly passed",
        "missing binary SHA-256 unexpectedly passed",
        "runtime validation FAILED did not become a BLOCKER with exit code 1",
        "rollback failure did not become a BLOCKER",
        "SetThreadGroupAffinity audit query failure did not become a BLOCKER",
        "SetThreadGroupAffinity audit mismatch did not become a BLOCKER",
        "timeline monotonicity failure did not become a BLOCKER",
        "heartbeat progression failure did not become a BLOCKER",
        "unsafe rollback state discard did not become a BLOCKER",
        "ApplyGuard rollback failure count: 1",
        "Rollback failure transferred to shutdown count: 1",
        "ApplyGuard rollback failure without transfer did not add transfer BLOCKER",
        "ApplyGuard rollback failure with transfer duplicated transfer BLOCKER",
        "ApplyGuard destructor rollback failure did not become a single BLOCKER",
        "Rollback preserved state count: 0",
        "not_stored_as_rollback_state_count",
        "Access Denied fallback did not remain WARN-only",
        "group 1+ mock did not remain WARN-only",
        "telemetry INFO did not remain INFO-only",
        "RC evidence self-test passed",
    ]
    combined_selftest_text = "\n".join([evidence_selftest_text, regression_text])
    for marker in required_selftest_markers:
        if marker not in combined_selftest_text:
            failures.append(f"[FAIL] RC evidence gate: self-test registration missing marker: {marker}")

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

    smoke_text = (ROOT / "run_release_gate_smoke.bat").read_text(encoding="utf-8", errors="replace")
    required_smoke_blocker_markers = [
        "[BLOCKER] RG-1 dry-run command failed",
        "[BLOCKER] RG-2 soft-apply command failed",
        "[BLOCKER] RG-3 apply command failed",
        "[BLOCKER] RG-4 timeout command failed",
        "[BLOCKER] RG-1 dry-run log assertions failed",
        "[BLOCKER] RG-2 soft-apply log assertions failed",
        "[BLOCKER] RG-3 apply log assertions failed",
        "[BLOCKER] RG-4 timeout log assertions failed",
    ]
    for marker in required_smoke_blocker_markers:
        if marker not in smoke_text:
            failures.append(f"[FAIL] RC smoke gate: missing BLOCKER marker: {marker}")

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


def check_rc_candidate_contract() -> list[str]:
    failures: list[str] = []
    if not VERIFY_RC_CANDIDATE_FILE.exists():
        return ["[FAIL] RC candidate gate: verify_rc_candidate.py is missing"]
    if not VERIFY_REAL_GAME_VALIDATION_FILE.exists():
        return ["[FAIL] RC candidate gate: verify_real_game_validation.py is missing"]
    if not CREATE_RC_EVIDENCE_BUNDLE_FILE.exists():
        return ["[FAIL] RC candidate gate: create_rc_evidence_bundle.py is missing"]
    if not EVIDENCE_SCHEMA_FILE.exists():
        return ["[FAIL] RC candidate gate: Evidence_Schema.md is missing"]
    if not RELEASE_BLOCKER_LIST_FILE.exists():
        return ["[FAIL] RC candidate gate: Release_Blocker_List.md is missing"]

    candidate_text = VERIFY_RC_CANDIDATE_FILE.read_text(encoding="utf-8", errors="replace")
    real_game_text = VERIFY_REAL_GAME_VALIDATION_FILE.read_text(encoding="utf-8", errors="replace")
    bundle_text = CREATE_RC_EVIDENCE_BUNDLE_FILE.read_text(encoding="utf-8", errors="replace")
    runbook_text = (RELEASE_DOCS / "Release_Gate_Spec.md").read_text(encoding="utf-8", errors="replace")
    rc_runbook_text = (RELEASE_DOCS / "RC_Runbook.md").read_text(encoding="utf-8", errors="replace")
    matrix_text = (RELEASE_DOCS / "Release_Regression_Matrix.md").read_text(encoding="utf-8", errors="replace")
    ops_text = (RELEASE_DOCS / "Operational_Runbook.md").read_text(encoding="utf-8", errors="replace")
    evidence_schema_text = EVIDENCE_SCHEMA_FILE.read_text(encoding="utf-8", errors="replace")
    blocker_list_text = RELEASE_BLOCKER_LIST_FILE.read_text(encoding="utf-8", errors="replace")
    adr_text = ARCHITECTURE_DECISION_RECORD_FILE.read_text(encoding="utf-8", errors="replace") if ARCHITECTURE_DECISION_RECORD_FILE.exists() else ""
    combined_text = "\n".join([
        candidate_text,
        real_game_text,
        bundle_text,
        runbook_text,
        rc_runbook_text,
        matrix_text,
        ops_text,
        evidence_schema_text,
        blocker_list_text,
        adr_text,
    ])

    required_markers = [
        "verify_rc_candidate.py --target <target.exe> --regression-log <log>",
        "create_rc_evidence_bundle.py --target <target.exe> --regression-log <log>",
        "v3.0-rc1",
        "Runbook",
        "blocker list",
        "Evidence_Schema.md",
        "Release_Blocker_List.md",
        "Architecture_Decision_Record.md",
        "evidence bundle",
        "rc_evidence_bundle_manifest.json",
        "rc_evidence_bundle_manifest.txt",
        "RC_CANDIDATE_PASS",
        "[PASS] RC evidence bundle created",
        "severity_policy",
        "thread_tracker_telemetry_summary",
        "input_latency_summary",
        "network_irq_summary",
        "access_denied_fallback_summary",
        "real_game_validation_matrix",
        "real_game_validation_matrix_sha256",
        "real_game_validation_matrix_artifact",
        "regression_selftest_summary",
        "collect_regression_selftest_summary",
        "collect_regression_selftest_summary_from_text",
        "validate_bundled_regression_selftest_summary",
        "find_bundle_artifact(manifest, \"final_regression_log\")",
        "RC evidence bundle regression selftest validation",
        "regression selftest summary mismatch",
        "final_regression_log",
        "regression selftest did not pass",
        "regression selftest summary is missing or invalid",
        "Regression selftest summary:",
        "validate_bundle_artifacts",
        "resolve_bundle_artifact_path",
        "RC evidence bundle artifact validation",
        "validate_written_manifests",
        "validate_bundle_source_reports",
        "validate_bundle_source_report_artifact_identity",
        "RC evidence bundle manifest validation",
        "\"schema_hash\"",
        "\"target_process\"",
        "\"git_commit\"",
        "\"binary_sha256\"",
        "Schema hash:",
        "Status:",
        "Target process:",
        "Git commit:",
        "Binary SHA-256:",
        "RC evidence bundle source report validation",
        "RC evidence bundle source report artifact validation",
        "JSON bundle manifest field mismatch",
        "text bundle manifest missing marker",
        "source report path is missing",
        "source report artifact SHA-256 mismatch",
        "SHA-256 mismatch",
        "byte size mismatch",
        "final regression result",
        "failed=0",
        "[PASS] all regression tests passed",
        "final regression log missing required selftest PASS marker",
        "[PASS] run_release_gate_static_checks_selftest passed",
        "[PASS] release_gate_evidence_selftest passed",
        "validate_evidence_bundle",
        "validate_real_game_matrix",
        "validate_regression_log",
        "validate_runbooks",
        "Real_Game_Validation_Runbook.md",
        "verify_real_game_validation.py",
        "Game_Verification_Matrix.json",
        "Game A",
        "Game B",
        "Game C",
        "dry-run",
        "soft-apply",
        "duration_minutes",
        "rollback_preserved_state_count",
        "policy_decision_telemetry",
        "SCHEMA_VERSION",
        "REQUIRED_RUN_FIELDS",
        "PLACEHOLDER_TOKENS",
        "contains_placeholder",
        "resolve_evidence_path",
        "has_run_evidence",
        "evidence_report file is missing",
        "evidence_report contains a placeholder",
        "Real game validation matrix still contains copied template notes",
        "Real game validation run is missing an `evidence_report` field",
        "Real game validation run `evidence_report` points to a missing artifact",
        "Real game validation `schema_version` is missing or mismatched",
        "every real-game validation run links to an existing `evidence_report` artifact",
        "placeholder value is forbidden",
        "real game validation notes still contain template or synthetic placeholder text",
        "real game validation schema_version mismatch",
        "real game validation requires at least 3 games",
        "real game validation requires at least 2 successful 60m soft-apply runs",
        "real game validation requires 1 limited apply-mode validation",
        "[BLOCKER] RC candidate verification",
        "gameoptimizer.rc_evidence.v1",
        "gameoptimizer.rc_evidence_bundle.v1",
        "git_dirty",
        "git_status_short",
        "shutdown_failure_classification.shutdown_reason",
        "apply_guard_rollback_failure",
        "apply_guard_rollback_failure_count",
        "rollback_failure_transferred_to_shutdown_count",
        "Runtime validation result is `FAILED`",
        "Dirty tree flag or status is missing from evidence",
        "v3.0-rc1 intentional exclusions",
        "ADR-001: Runtime Mutation Is Transactional",
        "ADR-008: Access-Boundary Failures Are Fallback Evidence Unless Mutation Escaped",
        "ADR-009: Input Thread Pinning Requires High-Confidence Concrete TID Evidence",
        "ADR-010: Apply Mode Remains Limited And Explicit",
        "A code, test, runbook, or release-gate change conflicts with an accepted ADR",
    ]
    for marker in required_markers:
        if marker not in combined_text:
            failures.append(f"[FAIL] RC candidate gate: missing marker: {marker}")

    soft_apply_preserved_marker = (
        "SoftApply baseline evidence increases `rollback_preserved_state_count` "
        "or creates BLOCKER/WARN findings by itself.")
    if not markdown_section_contains_marker(blocker_list_text, "BLOCKER", soft_apply_preserved_marker):
        failures.append(
            "[FAIL] RC candidate gate: SoftApply preserved-state evidence drift must be listed as BLOCKER")
    if markdown_section_contains_marker(blocker_list_text, "WARN", soft_apply_preserved_marker):
        failures.append(
            "[FAIL] RC candidate gate: SoftApply preserved-state evidence drift must not be listed as WARN")

    failures.extend(validate_ordered_markers("RC bundle real game validation", bundle_text, [
        "verify_rc_candidate.validate_evidence_bundle(target)",
        "verify_rc_candidate.validate_real_game_matrix()",
        "create_bundle_dir(commit)",
    ]))
    failures.extend(validate_ordered_markers("RC bundle artifact validation", bundle_text, [
        "artifact_failures = validate_bundle_artifacts(manifest)",
        "json_manifest_path = bundle_dir / \"rc_evidence_bundle_manifest.json\"",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]))
    failures.extend(validate_ordered_markers("RC written manifest validation", bundle_text, [
        "write_json(json_manifest_path, manifest)",
        "write_text_manifest(text_manifest_path, manifest)",
        "manifest_failures = validate_written_manifests(",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]))
    failures.extend(validate_ordered_markers("RC bundle source report validation", bundle_text, [
        "source_report_failures = validate_bundle_source_reports(manifest)",
        "source_identity_failures = validate_bundle_source_report_artifact_identity(manifest)",
        "write_json(json_manifest_path, manifest)",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]))
    failures.extend(validate_ordered_markers("RC bundle source report artifact validation", bundle_text, [
        "validate_bundle_source_reports(manifest)",
        "validate_bundle_source_report_artifact_identity(manifest)",
        "validate_bundled_regression_selftest_summary(manifest)",
        "write_json(json_manifest_path, manifest)",
    ]))
    failures.extend(validate_ordered_markers("RC bundle regression selftest validation", bundle_text, [
        "validate_bundle_artifacts(manifest)",
        "validate_bundled_regression_selftest_summary(manifest)",
        "write_json(json_manifest_path, manifest)",
        "print(f\"[PASS] RC evidence bundle created: {bundle_dir}\")",
    ]))
    return failures


def check_apply_mode_policy_contract() -> list[str]:
    failures: list[str] = []
    cli_text = (ROOT / "CliOptions.cpp").read_text(encoding="utf-8", errors="replace")
    startup_text = (ROOT / "StartupPipeline.cpp").read_text(encoding="utf-8", errors="replace")
    real_game_text = VERIFY_REAL_GAME_VALIDATION_FILE.read_text(encoding="utf-8", errors="replace")
    limitations_text = (RELEASE_DOCS / "Known_Limitations.md").read_text(encoding="utf-8", errors="replace")
    blocker_text = RELEASE_BLOCKER_LIST_FILE.read_text(encoding="utf-8", errors="replace")
    combined_text = "\n".join([cli_text, startup_text, real_game_text, limitations_text, blocker_text])

    required_markers = [
        "return SchedulerMode::SoftApply",
        "if (modeArgument == L\"--apply\")",
        "apply mode restriction policy",
        "dry-run PASS",
        "soft-apply PASS",
        "no Access Denied",
        "rollback state save success",
        "sufficient ThreadTracker confidence",
        "ApplyGuard audit PASS",
        "verified group-aware thread path",
        "anti-cheat is suspected",
        "Access Denied repeats",
        "rollback state save fails",
        "Raw Input TID confidence is Low",
        "group 1+ process-wide background restriction is required",
        "soft-apply WARN count is high",
        "MAX_SOFT_APPLY_WARN_COUNT_FOR_APPLY",
        "apply mode requires a prior PASS 30m dry-run",
        "apply mode requires a prior PASS 60m soft-apply",
        "apply mode must use an explicit --apply option",
        "apply mode must be limited",
    ]
    for marker in required_markers:
        if marker not in combined_text:
            failures.append(f"[FAIL] Apply mode policy gate: missing marker: {marker}")
    return failures


def check_known_limitations_contract() -> list[str]:
    failures: list[str] = []
    limitations_text = (RELEASE_DOCS / "Known_Limitations.md").read_text(encoding="utf-8", errors="replace")
    required_markers = [
        "Group 1+ process-wide background affinity is `monitoring-only`",
        "IRQ repin is available only when the NIC/driver/runtime path supports interrupt affinity control",
        "Raw Input not detected means input thread pinning is forbidden",
        "Anti-cheat Access Denied is a normal fallback path",
        "does not modify game memory",
        "does not use DLL injection",
        "does not use kernel patching",
        "does not use driver patching",
        "Apply mode remains in limited validation status",
    ]
    for marker in required_markers:
        if marker not in limitations_text:
            failures.append(f"[FAIL] Known limitations gate: missing marker: {marker}")
    return failures


def check_runtime_validation_failure_exit_code_contract() -> list[str]:
    failures: list[str] = []
    orchestrator_text = (ROOT / "RuntimeOrchestrator.cpp").read_text(encoding="utf-8", errors="replace")
    shutdown_text = (ROOT / "ShutdownPipeline.cpp").read_text(encoding="utf-8", errors="replace")
    for marker in [
        "ShutdownPipeline::execute(",
        "if (shutdownResult.failed())",
        'Logger::error("shutdown completed with runtime validation failure");',
        "return 1;",
    ]:
        if marker not in orchestrator_text:
            failures.append(f"[FAIL] Runtime validation exit-code gate: orchestrator missing marker: {marker}")

    for marker in [
        "ShutdownResult shutdownResult{};",
        "shutdownResult.runtimeValidationFailed = context.runtimeValidationMonitor->hasCriticalFailure();",
        "shutdown result: reason={}, timerRollbackFailed={}, schedulerRollbackFailed={}, runtimeValidationFailed={}, rollbackStatePreserved={}",
    ]:
        if marker not in shutdown_text:
            failures.append(f"[FAIL] Runtime validation exit-code gate: shutdown pipeline missing marker: {marker}")

    assertions_text = (ROOT / "run_release_gate_log_assertions.py").read_text(encoding="utf-8", errors="replace")
    if "runtime validation result: FAILED" not in assertions_text:
        failures.append("[FAIL] Runtime validation exit-code gate: log assertion must reject FAILED result")
    if "rollbackStatePreserved=true" not in assertions_text:
        failures.append("[FAIL] Runtime validation exit-code gate: log assertion must reject preserved rollback state")

    return failures


def check_architecture_decision_record_contract() -> list[str]:
    failures: list[str] = []
    if not ARCHITECTURE_DECISION_RECORD_FILE.exists():
        return ["[FAIL] ADR gate: docs/architecture/Architecture_Decision_Record.md is missing"]

    adr_text = ARCHITECTURE_DECISION_RECORD_FILE.read_text(encoding="utf-8", errors="replace")
    required_markers = [
        "This document is the architecture contract index",
        "The ADRs below are not design notes. They are merge and release contracts.",
        "ADR-001: Runtime Mutation Is Transactional",
        "ADR-002: ThreadTracker Is Observation-Only",
        "ADR-003: SchedulerController Owns Thread-Level Scheduling Mutation",
        "ADR-004: Processor-Group Policy Is Explicit And Group-Aware",
        "ADR-005: SoftApply Is Validation Evidence, Not Rollback State",
        "ADR-006: Release Decisions Are Evidence-Driven",
        "ADR-007: BackgroundController Owns Process-Level Background Restriction",
        "ADR-008: Access-Boundary Failures Are Fallback Evidence Unless Mutation Escaped",
        "ADR-009: Input Thread Pinning Requires High-Confidence Concrete TID Evidence",
        "ADR-010: Apply Mode Remains Limited And Explicit",
        "Non-invasive boundary",
        "Conservative apply policy",
        "Fallback-first policy",
        "ERROR_ACCESS_DENIED",
        "InputThreadTidConfidence::High",
        "ConcreteTid",
        "Broad background restriction in apply mode MUST require explicit deny/restrict configuration.",
        "SchedulerPolicy",
        "processorGroup",
        "affinityMask",
        "KAFFINITY",
        "ApplyGuard::commit()",
        "ApplyGuard::rollbackNow()",
        "run_release_gate_static_checks.py",
        "docs/release/Evidence_Schema.md",
        "docs/release/Release_Blocker_List.md",
        "Review trigger:",
    ]
    for marker in required_markers:
        if marker not in adr_text:
            failures.append(f"[FAIL] ADR gate: missing marker: {marker}")

    for adr_id in range(1, 11):
        marker = f"ADR-{adr_id:03d}"
        if adr_text.count(marker) < 2:
            failures.append(f"[FAIL] ADR gate: {marker} must appear in the index and body")

    forbidden_markers = [
        "CoreMask must include processorGroup",
        "group 0 hardcoding is forbidden.",
    ]
    for marker in forbidden_markers:
        if marker in adr_text:
            failures.append(f"[FAIL] ADR gate: stale terminology remains: {marker}")

    return failures


def check_contract_enforcement_matrix() -> list[str]:
    failures: list[str] = []
    if not CONTRACT_ENFORCEMENT_MATRIX_FILE.exists():
        return ["[FAIL] CEM gate: docs/architecture/Contract_Enforcement_Matrix.md is missing"]

    cem_text = CONTRACT_ENFORCEMENT_MATRIX_FILE.read_text(encoding="utf-8", errors="replace")
    required_markers = [
        "This matrix turns accepted architecture contracts into enforceable release gates.",
        "Runtime mutation is transactional",
        "ThreadTracker is observation-only",
        "Thread scheduling mutation owner",
        "Processor group policy",
        "SoftApply is evidence, not rollback state",
        "Release decisions are evidence-driven",
        "Background restriction owner",
        "Access boundary fallback",
        "Input pinning eligibility",
        "Apply mode is limited and explicit",
        "Runtime timeout safe point",
        "Forbidden pattern",
        "Severity",
        "Static gate",
        "Runtime validation",
        "Evidence fields",
        "BLOCKER",
        "WARN",
        "INFO",
        "check_apply_guard_transaction_patterns",
        "check_thread_tracker_runtime_contracts",
        "check_background_processor_group_policy_is_explicit",
        "check_anti_cheat_fallback_contract",
        "check_apply_mode_policy_contract",
        "FailedAffinityApplyDisposition",
        "auditFailedAffinityApply",
        "makeAffinityApplyFailureResult",
        "logAffinityRollbackFailure",
        "logStartupPolicySummary",
        "buildMainThreadPolicy",
        "loadAndPrepareBackgroundFilterConfig",
        "buildBackgroundRestrictionPolicy",
        "ApplyGuard rollback failure fixtures",
        "not duplicate the transfer-missing BLOCKER",
        "destructor rollback failure must add one rollback failure BLOCKER",
        "SoftApply baseline evidence stays separate",
        "rollback_preserved_state_summary.has_preserved_state",
        "report must remain `PASS`",
        "run_release_gate_static_checks.py",
        "release_gate_evidence.py",
        "verify_rc_candidate.py",
        "run_rc_gate.bat",
    ]
    for marker in required_markers:
        if marker not in cem_text:
            failures.append(f"[FAIL] CEM gate: missing marker: {marker}")

    adr_text = ARCHITECTURE_DECISION_RECORD_FILE.read_text(encoding="utf-8", errors="replace") if ARCHITECTURE_DECISION_RECORD_FILE.exists() else ""
    for adr_marker in [
        "Runtime Mutation Is Transactional",
        "ThreadTracker Is Observation-Only",
        "SchedulerController Owns Thread-Level Scheduling Mutation",
        "Processor-Group Policy Is Explicit And Group-Aware",
        "SoftApply Is Validation Evidence, Not Rollback State",
        "Release Decisions Are Evidence-Driven",
        "BackgroundController Owns Process-Level Background Restriction",
        "Access-Boundary Failures Are Fallback Evidence Unless Mutation Escaped",
        "Input Thread Pinning Requires High-Confidence Concrete TID Evidence",
        "Apply Mode Remains Limited And Explicit",
    ]:
        normalized = adr_marker.replace("SchedulerController Owns Thread-Level Scheduling Mutation", "Thread scheduling mutation owner")
        normalized = normalized.replace("Processor-Group Policy Is Explicit And Group-Aware", "Processor group policy")
        normalized = normalized.replace("SoftApply Is Validation Evidence, Not Rollback State", "SoftApply is evidence, not rollback state")
        normalized = normalized.replace("BackgroundController Owns Process-Level Background Restriction", "Background restriction owner")
        normalized = normalized.replace("Access-Boundary Failures Are Fallback Evidence Unless Mutation Escaped", "Access boundary fallback")
        normalized = normalized.replace("Input Thread Pinning Requires High-Confidence Concrete TID Evidence", "Input pinning eligibility")
        normalized = normalized.replace("Apply Mode Remains Limited And Explicit", "Apply mode is limited and explicit")
        normalized = normalized.replace("Runtime Mutation Is Transactional", "Runtime mutation is transactional")
        normalized = normalized.replace("ThreadTracker Is Observation-Only", "ThreadTracker is observation-only")
        normalized = normalized.replace("Release Decisions Are Evidence-Driven", "Release decisions are evidence-driven")
        if adr_marker in adr_text and normalized not in cem_text:
            failures.append(f"[FAIL] CEM gate: ADR contract missing from matrix: {adr_marker}")

    return failures


def check_module_ownership_matrix() -> list[str]:
    failures: list[str] = []
    if not MODULE_OWNERSHIP_MATRIX_FILE.exists():
        return ["[FAIL] ownership gate: docs/architecture/Module_Ownership_Matrix.md is missing"]

    matrix_text = MODULE_OWNERSHIP_MATRIX_FILE.read_text(encoding="utf-8", errors="replace")
    required_markers = [
        "This document is part of `GameOptimizer Runtime Safety & Release Governance`.",
        "Owner module",
        "Access allowed from",
        "Forbidden modules / paths",
        "thread observation",
        "`ThreadTracker`",
        "thread mutation",
        "`SchedulerController`",
        "process background restriction",
        "`BackgroundController`",
        "rollback state ownership",
        "`RollbackManager`",
        "transaction cleanup",
        "`ApplyGuard`",
        "shutdown rollback",
        "`ShutdownPipeline`",
        "runtime validation",
        "`RuntimeValidationMonitor`",
        "release evidence",
        "`release_gate_evidence.py`",
        "processor topology policy",
        "`TopologyAnalyzer`",
        "input pinning eligibility",
        "`InputLatencyController`",
        "ThreadTracker` is observation-only",
        "SchedulerController` is the only owner of thread-level scheduling mutation",
        "BackgroundController` is the only owner of process-level background restriction",
        "RollbackManager` owns rollback state",
        "ApplyGuard` owns transaction cleanup",
        "ShutdownPipeline` owns the final shutdown `rollbackAll()` attempt",
        "Any missing owner row is a `BLOCKER`.",
    ]
    for marker in required_markers:
        if marker not in matrix_text:
            failures.append(f"[FAIL] ownership gate: missing marker: {marker}")

    forbidden_stale_markers = [
        "TODO",
        "TBD",
        "to be decided",
    ]
    for marker in forbidden_stale_markers:
        if marker in matrix_text:
            failures.append(f"[FAIL] ownership gate: unresolved placeholder remains: {marker}")

    return failures


def check_atomic_governance_change_unit_contract() -> list[str]:
    failures: list[str] = []
    required_files = {
        "Engineering Handbook": ENGINEERING_DOCS / "Engineering_Handbook.md",
        "Contract Enforcement Matrix": CONTRACT_ENFORCEMENT_MATRIX_FILE,
        "Release Gate Spec": RELEASE_DOCS / "Release_Gate_Spec.md",
        "Release Blocker List": RELEASE_BLOCKER_LIST_FILE,
        "RC Runbook": RELEASE_DOCS / "RC_Runbook.md",
    }
    for label, path in required_files.items():
        if not path.exists():
            failures.append(f"[FAIL] atomic governance unit gate: {label} is missing")

    if failures:
        return failures

    combined_text = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in required_files.values()
    )
    required_markers = [
        "Atomic Governance Change Unit",
        "Atomic governance change unit",
        "document contract 1",
        "static gate 1",
        "validator/evidence coupling 1",
        "selftest or regression 1",
        "document/blocker/runbook sync",
        "verification",
        "commit 1",
        "check_atomic_governance_change_unit_contract",
        "A development change skips the atomic governance change unit",
        "verification notes identify the static gate and validator/selftest or regression evidence",
    ]
    for marker in required_markers:
        if marker not in combined_text:
            failures.append(f"[FAIL] atomic governance unit gate: missing marker: {marker}")

    return failures


def check_module_ownership_api_boundaries() -> list[str]:
    failures: list[str] = []
    production_files = [
        path for path in iter_source_files()
        if not path.name.endswith("Tests.cpp")
    ]

    ownership_rules = [
        (
            re.compile(r"\bSetThreadAffinityMask\s*\("),
            {"SchedulerController.cpp", "RollbackManager.cpp"},
            "thread affinity mutation must remain in SchedulerController/RollbackManager"),
        (
            re.compile(r"\bSetThreadGroupAffinity\s*\("),
            {"SchedulerController.cpp", "RollbackManager.cpp"},
            "thread group mutation must remain in SchedulerController/RollbackManager"),
        (
            re.compile(r"\bSetThreadPriority\s*\("),
            {"SchedulerController.cpp", "RollbackManager.cpp"},
            "thread priority mutation must remain in SchedulerController/RollbackManager"),
        (
            re.compile(r"\bSetProcessAffinityMask\s*\("),
            {"BackgroundController.cpp", "RollbackManager.cpp"},
            "process affinity mutation must remain in BackgroundController/RollbackManager"),
        (
            re.compile(r"\bSetPriorityClass\s*\("),
            {"BackgroundController.cpp", "RollbackManager.cpp"},
            "process priority mutation must remain in BackgroundController/RollbackManager"),
        (
            re.compile(r"\bApplyGuard::(?:forThread|forProcess)\s*\("),
            {"SchedulerController.cpp", "BackgroundController.cpp", "ApplyGuard.cpp"},
            "ApplyGuard transaction construction must remain in mutation owners"),
    ]

    for path in production_files:
        text = strip_comments_and_strings(path.read_text(encoding="utf-8", errors="replace"))
        for pattern, allowed_files, message in ownership_rules:
            for match in pattern.finditer(text):
                if path.name not in allowed_files:
                    line_number = text.count("\n", 0, match.start()) + 1
                    failures.append(
                        f"[FAIL] ownership API gate: {path.name}:{line_number}: {message}")

    thread_tracker_text = strip_comments_and_strings(
        (ROOT / "ThreadTracker.cpp").read_text(encoding="utf-8", errors="replace"))
    for forbidden in [
        "SetThreadAffinityMask",
        "SetThreadGroupAffinity",
        "SetThreadPriority",
        "SetPriorityClass",
        "SetProcessAffinityMask",
        "RollbackManager",
        "ApplyGuard",
    ]:
        if re.search(rf"\b{re.escape(forbidden)}\b", thread_tracker_text):
            failures.append(
                f"[FAIL] ownership API gate: ThreadTracker.cpp must remain observation-only; forbidden marker: {forbidden}")

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
    failures.extend(check_thread_tracker_runtime_contracts())
    failures.extend(check_runtime_orchestrator_signal_contracts())
    failures.extend(check_apply_guard_rollback_failure_ownership())
    failures.extend(check_soft_apply_baseline_storage_contract())
    failures.extend(check_dpc_monitoring_is_not_placeholder())
    failures.extend(check_network_irq_unsupported_policy_is_warn_only())
    failures.extend(check_background_processor_group_policy_is_explicit())
    failures.extend(check_cpp20_runtime_contracts())
    failures.extend(check_project_language_contracts())
    failures.extend(check_timer_input_module_registration())
    failures.extend(check_anti_cheat_fallback_contract())
    failures.extend(check_rc_gate_contract())
    failures.extend(check_static_gate_selftest_contract())
    failures.extend(check_long_soak_automation_contract())
    failures.extend(check_release_evidence_contract())
    failures.extend(check_apply_mode_policy_contract())
    failures.extend(check_known_limitations_contract())
    failures.extend(check_rc_candidate_contract())
    failures.extend(check_runtime_validation_failure_exit_code_contract())
    failures.extend(check_architecture_decision_record_contract())
    failures.extend(check_contract_enforcement_matrix())
    failures.extend(check_module_ownership_matrix())
    failures.extend(check_atomic_governance_change_unit_contract())
    failures.extend(check_module_ownership_api_boundaries())

    if failures:
        for failure in failures:
            print(failure)
        return 1

    print("[PASS] release gate static checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
