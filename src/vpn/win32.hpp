#pragma once

#include "platform.hpp"
#include "router/router.hpp"
#include "win32/exec.hpp"

#include <winsock2.h>

#include <windows.h>

#include <iphlpapi.h>
#include <session/router_context.hpp>

namespace srouter::win32
{
    using namespace srouter::vpn;
    class VPNPlatform : public Platform, public AbstractRouteManager
    {
        srouter::Context* const _ctx;
        const int m_Metric{2};

        const auto& Net() const { return _ctx->router->net(); }

        void make_route(std::string ip, std::string gw, std::string cmd);

        void default_route_via_interface(NetworkInterface& vpn, std::string cmd);

        void route_via_interface(NetworkInterface& vpn, std::string addr, std::string mask, std::string cmd);

      public:
        VPNPlatform(const VPNPlatform&) = delete;
        VPNPlatform(VPNPlatform&&) = delete;

        VPNPlatform(srouter::Context* ctx) : Platform{}, _ctx{ctx} {}

        ~VPNPlatform() override = default;

        void add_route(quic::Address ip, quic::Address gateway) override;

        void delete_route(quic::Address ip, quic::Address gateway) override;

        void add_route_via_interface(NetworkInterface& vpn, IPRange range) override;

        void delete_route_via_interface(NetworkInterface& vpn, IPRange range) override;

        std::vector<quic::Address> get_non_interface_gateways(NetworkInterface& vpn) override;

        void add_default_route_via_interface(NetworkInterface& vpn) override;

        void delete_default_route_via_interface(NetworkInterface& vpn) override;

        std::shared_ptr<NetworkInterface> obtain_interface(InterfaceInfo info, Router* router) override;

        std::shared_ptr<PacketIO> create_packet_io(
            unsigned int ifindex, const std::optional<quic::Address>& dns_upstream_src) override;

        AbstractRouteManager& RouteManager() override { return *this; }
    };

}  // namespace srouter::win32
