// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: Logger
// ERROR-POLICY: expected
// Reason: logging is a best-effort boundary and must not disturb control flow.
// LOGGING: best-effort — failures are intentionally ignored.

#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <format>
#include <mutex>
#include <print>
#include <string>
#include <string_view>

template <typename... Args>
concept LogFormattable = requires(std::string_view formatText, const Args&... args)
{
    std::vformat(formatText, std::make_format_args(args...));
};

class Logger
{
public:
    static void info(std::string_view message) noexcept
    {
        writeLine(stdout, "INFO", message);
    }

    template <typename... Args>
        requires LogFormattable<Args...>
    static void info(std::string_view formatText, const Args&... args) noexcept
    {
        writeFormatted(stdout, "INFO", formatText, args...);
    }

    static void warn(std::string_view message) noexcept
    {
        writeLine(stdout, "WARN", message);
    }

    template <typename... Args>
        requires LogFormattable<Args...>
    static void warn(std::string_view formatText, const Args&... args) noexcept
    {
        writeFormatted(stdout, "WARN", formatText, args...);
    }

    static void error(std::string_view message) noexcept
    {
        writeLine(stderr, "ERROR", message);
    }

    template <typename... Args>
        requires LogFormattable<Args...>
    static void error(std::string_view formatText, const Args&... args) noexcept
    {
        writeFormatted(stderr, "ERROR", formatText, args...);
    }

private:
    [[nodiscard]] static std::mutex& outputMutex() noexcept
    {
        static std::mutex mutex;
        return mutex;
    }

    [[nodiscard]] static std::atomic_uint64_t& sequenceCounter() noexcept
    {
        static std::atomic_uint64_t counter{0};
        return counter;
    }

    static void writeLine(std::FILE* stream, const char* level, std::string_view message) noexcept
    {
        try
        {
            const auto now = std::chrono::system_clock::now();
            const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % std::chrono::seconds(1);
            const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
            std::tm localTime{};
            (void)localtime_s(&localTime, &timeValue);
            const std::uint64_t sequence = sequenceCounter().fetch_add(1, std::memory_order_relaxed) + 1;

            const std::lock_guard lock(outputMutex());
            std::print(
                stream,
                "[{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}][#{:06}][{}] {}\n",
                localTime.tm_year + 1900,
                localTime.tm_mon + 1,
                localTime.tm_mday,
                localTime.tm_hour,
                localTime.tm_min,
                localTime.tm_sec,
                millis.count(),
                sequence,
                level,
                message);
            std::fflush(stream);
        }
        catch (...)
        {
        }
    }

    template <typename... Args>
        requires LogFormattable<Args...>
    static void writeFormatted(
        std::FILE* stream,
        const char* level,
        std::string_view formatText,
        const Args&... args) noexcept
    {
        try
        {
            const std::string message = std::vformat(formatText, std::make_format_args(args...));
            writeLine(stream, level, message);
        }
        catch (...)
        {
        }
    }
};
