#include "types.hpp"

#include "util/logging.hpp"

#include <sodium/randombytes.h>

namespace srouter
{
    static auto logcat = log::Cat("cryptoutils");

    SymmNonce SymmNonce::make_random()
    {
        SymmNonce n;
        randombytes_buf(n.data(), n.size());
        return n;
    }

}  // namespace srouter
