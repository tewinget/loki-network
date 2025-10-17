#include "transit_hop.hpp"

#include "crypto/crypto.hpp"
#include "link/endpoint.hpp"
#include "messages/common.hpp"
#include "messages/path.hpp"
#include "router/router.hpp"
#include "util/bspan.hpp"
#include "util/buffer.hpp"
#include "util/time.hpp"

#include <nlohmann/json.hpp>
#include <oxen/quic/connection_ids.hpp>
#include <sodium/randombytes.h>

#include <stdexcept>

namespace srouter::path
{
    static auto logcat = log::Cat("transit-hop");

    TransitHopError::TransitHopError(std::string err_code)
        : std::runtime_error{"TransitHop construction failed: {}"_format(err_code)}, error_code{std::move(err_code)}
    {}

    std::pair<std::variant<RouterID, quic::ConnectionID>, HopID> TransitHop::next_id(const HopID& h) const
    {
        std::pair<std::variant<RouterID, quic::ConnectionID>, HopID> ret;

        assert(h == rxid or h == txid);
        if (h == rxid)
            return {upstream, txid};
        return {downstream, rxid};
    }

    nlohmann::json TransitHop::ExtractStatus() const
    {
        return {
            {"rid", router_id.ToHex()}, {"rxid", rxid.ToHex()}, {"txid", txid.ToHex()}, {"expiry", to_json(expiry)}};
    }

    static std::string short_string(const std::variant<RouterID, quic::ConnectionID>& downstream)
    {
        if (auto* rid = std::get_if<RouterID>(&downstream))
            return rid->short_string().to_string();
        return std::get<quic::ConnectionID>(downstream).to_string();
    }

    std::string TransitHop::to_string() const
    {
        return "TransitHop:[ Terminal:{} | TX:{} | RX:{} | Upstream:{} | Downstream:{} | Expiry:{} ]"_format(
            terminal_hop, txid, rxid, upstream.short_string(), short_string(downstream), expiry.count());
    }

}  // namespace srouter::path
