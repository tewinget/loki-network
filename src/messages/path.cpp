#include "path.hpp"

#include "common.hpp"
#include "constants/path.hpp"
#include "crypto/crypto.hpp"
#include "util/bspan.hpp"

#include <ranges>
#include <stdexcept>

namespace srouter
{

    static auto logcat = srouter::log::Cat("path.msgs");

    // FIXME TODO: get rid of this file.  Serialization belongs with the thing being serialized, not
    // in some header far removed.

    namespace PATH
    {
        namespace BUILD
        {

            const std::string NO_TRANSIT = messages::serialize_status_response("NOT ALLOWING TRANSIT"sv);
            const std::string BAD_LIFETIME = messages::serialize_status_response("BAD PATH LIFETIME (TOO LONG)"sv);
            const std::string BAD_FRAMES = messages::serialize_status_response("BAD FRAMES"sv);
            const std::string BAD_PATHID = messages::serialize_status_response("BAD PATH ID"sv);

        }  // namespace BUILD

        namespace CONTROL
        {
            /** Fields for transmitting Path Control:
                - 'e' : request endpoint being invoked
                - 'p' : request payload
            */
            std::vector<std::byte> serialize(std::string_view endpoint, std::span<const std::byte> payload)
            {
                oxenc::bt_dict_producer btdp;
                btdp.append("e", endpoint);
                btdp.append("p", payload);
                return to_bytes(btdp);
            }

            std::pair<std::string, std::string> deserialize(oxenc::bt_dict_consumer&& btdc)
            {
                std::pair<std::string, std::string> ret;
                auto& [endpoint, payload] = ret;

                try
                {
                    endpoint = btdc.require<std::string>("e");
                    payload = btdc.require<std::string>("p");
                    return ret;
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error{"Exception caught deserializing path control: {}"_format(e.what())};
                }
            }
        }  // namespace CONTROL

    }  // namespace PATH

}  // namespace srouter
