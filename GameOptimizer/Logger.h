// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: Logger
// ERROR-POLICY: expected
// Reason: logging is a best-effort boundary and must not disturb control flow.
// LOGGING: best-effort — failures are intentionally ignored.

#pragma once

#include <cstdio>
#include <format>
#include <mutex>
#include <print>
#include <string>
#include <string_view>

class Logger
{
public:
    static void info(std::string_view message) noexcept
    {
        writeLine(stdout, "[INFO]  ", message);
    }

    template <typename... Args>
    static void info(std::string_view formatText, const Args&... args) noexcept
    {
        writeFormatted(stdout, "[INFO]  ", formatText, args...);
    }

    static void warn(std::string_view message) noexcept
    {
        writeLine(stdout, "[WARN]  ", message);
    }

    template <typename... Args>
    static void warn(std::string_view formatText, const Args&... args) noexcept
    {
        writeFormatted(stdout, "[WARN]  ", formatText, args...);
    }

    static void error(std::string_view message) noexcept
    {
        writeLine(stderr, "[ERROR] ", message);
    }

    template <typename... Args>
    static void error(std::string_view formatText, const Args&... args) noexcept
    {
        writeFormatted(stderr, "[ERROR] ", formatText, args...);
    }

private:
    [[nodiscard]] static std::mutex& outputMutex() noexcept
    {
        static std::mutex mutex;
        return mutex;
    }

    static void writeLine(std::FILE* stream, const char* prefix, std::string_view message) noexcept
    {
        try
        {
            const std::lock_guard lock(outputMutex());
            std::print(stream, "{}{}\n", prefix, message);
            std::fflush(stream);
        }
        catch (...)
        {
        }
    }

    template <typename... Args>
    static void writeFormatted(
        std::FILE* stream,
        const char* prefix,
        std::string_view formatText,
        const Args&... args) noexcept
    {
        try
        {
            const std::string message = std::vformat(formatText, std::make_format_args(args...));
            writeLine(stream, prefix, message);
        }
        catch (...)
        {
        }
    }
};
