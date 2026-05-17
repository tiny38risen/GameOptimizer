// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: ErrorCode
// ERROR-POLICY: expected
// Reason: shared recoverable error categories for Win32 API boundaries.

#pragma once

#include <string_view>

enum class ErrorCode
{
    InvalidArgument,
    SnapshotCreationFailed,
    EnumerationFailed,
    ProcessNotFound,
    ProcessOpenFailed,
    ProcessAffinityQueryFailed,
    ProcessAffinityApplyFailed,
    ProcessPriorityQueryFailed,
    ProcessPriorityApplyFailed,
    TopologyScanFailed,
    ThreadOpenFailed,
    ThreadTimeQueryFailed,
    ThreadPriorityQueryFailed,
    ThreadAffinityQueryFailed,
    ThreadPriorityApplyFailed,
    ThreadAffinityApplyFailed,
    ApplyVerificationFailed,
    RollbackVerificationFailed,
    RollbackStateNotFound,
    RollbackFailed,
    PolicyDriftReapplyLimitExceeded,
    AccessDenied,
    AllocationFailed,
    InternalError,
    ConsoleControlHandlerFailed
};

[[nodiscard]] constexpr std::string_view toString(ErrorCode errorCode) noexcept
{
    switch (errorCode)
    {
    case ErrorCode::InvalidArgument:
        return "invalid argument";
    case ErrorCode::SnapshotCreationFailed:
        return "snapshot creation failed";
    case ErrorCode::EnumerationFailed:
        return "enumeration failed";
    case ErrorCode::ProcessNotFound:
        return "process not found";
    case ErrorCode::ProcessOpenFailed:
        return "process open failed";
    case ErrorCode::ProcessAffinityQueryFailed:
        return "process affinity query failed";
    case ErrorCode::ProcessAffinityApplyFailed:
        return "process affinity apply failed";
    case ErrorCode::ProcessPriorityQueryFailed:
        return "process priority query failed";
    case ErrorCode::ProcessPriorityApplyFailed:
        return "process priority apply failed";
    case ErrorCode::TopologyScanFailed:
        return "topology scan failed";
    case ErrorCode::ThreadOpenFailed:
        return "thread open failed";
    case ErrorCode::ThreadTimeQueryFailed:
        return "thread time query failed";
    case ErrorCode::ThreadPriorityQueryFailed:
        return "thread priority query failed";
    case ErrorCode::ThreadAffinityQueryFailed:
        return "thread affinity query failed";
    case ErrorCode::ThreadPriorityApplyFailed:
        return "thread priority apply failed";
    case ErrorCode::ThreadAffinityApplyFailed:
        return "thread affinity apply failed";
    case ErrorCode::ApplyVerificationFailed:
        return "apply verification failed";
    case ErrorCode::RollbackVerificationFailed:
        return "rollback verification failed";
    case ErrorCode::RollbackStateNotFound:
        return "rollback state not found";
    case ErrorCode::RollbackFailed:
        return "rollback failed";
    case ErrorCode::PolicyDriftReapplyLimitExceeded:
        return "policy drift reapply limit exceeded";
    case ErrorCode::AccessDenied:
        return "access denied";
    case ErrorCode::AllocationFailed:
        return "allocation failed";
    case ErrorCode::InternalError:
        return "internal error";
    case ErrorCode::ConsoleControlHandlerFailed:
        return "console control handler failed";
    }

    return "unknown error";
}
