// Build: included by RuntimeOrchestrator.cpp and CliOptions.cpp
// MODULE: CliOptions
// ERROR-POLICY: expected
// Reason: isolates command-line parsing from runtime orchestration.

#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "ErrorCode.h"
#include "SchedulerController.h"

struct RuntimeLogConfig
{
    bool threadDetailLogEnabled = false;
    std::uint32_t threadDetailLogIntervalCycles = 1;
};

struct CliOptions
{
    std::wstring processName;
    std::uint32_t processId = 0;
    SchedulerMode schedulerMode = SchedulerMode::SoftApply;
    std::wstring backgroundFilterConfigPath;
    std::wstring latencyPingTarget;
    std::wstring stopSignalFilePath;
    bool backgroundDetailLogEnabled = false;
    RuntimeLogConfig runtimeLogConfig{};
    std::optional<std::chrono::seconds> maxRuntime;
};

[[nodiscard]] std::expected<CliOptions, ErrorCode> parseCliOptions(int argc, wchar_t* argv[]) noexcept;
[[nodiscard]] std::string_view schedulerModeName(SchedulerMode mode) noexcept;
