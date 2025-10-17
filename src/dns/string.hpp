#pragma once

#include <string>

struct buffer_t;

namespace srouter::dns
{
    using name_t = std::string;

    /// decode name from buffer
    bool decode_name(buffer_t* buf, name_t& name);

    /// encode name to buffer
    bool encode_name(buffer_t* buf, const name_t& name);

}  // namespace srouter::dns
