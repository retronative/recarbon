# ReCarbon — Roadmap (Xbox 360 → Apple Silicon)

Every milestone is gated. Starting each milestone needs the user's **explicit approval**, even
when the previous one passed. A milestone passes only when its acceptance criteria are met **and
observed**, not inferred. The "Evidence" line records what was observed. The "Verify" line is how
anyone can reproduce it from a clean checkout plus their own disc.

Each milestone, and each M3 checkpoint, is in one of three states:
- **Passed:** every criterion is observed, and the result is reproducible (the same result in 3
  consecutive runs where the criterion involves running the game).
- **Partial:** some criteria are observed and the rest are not yet met. The report lists which.
- **Blocked:** a known failure stops progress. It's documented with its root cause, or with what
  is still unknown.

Diagnosing a failure is progress. It does not pass a criterion.

## M0 — Correct input binary *(done 2026-10-22)*
- **Evidence:** FEASIBILITY §1.
  - Xbox 360 XEX2, title 0x454107EC, SHA-256 `947f95c7…c721e1`.
  - `xexsurvey` produced a 17,629,184 B image.
  - All 8 helper addresses found.
  - 369 imports, 11 of them new versus MW.
- **Done locally (git-ignored, M1 can reuse):**
  - XenonRecomp at `ddd128bc` plus SpeedBreaker patches 0001–0008, built in `tools/XenonRecomp`.
  - `xexsurvey` built in `tools/survey/`.
- **Do:**
  - Obtain your own Xbox 360 NFS: Carbon (USA) disc image.
  - Extract it to `../rom_extracted/` (or `game/files`).
  - Run XEX inspection, using SpeedBreaker's `survey/xexsurvey.cpp` built against XenonRecomp.
- **Accept:**
  - `default.xex` is present.
  - Header parsed: title ID, version, base, image size, sections, import libraries and counts, TLS, stack size.
  - Hashes recorded in `reference/FEASIBILITY.md`.
  - The XEX decrypts and decompresses via `Xex2LoadImage` from the player's own disc.
- **Verify:** `shasum -a 256 game/files/default.xex`, `python3 -I reference/analysis/xexinfo.py
  game/files/default.xex`, and `tools/survey/xexsurvey game/files/default.xex game/survey` (the
  image is 17,629,184 B). Compare the results with FEASIBILITY §1.

## M1 — Toolchain + recompile-only PoC *(done 2026-10-22)*
- **Evidence** (FEASIBILITY §3):
  - `scripts/setup_tools.sh` then `scripts/survey.sh`, run from scratch twice. The config and
    `ppc/` were byte-identical across runs.
  - **Output:** 72,864 functions, 289 files, 181 MB.
  - **Recompiler log:** 0 errors, warnings or unrecognized instructions.
  - **Structural validation** (`survey/validate_recomp.py`): all HARD checks pass.
    - All 30,984 `.pdata` functions are present.
    - Mapping and definitions match one-to-one.
    - 0 failure markers.
    - Every non-zero code byte is translated except 576 B of SEH data headers.
  - **Instruction tests** (`tests/run_instr_tests.sh`): 242 tests, 0 failures. That includes
    15 hand-computed ReCarbon tests for `eqv`, `bdnzt` and `dcbst`, and a mutation check
    proved failures are caught.
- **Do:**
  - Import SpeedBreaker's `scripts/setup_tools.sh` and `patches/xenonrecomp/`.
  - Build XenonRecomp and XenonAnalyse.
  - Parametrize the `survey/` scripts from the XEX section table.
  - Generate `config/recarbon.toml` and the switch tables.
  - Run XenonRecomp.
- **Accept:**
  - XenonRecomp completes on the Carbon XEX with **zero** unhandled instructions.
  - Counts are recorded for: functions, switch tables, "switch case outside function" errors, and secondary entries.
  - Structural validation passes.
  - `ppc/` is git-ignored and `git status` is clean of generated code.
- **Verify:** `scripts/setup_tools.sh && scripts/survey.sh` (it ends with `validate_recomp.py`; every
  check must say `ok`), then `tests/run_instr_tests.sh` (exit 0, "0 failed checks").

