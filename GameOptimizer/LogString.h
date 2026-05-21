// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: LogString
// ERROR-POLICY: none
// Reason: shared narrow logging helpers avoid duplicate lossy conversion code.

#pragma once

#include <string>
#include <string_view>

inline void narrowForLogInto(std::wstring_view value, std::string& outputBuffer)
{
    outputBuffer.clear();
    outputBuffer.reserve(value.size());

    for (const wchar_t ch : value)
    {
        outputBuffer.push_back(ch >= 0 && ch <= 0x7F ? static_cast<char>(ch) : '?');
    }
}

[[nodiscard]] inline std::string narrowForLog(std::wstring_view value)
{
    std::string result;
    narrowForLogInto(value, result);
    return result;
}
