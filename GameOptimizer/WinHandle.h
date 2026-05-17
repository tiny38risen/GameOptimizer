// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: WinHandle
// ERROR-POLICY: expected
// Reason: handle lifetime is deterministic; acquisition failures are reported by callers.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <utility>

class WinHandle
{
public:
    explicit WinHandle(HANDLE handle = nullptr) noexcept
        : handle_(handle)
    {
    }

    ~WinHandle() noexcept
    {
        reset();
    }

    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;

    WinHandle(WinHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr))
    {
    }

    WinHandle& operator=(WinHandle&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }

        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return isValid();
    }

    void reset(HANDLE newHandle = nullptr) noexcept
    {
        if (isValid())
        {
            CloseHandle(handle_);
        }

        handle_ = newHandle;
    }

private:
    HANDLE handle_ = nullptr;
};
