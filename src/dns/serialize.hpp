#pragma once

#include "util/buffer.hpp"

#include <nlohmann/json_fwd.hpp>

#include <vector>

namespace srouter::dns
{
    /// base type for serializable dns entities
    struct Serialize
    {
        virtual ~Serialize() = 0;

        /// encode entity to buffer
        virtual bool Encode(buffer_t* buf) const = 0;

        /// decode entity from buffer
        virtual bool Decode(buffer_t* buf) = 0;

        /// convert this whatever into json
        virtual nlohmann::json ToJSON() const = 0;

        static constexpr bool to_string_formattable = true;
    };

    bool EncodeRData(buffer_t* buf, const std::vector<uint8_t>& rdata);

    bool DecodeRData(buffer_t* buf, std::vector<uint8_t>& rdata);

}  // namespace srouter::dns
