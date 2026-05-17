// Build: cl /std:c++latest /O2 /W4 /WX /permissive- main.cpp ProcessFinder.cpp ThreadTracker.cpp TopologyAnalyzer.cpp RollbackManager.cpp SchedulerController.cpp TrackingWatchdog.cpp PolicyDispatcher.cpp LatencyDecisionLayer.cpp BackgroundController.cpp
// MODULE: main
// ERROR-POLICY: expected
// Reason: application boundary converts expected errors into user-facing logs.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <limits>
#include <optional>
#include <cstdint>
#include <cwchar>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "AppliedPolicyTracker.h"
#include "BackgroundController.h"
#include "ErrorCode.h"
#include "LatencyDecisionLayer.h"
#include "LatencyMetricsCollector.h"
#include "Logger.h"
#include "PolicyDispatcher.h"
#include "ProcessFinder.h"
#include "RollbackManager.h"
#include "SchedulerController.h"
#include "ThreadInfo.h"
#include "ThreadTracker.h"
#include "TopologyAnalyzer.h"
#include "TrackingWatchdog.h"

namespace
{
    std::atomic_bool gRunning = true;
    std::atomic_bool gRuntimeTimeoutRequested = false;

    BOOL WINAPI handleConsoleControl(DWORD controlType) noexcept
    {
        switch (controlType)
        {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            gRunning.store(false, std::memory_order_release);
            return TRUE;
        default:
            return FALSE;
        }
    }


    [[nodiscard]] SchedulerMode parseSchedulerMode(int argc, wchar_t* argv[]) noexcept
    {
        if (argc >= 3)
        {
            const std::wstring_view modeArgument = argv[2];
            if (modeArgument == L"--dry-run")
            {
                return SchedulerMode::DryRun;
            }
            if (modeArgument == L"--apply")
            {
                return SchedulerMode::Apply;
            }
        }

        return SchedulerMode::SoftApply;
    }


    [[nodiscard]] std::wstring parseBackgroundFilterConfigPath(int argc, wchar_t* argv[]) noexcept
    {
        for (int index = 2; index + 1 < argc; ++index)
        {
            const std::wstring_view argument = argv[index];
            if (argument == L"--background-filter")
            {
                return std::wstring(argv[index + 1]);
            }
        }

        return {};
    }


    [[nodiscard]] std::wstring parseLatencyPingTarget(int argc, wchar_t* argv[]) noexcept
    {
        for (int index = 2; index + 1 < argc; ++index)
        {
            const std::wstring_view argument = argv[index];
            if (argument == L"--latency-ping")
            {
                return std::wstring(argv[index + 1]);
            }
        }

        return {};
    }


