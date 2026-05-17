// Build: included by translation units built with cl /std:c++latest /O2 /W4 /WX /permissive-
// MODULE: IcmpHandle
// ERROR-POLICY: noexcept RAII
// Reason: ICMP handles must be closed with IcmpCloseHandle, not CloseHandle.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <WinSock2.h>
#include <Windows.h>
#include <Iphlpapi.h>
#include <IcmpAPI.h>
#include <utility>

class IcmpHandle
{
public:
    IcmpHandle() noexcept = default;

    ~IcmpHandle() noexcept
    {
        close();
    }

    IcmpHandle(const IcmpHandle&) = delete;
    IcmpHandle& operator=(const IcmpHandle&) = delete;

    IcmpHandle(IcmpHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE))
    {
    }

    IcmpHandle& operator=(IcmpHandle&& other) noexcept
    {
        if (this != &other)
        {
            close();
            handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
        }
        return *this;
    }

    [[nodiscard]] bool open() noexcept
    {
        if (isValid())
        {
            return true;
        }

        handle_ = IcmpCreateFile();
        return isValid();
    }

    void close() noexcept
    {
        if (isValid())
        {
            IcmpCloseHandle(handle_);
        }
        handle_ = INVALID_HANDLE_VALUE;
    }

    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};
