// Build: included by main.cpp and RuntimeOrchestrator.cpp
// MODULE: RuntimeOrchestrator
// ERROR-POLICY: expected
// Reason: owns startup, runtime loop, watchdog lifecycle, and shutdown sequencing.

#pragma once

#include "RuntimeContext.h"

class RuntimeOrchestrator
{
public:
    RuntimeOrchestrator(int argc, wchar_t* argv[]) noexcept;

    [[nodiscard]] int run() noexcept;

private:
    int argc_ = 0;
    wchar_t** argv_ = nullptr;
    RuntimeContext context_{};
};
