include(Version)

# Add an internal Session Router static library target, enables LTO (if enabled) on the target,
# and links it to the common session-router-base interface.
# Invoke with the target/library name (e.g. "session-router-foo") and list of source files, e.g.
#     session_router_add_library(session-router-foo foo/source1.cpp foo/source2.cpp)
function(session_router_add_library libname)
    add_library(${libname} STATIC ${ARGN})
    target_link_libraries(${libname} PUBLIC session-router-base PRIVATE session-router-base-internal)
    enable_lto(${libname})
endfunction()

session_router_add_library(session-router-cryptography
  crypto/crypto.cpp
  crypto/key_manager.cpp
  crypto/keys.cpp
  crypto/types.cpp
)

if(SROUTER_FULL)
  session_router_add_library(session-router-exit-auth
    auth/auth.cpp
    auth/file.cpp
    auth/session.cpp
  )
endif()

# Functional objects use by session-router-core and other libraries
# needed by vpn/ router/ rpc/ handlers/ net/ link/
session_router_add_library(session-router-core-utils
  handlers/session.cpp

  messages/common.cpp
  messages/dht.cpp
  messages/fetch.cpp
  messages/path.cpp
)

if (SROUTER_FULL)
  target_sources(session-router-core-utils PRIVATE handlers/tun.cpp vpn/egres_packet_router.cpp)
endif()

session_router_add_library(session-router-core
  context.cpp
  
  link/connection.cpp
  link/endpoint.cpp
  link/link_manager.cpp

  router/router.cpp

  session/session.cpp
)

if (SROUTER_FULL)
  target_sources(session-router-core
    PRIVATE
    router/route_poker.cpp
    consensus/reachability_testing.cpp
  )
endif()

if (SROUTER_FULL)
  session_router_add_library(session-router-rpc
    rpc/json_binary_proxy.cpp
    rpc/json_conversions.cpp
    rpc/oxend_rpc.cpp
    rpc/rpc_request_parser.cpp
    rpc/rpc_server.cpp
  )
endif()

# config, crypto, timeplace, nodedb, net, router
session_router_add_library(session-router-utils
  ${CMAKE_CURRENT_BINARY_DIR}/constants/version.cpp
  util/buffer.cpp
  util/file.cpp
  util/logging.cpp
  util/mem.cpp
  util/str.cpp
  util/thread/queue_manager.cpp  
  util/thread/threading.cpp
  util/time.cpp
  util/zstd.cpp
)

add_dependencies(session-router-utils genversion)

session_router_add_library(session-router-contact
  contact/client_contact.cpp
  contact/client_intro.cpp
  contact/contactdb.cpp
  contact/relay_contact.cpp
  contact/router_id.cpp
  contact/sns.cpp
)

# Network interface files used for mediating ip traffic
session_router_add_library(session-router-ip
  net/ip_packet.cpp
  net/policy.cpp
  net/utils.cpp
)

# Addressing and event loop files used by session-router-core and other libraries
# needed by rpc/ link/ config/ path/ dht/
session_router_add_library(session-router-addressing
  address/address.cpp
  address/ip_range.cpp
  address/utils.cpp

  ev/fd_poller.cpp
  ev/tcp.cpp
)

session_router_add_library(session-router-dns
  dns/srv_data.cpp
)

if (SROUTER_FULL)
  # session-router-platform holds all platform specific code
  session_router_add_library(session-router-platform
    vpn/packet_router.cpp
    vpn/platform.cpp
  )

  if (ANDROID)
    target_sources(session-router-platform PRIVATE android/ifaddrs.c util/nop_service_manager.cpp)
  endif()

  if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_sources(session-router-platform PRIVATE linux/dbus.cpp)
    if(WITH_SYSTEMD)
      target_sources(session-router-platform PRIVATE linux/sd_service_manager.cpp)
    else()
      target_sources(session-router-platform PRIVATE util/nop_service_manager.cpp)
    endif()
  endif()

  if (WIN32)
    target_sources(session-router-platform PRIVATE
      net/win32.cpp
      vpn/win32.cpp
      win32/service_manager.cpp
      win32/exec.cpp
      win32/dll.cpp
      win32/exception.cpp
      win32/wintun.cpp
      win32/windivert.cpp)
    target_include_directories(session-router-platform PRIVATE ${CMAKE_BINARY_DIR}/wintun/include/ ${CMAKE_BINARY_DIR}/WinDivert-${WINDIVERT_VERSION}/include/)
  else()
    target_sources(session-router-platform PRIVATE
      net/posix.cpp)
  endif()

  if(APPLE)
    add_subdirectory(apple)
    target_sources(session-router-platform PRIVATE util/nop_service_manager.cpp)
  endif()

  # session-router-dns is the dns parsing and hooking library that we use to
  # parse modify and reconstitute dns wire proto, dns queries and RR
  target_sources(session-router-dns PRIVATE
    dns/message.cpp   # dns/server
    dns/name.cpp      # srv_data, question, rr
    dns/platform.cpp
    dns/question.cpp  # message
    dns/rr.cpp
    dns/serialize.cpp
    dns/server.cpp
  )

  # platform specific bits and bobs for setting dns
  if(WITH_SYSTEMD)
    target_sources(session-router-dns PRIVATE dns/nm_platform.cpp dns/sd_platform.cpp)
  endif()
endif()

# session-router-nodedb holds all types and logic for storing parsing and constructing
# nodedb data published to the network and versions of it stored locally
session_router_add_library(session-router-nodedb
  nodedb.cpp
  profiling.cpp # path, router, service::endpoint
)

