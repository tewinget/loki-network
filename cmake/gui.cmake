
if(WIN32 AND GUI_EXE)
  message(STATUS "using pre-built Session Router gui executable: ${GUI_EXE}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GUI_EXE}" "${PROJECT_BINARY_DIR}/gui/session-router-gui.exe")
elseif(SROUTER_GUI)
  message(STATUS "Building session-router-gui from source")

  set(default_gui_target pack)
  if(APPLE)
    set(default_gui_target macos:raw)
  elseif(WIN32)
    set(default_gui_target win32)
  endif()

  set(GUI_YARN_TARGET "${default_gui_target}" CACHE STRING "yarn target for building the GUI")
  set(GUI_YARN_EXTRA_OPTS "" CACHE STRING "extra options to pass into the yarn build command")

  # allow manually specifying yarn with -DYARN=
  if(NOT YARN)
    find_program(YARN NAMES yarnpkg yarn REQUIRED)
  endif()
  message(STATUS "Building session-router-gui with yarn ${YARN}, target ${GUI_YARN_TARGET}")

  if(NOT WIN32)
    add_custom_target(session-router-gui
      COMMAND ${YARN} install --frozen-lockfile &&
      ${YARN} ${GUI_YARN_EXTRA_OPTS} ${GUI_YARN_TARGET}
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/gui")
  endif()

  if(APPLE)
    add_custom_target(assemble_gui ALL
      DEPENDS assemble session-router-gui
      COMMAND mkdir "${session_router_app}/Contents/Helpers"
      COMMAND cp -a "${PROJECT_SOURCE_DIR}/gui/release/mac/Session-Router-GUI.app" "${session_router_app}/Contents/Helpers/"
      COMMAND mkdir -p "${session_router_app}/Contents/Resources/en.lproj"
      COMMAND cp "${PROJECT_SOURCE_DIR}/contrib/macos/InfoPlist.strings" "${session_router_app}/Contents/Resources/en.lproj/"
      COMMAND cp "${session_router_app}/Contents/Resources/icon.icns" "${session_router_app}/Contents/Helpers/Session-Router-GUI.app/Contents/Resources/icon.icns"
      COMMAND cp "${PROJECT_SOURCE_DIR}/contrib/macos/InfoPlist.strings" "${session_router_app}/Contents/Helpers/Session-Router-GUI.app/Contents/Resources/en.lproj/"
      COMMAND /usr/libexec/PlistBuddy
      -c "Delete :CFBundleDisplayName"
      -c "Add :LSHasLocalizedDisplayName bool true"
      -c "Add :CFBundleDevelopmentRegion string en"
      -c "Set :CFBundleShortVersionString ${session_router_VERSION}"
      -c "Set :CFBundleVersion ${session_router_VERSION}.${SROUTER_APPLE_BUILD}"
      "${session_router_app}/Contents/Helpers/Session-Router-GUI.app/Contents/Info.plist"
    )
  elseif(WIN32)
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/gui")
    add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/gui/session-router-gui.exe"
      COMMAND ${YARN} install --frozen-lockfile &&
      USE_SYSTEM_7ZA=true DISPLAY= WINEDEBUG=-all WINEPREFIX="${PROJECT_BINARY_DIR}/wineprefix" ${YARN} ${GUI_YARN_EXTRA_OPTS} ${GUI_YARN_TARGET}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${PROJECT_SOURCE_DIR}/gui/release/Session-Router-GUI_portable.exe"
      "${PROJECT_BINARY_DIR}/gui/session-router-gui.exe"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/gui")
    add_custom_target(assemble_gui ALL COMMAND "true" DEPENDS "${PROJECT_BINARY_DIR}/gui/session-router-gui.exe")
  else()
    message(FATAL_ERROR "Building/bundling the GUI from this repository is not supported on this platform")
  endif()
else()
  message(STATUS "not building gui")
endif()

if(NOT TARGET assemble_gui)
  add_custom_target(assemble_gui COMMAND "true")
endif()
