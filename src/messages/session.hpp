#pragma once

#include "address/address.hpp"
#include "auth/auth.hpp"
#include "path/hopid.hpp"
#include "util/logging/buffer.hpp"

namespace srouter
{
    /** Fields for initiating sessions:
        - 'k' : ephemeral pubkey used to derive shared secret
        - 'n' : nonce used for key exchange
        - 'x' : encrypted payload
            - 'i' : RouterID of initiator
            - 'p' : HopID at the pivot taken from local ClientIntro
            - 'r' : HopID at the pivot taken from remote's ClientIntro
            - 'u' : Authentication field
                - bt-encoded dict, values TBD
    */

    /** Fields for switching session paths:
        - 'p' : HopID at the pivot taken from local ClientIntro
        - 'r' : HopID at the pivot taken from remote's ClientIntro
        - 't' : session_tag for current session
    */

}  // namespace srouter
