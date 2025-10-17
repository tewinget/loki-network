#pragma once

#include "address/address.hpp"
#include "address/types.hpp"
#include "net/ip_packet.hpp"

namespace srouter::handlers
{

    // Abstract class for TUN handling.  This base interface exists so that embedded clients can be
    // built without needing to compile any tun code at all.
    class TunEPBase
    {
      public:
        virtual ~TunEPBase() = default;

        virtual void start_poller() = 0;

        virtual std::optional<ipv4> map(const NetworkAddress& remote) = 0;
        virtual void unmap(const NetworkAddress& remote) = 0;

        virtual void handle_inbound_packet(IPPacket pkt, uint8_t type, NetworkAddress remote) = 0;
    };

}  // namespace srouter::handlers
