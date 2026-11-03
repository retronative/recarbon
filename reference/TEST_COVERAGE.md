# Test coverage

What ReCarbon's automated tests cover, where each came from, how each was checked, and what
is still not covered. Measured counts only: there is no coverage instrumentation, so no
code-coverage percentage is claimed anywhere.

## The suites

| Suite | Runs with | Count (2026-10-24, 8ad356e) |
|---|---|---|
| Unit tests | `ctest` in a `-DRECARBON_TESTS=ON` build | 48 CTest entries: 44 C++ programs, 3 SPIR-V compiles of FSR 1's shaders, 1 Python (`frame_times_test.py`) |
| Instruction tests | `tests/run_instr_tests.sh` | 32 files, 242 test functions, 622 register/memory checks (15 functions are ReCarbon's hand-computed `carbon_*.s`) |
| Recompiler structure | `survey/validate_recomp.py game/survey/recomp.log` | 0 errors and warnings |
| Installer, end to end | `scripts/install_check.sh <bin> <iso> [folder]` | 18 cases with an image (21 with a folder too), launch and cleanup included |

Results at 8ad356e:
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
| picture_fit | Letterboxed with Vert+ off (the default, `game/vert_plus.h` State(false)); with RECARBON_VERT_PLUS=1 a hand-computed Vert+ table (2940x1846, 2880x1800, 2560x1600, 3024x1890, 2160x1440, 1024x768, 1280x1024 and portrait clamped to 4:3), no Hor+, a filled frame's redraw; Auto at a Mac's budgets (4, 9); the M5/M1 picture sizes and choices as logged | 9/9; Vert+ (v0.1.4): see vert_plus |
| frame_rate_policy | Carbon's draw counts (hand04: race median 2250, front end 55); the Steam Frame case dropped; two added cases; the Frame Rate options' audit added the M5 at 3x (tries at 180/360/720/1440 s, the back-off's cap) and the M1 throttling while macOS says "fair" | 12/12, then 9/10 (the rise path's dwell is equivalent: its cooldown and try wait both exceed the dwell) |
| frame_rate_help | ReCarbon's own: the Frame Rate row's help line per value, word for word | 4/4 |
| swapchain_policy | The Mac's scenarios only; fullscreen as the M5 logged it; each watch trigger on its own | 10/11 (1 equivalent within the test's tolerance), plus 7 planted rules |
| quit_game | The prompt's name | 4/4 |
| clean_exit | The crash report's first line | 7/7 |
| depth24_pack, depth_only, zpd_report, pipeline_cache_header | Names only | 9, plus 1 that doesn't compile |
| display_tone | Carbon's logged ramp #1, all ten levels | 5/5 |
| present_schedule, fullscreen_policy, swapchain_use, cursor_idle | Names; swapchain_use's work guard and v0.1.5 canary given up to 10 s of runs | 10/11 |
| audio_cadence | None | 3/3 |
| memory_plan | Apple's part only, against the M5's and M1's logged plans | 3/4 (1 equivalent on Apple's inputs) |
| report_zip | A long-input checksum against Python's zlib | 2/2 |
| install_folder, portable | ReCarbon's names, Carbon's title ID | 5/5 |
| write_watch | Builds against Carbon's `ppc/`; 60 s CTest timeout | 3/3 (1 by hang) |
| switch_tables | Carbon's 820 tables and 180 leaf bounds; MW's dispatches dropped | A planted escaping default is rejected |
| refresh_fit | Names | 2/2 |
| fsr1, settings_scaling | Names; the SPIR-V compiles | 3/3 |
| perf_report (ReCarbon's own, v0.1.4) | Export Performance Report's format, quantiles, switch policy, scrub, denylist and present ring, with hand-worked values | 18/18: the scrub removed before the denylist; the private-path rule; the guest-address rule; quantile `int(p/100*n)`; quantile without rounding; any variable valued; the never-valued list ignored; a metric as a bare number; unavailable with a value; the warning filter inverted; the denylist's path rule; the host name split into words; JSON strings unescaped; `>= 50` for `> 50`; the measured bound off by one; the ring keeping overwritten slots; a guest thread's name kept; the thread names' hex-run rule |
| vert_plus (ReCarbon's own, v0.1.4), with picture_fit | `game/vert_plus.h`: the RECARBON_VERT_PLUS gate (exactly `1`), off changing nothing, the widened view ids, the projection, front-end matrix resend, mirror/PiP rectangle and movie aspect arithmetic; `gpu/edge_bars.h` Parked (SpeedBreaker Carbon's by-centre rule); hand-worked values | 20/20: the gate taking `0` or any 1-prefix; view 3 widened; view 21 dropped; scaling about 0; the movie aspect inverted; the tracker missing the first matrix; each off guard removed (projection, front end, rectangle, movie write); Hor+ reported; hooks reported before the hook ran; P[0][0] taking the y scale; the height kept; the y clamp at 0.25; Parked ignoring byCentre or never parking by centre; picture_fit's 4:3 clamp removed; its 1% margin at 15% |
| perf_report_check (ReCarbon's own, v0.1.4) | `scripts/perf_report_check.py` against synthetic reports | 9/9: the path, guest-address, name and file rules; null iff unavailable; the status set; bare values; the top-level keys; why required |

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

- **`install_check.sh` left games running** (dd00321; ROADMAP M8 "Test tooling"). It now
  tracks the game's process group, its descendants by PID and start time, and an owner
  marker, and stops them on any exit.
  - Verified: normal run, the subshell mutant, a launch that ignores SIGTERM with an escaped
    descendant, and SIGINT and SIGTERM to the script. Two decoy `ReCarbon` processes were
    alive after every case.
- **swapchain_use_test's work guard** (8ad356e). Upstream requires more than 1000 waiter uses
  in one 20,000-cycle run, which depends on the scheduler: it failed one full ctest run
  (539 uses) and 1 of 50 runs alone.
  - Every run is still checked for soundness. Only the work is now counted over up to 5 runs.
  - After the change: 0 of 100 failed alone, and 0 of 40 under an 8-process CPU load.
  - **Not enough under heavier load** (2026-10-24): with a load average of 13–19, full `ctest -j8`
    runs failed 7 of 8 times (317 uses in 5 runs), and once the v0.1.5 canary missed in its 5
    tries. Alone it passed 40 of 40. Both loops now run until their condition is met, for up
    to 10 s; the conditions themselves are unchanged.
    - Result: 10 of 10 full `ctest -j8` runs passed at a load average of 18–27, and 10 of 10
      with 8 extra CPU-bound processes (load average up to 66).
    - Planted bugs: no uses counted, and the canary's unsafe read removed, each fail at the
      10 s bound.

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

## Diagnostics: game time, handshake and D3D waits (2026-10-31)

Three new ctest entries, all label `unit`: `wait_span_test` (game/wait_span.h: a poll loop's
wait from its first poll to its exit, and the 10 s window's count, total, p50/p99/max and
polls), `game_time_log_test` (game/game_time_log.h: the CSV line, the game's 1-8 step rule,
the `[gametime]` summary) and `game_time_test.py` (scripts/game_time.py on synthetic CSVs,
and the runtime header it reads). `wait_span_test --bench` (not run by ctest) times the cost
the D3D wait timing adds to a wait: about 29 ns with 1 or 10 polls (two clock reads), 0.2 ns
for an exit without a wait, on the M5.

**Mutation checks**, planted in the real files and restored after each run: 21 of 21 failed
their test.
- wait_span.h (8): the start moved by every poll; a negative length; the span left open; the
  polls dropped; due strictly after 10 s; p90 reported as p99; a new window keeping counts; a
  new wait keeping the last one's polls.
- game_time_log.h (8): the cap at 7; the floor at 0; steps and raw steps swapped; 2 steps
  counted as 3+; frames without the world counted as no step; due strictly after 10 s; a new
  window keeping counts; a cut line's length past the buffer.
- game_time.py (5): 3+ steps left out of the irregular count; `--from` ignored; the frames
  before the first present counted into it; frames without the world counted; a cut last
  line raising instead of being skipped. (A first version of the last one, the cut line kept
  as a zero row, was equivalent: the window drops a host time of 0.)

Not covered: the mid-asm hooks themselves and the writer thread (they need the game), and the
command processor's dropped-packet counter (an increment in `ExecuteType3`).

## Recompiler and harness safety (v0.1.4)

- **Fatal recompiler problems** (`patches/xenonrecomp/0011`). `survey.sh` run twice from
  scratch: `ppc/` (289 files) and `config/` byte-identical to each other and to v0.1.3's,
  `validate_recomp.py` clean, `run_instr_tests.sh` 0 failed checks. Pass 1 of `survey.sh`
  (the function map, before the inline tables and leaf bounds) logs 3,189 switch cases outside
  their guessed functions; it is the one pass allowed to end with fatal problems.
  Planted faults, each in a scratch config or the private XenonRecomp, each caught:
  - a secondary entry at 0x80000000, a switch case moved outside its function, a dropped jump
    table (a bctr after a nop): XenonRecomp exits 1, one `FATAL:` line, one `#error` in the
    output, and `validate_recomp.py` fails on both the marker and the log line;
  - eqv's case removed: `survey.sh` fails in the final pass with 75 `FATAL:` lines and 75
    `#error` lines in `ppc/`;
  - eqv.'s cr0 write removed: `run_instr_tests.sh` fails on `record form eqv. ... writes no CR
    field` (before, only a warning that the script ignored);
  - a test using mfctr (unimplemented): `run_instr_tests.sh` fails.
  Not covered: the jump-table check only knows a bctr after `nop` or one other instruction
  (upstream's heuristic), so a missing table of another shape is not caught.
- **Harness: `BENCH_STOP_LEVEL=3` and `BENCH_MAX_SECS`** (`tests/bench_safety_test.py`, fake
  probes, no game). The defaults are unchanged (level 2 stops at once, 601 s refused). With
  level 3 a run stays at level 2 to its end and still stops at once at 3 and 4, and on memory,
  swap, the game's serious line and a broken probe; any other level, or 3 with a
  `BENCH_THERMAL_GRACE`, is refused before the game runs. `BENCH_MAX_SECS` raises the cap up to
  3600 s or lowers it; malformed values are refused. Ten planted faults (the stop level ignored,
  level 3 not an immediate stop, either value not validated, the grace allowed with level 3,
  default level 3, default cap 3600, the cap ignored either way, the verdict's note dropped):
  each fails the test. Level 3 not an immediate stop is caught only by the grace test: with
  level 3 the level-2 branch still stops it, under the wrong label.
- **Settings verification** (`scripts/verify_settings.py`, `tests/verify_settings_test.py`,
  offline only: the game launches are still to be run). The planned keys for the menu and
  persist phases are replayed through an independent model of the menu: they make exactly
  the planned changes, one logged line per press, and the menu phase puts every row back.
  The analysis is checked on synthetic runs: everything applied is all PASS; a missing effect
  line fails only that row; no baseline line, a scene that moves or a Frame Rate that stays at
  60 is INCONCLUSIVE; Mute and Internal Resolution (run_attempt.sh sets their variables) must
  be saved without changing the run; a strict picture row with no change, a change not put back, a wrong
  `how`, an unexpected change, a wrong value in `settings.toml`, a missing relaunch line, an
  unclean quit after a corrupt file and an unknown key each FAIL. Sixteen planted faults in a
  scratch copy of the script (the picture threshold reversed, `how` ignored, the undo value
  not checked, the baseline gate removed, strict rows made lenient, the put-back and
  still-scene checks dropped, unexpected changes ignored, pages stepped the wrong way, a bool
  press not flipping, unknown keys ignored, `settings.toml` numbers not compared, Frame Rate
  passing at 60, a killed game counted as running, an effect line not required, Mute's
  absent-line check inverted): each fails the test.

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
- **The performance report's collection** (`report/perf_report.cpp`: the counters it reads, the
  thread times, the file it writes) runs only in the game: checked by an export in a launch, with
  `scripts/perf_report_check.py` on the file.
- **No CI:** tests run by hand on the M5, and on the M1 through `remote_worker.sh`.
