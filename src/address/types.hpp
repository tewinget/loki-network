#pragma once

#include "util/formattable.hpp"

#include <oxen/quic/ip.hpp>

namespace srouter
{
    namespace quic = oxen::quic;
    using quic::ipv4;
    using quic::ipv4_net;
    using quic::ipv4_range;
    using quic::ipv6;
    using quic::ipv6_net;
    using quic::ipv6_range;

    // Used for hash combining, below
    inline constexpr size_t inverse_golden_ratio = sizeof(size_t) >= 8 ? 0x9e37'79b9'7f4a'7c15 : 0x9e37'79b9;
}  //   namespace srouter

template <>
struct std::hash<srouter::ipv4>
{
    size_t operator()(const srouter::ipv4& obj) const noexcept { return hash<uint32_t>{}(obj.addr); }
};

template <>
struct std::hash<srouter::ipv6>
{
    size_t operator()(const srouter::ipv6& obj) const noexcept
    {
        std::hash<uint64_t> subhash{};
        auto h = subhash(obj.hi);
        h ^= subhash(obj.lo) + srouter::inverse_golden_ratio + (h << 6) + (h >> 2);
        return h;
    }
};
