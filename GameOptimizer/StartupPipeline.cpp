// Build: cl /std:c++latest /O2 /W4 /WX /permissive- StartupPipeline.cpp ProcessFinder.cpp TopologyAnalyzer.cpp BackgroundController.cpp
// MODULE: StartupPipeline
// ERROR-POLICY: expected
// Reason: startup preparation failures must remain explicit before runtime mutation starts.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "StartupPipeline.h"

#include <Windows.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>

#include "AppliedPolicyTracker.h"
#include "CliOptions.h"
#include "InputLatencyController.h"
#include "LatencyDecisionLayer.h"
#include "LatencyMetricsCollector.h"
#include "LogString.h"
#include "Logger.h"
#include "NetworkInterruptController.h"
#include "PolicyDispatcher.h"
#include "ProcessFinder.h"
#include "RollbackManager.h"
#include "RuntimeValidationMonitor.h"
#include "SchedulerController.h"
#include "ThreadTracker.h"
#include "TimerResolutionController.h"
#include "TopologyAnalyzer.h"
#include "WinHandle.h"

namespace
{
    constexpr double kEmaAlpha = 0.25;
    constexpr auto kStickinessDuration = std::chrono::milliseconds(4000);
    constexpr int kSampleCount = 3;
    constexpr auto kSampleInterval = std::chrono::milliseconds(50);
    constexpr std::uint32_t kPolicyAuditIntervalCycles = 4;

    [[nodiscard]] std::expected<WORD, ErrorCode> queryPrimaryProcessGroup(HANDLE processHandle) noexcept
    {
        USHORT groupCount = 0;
        if (GetProcessGroupAffinity(processHandle, &groupCount, nullptr) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
            groupCount == 0)
        {
            return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
        }

        std::array<USHORT, 64> groups{};
        if (groupCount > groups.size())
        {
            return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
        }

        if (!GetProcessGroupAffinity(processHandle, &groupCount, groups.data()) || groupCount == 0)
        {
            return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
        }

        return groups.front();
    }

    [[nodiscard]] std::expected<SchedulerPolicy, ErrorCode>
    buildProcessAffinityFallbackPolicy(DWORD processId) noexcept
    {
        HANDLE rawHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (rawHandle == nullptr)
        {
            return std::unexpected(ErrorCode::ProcessOpenFailed);
        }

        WinHandle processHandle(rawHandle);
        DWORD_PTR processMask = 0;
        DWORD_PTR systemMask = 0;
        if (!GetProcessAffinityMask(processHandle.get(), &processMask, &systemMask) || processMask == 0)
        {
            return std::unexpected(ErrorCode::ProcessAffinityQueryFailed);
        }

        const auto processorGroupResult = queryPrimaryProcessGroup(processHandle.get());
        if (!processorGroupResult)
        {
            return std::unexpected(processorGroupResult.error());
        }

        const WORD processorGroup = processorGroupResult.value();
        const auto fallbackTopology = TopologyAnalyzer::buildProcessAffinityFallbackMask(
            processMask,
            systemMask,
            processorGroup);
        if (!fallbackTopology)
        {
            return std::unexpected(fallbackTopology.error());
        }

        const TopologyResult& topology = fallbackTopology.value();
        return SchedulerPolicy{
            .affinityMask = topology.validatedMask,
            .processorGroup = topology.processorGroup,
            .threadPriority = THREAD_PRIORITY_ABOVE_NORMAL};
    }
}

