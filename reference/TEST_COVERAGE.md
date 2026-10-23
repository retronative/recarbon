# Test coverage

What ReCarbon's automated tests cover, where each came from, how each was checked, and what
is still not covered. Measured counts only: there is no coverage instrumentation, so no
code-coverage percentage is claimed anywhere.

## The suites

| Suite | Runs with | Count (2026-10-24, b5993d0) |
|---|---|---|
| Unit tests | `ctest` in a `-DRECARBON_TESTS=ON` build | 48 CTest entries: 44 C++ programs, 3 SPIR-V compiles of FSR 1's shaders, 1 Python (`frame_times_test.py`) |
| Instruction tests | `tests/run_instr_tests.sh` | 32 files, 242 test functions, 622 register/memory checks (15 functions are ReCarbon's hand-computed `carbon_*.s`) |
| Recompiler structure | `survey/validate_recomp.py game/survey/recomp.log` | 0 errors and warnings |
| Installer, end to end | `scripts/install_check.sh <bin> <iso> [folder]` | 18 cases with an image (21 with a folder too), launch and cleanup included |

Results at b5993d0:
- **MacBook Air M5:** 48/48. `ctest --repeat until-fail:10`: 480 runs, all passed.
- **MacBook Air M1** (`scripts/remote_worker.sh`, its own build of the same commit): 48/48,
  0 failed instruction checks, `validate_recomp` clean.

## Ported from SpeedBreaker v0.1.8 (2026-10-24)

Every port builds against ReCarbon's runtime, and each file's header records what changed
from upstream. A port was made only where ReCarbon's Mac build reaches the code. Most Wanted
addresses, data and scenarios were left out or replaced by Carbon's own evidence.

**Mutation checks.** Each port was checked with bugs planted in a scratch copy of the
production source (never the real tree). The test had to fail every one, or the mutant is
listed here as equivalent, unbuildable or missed.

| Test | ReCarbon changes | Mutants caught |
|---|---|---|
| render_scale | Generic resolve pieces (one rectangle, strips, a grid, side-by-side halves) instead of MW's scene | 9/9 |
| picture_fit | Carbon has no view hooks: the letterbox cases only, tied to `title_hooks.cpp`; Auto at a Mac's budgets (4, 9); the M5/M1 picture sizes and choices as logged | 9/9 |
| frame_rate_policy | Carbon's draw counts (hand04: race median 2250, front end 55); the Steam Frame case dropped; two added cases | 12/12 |
| swapchain_policy | The Mac's scenarios only; fullscreen as the M5 logged it; each watch trigger on its own | 10/11 (1 equivalent within the test's tolerance), plus 7 planted rules |
| quit_game | The prompt's name | 4/4 |
| clean_exit | The crash report's first line | 7/7 |
| depth24_pack, depth_only, zpd_report, pipeline_cache_header | Names only | 9, plus 1 that doesn't compile |
| display_tone | Carbon's logged ramp #1, all ten levels | 5/5 |
| present_schedule, fullscreen_policy, swapchain_use, cursor_idle | Names; swapchain_use's work guard counted over up to 5 runs | 10/11 |
| audio_cadence | None | 3/3 |
| memory_plan | Apple's part only, against the M5's and M1's logged plans | 3/4 (1 equivalent on Apple's inputs) |
| report_zip | A long-input checksum against Python's zlib | 2/2 |
| install_folder, portable | ReCarbon's names, Carbon's title ID | 5/5 |
| write_watch | Builds against Carbon's `ppc/`; 60 s CTest timeout | 3/3 (1 by hang) |
| switch_tables | Carbon's 820 tables and 180 leaf bounds; MW's dispatches dropped | A planted escaping default is rejected |
| refresh_fit | Names | 2/2 |
| fsr1, settings_scaling | Names; the SPIR-V compiles | 3/3 |

Notes on the mutants:
- **Unbuildable:** the depth-pack truncation mutant doesn't compile, because the C++ mirror
  evaluates the macro with its own rounding types.
