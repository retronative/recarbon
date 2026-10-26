# The unit tests as CMake targets (-DRECARBON_TESTS=ON at the top level),
# built as each file's header says to build it by hand: -O2 with assertions
# on, no -march. Run them with ctest from the build folder. None needs game
# files, ppc/ or a GPU.
#
# From SpeedBreaker's tests/CMakeLists.txt (GPL-3.0-or-later). ReCarbon
# change: only the tests that build against ReCarbon's runtime unchanged and
# don't test Most Wanted's own behaviour are ported (the rest are listed at
# the end with why), plus ReCarbon's own tests. The instruction
# tests (instr/, run_instr_tests.sh) are separate and not built here.
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(RT ${ROOT}/runtime)
find_package(Threads REQUIRED)

function(recarbon_test name)
    cmake_parse_arguments(T "" "" "SOURCES;ARGS" ${ARGN})
    add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp ${T_SOURCES})
    target_include_directories(${name} PRIVATE ${RT})
    target_link_libraries(${name} PRIVATE Threads::Threads)
    target_compile_options(${name} PRIVATE -O2 -UNDEBUG)
    add_test(NAME ${name} COMMAND ${name} ${T_ARGS})
endfunction()

# The free ranges of an image memory block (gpu/block_ranges.h, header only).
recarbon_test(block_ranges_test)
# The game's clocks, which stand still while the game is suspended
# (cpu/guest_time.cpp).
recarbon_test(guest_time_test SOURCES ${RT}/cpu/guest_time.cpp)
# The version line in the log, and when the "What's new" note shows
# (user/version_check.h, header only).
recarbon_test(version_check_test)
# The keys a Controls row can hold and their names in settings.toml
# (user/key_names.h, header only), each row against SDL's scancode name.
# The only test that links SDL3, so it keeps the build tree's path to it.
find_package(SDL3 REQUIRED CONFIG)
recarbon_test(key_names_test)
target_link_libraries(key_names_test PRIVATE SDL3::SDL3)
set_target_properties(key_names_test PROPERTIES SKIP_BUILD_RPATH OFF)
# The Key options (Settings > Controls): the table, the [keys] names in the
# file, and what Set accepts (user/settings.cpp and the toml++ XenonRecomp
# vendors; no SDL). ReCarbon change: checks RECARBON_* variable names.
recarbon_test(settings_keys_test SOURCES ${RT}/user/settings.cpp ${RT}/user/atomic_file.cpp)
target_include_directories(settings_keys_test SYSTEM PRIVATE ${ROOT}/tools/XenonRecomp/thirdparty/tomlplusplus/include)
# The keyboard's controls and how held keys become a pad (hid/key_map.h), header only.
recarbon_test(key_map_test)
# ReCarbon: which RtlRaiseException records are thread-name requests
# (kernel/raise_exception.h, header only).
recarbon_test(raise_exception_test)
# gpu::RingState: a re-initialized ring isn't overwritten by an old batch's end
# (runtime/gpu/ring_state.h, header only; ReCarbon M3).
recarbon_test(ring_state_test)
# Which EDRAM colour formats share a target, and their sample sizes
# (runtime/gpu/edram_format.h, header only; ReCarbon M4: black movies).
recarbon_test(edram_format_test)
# Guest system time never goes back with several readers (cpu/guest_time.h;
# ReCarbon M4: guest_time_test's intermittent system-time failure).
recarbon_test(system_time_monotonic_test SOURCES ${RT}/cpu/guest_time.cpp)
# A guest critical section as guest code reads it, big-endian
# (kernel/critical_section.h, header only; ReCarbon M4: the movie-teardown hang).
recarbon_test(critical_section_test)
target_include_directories(critical_section_test PRIVATE ${ROOT}/tools/XenonRecomp/XenonUtils)
# The QoS class a guest thread priority gets on macOS (kernel/thread_priority.h,
# header only; ReCarbon M5: front-end audio running dry).
recarbon_test(thread_priority_test)
# ReCarbon: the guest threads that present or poll the controller raised to
# user-initiated once, never lowered (kernel/thread_priority.cpp, real pthreads);
# VdSwap's and XamInputGetState's sources read (ReCarbon M7: frame gaps under load).
recarbon_test(frame_thread_qos_test SOURCES ${RT}/kernel/thread_priority.cpp
    ARGS ${RT}/gpu/imports_video.cpp ${RT}/hid/input.cpp)
