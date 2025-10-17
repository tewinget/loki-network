#include "context_wrapper.h"

#include "config/config.hpp"
#include "constants/apple.hpp"
#include "context.hpp"
#include "net/ip_packet.hpp"
#include "util/logging.hpp"
#include "util/logging/buffer.hpp"
#include "util/logging/callback_sink.hpp"
#include "vpn_interface.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace
{
    static auto logcat = oxen::log::Cat("apple.ctx_wrapper");

    struct instance_data
    {
        srouter::apple::Context context;
        std::thread runner;
        packet_writer_callback packet_writer;
        start_reading_callback start_reading;

        std::weak_ptr<srouter::apple::VPNInterface> iface;
    };

}  // namespace

// Expose this with C linkage so that objective-c can use it
extern "C" const uint16_t dns_trampoline_port = srouter::apple::dns_trampoline_port;

void* llarp_apple_init(llarp_apple_config* appleconf)
{
    srouter::log::clear_sinks();
    srouter::log::add_sink(std::make_shared<srouter::logging::CallbackSink_mt>(
        [](const char* msg, void* nslog) { reinterpret_cast<ns_logger_callback>(nslog)(msg); },
        nullptr,
        reinterpret_cast<void*>(appleconf->ns_logger)));
    srouter::logRingBuffer = std::make_shared<srouter::log::RingBufferSink>(100);
    srouter::log::add_sink(srouter::logRingBuffer, srouter::log::DEFAULT_PATTERN_MONO);

    try
    {
        auto config_dir = std::filesystem::u8path(appleconf->config_dir);
        auto config = std::make_shared<srouter::Config>(config_dir);
        std::filesystem::path config_path = config_dir / "session_router.ini";
        if (!exists(config_path))
            srouter::ensure_config(config_dir, config_path, false, srouter::config::Type::FullClient);
        config->load(config_path);

        // If no range is specified then go look for a free one, set that in the config, and then
        // return it to the caller via the char* parameters.
        auto& range = config->network.if_addr;
        if (!range.addr.h)
        {
            if (auto maybe = srouter::net::Platform::Default_ptr()->find_free_range())
                range = *maybe;
            else
                throw std::runtime_error{"Could not find any free IP range"};
        }
        auto addr = srouter::net::TruncateV6(range.addr).to_string();
        auto mask = srouter::net::TruncateV6(range.netmask_bits).to_string();
        if (addr.size() > 15 || mask.size() > 15)
            throw std::runtime_error{"Unexpected non-IPv4 tunnel range configured"};
        std::strncpy(appleconf->tunnel_ipv4_ip, addr.c_str(), sizeof(appleconf->tunnel_ipv4_ip));
        std::strncpy(appleconf->tunnel_ipv4_netmask, mask.c_str(), sizeof(appleconf->tunnel_ipv4_netmask));

        // TODO: in the future we want to do this properly with our pubkey (see issue #1705), but
        // that's going to take a bit more work because we currently can't *get* the (usually)
        // ephemeral pubkey at this stage of Session Router configuration.  So for now we just stick our
        // IPv4 address into it until #1705 gets implemented.
        srouter::huint128_t ipv6{srouter::uint128_t{0xfd2e'6c6f'6b69'0000, srouter::net::TruncateV6(range.addr).h}};
        std::strncpy(appleconf->tunnel_ipv6_ip, ipv6.to_string().c_str(), sizeof(appleconf->tunnel_ipv6_ip));
        appleconf->tunnel_ipv6_prefix = 48;

        appleconf->upstream_dns[0] = '\0';
        for (auto& upstream : config->dns.upstream_dns)
        {
            if (upstream.isIPv4())
            {
                std::strcpy(appleconf->upstream_dns, upstream.hostString().c_str());
                appleconf->upstream_dns_port = upstream.getPort();
                break;
            }
        }

#ifdef MACOS_SYSTEM_EXTENSION
        std::strncpy(
            appleconf->dns_bind_ip, config->dns.m_bind.front().hostString().c_str(), sizeof(appleconf->dns_bind_ip));
#endif

        // If no explicit bootstrap then set the system default one included with the app bundle
        if (config->bootstrap.files.empty())
            config->bootstrap.files.push_back(std::filesystem::u8path(appleconf->default_bootstrap));

        auto inst = std::make_unique<instance_data>();
        inst->context.Configure(std::move(config));
        inst->context.route_callbacks = appleconf->route_callbacks;

        inst->packet_writer = appleconf->packet_writer;
        inst->start_reading = appleconf->start_reading;

        return inst.release();
    }
    catch (const std::exception& e)
    {
        oxen::log::error(logcat, "Failed to initialize Session Router from config: {}", e.what());
    }
    return nullptr;
}

int llarp_apple_start(void* Session Router, void* callback_context)
{
    auto* inst = static_cast<instance_data*>(Session Router);

    inst->context.callback_context = callback_context;

    inst->context.m_PacketWriter = [inst, callback_context](int af_family, void* data, size_t size) {
        inst->packet_writer(af_family, data, size, callback_context);
        return true;
    };

    inst->context.m_OnReadable = [inst, callback_context](srouter::apple::VPNInterface& iface) {
        inst->iface = iface.weak_from_this();
        inst->start_reading(callback_context);
    };

    std::promise<void> result;
    inst->runner = std::thread{[inst, &result] {
        const srouter::RuntimeOptions opts{};
        try
        {
            inst->context.Setup(opts);
        }
        catch (...)
        {
            result.set_exception(std::current_exception());
            return;
        }
        result.set_value();
        inst->context.Run(opts);
    }};

    try
    {
        result.get_future().get();
    }
    catch (const std::exception& e)
    {
        oxen::log::error(logcat, "Failed to initialize Session Router: {}", e.what());
        return -1;
    }

    return 0;
}

uv_loop_t* llarp_apple_get_uv_loop(void* Session Router)
{
    auto& inst = *static_cast<instance_data*>(Session Router);
    auto uvw = inst.context.loop->MaybeGetUVWLoop();
    assert(uvw);
    return uvw->raw();
}

int llarp_apple_incoming(void* Session Router, const llarp_incoming_packet* packets, size_t size)
{
    auto& inst = *static_cast<instance_data*>(Session Router);

    auto iface = inst.iface.lock();
    if (!iface)
        return -1;

    int count = 0;
    for (size_t i = 0; i < size; i++)
    {
        buffer_t buf{static_cast<const uint8_t*>(packets[i].bytes), packets[i].size};
        if (iface->OfferReadPacket(buf))
            count++;
        else
            oxen::log::error(logcat, "invalid IP packet: {}", srouter::buffer_printer(buf));
    }

    iface->MaybeWakeUpperLayers();
    return count;
}

void llarp_apple_shutdown(void* Session Router)
{
    auto* inst = static_cast<instance_data*>(Session Router);

    inst->context.CloseAsync();
    inst->context.Wait();
    inst->runner.join();
    delete inst;
}
