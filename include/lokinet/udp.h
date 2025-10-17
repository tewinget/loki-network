#pragma once

#include "context.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /// information about a udp flow
    struct session_router_udp_flowinfo
    {
        /// remote endpoint's .loki or .snode address
        char remote_host[256];
        /// remote endpont's port
        uint16_t remote_port;
        /// the socket id for this flow used for i/o purposes and closing this socket
        int socket_id;
    };

    /// a result from a session_router_udp_bind call
    struct session_router_udp_bind_result
    {
        /// a socket id used to close a Session Router udp socket
        int socket_id;
    };

    /// flow acceptor hook, return 0 success, return nonzero with errno on failure
    typedef int (*session_router_udp_flow_filter)(
        void* userdata, const struct session_router_udp_flowinfo* remote_address, void** flow_userdata, int* timeout_seconds);

    /// callback to make a new outbound flow
    typedef void(session_router_udp_create_flow_func)(void* userdata, void** flow_userdata, int* timeout_seconds);

    /// hook function for handling packets
    typedef void (*session_router_udp_flow_recv_func)(
        const struct session_router_udp_flowinfo* remote_address,
        const char* pkt_data,
        size_t pkt_length,
        void* flow_userdata);

    /// hook function for flow timeout
    typedef void (*session_router_udp_flow_timeout_func)(
        const struct session_router_udp_flowinfo* remote_address, void* flow_userdata);

    /// inbound listen udp socket
    /// expose udp port exposePort to the void
    ////
    /// @param filter MUST be non null, pointing to a flow filter for accepting new udp flows,
    /// called with user data
    ///
    /// @param recv MUST be non null, pointing to a packet handler function for each flow, called
    /// with per flow user data provided by filter function if accepted
    ///
    /// @param timeout MUST be non null,
    /// pointing to a cleanup function to clean up a stale flow, staleness determined by the value
    /// given by the filter function returns 0 on success
    ///
    /// @returns nonzero on error in which it is an errno value
    int EXPORT session_router_udp_bind(
        uint16_t exposedPort,
        session_router_udp_flow_filter filter,
        session_router_udp_flow_recv_func recv,
        session_router_udp_flow_timeout_func timeout,
        void* user,
        struct session_router_udp_bind_result* result,
        struct session_router_context* ctx);

    /// @brief establish a udp flow to remote endpoint
    ///
    /// @param create_flow the callback to create the new flow if we establish one
    ///
    /// @param user passed to new_flow as user data
    ///
    /// @param remote the remote address to establish to
    ///
    /// @param ctx the Session Router context to use
    ///
    /// @return 0 on success, non zero errno on fail
    int EXPORT session_router_udp_establish(
        session_router_udp_create_flow_func create_flow,
        void* user,
        const struct session_router_udp_flowinfo* remote,
        struct session_router_context* ctx);

    /// @brief send on an established flow to remote endpoint
    /// blocks until we have sent the packet
    ///
    /// @param flowinfo remote flow to use for sending
    ///
    /// @param ptr pointer to data to send
    ///
    /// @param len the length of the data
    ///
    /// @param ctx the Session Router context to use
    ///
    /// @returns 0 on success and non zero errno on fail
    int EXPORT session_router_udp_flow_send(
        const struct session_router_udp_flowinfo* remote, const void* ptr, size_t len, struct session_router_context* ctx);

    /// @brief close a bound udp socket
    /// closes all flows immediately
    ///
    /// @param socket_id the bound udp socket's id
    ///
    /// @param ctx Session Router context
    void EXPORT session_router_udp_close(int socket_id, struct session_router_context* ctx);

#ifdef __cplusplus
}
#endif
