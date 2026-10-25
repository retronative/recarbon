# ReCarbon — Roadmap (Xbox 360 → Apple Silicon)

> Commit IDs here were updated after the 2026-10-24 history rewrite (author email only; trees
> unchanged). Logs and build outputs made before it name the old IDs.

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
  - Extract it to `../NFS/rom_extracted/` (or `game/files`).
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
  - The EA logos and movies render in frame dumps (commit 113f1ee).
  - Unknown-format and memexport counts are 0 over 25 runs.
  - Guest clock fixed (commit 0074256).
  - Movie-teardown hang root-caused and fixed: guest critical-section counters stored
    host-endian (commit 698dd9d). 58/58 launches clean, of 100 planned (stopped by decision),
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

## M5 — Audio, movies, input *(Accepted 2026-10-23, by the user's decision, with open non-blocking limitations)*
- **Status decision** (user, 2026-10-23): M5 is closed as accepted. These stay open and are **not**
  recorded as passed:
  - the physical controller: Deferred — not verified;
  - movie judder (Carbon's pacing hooks): deferred to polish;
  - the '?' glyph on the Controls page: root-caused since then. A non-Latin keyboard layout
    (Myanmar) names letter keys in a script the menu's font lacks. Rebinding now refuses such
    keys and asks for an English layout (4a68b32, FEASIBILITY §6a). Existing labels on such a
    layout still draw as U+FFFD.

  The acceptance criteria below are unchanged.
- **Evidence so far** (FEASIBILITY §6a). M5 stays open until the user closes it:
  - Front-end music: over 5 minutes with no underruns or gaps, in 3/3 runs, after guest thread
    priorities were mapped to macOS QoS (commit 20dd2ed).
  - The main menu is reached with scripted keyboard input and with an SDL virtual controller.
    The default controller mapping is pinned by a test (commit 36b5510); the keyboard defaults
    by `settings_keys_test`.
  - Movies play at the correct speed (Nikki PSA 11.38 s against 11.40 s in the file), which
    is accepted as the correct cadence for M5. **Known limitation:** frame pacing isn't
    smooth. 20–28% of movie frames show for 1 or 3 vblanks instead of 2 (measured with a probe
    that may inflate it). Smoothing is deferred to polish unless it causes gameplay issues.
  - XMA decodes without errors (3,749 frames, 0 skipped). XMA1 against XMA2 isn't settled.
  - Hand test 2 (2026-10-23, by the user): keyboard navigation, `[keys]` rebinding in game
    surviving a restart, audio quality, movie lip-sync and audio latency on the built-in
    speakers all pass (FEASIBILITY §6a).
  - **Physical controller: Deferred — not verified.** By the user's decision (2026-10-23) it is no longer an
    alpha gate. It is to be tested by hand once the 8BitDo is available, and never recorded as a
    pass before then. The SDL virtual controller (menus, hot-unplug with `DETACH`/`ATTACH`, commit
    7d74acf) is automated evidence for the input path, not proof that physical controllers work.
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
  - Menus are navigable with a controller and with the keyboard. *(The physical controller:
    Deferred — not verified, not an alpha gate, user decision 2026-10-23. The keyboard remains required.)*
  - The default keyboard and controller mappings are recorded and pinned by passing unit tests.
  - Keyboard rebinding through Settings > Controls and `[keys]` still works and persists.
- **Verify:** a windowed run with the audio log (`RECARBON_AUDIO_LOG`): no underruns over 5 minutes
  in the front end, and the intro movie's frame times in the log. Navigate by hand with a pad (deferred)
  and with the keyboard, scripted with `RECARBON_INPUT_SCRIPT` where possible.

## M6 — First boot to free-roam / race load *(Open until M6.6 is done)*
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
- **Evidence so far** (FEASIBILITY §6b):
  - **Profile and save:** an alias save is created at Start Career (286,764 B) and reloads after
    a restart with its hash unchanged.
  - **Race loading:** the prologue reaches control of the car. A Quick Race loads from the front
    end (§6c).
  - **Stability:** hand test 3 (2026-10-23, by the user): 15 minutes in a row in free-roam after
    the prologue chase. The log shows no crash, hang report, strict stub or watchdog report, and
    a clean exit. Before that, the render-queue race was fixed (f4cf407), and two 16-minute
    soaks ran clean.
  - Each criterion has evidence. M6 stays open until M6.6, the written new-profile route
    (user decision, 2026-10-23).

## M7 — Playable racing *(In progress: the current milestone, user decision 2026-10-23)*
Input as in M5: SpeedBreaker's keyboard rebinding and `[keys]` keep working. No new
customization. The controls criterion is judged on the default mappings.
- **Accept** (each verified separately):
  - **Player controls:** steering, throttle, brake, handbrake, nitrous and the camera respond on a
    controller and on the keyboard. *(The physical controller: Deferred — not verified, not an alpha gate, user
    decision 2026-10-23. The keyboard remains required.)*
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
- **Evidence so far** (FEASIBILITY §6c):
  - **AI opponents, and race completion:** passed by hand in hand test 4, and the log agrees.
    - Zone Heat 1 (Circuit): 1st place; cash went from $5,000 to $10,200.
    - The results screen showed, both sessions exited cleanly, and the cash and progress were
      still there after a restart.
    - The log doesn't show whether session 1 wrote the save, and no hash was taken between
      the sessions, so persistence rests on what the user saw after the restart.
  - **Frame pacing:** open. Hand test 4's race averaged 58.5 fps over about 4 minutes. The user
    felt one brief stutter while driving. The log has spikes of two kinds, neither explained:
    about 1 s spent in GPU submission (5 frames during the race), and one where the command
    processor sat idle, like hand test 3's stall (FEASIBILITY §6c).
    - The first kind was reproduced in isolated runs and measured. The command processor waits
      for the Vulkan queue lock while the presenter, holding it, waits in MoltenVK for a free
      CAMetalLayer drawable. Why no drawable is free isn't known.
    - The second kind's cause isn't known either. Neither has been fixed.
    - **Measurement for the 10 minutes:** `RECARBON_FRAME_LOG=<csv>` logs every present without
      changing how frames are shown. `scripts/frame_times.py` reports fps, frame-time
      percentiles, missed-30-fps frames and the thermal state over a racing window, given as
      session seconds or wall-clock start and finish times. Both have only been checked on
      menu runs (they agree with `[perf]`), not yet on a race.
- **Known issues for the alpha** (each still open, not acceptance criteria):
  - Occasional stalls of about 1 s: the presenter waits for a drawable while holding the queue
    lock (FEASIBILITY §6c). To revisit before a public release, or sooner if they become
    frequent.
  - GPU-idle stalls of 250–600 ms, with no lock wait: cause unknown (FEASIBILITY §6c).
  - Frame rate dropping during long sessions on a fanless Mac, seen once (M1, M7 race 2,
    2026-10-25). After about 45 minutes of back-to-back sessions, the 7-lap race ran at 30.5 fps for
    about 75 s, then 21.6 fps to the end. Draws per frame were unchanged, GPU time rose from 31 to
    45–47 ms per frame, and the thermal state was "fair" from 115 s. The cause isn't confirmed; the
    leading hypothesis is GPU clocks reduced on a warm M1. The user didn't notice it. This is M1
    evidence only, and M7.5 stays on the M5.
  - Physical controller: Deferred — not verified (not an alpha gate, user decision 2026-10-23). The
    virtual-controller and hot-unplug tests pass, but they don't show physical compatibility.
  - Release packaging: resolved for a build against `scripts/macos/build_deps.sh`'s libraries,
    which `check_app` passes (M8, d780e34). Development builds still link Homebrew's libraries,
    and no release is published (the LGPL source offer is needed before any binary is
    distributed).
  - Key labels on a non-English keyboard layout: under Myanmar (or another non-Latin layout)
    Settings > Controls draws letter keys as '?' (U+FFFD), because the menu's font lacks the
    script. Binding such a key is refused safely and nothing is saved (verified, 4a68b32), but
    the labels themselves aren't fixed. Gameplay input is unaffected.
  - **Still pending:**
    - the driving controls on the keyboard: throttle, brake, steering and camera are hand-verified in
      the M1 races of 2026-10-25, but handbrake and nitrous aren't yet (the physical controller is
      deferred);
    - physics against the Xbox 360 version (no lap time or top speed compared yet);
    - frame pacing over 10 minutes of racing on the M5 (the M1 races are supplementary);
    - audio and picture: reported fine by the user on the M1, with sync resting on that report;
    - the written route repeated 3 times (2 of 3 done on the M1: races 1 and 2, 2026-10-25).

