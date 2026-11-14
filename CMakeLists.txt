cmake_minimum_required(VERSION 3.20)
project(TextGme LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TEXTGME_SANITIZE "Build with address and undefined-behaviour sanitizers" OFF)

add_library(textgme_core STATIC
  src/version.cpp
  src/term/terminal.cpp
  src/term/input.cpp
  src/gfx/ascii.cpp
  src/gfx/present.cpp
  src/world/map.cpp
)
target_include_directories(textgme_core PUBLIC src)
target_compile_options(textgme_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow)
if(TEXTGME_SANITIZE)
  target_compile_options(textgme_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(textgme_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(textgme src/main.cpp)
target_link_libraries(textgme PRIVATE textgme_core)
target_compile_definitions(textgme PRIVATE TEXTGME_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

enable_testing()
add_executable(textgme_tests tests/main.cpp tests/input_test.cpp tests/ascii_test.cpp tests/map_test.cpp)
target_link_libraries(textgme_tests PRIVATE textgme_core)
target_compile_definitions(textgme_tests PRIVATE TEXTGME_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
add_test(NAME textgme_tests COMMAND textgme_tests)