## M2 — Native compilation and link *(done 2026-10-22)*
- **Evidence** (FEASIBILITY §4):
  - **Build:** a clean `cmake` + `ninja -j8` builds `build/main/runtime/ReCarbon` (arm64 Mach-O,
    59,561,176 B) in 97.9 s, with a 2.56 GB peak, 0 errors and 3 upstream warnings.
  - **Link validation:**
    - 0 unbound undefined symbols, and no undefined-symbol tolerance flags;
    - all 72,864 recompiled bodies are present;
    - all 355 imports are defined (182 real, 173 logging stubs that exit);
    - no runtime override of any `sub_…` function.
  - **Isolation:** the MW game layer and the MW D3D hooks are removed, including a per-frame
    store into MW's data at 0x8200FAC0. They're replaced by `game/title_hooks.cpp`, whose
    explicit "no hooks" states are logged at startup.
- **Do:**
  - Compile `ppc/*.cpp` on arm64 with Homebrew clang.
  - Link against SpeedBreaker's runtime with the MW game layer removed and the identity adapted (otherwise unmodified).
  - Generate `import_stubs.cpp`.
- **Accept:**
  - A `ReCarbon` arm64 Mach-O builds with `-Werror` settings unchanged.
  - The build time and peak RSS on the target Mac are recorded.
  - The list of unimplemented imports is recorded.
  - No unresolved symbols; no silently disabled features or fabricated implementations.
- **Verify:** a clean `cmake` + `ninja -j8` (README "Building"), then the link checks in
  FEASIBILITY §4: `nm -m -u` shows no symbol without a library, and the generated-function and
  import name diffs show 0 missing.

## M3 — Runtime initialization *(Passed 2026-10-22, through C5; C6 reached)*
- **Evidence** (FEASIBILITY §5):
  - C1–C5 passed. 10/10 consecutive windowed launches on the committed code reached frame 1
    and ran to frame 900, after 10/10 on the ring-fix build.
  - C6 reached: the game's own loading screen, in frame dumps in 3/3 runs (ring-fix build)
    and 1/1 on the committed code.
  - 7 imports were implemented with cited semantics, plus `XboxKrnlVersion`. Two root-caused
    races in the command processor's ring were fixed, and a survey bug behind an
    indirect-call abort.
  - `ctest`: 8 tests, including regression tests for both races and the thread-name
    exception. Upstream `guest_time_test` is flaky (3/25).
  - Open: movie video is black (M4/M5).
- **Do:**
  - Run the binary at 1x (`scripts/run_attempt.sh`: strict stubs, never `RECARBON_STUB_CONTINUE`).
  - Debug one stop at a time, finding the root cause before changing anything.
  - Implement only the imports actually reached, with cited semantics and regression tests.
- **Checkpoints** (each marked Passed, Partial or Blocked by its log evidence):
  - **C1:** the image is loaded and all imports are wired (`[loader] wired …`).
  - **C2:** the guest entry point runs, the CRT initializes, and guest threads start.
  - **C3:** the game opens its data through the VFS (`ZDIR.BIN`, `ZZDATA*.BIN`).
  - **C4:** GPU bring-up: `VdInitializeEngines`, the ring buffer, the interrupt callback, and PM4
    packets reaching the command processor.
  - **C5:** the first `VdSwap`.
  - **C6:** the first presented frame. This is M4's goal; M3 doesn't require it.
- **Accept (Passed):**
  - The executable launches.
  - The deepest checkpoint claimed is reached, with the same stop point, in **3 consecutive runs**.
  - Every import the game reached on the way is implemented, with cited semantics. None is
    bypassed, and `RECARBON_STUB_CONTINUE` is never used.
  - Every stop or crash before that checkpoint is root-caused and fixed, and each significant fix
    has a regression test in `ctest`.
  - An attempt table records the stop point, the root cause and the fix of every run.
- **Partial:** a checkpoint below C4 is reached reproducibly, but the next stop isn't root-caused
  or fixed yet. **Blocked:** no checkpoint beyond C1 is reproducible.
