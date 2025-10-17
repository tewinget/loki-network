#pragma once

#include "constants.hpp"
#include "util/aligned.hpp"

#include <algorithm>

namespace srouter
{
    struct SharedSecret final : AlignedBuffer<SHAREDKEYSIZE>
    {};

    struct Signature final : AlignedBuffer<SIGSIZE>
    {};

    struct SymmNonce final : AlignedBuffer<NONCESIZE>
    {
        using AlignedBuffer<NONCESIZE>::AlignedBuffer;

        SymmNonce operator^(const SymmNonce& other) const
        {
            SymmNonce ret;
            std::transform(begin(), end(), other.begin(), ret.begin(), std::bit_xor<>());
            return ret;
        }

        static SymmNonce make_random();
    };

}  // namespace srouter
