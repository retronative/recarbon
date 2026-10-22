# ReCarbon (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
#
# Compiles XenonRecomp's output (../ppc, generated from the user's own XEX and
# never committed) into a static library. Regenerate ppc/ with
# scripts/survey.sh, or by hand:
#   tools/XenonRecomp/build/XenonRecomp/XenonRecomp config/recarbon.toml tools/XenonRecomp/XenonUtils/ppc_context.h
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
# Built from the top-level CMakeLists.txt.
file(GLOB PPC_SOURCES ${ROOT}/ppc/*.cpp)
if(NOT PPC_SOURCES)
    message(FATAL_ERROR "No generated sources in ppc/. Run scripts/survey.sh (XenonRecomp) first.")
endif()

add_library(ReCarbonRecompLib STATIC ${PPC_SOURCES})
target_compile_features(ReCarbonRecompLib PUBLIC cxx_std_20)
target_include_directories(ReCarbonRecompLib PUBLIC
    ${ROOT}/ppc
    ${ROOT}/tools/XenonRecomp/XenonUtils
    ${ROOT}/tools/XenonRecomp/thirdparty/simde)
target_compile_options(ReCarbonRecompLib PRIVATE
    -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable
    -fno-strict-aliasing
    # Checked indirect calls (runtime/cpu/indirect.cpp). SHELL: keeps each
    # -include with its file (CMake drops a repeated plain -include).
    "SHELL:-include \"${ROOT}/runtime/cpu/indirect.h\""
    # mftb reads the game's clock, which stands still while it's suspended.
    "SHELL:-include \"${ROOT}/runtime/cpu/guest_time.h\"")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(ReCarbonRecompLib PRIVATE -march=x86-64-v3)
endif()
if(IOS)
    target_compile_options(ReCarbonRecompLib PRIVATE -mcpu=apple-a15)
endif()
