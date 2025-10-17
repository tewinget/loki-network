#pragma once

#include "exception.hpp"

namespace srouter::win32
{
    inline void ensure_handle_is_valid(HANDLE h)
    {
        BY_HANDLE_FILE_INFORMATION info{};
        if (GetFileInformationByHandle(h, &info))
            return;
        if (auto err = GetLastError())
        {
            SetLastError(0);
            throw srouter::win32::error{err, "handle validity check failed"};
        }
    }
}  // namespace srouter::win32