## M8 — Polish and distribution *(In progress: started 2026-10-23 by the user's approval, B1 packaging first)*
- **License compliance:** `COPYING`, `NOTICE`, `THIRD_PARTY_NOTICES.md` and `LICENSES/` are kept
  complete. Any binary release also needs the LGPL source offer (FFmpeg, libmspack). No binary
  is published, and no claim of legal clearance is made.
- **Status decision** (user, 2026-10-23): M8 started, packaging (B1) first. Input profiles (B2)
  wait for the user's approval of the revised preset tables. Decisions are in
  `reference/M8_PLAN.md`:
  - macOS 27+;
  - Vulkan SDK 1.4.363, SDL3 3.4.18 and glslang 16.6.0, built from SHA-256-checked upstream
    sources;
  - Homebrew's MoltenVK 1.4.2, pinned by its binary SHA-256;
  - `check_app.py` kept strict;
  - the app verified on both Macs.
- **Groundwork kept as evidence:** `make_app.py` and `check_app.py` ported (e3173ce), the
  second-Mac bundle check (b063ffa), and check 3 measured (c0d9604).
- **Evidence so far (B1, 2026-10-23):**
  - **Libraries:** `scripts/macos/build_deps.sh` (77c0bcc, from SpeedBreaker v0.1.8) builds
    Vulkan SDK 1.4.363, SDL3 3.4.18, glslang 16.6.0 and FFmpeg 7.1.1 from 7 SHA-256-checked
    archives. Every library is arm64, macOS 27.0, and the loader names no `/opt/homebrew`.
  - **MoltenVK:** `make_app.py` pins Homebrew's MoltenVK 1.4.2 by its SHA-256 and records its
    bottle (d780e34).
  - **The release app (d780e34) passes `check_app.py`** (checks 1–6, the checks unchanged).
    - **M5:** the installer check 21/21 with the app's binary. A launch with a cleared
      environment loaded all 8 libraries from the app, with 0 `/opt/homebrew` mentions and no
      stubs.
    - **M1:** the signature verified, the same loads, and the installer folder cases 9/9.
  - **Mutants caught:** a MoltenVK with other bytes is refused, the Homebrew-linked app still
    fails check 3, and a wrong archive SHA-256 is refused.
  - **Not done:**
    - M8.4, the clean install on a new macOS account (the user creates it);
    - the LGPL source offer, needed before any binary is distributed (none is planned for the
      source release).
  - **The availability gate is untested for target 27.0:** its control API is macOS 15.2, and
    with SDK 27.0 equal to the target there's nothing newer to catch.
