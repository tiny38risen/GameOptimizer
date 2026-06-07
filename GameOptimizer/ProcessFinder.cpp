// Build: cl /std:c++latest /O2 /W4 /WX /permissive- ProcessFinder.cpp
// MODULE: ProcessFinder
// ERROR-POLICY: expected
// Reason: process discovery failures are expected startup/runtime conditions.

#include "ProcessFinder.h"

#include <TlHelp32.h>
#include <limits>

#include "WinHandle.h"

namespace
{
    [[nodiscard]] bool equalsProcessNameIgnoreCase(std::wstring_view expected, const wchar_t* actual) noexcept
    {
        if (actual == nullptr || expected.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        {
            return false;
        }

        return CompareStringOrdinal(
            expected.data(),
            static_cast<int>(expected.size()),
            actual,
            -1,
            TRUE) == CSTR_EQUAL;
    }
}

std::expected<DWORD, ErrorCode>
ProcessFinder::findProcessIdByName(std::wstring_view processName) noexcept
{
    if (processName.empty())
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    WinHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot)
    {
        return std::unexpected(ErrorCode::SnapshotCreationFailed);
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot.get(), &entry))
    {
        const DWORD lastError = GetLastError();
        if (lastError == ERROR_NO_MORE_FILES)
        {
            return std::unexpected(ErrorCode::ProcessNotFound);
        }

        return std::unexpected(ErrorCode::EnumerationFailed);
    }

    do
    {
        if (equalsProcessNameIgnoreCase(processName, entry.szExeFile))
        {
            return entry.th32ProcessID;
        }
    }
    while (Process32NextW(snapshot.get(), &entry));

    const DWORD lastError = GetLastError();
    if (lastError != ERROR_NO_MORE_FILES)
    {
        return std::unexpected(ErrorCode::EnumerationFailed);
    }

    return std::unexpected(ErrorCode::ProcessNotFound);
}

std::expected<DWORD, ErrorCode>
ProcessFinder::validateProcessId(DWORD processId, std::wstring_view expectedProcessName) noexcept
{
    if (processId == 0)
    {
        return std::unexpected(ErrorCode::InvalidArgument);
    }

    WinHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot)
    {
        return std::unexpected(ErrorCode::SnapshotCreationFailed);
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot.get(), &entry))
    {
        const DWORD lastError = GetLastError();
        if (lastError == ERROR_NO_MORE_FILES)
        {
            return std::unexpected(ErrorCode::ProcessNotFound);
        }

        return std::unexpected(ErrorCode::EnumerationFailed);
    }

    do
    {
        if (entry.th32ProcessID != processId)
        {
            continue;
        }

        if (!expectedProcessName.empty() && !equalsProcessNameIgnoreCase(expectedProcessName, entry.szExeFile))
        {
            return std::unexpected(ErrorCode::ProcessNotFound);
        }

        return processId;
    }
    while (Process32NextW(snapshot.get(), &entry));

    const DWORD lastError = GetLastError();
    if (lastError != ERROR_NO_MORE_FILES)
    {
        return std::unexpected(ErrorCode::EnumerationFailed);
    }

    return std::unexpected(ErrorCode::ProcessNotFound);
}
