#pragma once

#include "context.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /// the result of a Session Router stream mapping attempt
    struct session_router_stream_result
    {
        /// set to zero on success otherwise the error that happened
        /// use strerror(3) to get printable string of this error
        int error;

        /// the local ip address we mapped the remote endpoint to
        /// null terminated
        char local_address[256];
        /// the local port we mapped the remote endpoint to
        int local_port;
        /// the id of the stream we created
        int stream_id;
    };

    /// connect out to a remote endpoint
    /// remoteAddr is in the form of "name:port"
    /// localAddr is either NULL for any or in the form of "ip:port" to bind to an explicit address
    void EXPORT session_router_outbound_stream(
        struct session_router_stream_result* result,
        const char* remoteAddr,
        const char* localAddr,
        struct session_router_context* context);

    /// stream accept filter determines if we should accept a stream or not
    /// return 0 to accept
    /// return -1 to explicitly reject
    /// return -2 to silently drop
    typedef int (*session_router_stream_filter)(const char* remote, uint16_t port, void* userdata);

    /// set stream accepter filter
    /// passes user parameter into stream filter as void *
    /// returns stream id
    int EXPORT
    session_router_inbound_stream_filter(session_router_stream_filter acceptFilter, void* user, struct session_router_context* context);

    /// simple stream acceptor
    /// simple variant of session_router_inbound_stream_filter that maps port to localhost:port
    int EXPORT session_router_inbound_stream(uint16_t port, struct session_router_context* context);

    void EXPORT session_router_close_stream(int stream_id, struct session_router_context* context);

#ifdef __cplusplus
}
#endif
