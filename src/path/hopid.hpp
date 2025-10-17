#pragma once

#include "util/aligned.hpp"

namespace srouter
{

    inline constexpr size_t HOPID_SIZE = 16;

    struct HopID final : public AlignedBuffer<HOPID_SIZE>
    {
        using AlignedBuffer<HOPID_SIZE>::AlignedBuffer;

        static HopID make_random();
    };

}  // namespace srouter

template <>
struct std::hash<srouter::HopID> : hash<srouter::AlignedBuffer<srouter::HopID::SIZE>>
{};
