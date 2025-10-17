#include "nm_platform.hpp"
#ifdef WITH_SYSTEMD

extern "C"
{
#include <net/if.h>
}

#include "linux/dbus.hpp"

namespace srouter::dns::nm
{
    void Platform::set_resolver(unsigned int, quic::Address, bool)
    {
        // todo: implement me eventually
    }
}  // namespace srouter::dns::nm
#endif
