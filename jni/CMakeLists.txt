add_library(session-router-android
    SHARED
    sessionrouter_config.cpp
    sessionrouter_daemon.cpp)
target_link_libraries(session-router-android session-router-amalgum)
