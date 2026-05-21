// Build: cl /std:c++latest /O2 /W4 /WX /permissive- CliOptions.cpp
// MODULE: CliOptions
// ERROR-POLICY: expected
// Reason: command-line parsing must stay separate from runtime orchestration.

#include "CliOptions.h"

#include <cwchar>
#include <limits>

#include "LogString.h"
#include "Logger.h"

namespace
{
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

    [[nodiscard]] std::wstring parseStringArgument(int argc, wchar_t* argv[], std::wstring_view flag) noexcept
    {
        for (int index = 2; index + 1 < argc; ++index)
        {
            const std::wstring_view argument = argv[index];
            if (argument == flag)
            {
                return std::wstring(argv[index + 1]);
            }
        }

        return {};
    }

    [[nodiscard]] bool parseBooleanFlag(int argc, wchar_t* argv[], std::wstring_view flag) noexcept
    {
        for (int index = 2; index < argc; ++index)
        {
            const std::wstring_view argument = argv[index];
            if (argument == flag)
            {
                return true;
            }
        }

        return false;
    }

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
        config.threadDetailLogEnabled = parseBooleanFlag(argc, argv, L"--thread-detail-log");
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
}

std::expected<CliOptions, ErrorCode> parseCliOptions(int argc, wchar_t* argv[]) noexcept
{
    constexpr int kRequiredArgumentCount = 2;
    if (argc < kRequiredArgumentCount)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    return CliOptions{
        .processName = argv[1],
        .schedulerMode = parseSchedulerMode(argc, argv),
        .backgroundFilterConfigPath = parseStringArgument(argc, argv, L"--background-filter"),
        .latencyPingTarget = parseStringArgument(argc, argv, L"--latency-ping"),
        .backgroundDetailLogEnabled = parseBooleanFlag(argc, argv, L"--background-detail-log"),
        .runtimeLogConfig = parseRuntimeLogConfig(argc, argv),
        .maxRuntime = parseMaxRuntime(argc, argv)};
}

std::string_view schedulerModeName(SchedulerMode mode) noexcept
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