# The controller's default button mapping, the M5 regression baseline
# (hid/pad_map.h, header only; SDL3 for its button enum only).
recarbon_test(pad_map_test)
target_link_libraries(pad_map_test PRIVATE SDL3::SDL3)
set_target_properties(pad_map_test PROPERTIES SKIP_BUILD_RPATH OFF)
# getGradients (fetch opcode 18) in the shader translator: Carbon's road shader
# (gpu/shader_translator.cpp, gpu/xenos/ucode.cpp; ReCarbon M5: see-through road).
recarbon_test(shader_gradients_test SOURCES ${RT}/gpu/shader_translator.cpp ${RT}/gpu/xenos/ucode.cpp)
# Returning freed guest pages to zero touches no byte outside the range on a
# 16 KB host (kernel/vmem.h ResetHostRange, header only; from SpeedBreaker v0.1.8).
recarbon_test(vmem_host_pages_test)
# The keys Settings > Controls may bind, by the name the keyboard layout gives
# them: ASCII only (user/key_names.h Bindable, header only; ReCarbon M5.15).
recarbon_test(key_bindable_test)
# The built-in keyboard profiles against the approved tables, Default against
# the settings' defaults, no key on two controls in Arrows, the
# controller's default (hid/profiles.h; ReCarbon M8). SDL's headers only.
recarbon_test(profiles_test SOURCES ${RT}/user/settings.cpp ${RT}/user/atomic_file.cpp)
target_link_libraries(profiles_test PRIVATE SDL3::SDL3)
set_target_properties(profiles_test PROPERTIES SKIP_BUILD_RPATH OFF)
# The input profiles' files, the startup that keeps [keys], the edit rule, and
# the running store: switching, edits, a failed write (user/profiles.cpp; M8).
recarbon_test(profile_store_test SOURCES ${RT}/user/profiles.cpp ${RT}/user/settings.cpp ${RT}/user/atomic_file.cpp)
target_link_libraries(profile_store_test PRIVATE SDL3::SDL3)
set_target_properties(profile_store_test PROPERTIES SKIP_BUILD_RPATH OFF)
# Check for Updates' logic (update/check.cpp): versions, the strict JSON reader,
# the HTTP answer, release notes and every outcome. From SpeedBreaker's, with
# ReCarbon's repository (ReCarbon M8).
recarbon_test(update_check_test SOURCES ${RT}/update/check.cpp)
# The internal resolution's guest <-> host pixel arithmetic and the scaled
# resolve memory's layout (gpu/render_scale.h, header only), and that
# renderer.cpp's shaders splice the shared code. From SpeedBreaker v0.1.8's,
# with generic resolve pieces instead of MW's scene (ReCarbon M8 tests).
recarbon_test(render_scale_test ARGS ${RT}/gpu/renderer.cpp)
# Where the picture goes on a screen of any shape, letterboxed (Carbon has no
# view-scaling hooks: game/title_hooks.cpp is linked to check), and Auto
# internal resolution at a Mac's budgets (video/picture_fit.cpp). From
# SpeedBreaker v0.1.8's, without its Hor+/Vert+ cases (ReCarbon M8 tests).
recarbon_test(picture_fit_test SOURCES ${RT}/video/picture_fit.cpp ${RT}/game/title_hooks.cpp)
# Frame Rate's Auto: when to run at 30 instead of 60 (video/frame_rate_policy.cpp).
# From SpeedBreaker v0.1.8's, with Carbon's draw counts, and two rules
# upstream's cases didn't pin (ReCarbon M8 tests).
recarbon_test(frame_rate_policy_test SOURCES ${RT}/video/frame_rate_policy.cpp)
# When the presenter builds its swapchain again and at what size, and the UI's
# display size (video/swapchain_policy.cpp): the Mac's scenarios of
# SpeedBreaker v0.1.8's, plus fullscreen as the M5 logged it (ReCarbon M8 tests).
recarbon_test(swapchain_policy_test SOURCES ${RT}/video/swapchain_policy.cpp)
# Settings > Quit Game: where it is offered, and its confirmation (ui/quit_game.h,
# header only); the code around it is read: the gate, the focus on Cancel, B and
# Escape cancelling, the quit taking the window close's way out. From SpeedBreaker
# v0.1.8's (ReCarbon M8 tests).
recarbon_test(quit_game_test ARGS ${RT})
# A requested termination (SIGTERM, SIGINT, SIGHUP) quits cleanly, and only a
# session that didn't end cleanly leaves its crash marker (platform/quit_signals.h,
# report/crash_marker.h): signals in child processes, markers in a scratch folder
# in the build tree; main.cpp, presenter.cpp and crash.cpp are read. From
# SpeedBreaker v0.1.8's (ReCarbon M8 tests).
recarbon_test(clean_exit_test ARGS ${CMAKE_CURRENT_BINARY_DIR}/clean_exit_scratch ${RT})
# GPU, from SpeedBreaker v0.1.8's (ReCarbon M8 tests): the resolve shader's
# 24-bit depth pack (gpu/depth24_glsl.h; renderer.cpp's shader read), depth-only
# draws running no pixel shader (gpu/depth_only.h; renderer.cpp's Draw read),
# occlusion query reports (gpu/zpd_report.h), and when a saved pipelines.bin was
# made by another GPU, driver or OS (gpu/pipeline_cache_header.h). Header only.
recarbon_test(depth24_pack_test ARGS ${RT}/gpu/renderer.cpp)
recarbon_test(depth_only_test ARGS ${RT}/gpu/renderer.cpp)
recarbon_test(zpd_report_test)
recarbon_test(pipeline_cache_header_test)
# The picture's tone curve (Settings > Display > Brightness) and the 360's
# display gamma ramp as the game programs it (video/display_tone.h,
# video/display_gamma.h, header only), Carbon's logged ramp included;
# presenter.cpp's shaders and settings.cpp's defaults read. From SpeedBreaker
# v0.1.8's (ReCarbon M8 tests).
recarbon_test(display_tone_test ARGS ${RT}/video/presenter.cpp ${RT}/user/settings.cpp)
# Presentation, from SpeedBreaker v0.1.8's (ReCarbon M8 tests): the 120 Hz
# present schedule and its watchdog (video/present_schedule.h), Fullscreen
# against the window's own state (video/fullscreen_policy.h; presenter.cpp read),
# the present waiter's use of a swapchain (video/swapchain_use.h; presenter.cpp
# read), and when the mouse cursor shows (ui/cursor_idle.h; ui.cpp read).
recarbon_test(present_schedule_test)
recarbon_test(fullscreen_policy_test ARGS ${RT}/video/presenter.cpp)
recarbon_test(swapchain_use_test ARGS ${RT}/video/presenter.cpp)
recarbon_test(cursor_idle_test ARGS ${RT}/ui/ui.cpp)
# The audio render callback's cadence against the output device, a device that
# stops taking audio included (apu/audio_cadence.h, header only). From
# SpeedBreaker v0.1.8's (ReCarbon M8 tests).
recarbon_test(audio_cadence_test)
# Where guest memory and the upload ring go on Apple silicon (gpu/memory_plan.h,
# header only), checked against the M5's and M1's logged plans. The Mac's part
# of SpeedBreaker v0.1.8's (ReCarbon M8 tests).
recarbon_test(memory_plan_test)
# The bug report's own deflate, PNG and zip writers (report/zip.cpp), against
# published checksums and Python's zlib. From SpeedBreaker v0.1.8's (ReCarbon M8 tests).
recarbon_test(report_zip_test SOURCES ${RT}/report/zip.cpp ARGS ${CMAKE_CURRENT_BINARY_DIR}/report_zip_out)
# The installer's folder rules and messages (install/placement.cpp and the
# installer it calls), and portable mode with --portable on|off
# (user/portable.cpp, install/portable_switch.cpp), in scratch folders at the
# build tree's top (not beside the test: that is its own folder, refused).
# From SpeedBreaker v0.1.8's, with ReCarbon's names (ReCarbon M8 tests).
set(INSTALL_SOURCES ${RT}/install/placement.cpp ${RT}/install/installer.cpp ${RT}/install/locate.cpp
    ${RT}/install/manifest.cpp ${RT}/install/result.cpp ${RT}/install/disc_source.cpp ${RT}/install/sha256.cpp
    ${RT}/user/portable.cpp)
