cmake_minimum_required(VERSION 3.21)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(rp_soundboard_ultimate_ts3_plugin VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT WIN32)
  message(FATAL_ERROR "RP Soundboard Ultimate TS3 plugin currently supports Windows only.")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

set(PLUGIN_NAME "rp_soundboard_ultimate")
set(PLUGIN_RUNTIME_NAME "rp_soundboard_ultimate_runtime")
set(RPSU_PLUGIN_SUPPORT_SUBDIR "plugins/rp_soundboard_ultimate")
set(RPSU_PLUGINFILE_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/release" CACHE PATH "Output directory for the final ts3_plugin package")
option(RPSU_MAKE_PLUGIN_FILE "Create the ts3_plugin file" OFF)

set(PLUGIN_RUNTIME_SOURCES
  src/plugin.cpp
  src/core/models.cpp
  src/core/storage.cpp
  src/core/youtube_service.cpp
  src/audio/preview_player.cpp
  src/audio/wav_decoder.cpp
  src/audio/mixer.cpp
  src/ui/main_window.cpp
)

set(HEADERS
  src/plugin.h
  src/plugin_context.h
  src/core/models.h
  src/core/storage.h
  src/core/youtube_service.h
  src/audio/preview_player.h
  src/audio/wav_decoder.h
  src/audio/mixer.h
  src/ui/main_window.h
)

add_library(${PLUGIN_NAME} SHARED
  src/plugin_loader.c
)

target_include_directories(${PLUGIN_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pluginsdk/include
)

target_compile_definitions(${PLUGIN_NAME}
  PRIVATE
    QT_NO_KEYWORDS
    NOMINMAX
)

if(MSVC)
  target_link_options(${PLUGIN_NAME} PRIVATE /MANIFEST:NO)
else()
  target_link_options(${PLUGIN_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

set_target_properties(${PLUGIN_NAME} PROPERTIES
  AUTOMOC OFF
  PREFIX ""
  OUTPUT_NAME "rp_soundboard_ultimate_win64"
  SUFFIX ".dll"
)

add_library(${PLUGIN_RUNTIME_NAME} SHARED ${PLUGIN_RUNTIME_SOURCES} ${HEADERS})

target_include_directories(${PLUGIN_RUNTIME_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pluginsdk/include
)

target_link_libraries(${PLUGIN_RUNTIME_NAME}
  PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    winmm
)

target_compile_definitions(${PLUGIN_RUNTIME_NAME}
  PRIVATE
    QT_NO_KEYWORDS
    NOMINMAX
)

if(MSVC)
  target_link_options(${PLUGIN_RUNTIME_NAME} PRIVATE /MANIFEST:NO)
endif()

set_target_properties(${PLUGIN_RUNTIME_NAME} PROPERTIES
  PREFIX ""
  OUTPUT_NAME "rp_soundboard_ultimate_runtime"
  SUFFIX ".bin"
)

add_executable(rpsu_ui_preview
  src/ui_preview_main.cpp
  src/core/models.cpp
  src/core/storage.cpp
  src/core/youtube_service.cpp
  src/audio/preview_player.cpp
  src/audio/wav_decoder.cpp
  src/audio/mixer.cpp
  src/ui/main_window.cpp
  src/plugin_context.h
  src/core/models.h
  src/core/storage.h
  src/core/youtube_service.h
  src/audio/preview_player.h
  src/audio/wav_decoder.h
  src/audio/mixer.h
  src/ui/main_window.h
)

target_include_directories(rpsu_ui_preview
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rpsu_ui_preview
  PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    winmm
)

target_compile_definitions(rpsu_ui_preview
  PRIVATE
    QT_NO_KEYWORDS
    NOMINMAX
)

if(MSVC)
  target_link_options(rpsu_ui_preview PRIVATE /MANIFEST:NO)
endif()

set(RPSU_PACKAGE_VERSION "${PROJECT_VERSION}")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE RPSU_GIT_VERSION_RAW
    ERROR_QUIET
    RESULT_VARIABLE RPSU_GIT_DESCRIBE_RESULT
  )
  if(RPSU_GIT_DESCRIBE_RESULT EQUAL 0)
    string(STRIP "${RPSU_GIT_VERSION_RAW}" RPSU_PACKAGE_VERSION)
  endif()
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/deploy")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/package.ini.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/deploy/package.ini"
  @ONLY
)

install(TARGETS ${PLUGIN_NAME}
  RUNTIME DESTINATION plugins
  LIBRARY DESTINATION plugins
)

install(TARGETS ${PLUGIN_RUNTIME_NAME}
  RUNTIME DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}
  LIBRARY DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/deploy/package.ini" DESTINATION .)

if(TARGET Qt${QT_VERSION_MAJOR}::qmake)
  get_target_property(RPSU_QMAKE_EXE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
  get_filename_component(RPSU_QT_BIN_DIR "${RPSU_QMAKE_EXE}" DIRECTORY)
  get_filename_component(RPSU_QT_ROOT_DIR "${RPSU_QT_BIN_DIR}/.." ABSOLUTE)

  if(QT_VERSION_MAJOR EQUAL 5)
    install(FILES
      "${RPSU_QT_BIN_DIR}/Qt5Core.dll"
      "${RPSU_QT_BIN_DIR}/Qt5Gui.dll"
      "${RPSU_QT_BIN_DIR}/Qt5Network.dll"
      "${RPSU_QT_BIN_DIR}/Qt5Widgets.dll"
      DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}
      OPTIONAL
    )
  else()
    install(FILES
      "${RPSU_QT_BIN_DIR}/Qt6Core.dll"
      "${RPSU_QT_BIN_DIR}/Qt6Gui.dll"
      "${RPSU_QT_BIN_DIR}/Qt6Network.dll"
      "${RPSU_QT_BIN_DIR}/Qt6Widgets.dll"
      DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}
      OPTIONAL
    )
  endif()

  install(FILES
    "${RPSU_QT_ROOT_DIR}/plugins/platforms/qwindows.dll"
    DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}/platforms
    OPTIONAL
  )
endif()

get_filename_component(RPSU_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/yt-dlp.exe"
  "${RPSU_COMPILER_BIN_DIR}/libstdc++-6.dll"
  "${RPSU_COMPILER_BIN_DIR}/libgcc_s_seh-1.dll"
  "${RPSU_COMPILER_BIN_DIR}/libwinpthread-1.dll"
  DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}
  OPTIONAL
)

install(TARGETS rpsu_ui_preview
  RUNTIME DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}
)

if(RPSU_MAKE_PLUGIN_FILE)
  install(CODE "set(RPSU_PLUGINFILE_OUTPUT_DIR \"${RPSU_PLUGINFILE_OUTPUT_DIR}\")")
  install(CODE "set(RPSU_PLUGIN_PACKAGE_VERSION \"${RPSU_PACKAGE_VERSION}\")")
  install(SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/create-pluginfile.cmake")
endif()
