cmake_minimum_required(VERSION 3.21)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(rp_soundboard_ultimate_ts3_plugin VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_COMPILER_PREDEFINES OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)
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
set(RPSU_YT_DLP_EXE "" CACHE FILEPATH "Path to a yt-dlp.exe binary to bundle with the plugin package")
option(RPSU_DOWNLOAD_YT_DLP "Download yt-dlp.exe during configure when no local binary is available" ON)

set(RPSU_QRC_FILES
  assets/assets.qrc
)

set(PLUGIN_RUNTIME_SOURCES
  src/plugin.cpp
  src/core/models.cpp
  src/core/storage.cpp
  src/core/youtube_service.cpp
  src/engine/playback_engine.cpp
  src/engine/upstream/SampleBuffer.cpp
  src/engine/upstream/SampleProducerThread.cpp
  src/engine/upstream/Sampler.cpp
  src/engine/upstream/SoundInfo.cpp
  src/engine/upstream/TalkStateManager.cpp
  src/engine/upstream/inputfileffmpeg.cpp
  src/audio/audio_decoder.cpp
  src/audio/preview_player.cpp
  src/audio/wav_decoder.cpp
  src/audio/mixer.cpp
  src/ui/main_window.cpp
  src/ui/emoji_picker.cpp
  ${RPSU_QRC_FILES}
)

set(HEADERS
  src/plugin.h
  src/plugin_context.h
  src/core/models.h
  src/core/storage.h
  src/core/youtube_service.h
  src/engine/playback_engine.h
  src/engine/upstream/SampleBuffer.h
  src/engine/upstream/SampleProducer.h
  src/engine/upstream/SampleProducerThread.h
  src/engine/upstream/SampleSource.h
  src/engine/upstream/Sampler.h
  src/engine/upstream/SoundInfo.h
  src/engine/upstream/TalkStateManager.h
  src/engine/upstream/inputfile.h
  src/engine/upstream/peakmeter.h
  src/audio/audio_decoder.h
  src/audio/preview_player.h
  src/audio/wav_decoder.h
  src/audio/mixer.h
  src/ui/main_window.h
  src/ui/emoji_picker.h
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
    RPSU_ENABLE_TS3_ROUTING
    QT_NO_KEYWORDS
    NOMINMAX
)

if(MSVC)
  target_link_options(${PLUGIN_NAME} PRIVATE /MANIFEST:NO)
  target_compile_options(${PLUGIN_NAME} PRIVATE /utf-8)
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
    RPSU_ENABLE_TS3_ROUTING
    QT_NO_KEYWORDS
    NOMINMAX
)

if(MSVC)
  target_link_options(${PLUGIN_RUNTIME_NAME} PRIVATE /MANIFEST:NO)
  target_compile_options(${PLUGIN_RUNTIME_NAME} PRIVATE /utf-8)
endif()

set_target_properties(${PLUGIN_RUNTIME_NAME} PROPERTIES
  PREFIX ""
  OUTPUT_NAME "rp_soundboard_ultimate_runtime"
  SUFFIX ".bin"
)

add_executable(rpsu_ui_preview
  src/ui_preview_main.cpp
  ${RPSU_QRC_FILES}
  src/core/models.cpp
  src/core/storage.cpp
  src/core/youtube_service.cpp
  src/engine/playback_engine.cpp
  src/engine/upstream/SampleBuffer.cpp
  src/engine/upstream/SampleProducerThread.cpp
  src/engine/upstream/Sampler.cpp
  src/engine/upstream/SoundInfo.cpp
  src/engine/upstream/TalkStateManager.cpp
  src/engine/upstream/inputfileffmpeg.cpp
  src/audio/audio_decoder.cpp
  src/audio/preview_player.cpp
  src/audio/wav_decoder.cpp
  src/audio/mixer.cpp
  src/ui/main_window.cpp
  src/ui/emoji_picker.cpp
  src/plugin_context.h
  src/core/models.h
  src/core/storage.h
  src/core/youtube_service.h
  src/engine/playback_engine.h
  src/engine/upstream/SampleBuffer.h
  src/engine/upstream/SampleProducer.h
  src/engine/upstream/SampleProducerThread.h
  src/engine/upstream/SampleSource.h
  src/engine/upstream/Sampler.h
  src/engine/upstream/SoundInfo.h
  src/engine/upstream/TalkStateManager.h
  src/engine/upstream/inputfile.h
  src/engine/upstream/peakmeter.h
  src/audio/audio_decoder.h
  src/audio/preview_player.h
  src/audio/wav_decoder.h
  src/audio/mixer.h
  src/ui/main_window.h
  src/ui/emoji_picker.h
)