recarbon_test(install_folder_test SOURCES ${INSTALL_SOURCES} ARGS ${CMAKE_BINARY_DIR}/install_folder_scratch)
recarbon_test(portable_test SOURCES ${RT}/install/portable_switch.cpp ${INSTALL_SOURCES} ARGS ${CMAKE_BINARY_DIR}/portable_scratch)
target_link_libraries(install_folder_test PRIVATE "-framework CoreFoundation")  # placement.cpp's iCloud check
target_link_libraries(portable_test PRIVATE "-framework CoreFoundation")
# Why the installer turns a disc down (install/installer.cpp CheckGameVersion,
# install/disc_source.cpp): another platform, another game, another region or
# version of Carbon, on synthetic files in a scratch folder (ReCarbon's own).
recarbon_test(disc_check_test SOURCES ${INSTALL_SOURCES} ARGS ${CMAKE_BINARY_DIR}/disc_check_scratch)
target_link_libraries(disc_check_test PRIVATE "-framework CoreFoundation")
# The write watch around CPU write faults on 16 KB host pages, on physical
# memory mapped as the runtime maps it (kernel/write_watch.cpp), with ppc/'s
# ppc_config.h for the guest base. From SpeedBreaker v0.1.8's (ReCarbon M8
# tests). A bug here can loop on faults: hence the timeout.
recarbon_test(write_watch_test SOURCES ${RT}/kernel/write_watch.cpp ${RT}/cpu/guest_time.cpp)
target_include_directories(write_watch_test PRIVATE ${RT}/kernel ${RT}/thirdparty/o1heap ${ROOT}/ppc
    ${ROOT}/tools/XenonRecomp/XenonUtils ${ROOT}/tools/XenonRecomp/thirdparty/simde)
