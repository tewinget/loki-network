#pragma once

#include <memory>

namespace srouter
{
    class Router;
}

namespace srouter::vpn
{
    struct InterfaceInfo;
    class NetworkInterface;
}  // namespace srouter::vpn

namespace srouter::win32::wintun
{
    /// makes a new vpn interface with a wintun context given info and a router pointer
    std::shared_ptr<vpn::NetworkInterface> make_interface(const vpn::InterfaceInfo& info, Router* router);

}  // namespace srouter::win32::wintun