set(BOOTSTRAP_FALLBACKS)
foreach(bs IN ITEMS MAINNET TESTNET)
  if(SROUTER_BOOTSTRAP_FALLBACK_${bs})
    message(STATUS "Building with ${bs} fallback bootstrap path \"${SROUTER_BOOTSTRAP_FALLBACK_${bs}}\"")
    file(READ "${SROUTER_BOOTSTRAP_FALLBACK_${bs}}" bs_data HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "\\\\x\\1" bs_data "${bs_data}")
    set(BOOTSTRAP_FALLBACKS "${BOOTSTRAP_FALLBACKS}        {NetID::${bs}, \"${bs_data}\"sv},\n")
  endif()
endforeach()
configure_file("nodedb-bootstraps.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/nodedb-bootstraps.cpp" @ONLY)
target_sources(session-router-nodedb PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/nodedb-bootstraps.cpp")

# session-router-config is for all configuration types and parsers
session_router_add_library(session-router-config
  config/config.cpp
  config/definition.cpp
  config/ini.cpp
)

# All path objects; link directly to session-router-core
session_router_add_library(session-router-path
  path/build_stats.cpp
  path/hopid.cpp
  path/path.cpp
  path/path_context.cpp
  path/path_handler.cpp
  path/transit_hop.cpp
)

if(SROUTER_DEBUG_PATH_SEED)
  target_compile_definitions(session-router-nodedb PRIVATE SROUTER_DEBUG_PATH_SEED)
  target_compile_definitions(session-router-config PRIVATE SROUTER_DEBUG_PATH_SEED)
  target_compile_definitions(session-router-path PRIVATE SROUTER_DEBUG_PATH_SEED)
endif()

# Link libraries to their internals
target_link_libraries(session-router-nodedb PUBLIC session-router-addressing session-router-cryptography)
target_link_libraries(session-router-contact PUBLIC session-router-dns)
if(SROUTER_FULL)
  target_link_libraries(session-router-rpc PUBLIC session-router-contact session-router-config)
endif()
target_link_libraries(session-router-addressing PUBLIC session-router-utils session-router-cryptography session-router-contact session-router-ip)
target_link_libraries(session-router-config PUBLIC session-router-cryptography session-router-addressing)
if(SROUTER_FULL)
  target_link_libraries(session-router-platform PUBLIC session-router-cryptography session-router-ip)
  target_link_libraries(session-router-config PUBLIC session-router-exit-auth)
endif()
target_link_libraries(session-router-dns
  PUBLIC
  session-router-cryptography
  session-router-config
)
if(SROUTER_FULL)
  target_link_libraries(session-router-dns PUBLIC session-router-platform)
endif()

target_link_libraries(session-router-path
  PUBLIC
  session-router-addressing
)

target_link_libraries(session-router-core-utils 
  PUBLIC 
  session-router-config
  session-router-dns
)

if(SROUTER_FULL)
  target_link_libraries(session-router-core-utils
    PUBLIC
    session-router-rpc
    session-router-platform
  )
endif()

target_link_libraries(session-router-cryptography
  PUBLIC
  session-router-libcrypt
)

if(SROUTER_FULL)
  target_link_libraries(session-router-exit-auth PUBLIC session-router-core-utils session-router-cryptography)
endif()

target_link_libraries(session-router-utils
  PUBLIC
  session-router-cryptography
)

target_link_libraries(session-router-core 
  PUBLIC 
  session-router-core-utils
  session-router-nodedb
  session-router-path
)
if(SROUTER_FULL)
  target_link_libraries(session-router-core PUBLIC session-router-exit-auth)
endif()


target_link_libraries(session-router-base 
  INTERFACE 

  Threads::Threads

  oxenc::oxenc 
  oxen::logging 
  
  sodium
)


if(SROUTER_FULL)
  target_link_libraries(session-router-base INTERFACE libunbound oxenmq::oxenmq)
endif()

# libevent
if(NOT TARGET libevent::core)
  add_library(libevent_core INTERFACE)
  pkg_check_modules(LIBEVENT_core libevent_core>=2.1 IMPORTED_TARGET REQUIRED)
  target_link_libraries(libevent_core INTERFACE PkgConfig::LIBEVENT_core)
  add_library(libevent::core ALIAS libevent_core)
endif()
target_link_libraries(session-router-addressing PRIVATE libevent::core)


# libzstd
if(TARGET libzstd::static)  # static dep or libsession-util's embedded static target
    target_link_libraries(session-router-utils PRIVATE libzstd::static)
else()
    pkg_check_modules(LIBZSTD libzstd>=1.3 IMPORTED_TARGET REQUIRED)
    target_link_libraries(session-router-utils PRIVATE PkgConfig::LIBZSTD)
endif()


add_library(libsessionrouter session_router.cpp)
target_link_libraries(libsessionrouter PUBLIC session-router-core)

set_target_properties(libsessionrouter PROPERTIES OUTPUT_NAME session-router)
if(BUILD_SHARED_LIBS AND SROUTER_VERSION_SO)
  set_target_properties(libsessionrouter PROPERTIES
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
endif()

if(WIN32)
  target_link_libraries(libsessionrouter PUBLIC ws2_32 iphlpapi -fstack-protector)
  install(TARGETS libsessionrouter DESTINATION bin COMPONENT libsessionrouter)
elseif(NOT APPLE)
  include(GNUInstallDirs)
  install(TARGETS libsessionrouter LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libsessionrouter)
endif()

add_library(session-router::libsessionrouter ALIAS libsessionrouter)

file(GLOB_RECURSE docs_SRC */*.hpp *.hpp)
set(DOCS_SRC ${docs_SRC} PARENT_SCOPE)
