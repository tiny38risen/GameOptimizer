// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: PolicyDispatcher
// ERROR-POLICY: expected
// Reason: PolicyCommand dispatch can trigger rollback and scheduler state changes.

#pragma once

#include <expected>

#include "ErrorCode.h"
#include "PolicyCommand.h"

class BackgroundController;
class NetworkInterruptController;
class SchedulerController;
class ThreadTracker;
struct BackgroundRestrictionPolicy;

class PolicyDispatcher
{
public:
    PolicyDispatcher(
        SchedulerController& schedulerController,
        ThreadTracker& threadTracker,
        BackgroundController& backgroundController,
        const BackgroundRestrictionPolicy& backgroundPolicy,
        NetworkInterruptController* networkInterruptController = nullptr) noexcept;

    [[nodiscard]] std::expected<void, ErrorCode> dispatch(PolicyCommand command) noexcept;

private:
    SchedulerController& schedulerController_;
    ThreadTracker& threadTracker_;
    BackgroundController& backgroundController_;
    const BackgroundRestrictionPolicy& backgroundPolicy_;
    NetworkInterruptController* networkInterruptController_ = nullptr;
};
