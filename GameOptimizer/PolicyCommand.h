// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: PolicyCommand
// ERROR-POLICY: expected
// Reason: decision-layer commands must be explicit and side-effect free until dispatched.

#pragma once

#include <string_view>

enum class PolicyCommand
{
    None,
    BgRestrictUp,
    IrqRepin,
    StickyUp,
    Rollback
};

[[nodiscard]] constexpr std::string_view toString(PolicyCommand command) noexcept
{
    switch (command)
    {
    case PolicyCommand::None:
        return "NONE";
    case PolicyCommand::BgRestrictUp:
        return "BG_RESTRICT_UP";
    case PolicyCommand::IrqRepin:
        return "IRQ_REPIN";
    case PolicyCommand::StickyUp:
        return "STICKY_UP";
    case PolicyCommand::Rollback:
        return "ROLLBACK";
    }

    return "UNKNOWN";
}
