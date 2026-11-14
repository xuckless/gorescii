cmake_minimum_required(VERSION 3.20)
project(Gorescii LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(GORESCII_SANITIZE "Build with address and undefined-behaviour sanitizers" OFF)

add_library(gorescii_core STATIC
  src/version.cpp
  src/term/terminal.cpp
  src/term/input.cpp
  src/gfx/ascii.cpp
  src/gfx/present.cpp
  src/world/map.cpp
  src/world/trace.cpp
  src/render/textures.cpp
  src/render/renderer.cpp
  src/render/sprites.cpp
  src/game/player.cpp
  src/game/game.cpp
  src/ui/hud.cpp
  src/audio/synth.cpp
  src/audio/player.cpp
)
target_include_directories(gorescii_core PUBLIC src)
target_compile_options(gorescii_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow)
if(GORESCII_SANITIZE)
  target_compile_options(gorescii_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(gorescii_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(gorescii src/main.cpp)
target_link_libraries(gorescii PRIVATE gorescii_core)
target_compile_definitions(gorescii PRIVATE GORESCII_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

enable_testing()
add_executable(gorescii_tests tests/main.cpp tests/input_test.cpp tests/ascii_test.cpp tests/map_test.cpp tests/player_test.cpp tests/game_test.cpp tests/audio_test.cpp)
target_link_libraries(gorescii_tests PRIVATE gorescii_core)
target_compile_definitions(gorescii_tests PRIVATE GORESCII_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
add_test(NAME gorescii_tests COMMAND gorescii_tests)