target_include_directories(rpsu_ui_preview
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pluginsdk/include
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
  target_compile_options(rpsu_ui_preview PRIVATE /utf-8)
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
find_program(RPSU_FFMPEG_EXE ffmpeg.exe)

if(NOT RPSU_YT_DLP_EXE)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/yt-dlp.exe")
    set(RPSU_YT_DLP_EXE "${CMAKE_CURRENT_SOURCE_DIR}/resources/yt-dlp.exe")
  else()
    find_program(RPSU_PATH_YT_DLP_EXE NAMES yt-dlp.exe yt-dlp)
    if(RPSU_PATH_YT_DLP_EXE)
      set(RPSU_YT_DLP_EXE "${RPSU_PATH_YT_DLP_EXE}")
    endif()
  endif()
endif()

if(NOT RPSU_YT_DLP_EXE AND RPSU_DOWNLOAD_YT_DLP)
  set(RPSU_DOWNLOADED_YT_DLP_EXE "${CMAKE_CURRENT_BINARY_DIR}/tools/yt-dlp.exe")
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tools")
  message(STATUS "Downloading yt-dlp.exe for the TeamSpeak plugin package")
  file(DOWNLOAD
    "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"
    "${RPSU_DOWNLOADED_YT_DLP_EXE}"
    STATUS RPSU_YT_DLP_DOWNLOAD_STATUS
    TLS_VERIFY ON
    SHOW_PROGRESS
  )
  list(GET RPSU_YT_DLP_DOWNLOAD_STATUS 0 RPSU_YT_DLP_DOWNLOAD_CODE)
  if(RPSU_YT_DLP_DOWNLOAD_CODE EQUAL 0)
    set(RPSU_YT_DLP_EXE "${RPSU_DOWNLOADED_YT_DLP_EXE}")
  else()
    list(GET RPSU_YT_DLP_DOWNLOAD_STATUS 1 RPSU_YT_DLP_DOWNLOAD_MESSAGE)
    message(WARNING "Could not download yt-dlp.exe: ${RPSU_YT_DLP_DOWNLOAD_MESSAGE}")
  endif()
endif()

if(RPSU_YT_DLP_EXE)
  install(FILES "${RPSU_YT_DLP_EXE}" DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR} RENAME yt-dlp.exe)
else()
  set(RPSU_YT_DLP_MISSING_MESSAGE
    "yt-dlp.exe was not found and could not be downloaded. Set RPSU_YT_DLP_EXE to a local yt-dlp.exe path, place resources/yt-dlp.exe in the source tree, install yt-dlp.exe on PATH, or enable RPSU_DOWNLOAD_YT_DLP."
  )
  if(RPSU_MAKE_PLUGIN_FILE)
    message(FATAL_ERROR "${RPSU_YT_DLP_MISSING_MESSAGE}")
  else()
    message(WARNING "${RPSU_YT_DLP_MISSING_MESSAGE}")
  endif()
endif()

install(FILES
  "${RPSU_COMPILER_BIN_DIR}/libstdc++-6.dll"
  "${RPSU_COMPILER_BIN_DIR}/libgcc_s_seh-1.dll"
  "${RPSU_COMPILER_BIN_DIR}/libwinpthread-1.dll"
  DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}
  OPTIONAL
)
if(RPSU_FFMPEG_EXE)
  install(FILES "${RPSU_FFMPEG_EXE}" DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR} OPTIONAL)
endif()

install(TARGETS rpsu_ui_preview
  RUNTIME DESTINATION ${RPSU_PLUGIN_SUPPORT_SUBDIR}
)

if(RPSU_MAKE_PLUGIN_FILE)
  install(CODE "set(RPSU_PLUGINFILE_OUTPUT_DIR \"${RPSU_PLUGINFILE_OUTPUT_DIR}\")")
  install(CODE "set(RPSU_PLUGIN_PACKAGE_VERSION \"${RPSU_PACKAGE_VERSION}\")")
  install(SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/create-pluginfile.cmake")
endif()
