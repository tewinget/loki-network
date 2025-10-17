#pragma once
#include "constants/platform.hpp"
#include "platform.hpp"

#include <type_traits>
#include <unordered_map>

namespace srouter::dns
{
    namespace nm
    {
        // a dns platform that sets dns via network manager
        class Platform : public I_Platform
        {
          public:
            ~Platform() override = default;

            void set_resolver(unsigned int index, quic::Address dns, bool global) override;
        };
    };  // namespace nm
    using NM_Platform_t = std::conditional_t<false, nm::Platform, Null_Platform>;
}  // namespace srouter::dns