- **Missed:** in present_schedule, the 120 Hz band's edges (117–123 Hz) are untested. It is
  listed as a gap rather than pinned to a constant.

Evidence the ports were checked against:
- **Gamma ramp:** Carbon's logged ramp equals display_tone's model at g = 0.815, BT.709,
  within 0.05 levels.
- **Memory:** the M5 and M1 memory plans match their log lines word for word.
- **Auto frame rate:** replaying hand test 4's two sessions (944 s and 474 s) with nominal
  thermal, Auto would not have switched.

Not ported: the end of `tests/CMakeLists.txt` lists each remaining upstream test with the
reason.

## Fixed in this pass

- **`install_check.sh` left games running** (477c09e; ROADMAP M8 "Test tooling"). It now
  tracks the game's process group, its descendants by PID and start time, and an owner
  marker, and stops them on any exit.
  - Verified: normal run, the subshell mutant, a launch that ignores SIGTERM with an escaped
    descendant, and SIGINT and SIGTERM to the script. Two decoy `ReCarbon` processes were
    alive after every case.
- **swapchain_use_test's work guard** (b5993d0). Upstream requires more than 1000 waiter uses
  in one 20,000-cycle run, which depends on the scheduler: it failed one full ctest run
  (539 uses) and 1 of 50 runs alone.
  - Every run is still checked for soundness. Only the work is now counted over up to 5 runs.
  - After the change: 0 of 100 failed alone, and 0 of 40 under an 8-process CPU load.

## The earlier unexplained ctest failure (2026-10-24)

One full run reported 1 failure out of the 20 tests of the time. Its log was overwritten, so
which test failed isn't known.
- **What was ruled out:**
  - **Shared state:** ctest runs the tests one at a time, each in its own process.
  - **Temp files:** every temporary folder is unique (`mkdtemp`, or a name with the pid), and
    none were left behind after the 480 repeated runs.
  - **Shared environment:** HOME and other environment changes are made inside each test's
    own process.
- **What was measured:** each of those 20 tests 50 times under an 8-process CPU load: all
  1,000 runs passed.
- **Circumstances:** the run happened while three orphaned games were running at 86–91% CPU
  (the `install_check.sh` bug above).
- **Suspects:** update_check_test's best-of-3 timing check (64 KB under 8 × 16 KB + 20 ms)
  is the only wall-clock check among them. swapchain_use's guard, the one flake found since,
  wasn't in the suite then.
- **Status:** not reproduced, and not attributed.

## Gaps (not covered by any automated test)

- **Which render targets scale** (`renderer.cpp`'s surface-pitch window, 640–1280 by
  default, tuned on MW), and **Auto's budget choice** (4, or 9 on an M3 or later Max): both
  live in `renderer.cpp`'s state, which no test can reach without a production change.
- **`presenter.cpp`'s FrameRegion** sizes Auto for a filled picture, although Carbon's is
  letterboxed (resolution audit).
- **120 Hz presentation** (present schedule, swapchain use with VK_GOOGLE_display_timing) is
  covered by unit tests only: the M5 and M1 have 60 Hz panels.
- **Frame Rate's Auto thresholds** are SpeedBreaker's iOS measurements, not retuned for
  Carbon. Auto is opt-in (the default is 60).
- **Not ported:** `vblank_timer_test` (timing-sensitive); memory_status's Mac log line;
  gpu_choice (an Apple silicon Mac has one GPU).
- **Visual correctness of scaled 3D rendering:** only the intro movie was captured at each
  internal resolution.
- **Not automated:**
  - gameplay beyond the scripted menu routes, which stall at the title screen;
  - soak runs;
  - the M7 10-minute frame-pacing run;
  - the human tests listed in ROADMAP (hand05/05b/05c, M8.11; the physical controller
    deferred; Handbrake, Nitrous, Speedbreaker, Change Camera and Look Back unverified).
- **No CI:** tests run by hand on the M5, and on the M1 through `remote_worker.sh`.
