set(TOOLING_SRC
  router_event.cpp
  router_hive.cpp
  hive_config.cpp
  hive_main.cpp
)

set(TOOLING_EXE lokinet_tooling_hive)


add_executable(${TOOLING_EXE} ${TOOLING_SRC})
target_include_directories(${TOOLING_EXE} PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(${TOOLING_EXE} ${STATIC_LIB} ${LIBS})