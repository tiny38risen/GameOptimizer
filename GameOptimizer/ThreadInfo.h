// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: ThreadInfo
// ERROR-POLICY: expected
// Reason: plain value object used to expose thread observation data.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdint>

struct ThreadInfo
{
    std::uint64_t deltaCpuTime100ns = 0;
    double emaCpuTime100ns = 0.0;
    DWORD threadId = 0;
    bool isEmaCandidate = false;
    bool isMainThread = false;
};