- **Verify:** build, then run `scripts/run_attempt.sh <name> 90 [--headless]` three times and
  compare the last meaningful log lines with the attempt table. Run `ctest --test-dir
  build/tests`.

## M4 — First frame (rendering) *(Passed 2026-10-22)*
- **Evidence** (FEASIBILITY §6):
  - The EA logos and movies render in frame dumps (commit 487cf65).
  - Unknown-format and memexport counts are 0 over 25 runs.
  - Guest clock fixed (commit 124cf31).
  - Movie-teardown hang root-caused and fixed: guest critical-section counters stored
    host-endian (commit 7038a47). 58/58 launches clean, of 100 planned (stopped by decision),
    plus 3/3 unmuted. The bug's code path wasn't observed in 28 instrumented launches, so the
    fix rests on the hang report and `critical_section_test`.
- **Do:**
  - Enable the PM4/Xenos path at 1x, 16:9, with the MW-only renderer features disabled.
  - Add logging for unknown texture formats and dropped memexport draws.
- **Accept:**
  - The EA/Black Box logos or the first front-end frame are presented through MoltenVK.
  - A screenshot is captured locally (never committed).
  - Unknown-format and dropped-draw counts are recorded.
- **Verify:** `scripts/run_attempt.sh` windowed, three times. Each log shows a presented frame,
  and the screenshot matches.

## M5 — Audio, movies, input *(In progress)*
- **Evidence so far** (FEASIBILITY §6a). M5 stays open until the hand tests below are done:
  - Front-end music: over 5 minutes with no underruns or gaps, in 3/3 runs, after guest thread
    priorities were mapped to macOS QoS (commit 79573bb).
  - The main menu is reached with scripted keyboard input and with an SDL virtual controller.
    The default controller mapping is pinned by a test (commit af3aeea); the keyboard defaults
    by `settings_keys_test`.
  - Movies play at the correct speed (Nikki PSA 11.38 s against 11.40 s in the file), which
    is accepted as the correct cadence for M5. **Known limitation:** frame pacing isn't
    smooth. 20–28% of movie frames show for 1 or 3 vblanks instead of 2 (measured with a probe
    that may inflate it). Smoothing is deferred to polish unless it causes gameplay issues.
  - XMA decodes without errors (3,749 frames, 0 skipped). XMA1 against XMA2 isn't settled.
  - **Pending, by hand:** keyboard navigation and `[keys]` rebinding in game, a physical
    controller (expected in about a week), and audio quality and movie lip-sync by ear and eye.
- **Do:**
  - Enable XMA → FFmpeg → SDL3.
  - Movies: verified from the extracted files' headers (2026-10-22). All 65 are ASF (`.wmv`):
    - video WMV3 (WMV9 Main), 1280×720 (63 files) or 640×480 (2);
    - audio WMA Pro (format tag 0x0162), 5.1 at 44.1 kHz (60) or stereo at 48 kHz (1), or
      WMA v2 (0x0161) stereo at 48 kHz (4).

    None uses XMA. The title links the XDK's XMEDIA/XMV player (`.XBMOVIE` section), so the
    decoding runs in recompiled guest code, as in SpeedBreaker. The host side needs only pacing
    hooks, which must be found in Carbon. Confirm at runtime that guest decoding is what plays
    them before adding any host decoder.
  - Port the controller and keyboard input with a Carbon key map.
  - Keep SpeedBreaker's keyboard rebinding fully working: the Settings > Controls page and the
    `[keys]` table of `settings.toml`. Don't hide, disable or remove them, and keep their
    behaviour. No new customization (profiles, presets, switching) before M8.
  - Keep the input in three layers, ready for M8 (ARCHITECTURE §3.6): device input → binding
    resolution → a virtual Xbox 360 controller. Mappings are data that the binding layer reads,
    nothing in it names a Carbon address, and input stays independent of game logic.
  - Record the default keyboard and controller mappings as the regression baseline: a table in
    the repository and unit tests that pin it (in the style of `key_map_test`).
