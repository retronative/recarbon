# ReCarbon (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
#
#   cmake -S . -B build/main -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
#         -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
#   ninja -C build/main ReCarbon
#
# Needs ppc/ (XenonRecomp output from your own default.xex, scripts/survey.sh)
# and tools/ (scripts/setup_tools.sh, including tools/ffmpeg-xma), plus
# Homebrew's sdl3, vulkan-loader, vulkan-headers, glslang, spirv-tools and
# molten-vk. -DRECARBON_TESTS=ON adds the unit tests (tests/CMakeLists.txt:
# a first set ported from SpeedBreaker, plus ReCarbon's own; run with ctest).
cmake_minimum_required(VERSION 3.20)

# macOS deployment target. ReCarbon change: for now the game links Homebrew's
# libraries, whose bottles are built for the macOS they were made on (27.0 on
# macOS 27), so the default is this Mac's macOS: MACOSX_DEPLOYMENT_TARGET
# when set, else <host major version>.0. scripts/setup_tools.sh builds
# tools/ffmpeg-xma for the same default. The result runs only on that macOS
# or newer. -DCMAKE_OSX_DEPLOYMENT_TARGET=<version> chooses another; a build
# folder keeps the target it was first configured with.
# Packaging: scripts/macos/make_app.py (from SpeedBreaker's) bundles this
# build as is. SpeedBreaker's release targets macOS 15.0 with the libraries
# built from source (its scripts/macos/build_deps.sh, not ported yet, passed
# with -DCMAKE_PREFIX_PATH=<prefix> -DFFMPEG_XMA=<prefix>/ffmpeg-xma). The
# target sets only which macOS APIs and libc++ features the code may use, not
# the CPU code. Before project(): it picks the SDK's settings.
if(CMAKE_HOST_APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    if(DEFINED ENV{MACOSX_DEPLOYMENT_TARGET} AND NOT "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
        set(_recarbon_osx_target "$ENV{MACOSX_DEPLOYMENT_TARGET}")
    else()
        execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE _recarbon_host_macos
            OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
        string(REGEX MATCH "^[0-9]+" _recarbon_host_major "${_recarbon_host_macos}")
        set(_recarbon_osx_target "${_recarbon_host_major}.0")
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${_recarbon_osx_target}" CACHE STRING
        "The oldest macOS the game runs on (default: this Mac's, for Homebrew's libraries)")
    message(STATUS "macOS deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET} (this Mac's, for Homebrew's libraries)")
endif()

# The version: the one place it is set. Everything else comes from it: the
# log's [build] line, Settings > Advanced, bug reports' names and, once there
# are releases, the release tag (v<version>). CHANGELOG.md has an entry for
# each version (the in-game "What's new" note is built from it).
project(ReCarbon VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Binaries name their sources from the repository root (assert messages,
# debug information), never by the folders they were built in: a published
# build carries no one's home folder (a build folder inside the checkout
# reads ./build/...). The real path too, for a checkout reached through a
# symlink; both map to the same name, so their order doesn't matter.
file(REAL_PATH "${CMAKE_SOURCE_DIR}" RECARBON_SOURCE_REAL)
add_compile_options("-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
if(NOT RECARBON_SOURCE_REAL STREQUAL CMAKE_SOURCE_DIR)
    add_compile_options("-ffile-prefix-map=${RECARBON_SOURCE_REAL}=.")
endif()

# macOS: a call to an API newer than the deployment target is an error unless
# it's checked for (@available / __builtin_available), since it would crash
# on an older macOS the app says it runs on. A diagnostic only: the code
# generated is the same.
if(APPLE AND NOT IOS)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Werror=unguarded-availability-new>")
endif()

add_subdirectory(recomp)
add_subdirectory(runtime)

option(RECARBON_TESTS "Build the unit tests (tests/)" OFF)
if(RECARBON_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