    [[nodiscard]] bool parseBackgroundDetailLogEnabled(int argc, wchar_t* argv[]) noexcept
    {
        for (int index = 2; index < argc; ++index)
        {
            const std::wstring_view argument = argv[index];
            if (argument == L"--background-detail-log")
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] std::string_view schedulerModeName(SchedulerMode mode) noexcept
    {
        switch (mode)
        {
        case SchedulerMode::DryRun:
            return "dry-run";
        case SchedulerMode::SoftApply:
            return "soft-apply";
        case SchedulerMode::Apply:
            return "apply";
        }

        return "unknown";
    }

    [[nodiscard]] std::string narrowForLog(std::wstring_view value)
    {
        std::string result;
        result.reserve(value.size());

        for (const wchar_t ch : value)
        {
            result.push_back(ch >= 0 && ch <= 0x7F ? static_cast<char>(ch) : '?');
        }

        return result;
    }

    struct RuntimeLogConfig
    {
        bool threadDetailLogEnabled = false;
        std::uint32_t threadDetailLogIntervalCycles = 1;
    };

    [[nodiscard]] std::uint32_t parsePositiveUIntArgument(
        int argc,
        wchar_t* argv[],
        std::wstring_view flag,
        std::uint32_t fallbackValue) noexcept
    {
        for (int index = 2; index + 1 < argc; ++index)
        {
            const std::wstring_view argument = argv[index];
            if (argument != flag)
            {
                continue;
            }

            wchar_t* parseEnd = nullptr;
            const unsigned long parsedValue = std::wcstoul(argv[index + 1], &parseEnd, 10);
            if (parseEnd == argv[index + 1] || *parseEnd != L'\0' || parsedValue == 0UL)
            {
                Logger::warn(
                    "invalid positive integer for {}; using fallback={}",
                    narrowForLog(flag),
                    fallbackValue);
                return fallbackValue;
            }

            constexpr unsigned long kMaxUint32 = 0xFFFFFFFFUL;
            if (parsedValue > kMaxUint32)
            {
                Logger::warn(
                    "integer value for {} is too large; using fallback={}",
                    narrowForLog(flag),
                    fallbackValue);
                return fallbackValue;
            }

            return static_cast<std::uint32_t>(parsedValue);
        }

        return fallbackValue;
    }

    [[nodiscard]] RuntimeLogConfig parseRuntimeLogConfig(int argc, wchar_t* argv[]) noexcept
    {
        RuntimeLogConfig config{};
        for (int index = 2; index < argc; ++index)
        {
            const std::wstring_view argument = argv[index];
            if (argument == L"--thread-detail-log")
            {
                config.threadDetailLogEnabled = true;
            }
        }

        config.threadDetailLogIntervalCycles = parsePositiveUIntArgument(
            argc,
            argv,
            L"--thread-log-interval",
            config.threadDetailLogIntervalCycles);

        return config;
    }

    [[nodiscard]] std::optional<std::chrono::seconds> parseMaxRuntime(int argc, wchar_t* argv[]) noexcept
    {
        constexpr unsigned long kMaxRuntimeSeconds = 86400UL;

        for (int index = 2; index + 1 < argc; ++index)
        {
            const std::wstring_view argument = argv[index];
            if (argument != L"--max-runtime-seconds")
            {
                continue;
            }

            const wchar_t* valueText = argv[index + 1];
            if (valueText[0] == L'-' || valueText[0] == L'+')
            {
                Logger::warn("invalid --max-runtime-seconds value; runtime limit disabled");
                return std::nullopt;
            }

            wchar_t* parseEnd = nullptr;
            const unsigned long parsedValue = std::wcstoul(valueText, &parseEnd, 10);
            if (parseEnd == valueText || *parseEnd != L'\0' || parsedValue == 0UL)
            {
                Logger::warn("invalid --max-runtime-seconds value; runtime limit disabled");
                return std::nullopt;
            }

            if (parsedValue > kMaxRuntimeSeconds)
            {
                Logger::warn(
                    "--max-runtime-seconds value is too large; clamped to {} seconds",
                    kMaxRuntimeSeconds);
                return std::chrono::seconds(kMaxRuntimeSeconds);
            }

            return std::chrono::seconds(parsedValue);
        }

        return std::nullopt;
    }
    void logObservedThreads(const ThreadInfoBuffer& threads) noexcept
    {
        constexpr double kFileTimeUnitsPerMillisecond = 10000.0;

        Logger::info("observed top thread count: {}", threads.size());

        for (const ThreadInfo& thread : threads)
        {
            const double deltaMilliseconds =
                static_cast<double>(thread.deltaCpuTime100ns) / kFileTimeUnitsPerMillisecond;
            const double emaMilliseconds =
                thread.emaCpuTime100ns / kFileTimeUnitsPerMillisecond;

            Logger::info(
                "TID: {} | avg-sample-cpu: {:.3f} ms | EMA-sample-cpu: {:.3f} ms | candidate: {} | main: {}",
                thread.threadId,
                deltaMilliseconds,
                emaMilliseconds,
                thread.isEmaCandidate,
                thread.isMainThread);
        }
    }

    void logDecisionState(const ThreadTracker& threadTracker) noexcept
    {
        const auto candidateThreadId = threadTracker.getEmaCandidateThreadId();
        const auto mainThreadId = threadTracker.getMainThreadId();

        if (candidateThreadId.has_value())
        {
            Logger::info("EMA candidate TID: {}", *candidateThreadId);
        }
        else
        {
            Logger::info("EMA candidate TID: none");
        }

        if (mainThreadId.has_value())
        {
            Logger::info("confirmed main TID: {}", *mainThreadId);
        }
        else
        {
            Logger::info("confirmed main TID: none");
        }
    }

    void applyPolicyWhenMainThreadChanges(
        SchedulerController& schedulerController,
        std::optional<DWORD>& lastAppliedThreadId,
        DWORD mainThreadId,
        const SchedulerPolicy& policy) noexcept
    {
        const auto policyShapeResult = SchedulerController::validatePolicyShape(policy);
        if (!policyShapeResult)
        {
            Logger::warn(
                "pre-apply gate blocked scheduler validation for main TID {}: {}",
                mainThreadId,
                toString(policyShapeResult.error()));
            return;
        }

        if (lastAppliedThreadId.has_value() && *lastAppliedThreadId == mainThreadId)
        {
            return;
        }

        const std::optional<DWORD> previousMainThreadId = lastAppliedThreadId;
        if (previousMainThreadId.has_value())
        {
            Logger::info(
                "main thread handoff started: previous TID {} remains active until new TID {} passes apply verification",
                *previousMainThreadId,
                mainThreadId);
        }

        const auto applyResult = schedulerController.applyMainThreadPolicy(mainThreadId, policy);
        if (!applyResult)
        {
            Logger::error(
                "failed to validate/apply policy for new main TID {}; previous main TID {} remains the active optimized thread: {}",
                mainThreadId,
                previousMainThreadId.value_or(0),
                toString(applyResult.error()));
            return;
        }

        if (previousMainThreadId.has_value())
        {
            Logger::info(
                "new main TID {} passed apply verification; rolling back previous main TID {}",
                mainThreadId,
                *previousMainThreadId);

            const auto rollbackResult = schedulerController.rollbackThread(*previousMainThreadId);
            if (!rollbackResult)
            {
                if (rollbackResult.error() == ErrorCode::ThreadOpenFailed)
                {
                    Logger::warn(
                        "handoff rollback skipped for previous main TID {} after new TID {} was applied: previous thread is no longer openable, so dual optimization risk is gone ({})",
                        *previousMainThreadId,
                        mainThreadId,
                        toString(rollbackResult.error()));
                }
                else
                {
                    Logger::error(
                        "handoff rollback failed for previous main TID {} after new TID {} was applied: {}; rolling back new TID to avoid dual optimized threads",
                        *previousMainThreadId,
                        mainThreadId,
                        toString(rollbackResult.error()));

                    const auto newRollbackResult = schedulerController.rollbackThread(mainThreadId);
                    if (!newRollbackResult)
                    {
                        Logger::error(
                            "failed to rollback newly applied main TID {} after handoff failure: {}",
                            mainThreadId,
                            toString(newRollbackResult.error()));
                    }
                    return;
                }
            }
        }

        lastAppliedThreadId = mainThreadId;
    }

    void reconcilePolicyIfNeeded(
        SchedulerController& schedulerController,
        std::optional<DWORD> lastAppliedThreadId,
        DWORD currentMainThreadId,
        const SchedulerPolicy& policy,
        SchedulerMode schedulerMode,
        std::uint32_t& auditCycleCounter,
        std::uint32_t auditIntervalCycles) noexcept
    {
        if (schedulerMode != SchedulerMode::Apply)
        {
            return;
        }

        if (!lastAppliedThreadId.has_value() || *lastAppliedThreadId != currentMainThreadId)
        {
            auditCycleCounter = 0;
            return;
        }

        ++auditCycleCounter;
        if (auditCycleCounter < auditIntervalCycles)
        {
            return;
        }

        auditCycleCounter = 0;

        const auto reconcileResult = schedulerController.reconcileMainThreadPolicy(currentMainThreadId, policy);
        if (!reconcileResult)
        {
            Logger::error(
                "policy drift audit failed for main TID {}: {}",
                currentMainThreadId,
                toString(reconcileResult.error()));
        }
    }

}

int wmain(int argc, wchar_t* argv[])
{
    constexpr int kRequiredArgumentCount = 2;
    constexpr std::size_t kMaxDisplayedThreads = 8;
    constexpr double kEmaAlpha = 0.25;
    constexpr auto kStickinessDuration = std::chrono::milliseconds(4000);
    constexpr int kSampleCount = 3;
    constexpr auto kSampleInterval = std::chrono::milliseconds(50);
    constexpr auto kWatchdogInterval = std::chrono::milliseconds(3000);
    constexpr std::uint32_t kPolicyAuditIntervalCycles = 4;

    if (argc < kRequiredArgumentCount)
    {
        Logger::error("usage: GameOptimizer.exe <process-name.exe> [--dry-run|--apply] [--background-filter <path>] [--latency-ping <ipv4>] [--background-detail-log] [--thread-detail-log] [--thread-log-interval <cycles>] [--max-runtime-seconds <seconds>]");
        return 1;
    }

    if (!SetConsoleCtrlHandler(handleConsoleControl, TRUE))
    {
        Logger::error("startup failed: {}", toString(ErrorCode::ConsoleControlHandlerFailed));
        return 1;
    }

    const std::wstring processName = argv[1];
    const SchedulerMode schedulerMode = parseSchedulerMode(argc, argv);
    const std::wstring backgroundFilterConfigPath = parseBackgroundFilterConfigPath(argc, argv);
    const std::wstring latencyPingTarget = parseLatencyPingTarget(argc, argv);
    const bool backgroundDetailLogEnabled = parseBackgroundDetailLogEnabled(argc, argv);
    const RuntimeLogConfig runtimeLogConfig = parseRuntimeLogConfig(argc, argv);
    const std::optional<std::chrono::seconds> maxRuntime = parseMaxRuntime(argc, argv);

    const auto processId = ProcessFinder::findProcessIdByName(processName);
    if (!processId)
    {
        Logger::error("failed to find process: {}", toString(processId.error()));
        return 1;
    }

    const DWORD targetProcessId = *processId;
    Logger::info("target PID: {}", targetProcessId);
    Logger::info("scheduler mode: {}", schedulerModeName(schedulerMode));
    if (schedulerMode == SchedulerMode::Apply)
    {
        Logger::warn("apply mode enabled: SetThreadGroupAffinity and SetThreadPriority may modify the target thread until shutdown rollback");
    }
    else if (schedulerMode == SchedulerMode::SoftApply)
    {
        Logger::info("soft-apply: OpenThread scheduling-rights validation is enabled, but SetThread* calls are blocked");
    }
    else
    {
        Logger::info("dry-run: target thread will not be opened or modified");
    }
    Logger::info("pre-apply gate enabled: non-zero affinity mask and valid processor group are required before thread access");
    Logger::info(
        "policy drift watchdog enabled for apply mode: current thread policy will be audited every {} watchdog cycle(s)",
        kPolicyAuditIntervalCycles);
    Logger::info(
        "multi-sampling enabled: {} samples, {} ms interval",
        kSampleCount,
        kSampleInterval.count());
    Logger::info("policy command router enabled: E2E decision layer can emit BG_RESTRICT_UP, IRQ_REPIN, STICKY_UP, ROLLBACK");
    Logger::info("policy stabilizer enabled: hysteresis + debounce + per-command cooldown are active");
    Logger::info("background controller enabled: BG_RESTRICT_UP can isolate non-target processes from the selected game mask");
    if (runtimeLogConfig.threadDetailLogEnabled)
    {
        Logger::info(
            "thread detail log enabled: interval={} watchdog cycle(s)",
            runtimeLogConfig.threadDetailLogIntervalCycles);
    }
    else
    {
        Logger::info("thread detail log disabled by default: pass --thread-detail-log to print top thread rows");
    }
    if (!backgroundFilterConfigPath.empty())
    {
        Logger::info("background filter config path: {}", narrowForLog(backgroundFilterConfigPath));
    }
    if (!latencyPingTarget.empty())
    {
        Logger::info("ICMP RTT fallback enabled: target={}", narrowForLog(latencyPingTarget));
    }
    else
    {
        Logger::info("ICMP RTT fallback disabled: pass --latency-ping <ipv4> to enable RTT jitter metrics");
    }
    if (maxRuntime.has_value())
    {
        Logger::info("max runtime limit enabled: {} seconds", maxRuntime->count());
    }

    SchedulerPolicy mainThreadPolicy{
        .affinityMask = 0,
        .processorGroup = 0,
        .threadPriority = THREAD_PRIORITY_ABOVE_NORMAL};

    const auto topologyResult = TopologyAnalyzer::buildMainThreadMask(targetProcessId);
    if (topologyResult)
    {
        const auto& topology = *topologyResult;
        mainThreadPolicy.affinityMask = topology.validatedMask;
        mainThreadPolicy.processorGroup = topology.processorGroup;
    }
    else
    {
        Logger::warn(
            "topology analyzer unavailable: {}; soft-apply cannot validate scheduler policy until a non-zero mask is available",
            toString(topologyResult.error()));
    }

    BackgroundRestrictionPolicy backgroundPolicy{
        .targetProcessId = targetProcessId,
        .gameAffinityMask = mainThreadPolicy.affinityMask,
        .processorGroup = mainThreadPolicy.processorGroup};

    const ThreadTrackerConfig trackerConfig{
        .emaAlpha = kEmaAlpha,
        .stickinessDuration = kStickinessDuration,
        .sampleCount = kSampleCount,
        .sampleInterval = kSampleInterval};

    ThreadTracker threadTracker(targetProcessId, trackerConfig);
    RollbackManager rollbackManager;
    SchedulerController schedulerController(rollbackManager, schedulerMode);
    BackgroundFilterConfig backgroundFilterConfig =
        BackgroundController::loadFilterConfigFromFile(backgroundFilterConfigPath);
    backgroundFilterConfig.logRestrictedProcessDetails = backgroundDetailLogEnabled;
    backgroundFilterConfig.logSkippedProcessDetails = backgroundDetailLogEnabled;
    if (backgroundDetailLogEnabled)
    {
        Logger::info("background detail log enabled: per-process restriction and skip lines will be printed");
    }
    if (schedulerMode == SchedulerMode::Apply &&
        backgroundFilterConfig.requireExplicitRestrictionTargetsInApply &&
        backgroundFilterConfig.restrictionTargetProcessNames.empty())
    {
        Logger::warn(
            "apply mode safety: background restriction will be blocked because no deny/restrict targets were loaded; "
            "main-thread scheduling may still run, but BG_RESTRICT_UP will not change other processes");
    }
    else if (!backgroundFilterConfig.restrictionTargetProcessNames.empty())
    {
        Logger::info(
            "background apply safety: explicit restriction target count={}",
            backgroundFilterConfig.restrictionTargetProcessNames.size());
    }
    BackgroundController backgroundController(rollbackManager, schedulerMode, backgroundFilterConfig);
    PolicyDispatcher policyDispatcher(
        schedulerController,
        threadTracker,
        backgroundController,
        backgroundPolicy);
    LatencyDecisionLayer latencyDecisionLayer;
    AppliedPolicyTracker appliedPolicyTracker;
    LatencyMetricsCollector latencyMetricsCollector(LatencyMetricsCollectorConfig{
        .icmpTargetIpv4 = latencyPingTarget,
        .icmpTimeout = std::chrono::milliseconds(50),
        .icmpSampleInterval = std::chrono::milliseconds(1000),
        .rttWindowSize = 10,
        .interruptAffinitySupported = false});
    if (!latencyMetricsCollector.start())
    {
        Logger::error("startup failed: latency metrics sensor start failed");
        return 1;
    }

    std::optional<DWORD> lastAppliedThreadId;
    std::uint32_t policyAuditCycleCounter = 0;
    std::uint32_t threadDetailLogCycleCounter = 0;

    TrackingWatchdog watchdog;
    const bool watchdogStarted = watchdog.start(
        [&]() noexcept
        {
            const auto updateResult = threadTracker.update();
            if (!updateResult)
            {
                Logger::warn("thread update failed: {}; retrying next cycle", toString(updateResult.error()));
                return;
            }

            bool shouldLogThreadDetails = runtimeLogConfig.threadDetailLogEnabled;
            if (shouldLogThreadDetails)
            {
                ++threadDetailLogCycleCounter;
                if (threadDetailLogCycleCounter < runtimeLogConfig.threadDetailLogIntervalCycles)
                {
                    shouldLogThreadDetails = false;
                }
                else
                {
                    threadDetailLogCycleCounter = 0;
                }
            }

            if (shouldLogThreadDetails)
            {
                const auto topThreads = threadTracker.getTopThreads(kMaxDisplayedThreads);
                if (!topThreads)
                {
                    Logger::warn("thread data collection failed: {}; retrying next cycle", toString(topThreads.error()));
                    return;
                }

                const auto& observedThreads = *topThreads;
                logObservedThreads(observedThreads);
            }
            logDecisionState(threadTracker);

            const auto mainThreadId = threadTracker.getMainThreadId();
            if (mainThreadId.has_value())
            {
                applyPolicyWhenMainThreadChanges(
                    schedulerController,
                    lastAppliedThreadId,
                    *mainThreadId,
                    mainThreadPolicy);

                reconcilePolicyIfNeeded(
                    schedulerController,
                    lastAppliedThreadId,
                    *mainThreadId,
                    mainThreadPolicy,
                    schedulerMode,
                    policyAuditCycleCounter,
                    kPolicyAuditIntervalCycles);
            }

            RuntimeMetrics runtimeMetrics = latencyMetricsCollector.collect(
                threadTracker.consumeThreadMigrationCount());
            runtimeMetrics.timestamp = std::chrono::steady_clock::now();

            const auto feedbackCommands = appliedPolicyTracker.evaluate(runtimeMetrics, runtimeMetrics.timestamp);
            for (PolicyCommand command : feedbackCommands)
            {
                const auto dispatchResult = policyDispatcher.dispatch(command);
                const bool dispatchSucceeded = dispatchResult.has_value();
                appliedPolicyTracker.recordDispatchResult(
                    command,
                    runtimeMetrics,
                    dispatchSucceeded,
                    runtimeMetrics.timestamp);

                if (!dispatchResult)
                {
                    Logger::error(
                        "policy feedback command dispatch failed (command={}): {}",
                        toString(command),
                        toString(dispatchResult.error()));
                    continue;
                }

                if (command == PolicyCommand::Rollback)
                {
                    Logger::error("policy feedback requested rollback; shutting down watchdog loop");
                    gRunning.store(false, std::memory_order_release);
                    return;
                }
            }

            const auto commandsResult = latencyDecisionLayer.evaluate(runtimeMetrics, runtimeMetrics.timestamp);
            if (!commandsResult)
            {
                Logger::error(
                    "latency decision evaluation failed: {}",
                    toString(commandsResult.error()));
                return;
            }

            const auto& commands = *commandsResult;
            for (PolicyCommand command : commands)
            {
                const auto dispatchResult = policyDispatcher.dispatch(command);
                const bool dispatchSucceeded = dispatchResult.has_value();
                appliedPolicyTracker.recordDispatchResult(
                    command,
                    runtimeMetrics,
                    dispatchSucceeded,
                    runtimeMetrics.timestamp);

                if (!dispatchResult)
                {
                    Logger::error(
                        "policy command dispatch failed (command={}): {}",
                        toString(command),
                        toString(dispatchResult.error()));
                }
            }

            if (gRuntimeTimeoutRequested.load(std::memory_order_acquire))
            {
                Logger::info("runtime timeout reached at watchdog cycle boundary; requesting clean shutdown");
                gRunning.store(false, std::memory_order_release);
            }
        },
        kWatchdogInterval);

    if (!watchdogStarted)
    {
        Logger::error("startup failed: watchdog start failed");
        return 1;
    }

    const auto runtimeStart = std::chrono::steady_clock::now();
    while (gRunning.load(std::memory_order_acquire))
    {
        if (maxRuntime.has_value())
        {
            const auto elapsed = std::chrono::steady_clock::now() - runtimeStart;
            if (elapsed >= *maxRuntime)
            {
                if (!gRuntimeTimeoutRequested.exchange(true, std::memory_order_acq_rel))
                {
                    Logger::info(
                        "max runtime limit reached: {} seconds; shutdown will be requested at the next watchdog safe point",
                        maxRuntime->count());
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Logger::info("shutdown requested; stopping watchdog and latency sensor before rollback");
    watchdog.requestStop();
    latencyMetricsCollector.requestStop();
    watchdog.join();
    latencyMetricsCollector.join();

    const auto rollbackResult = schedulerController.rollbackAll();
    if (!rollbackResult)
    {
        Logger::error("shutdown rollback failed: {}", toString(rollbackResult.error()));
        return 1;
    }

    Logger::info("shutdown completed cleanly");
    return 0;
}
