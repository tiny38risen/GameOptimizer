// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: WinApiError
// ERROR-POLICY: expected
// Reason: Win32 error translation is deterministic and non-throwing.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include "ErrorCode.h"

[[nodiscard]] constexpr ErrorCode mapLastErrorToErrorCode(DWORD lastError) noexcept
{
    switch (lastError)
    {
    case ERROR_ACCESS_DENIED:
        return ErrorCode::AccessDenied;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ErrorCode::AllocationFailed;
    default:
        return ErrorCode::InternalError;
    }
}
