#pragma once

#include "export.h"

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct session_router_context;

    /// allocate a new Session Router context
    struct session_router_context* EXPORT session_router_context_new();

    /// free a context allocated by session_router_context_new
    void EXPORT session_router_context_free(struct session_router_context*);

    /// spawn all the threads needed for operation and start running
    /// return 0 on success
    /// return non zero on fail
    int EXPORT session_router_context_start(struct session_router_context*);

    /// return 0 if we our endpoint has published on the network and is ready to send
    /// return -1 if we don't have enough paths ready
    /// retrun -2 if we look deadlocked
    /// retrun -3 if context was null or not started yet
    int EXPORT session_router_status(struct session_router_context*);

    /// wait at most N milliseconds for Session Router to build paths and get ready
    /// return 0 if we are ready
    /// return nonzero if we are not ready
    int EXPORT session_router_wait_for_ready(int N, struct session_router_context*);

    /// stop all operations on this Session Router context
    void EXPORT session_router_context_stop(struct session_router_context*);

    /// load a bootstrap RC from memory
    /// return 0 on success
    /// return non zero on fail
    int EXPORT session_router_add_bootstrap_rc(const char*, size_t, struct session_router_context*);

#ifdef __cplusplus
}
#endif
