set(default_build_gui OFF)
if(SROUTER_DAEMON AND (APPLE OR WIN32))
  set(default_build_gui ON)
endif()

if(WIN32)
  set(GUI_EXE "" CACHE FILEPATH "path to a pre-built Windows GUI .exe to use (implies -DSROUTER_GUI=OFF)")
  if(GUI_EXE)
    set(default_build_gui OFF)
  endif()
endif()

option(SROUTER_GUI "build electron gui from 'gui' submodule source" ${default_build_gui})

if(SROUTER_GUI AND GUI_EXE)
  message(FATAL_ERROR "-DGUI_EXE=... and -DSROUTER_GUI=ON are mutually exclusive")
endif()