- **Accept:**
  - Front-end music plays without stalls for 5 minutes.
  - The intro movie plays at the correct cadence.
  - Menus are navigable with a controller and with the keyboard.
  - The default keyboard and controller mappings are recorded and pinned by passing unit tests.
  - Keyboard rebinding through Settings > Controls and `[keys]` still works and persists.
- **Verify:** a windowed run with the audio log (`RECARBON_AUDIO_LOG`): no underruns over 5 minutes
  in the front end, and the intro movie's frame times in the log. Navigate by hand with a pad and
  with the keyboard, scripted with `RECARBON_INPUT_SCRIPT` where possible.

## M6 — First boot to free-roam / race load
Input as in M5: SpeedBreaker's keyboard rebinding and `[keys]` keep working. No new
customization.
- **Do:**
  - Implement the missing kernel/XAM calls: content, saves and profile, scoped per title.
  - Find Carbon's render-queue and frame-time equivalents and add the ARM fence hooks.
- **Accept** (each verified separately):
  - **Profile and save:** a new profile is created, the save is written, and it reloads intact
    after a restart.
  - **Race loading:** a race or free-roam loads from the front end and reaches control of the
    car.
  - **Stability:** no deadlock or crash for 10 minutes in free-roam.
- **Verify:** a windowed session following a written route (new profile → free-roam → race
  load), then a restart. Logs and the save file's presence and size are recorded; watchdog
  reports must be empty.

## M7 — Playable racing
Input as in M5: SpeedBreaker's keyboard rebinding and `[keys]` keep working. No new
customization. The controls criterion is judged on the default mappings.
- **Accept** (each verified separately):
  - **Player controls:** steering, throttle, brake, handbrake, nitrous and the camera respond on a
    controller and on the keyboard.
  - **Vehicle physics:** the car behaves consistently. Compare against the Xbox 360 version where
    possible (e.g. top speed or a lap time on a known route), with notes.
  - **AI opponents:** they spawn, race and finish.
  - **Race completion:** one full career race finishes, the results screen shows, and the career
    progress is saved and survives a restart.
  - **Frame pacing:** a stable 30 fps or better on the MacBook Air M5, from frame-time logs over
    10 minutes of racing, with the thermal state noted.
  - **Audio and picture:** audio stays in sync, and no world geometry is missing.
- **Verify:** a written test route repeated 3 times; the frame-time log and the session log are
  attached per run.

## M8 — Polish and distribution
- **Do:**
  - Parametrize the installer: Carbon manifest, title ID, ReCarbon names.
  - Package `ReCarbon.app` (bundle ID `org.retronative.recarbon`, via `make_app.py`) with license notices and LGPL sources.
  - Optional: render scaling and widescreen hooks, reintroduced per Carbon render target.
  - Input customization, extending SpeedBreaker's existing rebinding on M5's binding layer
    (ARCHITECTURE §3.6):
    - Separate persistent profiles for the keyboard and the controller, stored in the user
      folder, each holding user-defined mappings (rebinding extended to the controller).
    - Keyboard presets: WASD and Arrows.
    - Manual profile switching.
    - Reset to default, which restores M5's baseline mapping exactly.
    - Existing `[keys]` settings are preserved: migrated into the keyboard profile, never
      dropped.
- **Accept:**
  - A clean install from a disc image on a fresh macOS user account.
  - `check_app.py` passes.
  - The unit tests run in `ctest`.
  - **Input customization:**
    - The WASD and Arrows presets both drive the game.
    - A custom keyboard mapping and a custom controller mapping each survive a restart,
      independently of each other.
    - Switching profiles takes effect without a restart.
    - Reset to default matches M5's baseline table (the baseline tests still pass unchanged).
    - A `settings.toml` with a customized `[keys]` table migrates to a keyboard profile with the
      same bindings.
- **Verify:** `scripts/macos` packaging plus `check_app.py`, an install on a fresh user account
  from the disc image, and `ctest`. For input: unit tests for profile load, save, reset and
  migration, plus a windowed session per preset with `RECARBON_INPUT_SCRIPT` where possible.

## Not planned
- Any original-Xbox (XBE) port. That is out of scope unless re-approved (see ARCHITECTURE appendix).
- Wine/CrossOver or runtime CPU emulation.
