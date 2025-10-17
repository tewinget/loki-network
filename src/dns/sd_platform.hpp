#pragma once
#include "constants/platform.hpp"
#include "platform.hpp"

#include <type_traits>

namespace srouter::dns
{
    namespace sd
    {
        /// a dns platform that sets dns via systemd resolved
        class Platform : public I_Platform
        {
          public:
            ~Platform() override = default;

            void set_resolver(unsigned int if_index, quic::Address dns, bool global) override;
        };
    }  // namespace sd
    using SD_Platform_t = std::conditional_t<srouter::platform::has_systemd, sd::Platform, Null_Platform>;
}  // namespace srouter::dns
