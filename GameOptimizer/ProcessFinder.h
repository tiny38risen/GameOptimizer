// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: ProcessFinder
// ERROR-POLICY: expected
// Reason: process discovery failures are expected startup/runtime conditions.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <expected>
#include <string_view>

#include "ErrorCode.h"

class ProcessFinder
{
public:
    [[nodiscard]] static std::expected<DWORD, ErrorCode>
    findProcessIdByName(std::wstring_view processName) noexcept;

    [[nodiscard]] static std::expected<DWORD, ErrorCode>
    validateProcessId(DWORD processId, std::wstring_view expectedProcessName) noexcept;
};