- **Evidence so far (B2, 2026-10-24):** input profiles, as approved in `reference/M8_PLAN.md`.
  - **Mappings and presets** (16a1f8e): the WASD and Arrows tables, exactly as approved; no key
    on two controls (`profiles_test`, mutant caught). Default is the settings' defaults, M5's
    baseline.
  - **Profile files** (812c921): `profiles/keyboard.toml` and `controller.toml`, written
    atomically (the shared writer, bddd7b3) and validated by parsing the text back before
    writing. A failed write changes nothing.
    - Startup never changes `[keys]`. Migration keeps a customized `[keys]` as "Custom", and a
      `[keys]` the profiles didn't know of is kept as "Custom (settings.toml)".
    - An unreadable file is left untouched, and profiles are off for that session.
    - Built-ins are never edited: an edit makes "<name> (custom)".
    - `profile_store_test` covers it; three safety mutants are caught.
  - **In game** (ab576ad, 577a259):
    - Settings > Controls has a Keyboard Profile row, in effect at once, and Reset page picks
      Default.
    - Settings > Controller rebinds buttons only. A button already in use swaps, and the Back +
      Start chord stays physical.
    - Scripted runs in isolated homes:
      - migration;
      - switching, and persistence after a restart;
      - an edit with WASD active making "WASD (custom)";
      - a corrupt `keyboard.toml` left byte-identical;
      - with no `settings.toml`, the profile restoring the keys;
      - controller rebinding with the virtual pad (east then drives A, south drives B),
        persisting after a restart.
  - **Presets in menus:** `profiles_test` shows each preset's D-pad keys resolve to D-pad
    buttons only, through the input code's own `keymap::Resolve`. Carbon's menus answer D-pad
    buttons (the M6/M7 scripted routes navigate with them). A real key moving a menu under each
    preset is left for a hand test: the scripted menu route didn't reach the main menu here.
  - **Game actions on the virtual controller (2026-10-24, M8_PLAN):** in a Quick Race with a
    manual gearbox, RB changes the camera, LB looks back, the right stick shifts up and down,
    the Left Trigger reverses from a standstill and Start pauses. Nitrous, Speedbreaker and the
    Handbrake weren't shown by any control and stay unverified.
  - **Not verified:** the physical controller (deferred), and controller rebinding on hardware.
  - **Reset to default and the M5 baseline (complete, 2026-10-24):** the user approved treating
    label-only changes as compatible with the unchanged baseline, provided key values, expected
    mappings and behavioural assertions are intact. Checked against M5's `settings_keys_test.cpp`
    (592fda3):
    - The only later change to the file is 0dc8bf1: 4 rows, where only the label changed
      (Throttle, Brake, Steering Left, Steering Right). The id, config key and both default keys
      are unchanged in each row, and so are `settings.cpp`'s defaults (w/none, s/none,
      a/left, d/right).
    - M5's file, built unmodified against HEAD (d1d914f), fails 4 checks, all of them the label
      check (line 107) on those 4 rows. Every key-default, mapping, row-order, parse and save
      check passes.
    - The other baseline tests are unchanged since they were written and pass:
      `key_map_test`, `key_names_test` (592fda3), and `pad_map_test` (36b5510, the controller's
      M5 baseline). The test's CMake entry gained `atomic_file.cpp` (bddd7b3, a source split, not a
      test change).
