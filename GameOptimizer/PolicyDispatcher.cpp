// Build: cl /std:c++latest /O2 /W4 /WX /permissive- PolicyDispatcher.cpp
// MODULE: PolicyDispatcher
// ERROR-POLICY: expected
// Reason: dispatch boundaries must preserve SRS command ownership and rollback priority.

#include "PolicyDispatcher.h"

#include <chrono>

#include "BackgroundController.h"
#include "Logger.h"
#include "SchedulerController.h"
#include "ThreadTracker.h"

PolicyDispatcher::PolicyDispatcher(
    SchedulerController& schedulerController,
    ThreadTracker& threadTracker,
    BackgroundController& backgroundController,
    const BackgroundRestrictionPolicy& backgroundPolicy) noexcept
    : schedulerController_(schedulerController),
      threadTracker_(threadTracker),
      backgroundController_(backgroundController),
      backgroundPolicy_(backgroundPolicy)
{
}

std::expected<void, ErrorCode> PolicyDispatcher::dispatch(PolicyCommand command) noexcept
{
    if (command == PolicyCommand::None)
    {
        return {};
    }

    Logger::info("policy command received: {}", toString(command));

    switch (command)
    {
    case PolicyCommand::Rollback:
        Logger::warn("dispatching ROLLBACK command: all scheduler state will be restored");
        return schedulerController_.rollbackAll();

    case PolicyCommand::StickyUp:
        threadTracker_.increaseStickinessBy(ThreadTrackerPolicy::kStickyUpStep);
        return {};

    case PolicyCommand::BgRestrictUp:
    {
        const auto result = backgroundController_.applyRestriction(backgroundPolicy_);
        if (!result)
        {
            return std::unexpected(result.error());
        }
        return {};
    }

    case PolicyCommand::IrqRepin:
        Logger::warn("IRQ_REPIN command is recognized but network/interrupt module is not connected yet");
        return {};

    case PolicyCommand::None:
        return {};
    }

    return std::unexpected(ErrorCode::InternalError);
}
