#pragma once

#include <array>
#include <cstdint>

namespace srouter
{
    // Given a full Session Router version of: session-router-1.2.3-abc these are:
    extern const std::array<uint8_t, 3> SROUTER_VERSION;  // [1, 2, 3]
    extern const char* const SROUTER_VERSION_TAG;         // "abc"
    extern const char* const SROUTER_VERSION_FULL;        // "session-router-1.2.3-abc"
}  // namespace srouter
