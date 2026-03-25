cmake_minimum_required(VERSION 3.21)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(rp_soundboard_ultimate_ts3_plugin VERSION 0.2.0 LANGUAGES CXX)

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

set(SOURCES
  src/plugin.cpp
  src/core/models.cpp
  src/core/storage.cpp
  src/audio/wav_decoder.cpp
  src/audio/mixer.cpp
  src/ui/main_window.cpp
)

set(HEADERS
  src/plugin.h
  src/plugin_context.h
  src/core/models.h
  src/core/storage.h
  src/audio/wav_decoder.h
  src/audio/mixer.h
  src/ui/main_window.h
)

add_library(${PLUGIN_NAME} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${PLUGIN_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pluginsdk/include
)

target_link_libraries(${PLUGIN_NAME}
  PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

target_compile_definitions(${PLUGIN_NAME}
  PRIVATE
    QT_NO_KEYWORDS
    NOMINMAX
)

if(MSVC)
  target_link_options(${PLUGIN_NAME} PRIVATE /MANIFEST:NO)
endif()

set_target_properties(${PLUGIN_NAME} PROPERTIES
  PREFIX ""
  OUTPUT_NAME "rp_soundboard_ultimate_win64"
  SUFFIX ".dll"
)

add_executable(rpsu_ui_preview
  src/ui_preview_main.cpp
  src/core/models.cpp
  src/core/storage.cpp
  src/audio/wav_decoder.cpp
  src/audio/mixer.cpp
  src/ui/main_window.cpp
  src/plugin_context.h
  src/core/models.h
  src/core/storage.h
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
)

target_compile_definitions(rpsu_ui_preview
  PRIVATE
    QT_NO_KEYWORDS
    NOMINMAX
)

if(MSVC)
  target_link_options(rpsu_ui_preview PRIVATE /MANIFEST:NO)
endif()
