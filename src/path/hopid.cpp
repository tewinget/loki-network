#include "hopid.hpp"

#include <sodium/randombytes.h>

namespace srouter
{
    HopID HopID::make_random()
    {
        HopID h;
        randombytes_buf(h.data(), h.size());
        return h;
    }
}  // namespace srouter
