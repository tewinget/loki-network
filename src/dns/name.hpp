#pragma once

#include "address/types.hpp"
#include "util/buffer.hpp"

#include <optional>
#include <string>

namespace srouter::dns
{
    /// decode name from buffer; return nullopt on failure
    std::optional<std::string> DecodeName(buffer_t* buf, bool trimTrailingDot = false);

    /// encode name to buffer
    bool EncodeNameTo(buffer_t* buf, std::string_view name);

    std::optional<std::variant<ipv4, ipv6>> DecodePTR(std::string_view name);

    bool NameIsReserved(std::string_view name);

}  // namespace srouter::dns