- **Test tooling (2026-10-24):** `scripts/install_check.sh` stops every process it starts.
  - **The bug:** it launched the game as `(cd … && game &)`, so `$!` was a subshell's pid and
    SIGTERM never reached the game. Three launches were left running for 1–2.5 hours at 86–91%
    CPU, during the M5 resolution measurements. 5b1c72d launched the game directly, which
    still had gaps:
    - a game that ignored SIGTERM hung the script in `wait`;
    - a descendant outside the game's process group was never looked for.
  - **Now:** the launch gets its own process group. The script records the group and its
    descendants while the game runs (pid and start time, so a reused pid is never signalled),
    and marks every game it starts with `RECARBON_TEST_OWNER`. On any exit (done, Ctrl-C,
    SIGTERM) it sends SIGTERM, then SIGKILL after 10 s, and checks that nothing is left
    (`launch_cleaned_up`).
  - **macOS details found on the way:**
    - `ps -e` means every process; environments need `-E`.
    - `ps -E` shows no environment for Apple's own binaries, such as `sleep`, so the marker
      alone can't find them.
    - A process whose own arguments contain the marker matches itself.
    - A copied `/bin/sleep` is killed at launch, so decoys must be built locally.
  - **Verified on the M5, with two decoy `ReCarbon` processes (one unmarked, one with another
    run's marker), both alive after every case, and nothing test-owned left after each:**
    - **Normal run:** 18/18 pass.
    - **The subshell launch restored (mutant):** `launch_quits` fails ("never logged the
      SIGTERM"), and the cleanup still stops the shell and the game.
    - **A launch that ignores SIGTERM**, with two descendants in its group and one that left
      it: `launch_quits` fails, all four are killed after 10 s, and none is left.
    - **The script interrupted during the launch** by SIGINT and by SIGTERM: exit 130, nothing
      left.
  - **Unit tests ported from SpeedBreaker v0.1.8** (a544d36..8ad356e, 2026-10-24): 25 tests
    and FSR 1's 3 SPIR-V compiles, adapted to Carbon where the cases stood for MW. Each was
    checked with planted bugs. ctest grows from 20 to 48 entries, and the same commit passes
    48/48 on the M5 (10 repeats) and on the M1. Provenance, mutation results and the
    remaining gaps are in `reference/TEST_COVERAGE.md`. A read-only proposal of what could go
    upstream is in `reference/UPSTREAM_TEST_PROPOSAL.md` (nothing sent). No acceptance
    criterion changed.
- **Do:**
  - Parametrize the installer: Carbon manifest, title ID, ReCarbon names.
  - Package `ReCarbon.app` (bundle ID `org.retronative.recarbon`, via `make_app.py`) with license notices and LGPL sources.
  - Optional: render scaling and widescreen hooks, reintroduced per Carbon render target.
  - Input customization, extending SpeedBreaker's existing rebinding on M5's binding layer
    (ARCHITECTURE §3.6):
    - Separate persistent profiles for the keyboard and the controller, stored in the user
      folder, each holding user-defined mappings (rebinding extended to the controller).
    - Keyboard presets: WASD and Arrows. (2026-10-24, user decision: the WASD preset is merged
      into Default, which is shown as WASD; a new player starts on it.)
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

## Future improvements (post-alpha backlog, from SpeedBreaker v0.1.8)
Backlog, not milestone criteria. Details and evidence: `UPSTREAM_SPEEDBREAKER_0.1.8.md`.
Nothing upstream counts as verified for Carbon until ReCarbon reproduces it.
1. Port SpeedBreaker's 16 KB-host page-release fix (`kernel/vmem.cpp` `ResetHostRange`, with
   its test). ReCarbon has the latent bug. Value high, risk low, effort low.
2. Find whether Carbon's EA sound library has MW's CPU-budget voice expulsion (issue #35: sounds
   drop until re-triggered when the mixer thread is starved). If so, port the "never expel for
   time" hook with Carbon addresses and a test. Value high, risk medium, effort medium.
3. Virtual-pad `DETACH`/`ATTACH` steps for controller hot-unplug tests. Value medium, risk low,
   effort low.
4. World-step timed scripted input, once Carbon's world stepper is found. Value medium, risk
   low, effort medium.
5. Generated-code assertions in the test of every title hook. Value medium, risk low, effort
   low.
6. Vertex-range narrowing for dynamic buffers, only if racing profiles show guard stalls.
   Value low–medium, risk medium, effort medium.

Optional future features: Smooth Motion (120 FPS) and USB steering wheels. Excluded from
planning: split screen and multiple controllers.
