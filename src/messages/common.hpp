#pragma once

#include "contact/relay_contact.hpp"
#include "crypto/crypto.hpp"
#include "path/transit_hop.hpp"
#include "util/buffer.hpp"
#include "util/logging.hpp"

#include <oxenc/bt_producer.h>

namespace srouter::messages
{
    inline constexpr auto STATUS_KEY = "!"sv;
    std::string serialize_status_response(std::string_view value);

    extern const std::string TIMEOUT_RESPONSE;
    extern const std::string ERROR_RESPONSE;
    extern const std::string OK_RESPONSE;
}  // namespace srouter::messages

namespace srouter
{

    // Copies the contents out of a bt_dict_producer into a std::vector<std::byte>.
    // TODO FIXME - avoid the need to use this, by making bt_dict_producer able to write into and
    // extract a vector directly.
    inline std::vector<std::byte> to_bytes(const oxenc::bt_dict_producer& btdp)
    {
        auto content = btdp.span<std::byte>();
        return {content.begin(), content.end()};
    }

}  // namespace srouter