std::expected<RuntimeContext, ErrorCode> StartupPipeline::run(int argc, wchar_t* argv[]) noexcept
{
    try
    {
        const auto optionsResult = parseCliOptions(argc, argv);
        if (!optionsResult)
        {
            Logger::error("usage: GameOptimizer.exe <process-name.exe> [--dry-run|--apply] [--background-filter <path>] [--latency-ping <ipv4>] [--background-detail-log] [--thread-detail-log] [--thread-log-interval <cycles>] [--max-runtime-seconds <seconds>]");
            return std::unexpected(optionsResult.error());
        }

        RuntimeContext context{};
        context.options = optionsResult.value();

        const auto startupPlanResult = prepare(context.options);
        if (!startupPlanResult)
        {
            return std::unexpected(startupPlanResult.error());
        }

        context.startupPlan = startupPlanResult.value();

        context.threadTracker = std::make_unique<ThreadTracker>(
            context.startupPlan.targetProcessId,
            context.startupPlan.trackerConfig);
        context.rollbackManager = std::make_unique<RollbackManager>();
        context.schedulerController = std::make_unique<SchedulerController>(
            *context.rollbackManager,
            context.options.schedulerMode);
        context.timerResolutionController = std::make_unique<TimerResolutionController>(
            context.options.schedulerMode);

        const auto timerResolutionResult = context.timerResolutionController->apply();
        if (!timerResolutionResult)
        {
            Logger::error("startup failed: timer resolution apply failed: {}", toString(timerResolutionResult.error()));
            return std::unexpected(timerResolutionResult.error());
        }

        context.inputLatencyController = std::make_unique<InputLatencyController>(context.options.schedulerMode);
        const auto inputLatencyResult = context.inputLatencyController->detectAndApply(
            context.startupPlan.targetProcessId,
            context.startupPlan.mainThreadPolicy);
        if (!inputLatencyResult)
        {
            Logger::warn("input latency controller unavailable: {}", toString(inputLatencyResult.error()));
        }

        context.backgroundController = std::make_unique<BackgroundController>(
            *context.rollbackManager,
            context.options.schedulerMode,
            context.startupPlan.backgroundFilterConfig);
        context.networkInterruptController = std::make_unique<NetworkInterruptController>();
        (void)context.networkInterruptController->initialize();
        context.policyDispatcher = std::make_unique<PolicyDispatcher>(
            *context.schedulerController,
            *context.threadTracker,
            *context.backgroundController,
            context.startupPlan.backgroundPolicy,
            context.networkInterruptController.get());
        context.latencyDecisionLayer = std::make_unique<LatencyDecisionLayer>();
        context.appliedPolicyTracker = std::make_unique<AppliedPolicyTracker>();
        context.runtimeValidationMonitor = std::make_unique<RuntimeValidationMonitor>();
        context.latencyMetricsCollector = std::make_unique<LatencyMetricsCollector>(LatencyMetricsCollectorConfig{
            .icmpTargetIpv4 = context.options.latencyPingTarget,
            .icmpTimeout = std::chrono::milliseconds(50),
            .icmpSampleInterval = std::chrono::milliseconds(1000),
            .rttWindowSize = 10,
            .interruptAffinitySupported = false,
            .networkInterruptController = context.networkInterruptController.get()});

        if (!context.latencyMetricsCollector->start())
        {
            Logger::error("startup failed: latency metrics sensor start failed");
            (void)context.timerResolutionController->rollback();
            return std::unexpected(ErrorCode::InternalError);
        }

        return context;
    }
    catch (const std::bad_alloc&)
    {
        Logger::error("startup failed: allocation failed");
        return std::unexpected(ErrorCode::AllocationFailed);
    }
    catch (...)
    {
        Logger::error("startup failed: unexpected internal error");
        return std::unexpected(ErrorCode::InternalError);
    }
}

