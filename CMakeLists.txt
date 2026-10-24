cmake_minimum_required(VERSION 3.28...3.30)

# Adapted for the official obsproject/obs-plugintemplate build environment.
# Keep the template's cmake/, .github/actions and .github/scripts files.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
project(${_name} VERSION ${_version} LANGUAGES C CXX)

include(compilerconfig)
include(defaults)
include(helpers)

if(NOT WIN32)
  message(FATAL_ERROR "This Stream Notify test branch supports Windows x64 only")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Requires Windows x64")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Qt6WebSockets QUIET)
if(NOT Qt6WebSockets_FOUND)
  # obs-deps omits WebSockets. Build the matching Qt module, never replace OBS Qt.
  include(FetchContent)
  FetchContent_Declare(qtwebsockets
    GIT_REPOSITORY https://github.com/qt/qtwebsockets.git
    GIT_TAG 451920600d7f0b8a4b458bba56a2dd303e587026
    SOURCE_SUBDIR unused)
  FetchContent_MakeAvailable(qtwebsockets)
  set(_ws_prefix "${CMAKE_BINARY_DIR}/qtwebsockets-install")
  execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${qtwebsockets_SOURCE_DIR}" -B "${CMAKE_BINARY_DIR}/qtwebsockets-build"
    -G "${CMAKE_GENERATOR}" -A "${CMAKE_GENERATOR_PLATFORM}"
    "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
    "-DCMAKE_INSTALL_PREFIX=${_ws_prefix}"
    -DBUILD_WITH_PCH=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF
    COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/qtwebsockets-build"
    --config Release --parallel COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}/qtwebsockets-build"
    --config Release COMMAND_ERROR_IS_FATAL ANY)
  list(PREPEND CMAKE_PREFIX_PATH "${_ws_prefix}")
  set(Qt6WebSockets_DIR "${_ws_prefix}/lib/cmake/Qt6WebSockets" CACHE PATH "" FORCE)
endif()
find_package(Qt6WebSockets 6.11.1 EXACT REQUIRED)

add_library(${CMAKE_PROJECT_NAME} MODULE
  src/plugin.cpp
  src/settings.cpp
  src/net.cpp
  src/dock.cpp
  src/donations.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  OBS::libobs OBS::obs-frontend-api
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets
  crypt32
)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  QT_NO_KEYWORDS WIN32_LEAN_AND_MEAN NOMINMAX
)
if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /utf-8 /permissive-)
endif()

# The upstream helper is responsible for the OBS plugin target conventions.
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

install(FILES "$<TARGET_FILE:Qt6::WebSockets>" DESTINATION "${CMAKE_PROJECT_NAME}/bin/64bit")

enable_testing()
add_executable(stream-notify-logic-tests tests/test_logic.cpp)
if(MSVC)
  target_compile_options(stream-notify-logic-tests PRIVATE /UNDEBUG /utf-8)
endif()
add_test(NAME donation-thresholds-and-currency COMMAND stream-notify-logic-tests)

add_executable(stream-notify-telegram-tests tests/test_telegram_channel.cpp)
target_link_libraries(stream-notify-telegram-tests PRIVATE Qt6::Core)
add_test(NAME telegram-channel-links COMMAND stream-notify-telegram-tests)
set_tests_properties(telegram-channel-links PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

# Render and exercise the actual Qt dock without OBS host state or service tokens.
add_executable(stream-notify-ui-preview
  tests/ui_preview.cpp
  src/dock.cpp src/dock.hpp src/settings.cpp src/settings.hpp
  src/net.cpp src/net.hpp src/donations.cpp src/donations.hpp)
get_target_property(_obs_includes OBS::libobs INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_frontend_includes OBS::obs-frontend-api INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(stream-notify-ui-preview PRIVATE src ${_obs_includes} ${_frontend_includes})
target_link_libraries(stream-notify-ui-preview PRIVATE
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets crypt32 SIMDe::SIMDe)
target_compile_definitions(stream-notify-ui-preview PRIVATE QT_NO_KEYWORDS WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(stream-notify-ui-preview PRIVATE /utf-8 "/FI${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_glue.hpp")