set_tests_properties(write_watch_test PROPERTIES TIMEOUT 60)
# Carbon's jump tables (config/switch_tables*.toml) keep their labels and
# default inside the leaf functions config/recarbon.toml bounds. From
# SpeedBreaker v0.1.8's, without its Most Wanted dispatches (ReCarbon M8 tests).
recarbon_test(switch_tables_test ARGS ${ROOT}/config/switch_tables.toml ${ROOT}/config/switch_tables_inline.toml
    ${ROOT}/config/recarbon.toml)
# The display's refresh measured from present times, and which displays are a
# whole multiple of 60 Hz (video/refresh_fit.cpp, video/refresh_multiple.h).
# From SpeedBreaker v0.1.8's (ReCarbon M8 tests).
recarbon_test(refresh_fit_test SOURCES ${RT}/video/refresh_fit.cpp)
# Upscaling, from SpeedBreaker v0.1.8's (ReCarbon M8 tests): which pass draws a
# frame and FSR 1's GLSL (video/fsr1.cpp, with runtime/generated/fsr1_headers.inc;
# it writes the GLSL to fsr1_glsl, compiled to SPIR-V after it where
# glslangValidator is installed), and Upscaling's values in settings.toml
# (user/settings.cpp), in a scratch folder.
recarbon_test(fsr1_test SOURCES ${RT}/video/fsr1.cpp ${RT}/video/picture_fit.cpp
    ARGS ${RT}/thirdparty/fsr1 ${RT}/video ${CMAKE_CURRENT_BINARY_DIR}/fsr1_glsl)
