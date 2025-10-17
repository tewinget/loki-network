#pragma once
#include "context.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /// get a free()-able null terminated string that holds our .loki address
    /// returns NULL if we dont have one right now
    char* EXPORT session_router_address(struct session_router_context*);
#ifdef __cplusplus
}
#endif
