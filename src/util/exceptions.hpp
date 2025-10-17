#pragma once

namespace srouter::util
{
    class bind_socket_error : public std::runtime_error
    {
      public:
        using std::runtime_error::runtime_error;
    };
}  // namespace srouter::util
