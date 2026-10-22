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
recarbon_test(settings_keys_test SOURCES ${RT}/user/settings.cpp)
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

# Not ported from SpeedBreaker (one line each: why).
# write_watch_test: needs ppc/ (ppc_config.h, generated from the XEX) and XenonUtils.
# switch_tables_test: reads config/nfsmw.toml, MW's leaf functions and switch tables.
# edge_bars_test: MW's police vid-cam bars, intro frames and results backdrop.
# menu_borders_test: MW's Showcase vignette and Blacklist bio panel (GitHub #19).
# glare_fit_test: game/glare.h, removed with MW's game layer.
# front_end_matrix_test: game/front_end_matrix.h, removed (MW's front-end matrix hook).
# whole_vblanks_test: game/whole_vblanks.h, removed (MW's game time).
# light_trails_test: game/light_trails.h, removed (MW's nitrous trails, GitHub #10).
# movie_cadence_sim.py: MW's movie cadence (not a ctest in SpeedBreaker either).
# install_folder_test: asserts MW install names (.nfsmw-install*, SpeedBreaker folders) ~20 times; ReCarbon uses .recarbon-install-*; adapt separately.
# portable_test: asserts the same MW install and folder names ~40 times; adapt separately.
# launcher_prime_test.sh: needs scripts/steamos/bundle/speedbreaker.sh, which ReCarbon doesn't have.
# depth24_pack_test, depth_only_test, render_scale_test,
#   display_tone_test, swapchain_use_test, fullscreen_policy_test,
#   cursor_idle_test, quit_game_test, clean_exit_test: grep runtime sources
#   for SpeedBreaker's code; deferred to keep this first set small.
# memory_plan_test, zpd_report_test, pipeline_cache_header_test,
#   gpu_choice_test, present_schedule_test, audio_cadence_test,
#   picture_fit_test, band_filter_test, scale_budget_test, refresh_fit_test,
#   refresh_notice_test, frame_rate_policy_test, swapchain_policy_test,
#   code_signature_test, memory_status_test, update_check_test, report_zip_test,
#   settings_scaling_test, fsr1_test: not checked against ReCarbon's runtime yet;
#   deferred to keep this first set small.
# vblank_timer_test, vblank_lock_sim: timing-sensitive (scheduler latency); deferred.
