#include "connection.hpp"

#include "util/logging.hpp"

namespace srouter::link
{
    static auto logcat = log::Cat("link_conn");

    Connection::Connection(std::shared_ptr<quic::Connection> c, std::shared_ptr<quic::BTRequestStream> s)
        : conn{std::move(c)}, datagrams{conn->datagrams()}, control_stream{std::move(s)}
    {}

    void Connection::close_quietly()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        conn->set_close_quietly();
        conn->close_connection();
    }
}  // namespace srouter::link
