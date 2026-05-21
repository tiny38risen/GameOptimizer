// Build: included by RuntimeOrchestrator, StartupPipeline, WatchdogCycleRunner, and ShutdownPipeline
// MODULE: RuntimeContext
// ERROR-POLICY: expected
// Reason: owns runtime services without forcing core modules to know orchestration layers.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <WinSock2.h>
#include <Windows.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "AppliedPolicyTracker.h"
#include "BackgroundController.h"
#include "CliOptions.h"
#include "InputLatencyController.h"
#include "LatencyDecisionLayer.h"
#include "LatencyMetricsCollector.h"
#include "NetworkInterruptController.h"
#include "PolicyDispatcher.h"
#include "RollbackManager.h"
#include "RuntimeValidationMonitor.h"
#include "SchedulerController.h"
#include "ThreadTracker.h"
#include "TimerResolutionController.h"

struct StartupPlan
{
    DWORD targetProcessId = 0;
    SchedulerPolicy mainThreadPolicy{};
    BackgroundRestrictionPolicy backgroundPolicy{};
    ThreadTrackerConfig trackerConfig{};
    BackgroundFilterConfig backgroundFilterConfig{};
    std::chrono::milliseconds watchdogInterval = std::chrono::milliseconds(3000);
    std::uint32_t policyAuditIntervalCycles = 4;
    std::size_t maxDisplayedThreads = 8;
};

struct RuntimeContext
{
    CliOptions options{};
    StartupPlan startupPlan{};

    std::unique_ptr<ThreadTracker> threadTracker;
    std::unique_ptr<RollbackManager> rollbackManager;
    std::unique_ptr<SchedulerController> schedulerController;
    std::unique_ptr<TimerResolutionController> timerResolutionController;
    std::unique_ptr<InputLatencyController> inputLatencyController;
    std::unique_ptr<BackgroundController> backgroundController;
    std::unique_ptr<NetworkInterruptController> networkInterruptController;
    std::unique_ptr<PolicyDispatcher> policyDispatcher;
    std::unique_ptr<LatencyDecisionLayer> latencyDecisionLayer;
    std::unique_ptr<AppliedPolicyTracker> appliedPolicyTracker;
    std::unique_ptr<RuntimeValidationMonitor> runtimeValidationMonitor;
    std::unique_ptr<LatencyMetricsCollector> latencyMetricsCollector;

    std::optional<DWORD> lastAppliedThreadId;
    std::uint32_t policyAuditCycleCounter = 0;
    std::uint32_t threadDetailLogCycleCounter = 0;
};