std::expected<StartupPlan, ErrorCode> StartupPipeline::prepare(const CliOptions& options) noexcept
{
    const auto processId = ProcessFinder::findProcessIdByName(options.processName);
    if (!processId)
    {
        Logger::error("failed to find process: {}", toString(processId.error()));
        return std::unexpected(processId.error());
    }

    const DWORD targetProcessId = processId.value();
    Logger::info("target PID: {}", targetProcessId);
    Logger::info("scheduler mode: {}", schedulerModeName(options.schedulerMode));
    if (options.schedulerMode == SchedulerMode::Apply)
    {
        Logger::warn("apply mode enabled: SetThreadGroupAffinity and SetThreadPriority may modify the target thread until shutdown rollback");
        Logger::warn(
            "apply mode restriction policy: use only after dry-run PASS, soft-apply PASS, no Access Denied, rollback state save success, sufficient ThreadTracker confidence, ApplyGuard audit PASS, and verified group-aware thread path");
        Logger::warn(
            "apply mode blocked by policy when anti-cheat is suspected, Access Denied repeats, rollback state save fails, Raw Input TID confidence is Low, group 1+ process-wide background restriction is required, or soft-apply WARN count is high");
    }
    else if (options.schedulerMode == SchedulerMode::SoftApply)
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
    if (options.runtimeLogConfig.threadDetailLogEnabled)
    {
        Logger::info(
            "thread detail log enabled: interval={} watchdog cycle(s)",
            options.runtimeLogConfig.threadDetailLogIntervalCycles);
    }
    else
    {
        Logger::info("thread detail log disabled by default: pass --thread-detail-log to print top thread rows");
    }
    if (!options.backgroundFilterConfigPath.empty())
    {
        Logger::info("background filter config path: {}", narrowForLog(options.backgroundFilterConfigPath));
    }
    if (!options.latencyPingTarget.empty())
    {
        Logger::info("ICMP RTT fallback enabled: target={}", narrowForLog(options.latencyPingTarget));
    }
    else
    {
        Logger::info("ICMP RTT fallback disabled: pass --latency-ping <ipv4> to enable RTT jitter metrics");
    }
    if (options.maxRuntime.has_value())
    {
        Logger::info("max runtime limit enabled: {} seconds", options.maxRuntime->count());
    }

    SchedulerPolicy mainThreadPolicy{
        .affinityMask = 0,
        .processorGroup = 0,
        .threadPriority = THREAD_PRIORITY_ABOVE_NORMAL};

    const auto topologyResult = TopologyAnalyzer::buildMainThreadMask(targetProcessId);
    if (topologyResult)
    {
        const auto& topology = topologyResult.value();
        mainThreadPolicy.affinityMask = topology.validatedMask;
        mainThreadPolicy.processorGroup = topology.processorGroup;
    }
    else
    {
        Logger::warn(
            "topology analyzer unavailable: {}; attempting process-affinity fallback policy",
            toString(topologyResult.error()));

        const auto fallbackPolicy = buildProcessAffinityFallbackPolicy(targetProcessId);
        if (fallbackPolicy)
        {
            const auto& fallback = fallbackPolicy.value();
            mainThreadPolicy = fallback;
            Logger::warn(
                "topology fallback policy selected from process affinity: group={}, affinity=0x{:X}",
                static_cast<unsigned int>(mainThreadPolicy.processorGroup),
                static_cast<unsigned long long>(mainThreadPolicy.affinityMask));
        }
        else
        {
            Logger::error(
                "topology fallback policy unavailable: {}; scheduler/background policies will remain disabled until a valid mask is available",
                toString(fallbackPolicy.error()));
        }
    }

    BackgroundFilterConfig backgroundFilterConfig =
        BackgroundController::loadFilterConfigFromFile(options.backgroundFilterConfigPath);
    backgroundFilterConfig.logRestrictedProcessDetails = options.backgroundDetailLogEnabled;
    backgroundFilterConfig.logSkippedProcessDetails = options.backgroundDetailLogEnabled;
    if (options.backgroundDetailLogEnabled)
    {
        Logger::info("background detail log enabled: per-process restriction and skip lines will be printed");
    }
    if (options.schedulerMode == SchedulerMode::Apply &&
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

    return StartupPlan{
        .targetProcessId = targetProcessId,
        .mainThreadPolicy = mainThreadPolicy,
        .backgroundPolicy = BackgroundRestrictionPolicy{
            .targetProcessId = targetProcessId,
            .gameAffinityMask = mainThreadPolicy.affinityMask,
            .processorGroup = mainThreadPolicy.processorGroup},
        .trackerConfig = ThreadTrackerConfig{
            .emaAlpha = kEmaAlpha,
            .stickinessDuration = kStickinessDuration,
            .sampleCount = kSampleCount,
            .sampleInterval = kSampleInterval},
        .backgroundFilterConfig = std::move(backgroundFilterConfig)};
}
