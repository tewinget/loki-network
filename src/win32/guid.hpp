#pragma once

#include "crypto/crypto.hpp"

#include <windows.h>

namespace srouter::win32
{
    /// @brief given a container of data hash it and make it into a GUID so we have a way to
    /// deterministically generate GUIDs
    template <typename Data>
    inline GUID MakeDeterministicGUID(Data data)
    {
        auto h = crypto::shorthash(std::span{reinterpret_cast<const std::byte*>(data.data()), data.size()});
        static_assert(sizeof(GUID) <= decltype(h)::SIZE);
        GUID guid{};
        std::memcpy(&guid, h.data(), sizeof(guid));
        return guid;
    }
}  // namespace srouter::win32
