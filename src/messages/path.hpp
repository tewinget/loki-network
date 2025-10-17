#pragma once

#include "address/address.hpp"
#include "constants/path.hpp"
#include "router/router.hpp"
#include "util/logging/buffer.hpp"

namespace srouter::PATH
{
    namespace BUILD
    {
        extern const std::string NO_TRANSIT;
        extern const std::string BAD_LIFETIME;
        extern const std::string BAD_FRAMES;
        extern const std::string BAD_PATHID;
        extern const std::string BAD_CRYPTO;

    }  // namespace BUILD

    namespace CONTROL
    {
        std::vector<std::byte> serialize(std::string_view endpoint, std::span<const std::byte> payload);

        std::pair<std::string, std::string> deserialize(oxenc::bt_dict_consumer&& btdc);

    }  // namespace CONTROL

}  // namespace srouter::PATH