target_include_directories(fsr1_test PRIVATE ${CMAKE_BINARY_DIR}/runtime/generated)
find_program(RECARBON_GLSLANG_VALIDATOR glslangValidator)
if(RECARBON_GLSLANG_VALIDATOR)
    foreach(shader easu.comp rcas.frag rcas_tone.frag)
        add_test(NAME fsr1_glsl_check_${shader} COMMAND ${RECARBON_GLSLANG_VALIDATOR} -V --target-env vulkan1.1
            ${CMAKE_CURRENT_BINARY_DIR}/fsr1_glsl/${shader} -o ${CMAKE_CURRENT_BINARY_DIR}/fsr1_glsl/${shader}.spv)
        set_tests_properties(fsr1_glsl_check_${shader} PROPERTIES DEPENDS fsr1_test)
    endforeach()
endif()
recarbon_test(settings_scaling_test SOURCES ${RT}/user/settings.cpp ${RT}/user/atomic_file.cpp
    ARGS ${CMAKE_CURRENT_BINARY_DIR}/settings_scaling_scratch)
target_include_directories(settings_scaling_test SYSTEM PRIVATE ${ROOT}/tools/XenonRecomp/thirdparty/tomlplusplus/include)
# scripts/frame_times.py (M7 frame pacing) on synthetic frame logs with known
# answers, and the runtime's log and CSV formats it reads. Python 3.9+.
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME frame_times_test COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/frame_times_test.py)
# scripts/run_attempt.sh's retention marker for game/m3 (written once, never changed).
add_test(NAME run_attempt_marker_test COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_attempt_marker_test.py)
# scripts/release_candidate.sh's preflight, rc_manifest.py's content gate and manifest, and
# scripts/rc_transfer.sh's refusals, in a scratch tree (no build, no game files, no network).
add_test(NAME release_candidate_test COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/release_candidate_test.py)
# scripts/macos/make_app.py's app icon (AppIcon.icns) and dmg volume icon (the custom-icon flag),
# and check_app.py's checks of both (macOS: sips, iconutil, xattr; no build).
add_test(NAME app_icon_test COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/app_icon_test.py)
# scripts/macos/make_app.py's dmg Finder layout (the background, the .DS_Store) on a scratch HFS+
# volume, and check_app.py's check of it (macOS; skipped without tools/dmg-python: setup_tools.sh).
add_test(NAME dmg_layout_test COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/dmg_layout_test.py)
set_tests_properties(dmg_layout_test PROPERTIES SKIP_RETURN_CODE 77)
# Alpha gate G2: check_app.py's check that FFmpeg and libmspack's LZX decoder are separate dylibs in
# the app, never linked statically again, on stand-in apps built with clang (macOS; no build).
add_test(NAME lgpl_dylibs_test COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/lgpl_dylibs_test.py)

# Not ported from SpeedBreaker v0.1.8 (one line each: why).
# edge_bars_test, band_filter_test, fade_layers_test: the edge-bar fills and the
#   band split act only with a view scale below 1, which needs MW's view hooks;
#   Carbon has none (game/title_hooks.cpp).
# menu_borders_test: MW's Showcase vignette and Blacklist bio panel (GitHub #19).
# glare_fit_test, front_end_matrix_test, whole_vblanks_test, light_trails_test,
#   player_slots_test, split_views_test, ea_cpu_budget_test: MW's game layer
#   (game/*.h, apu/ea_cpu_budget.h), removed.
# hfr_blend_test, hfr_grid_test, hfr_schedule_test, wheel_map_test: v0.1.8
#   features ReCarbon's runtime doesn't have (HFR, wheels).
# refresh_notice_test: v0.1.8's notice (its HFR inputs); ReCarbon's is older.
# scale_budget_test: the iPhone and iPad budgets only.
# code_signature_test, memory_status_test: read only on iOS (a Mac build reads no
#   signature); memory_status's Mac part is the [memory] log line.
# gpu_choice_test: picks among several GPUs; an Apple silicon Mac has one.
# vblank_timer_test, vblank_lock_sim: timing-sensitive (scheduler latency).
# movie_cadence_sim.py: MW's movie cadence (not a ctest in SpeedBreaker either).
# launcher_prime_test.sh: needs scripts/steamos/bundle/speedbreaker.sh, which ReCarbon doesn't have.
