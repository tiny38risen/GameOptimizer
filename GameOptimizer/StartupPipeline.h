// Build: included by RuntimeOrchestrator.cpp and StartupPipeline.cpp
// MODULE: StartupPipeline
// ERROR-POLICY: expected
// Reason: startup preparation crosses process discovery, topology, and configuration boundaries.

#pragma once

#include <expected>

#include "ErrorCode.h"
#include "RuntimeContext.h"

class StartupPipeline
{
public:
    [[nodiscard]] static std::expected<RuntimeContext, ErrorCode> run(int argc, wchar_t* argv[]) noexcept;
    [[nodiscard]] static std::expected<StartupPlan, ErrorCode> prepare(const CliOptions& options) noexcept;
};
