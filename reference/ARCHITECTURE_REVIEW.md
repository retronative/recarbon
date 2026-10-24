# ReCarbon — Architecture review for M5–M8

Written 2026-10-22, during M4 (M4.8 verification running). A read-only review of the current
implementation against `ARCHITECTURE.md` and `ROADMAP.md`. It changes no milestone scope,
acceptance criterion or status. Each finding is marked:
- **Confirmed:** seen in the code, the generated code or a log, with the place named.
- **Assumed:** inferred, and to be checked before anything depends on it.

`M5_PREPARATION.md` has the audio, movie and input details. This review only summarizes them.

## 1. Summary

The architecture holds. The generic layers (CPU, kernel, GPU, audio, input, installer) are
SpeedBreaker's, unchanged except for identity, and they already cover most of what M5–M8 need.
The risks are in three places:
1. **The title layer is empty.** Every hook that made MW correct or smooth (render-queue
   fences, frame time, movie pacing, view scaling) is absent for Carbon. M6 and M7 depend on
   finding Carbon's equivalents, and one M7 measurement silently depends on a hook (F7).
2. **Strict stubs on paths M5–M6 will reach.** 166 imports still stop the game when reached.
   That's the right policy, but several sit on front-end, profile and save paths (F9, F10).
3. **Input has the right pieces but not yet the seams M8 needs.** The binding logic is pure and
   tested, but it reads the global settings directly, and the controller mapping is an inline
   table (F12, F13). Two small, behaviour-preserving seams make M8 an extension, not a rewrite.

M4.8 itself was a host/guest data-layout bug: the guest read a field the runtime stored
host-endian. An audit of `xbox.h` found no other live case (F17).

## 2. Implementation against the documents

| Area | Documents say | Implementation now | |
|---|---|---|---|
| CPU | XenonRecomp static recompile, patches 0001–0009, plus 0010 from the pending M4.8 fix | As documented; validated in M1–M3 | Matches |
| Kernel/XAM | SpeedBreaker HLE, strict stubs | 166 strict stubs (`import_stubs.cpp`) | Matches; stubs are M5–M6 risk |
| GPU | PM4/Xenos → Vulkan/MoltenVK, 1x, 16:9 | As documented; EDRAM aliasing fixed (113f1ee) | Matches |
| Audio | XMA → FFmpeg → SDL3; EA ring semantics carry over | Implemented; never run with a device (harness mutes) | Unverified |
| Movies | Guest XMV decode; host pacing hooks with new addresses | Guest decode confirmed indirectly; no pacing hooks | Partial |
| Input | Reuse `hid/` with a Carbon key map; three layers (§3.6) | `hid/` as in SpeedBreaker; layers are logical, not separate code | Partial (F12) |
| Saves/profiles | Content, saves, profile scoped per title (M6) | `XamContentCreateEx`, enumerators, profile settings implemented; UI and device stubs remain | Partial |
| Sync | Find Carbon's render-queue and frame-time hooks (M6) | None (`game/title_hooks.cpp` reports all inactive) | Not started (expected) |
| Performance | `-mcpu=apple-m4` or newer after a generic build (§3.5) | No `-mcpu` for macOS arm64 in either CMake file | Gap (F15) |
| Packaging | `make_app.py`, `check_app.py` (M8) | `scripts/macos/` not in ReCarbon; manifest already Carbon's | Partial (F16) |

## 3. Findings

### Audio and video
- **F1, confirmed: audio has never been exercised with an output device.**
  `scripts/run_attempt.sh` sets `RECARBON_MUTE=1`, which opens no device. The render cadence is
  the same either way, so game-side behaviour is covered. The device path (SDL stream, queue
  target, stall handling) isn't.
- **F2, confirmed: `XAudioQueryDriverPerformance` is a strict stub** (`import_stubs.cpp:113`).
  It hasn't been reached so far.
- **F3, assumed: XMA1 versus XMA2.** The decoder defaults to XMA1 (MW). Carbon's version is
  unknown: its data is packed in `ZZDATA*.BIN`.
- **F4, assumed: EA's stream-player ring semantics.** `xma.cpp`'s ring rules are tuned to MW's
  player. Carbon's audio core (`RWAudioCore`) may poll differently. Carbon also imports neither
  `XMASetLoopData` nor `XMASetInputBufferReadOffset`.
- **F5, confirmed: no movie pacing hooks.** Movies play, but the 1/3-vblank judder that
  SpeedBreaker fixed for MW is expected. Carbon's XMV player class is known (vtable
  `0x82010C18`, from M4.8), which is the starting point.
- **F6, confirmed: the XAudio voice lock is on the movie-teardown path** (M4.8). Movie playback
  and teardown are the most lock-heavy guest audio path seen so far.

### Gameplay synchronization and performance
- **F7, confirmed (hidden dependency): M7's frame-pacing criterion needs a source of frame
  times.** `GetFrameTimeStats()` returns zeros without a Carbon frame-time hook
  (`game/title_hooks.cpp`). M7 asks for "frame-time logs over 10 minutes". Either M6's
  frame-time hook (ROADMAP M6 "Do") must land first, or the presenter's host-side present
  times must be logged as the measure. Decide which before M7, so it isn't found at the end.
- **F8, confirmed: no render-queue fence hooks.** MW needed 7. Carbon shows no symptom yet
  (FEASIBILITY R4), but M6's 10-minute free-roam stability criterion is where weak ordering
  would show. Symptoms would be rare and look like M4.8: an intermittent hang or corruption.

### Saves and profiles (M6)
- **F9, confirmed: these are implemented** (from SpeedBreaker/Unleashed): `XamContentCreateEx`
  (saves under the ReCarbon user folder, per save name), the content enumerator, and
  `XamUserRead/WriteProfileSettings` (persisted to `profile_settings.bin`).
- **F10, confirmed (stubs) / assumed (that they're reached): still strict stubs on likely M6
  paths:**
  - `XamShowKeyboardUI` (profile name entry), `XamShowMessageBoxUIEx` (save prompts) and
    `XamShowSigninUIp`;
  - `XamContentCreateDeviceEnumerator` (storage device selection);
  - `XamUserCreateAchievementEnumerator`, `XamUserCreateStatsEnumerator` and
    `XamUserAreUsersFriends`.

  Whether Carbon calls each one, and where, is assumed. A strict stop names the first one
  reached, so they surface one at a time.
- **F11, assumed: save/profile scoping is per app, not per title.** It's the ReCarbon user
  folder, which is effectively per title while ReCarbon runs only Carbon.
  `profile_settings.bin` isn't keyed by title ID. That's fine for one title; note it for any
  shared runtime.

### Input (M5 baseline, M8 profiles)
- **F12, confirmed: the three layers of ARCHITECTURE §3.6 are logical, not separate code.**
  `hid/input.cpp` samples SDL, applies deadzones, resolves keys and builds `XAMINPUT_STATE` in
  one per-frame function. The binding step itself is pure and tested (`keymap::Resolve`,
  `key_map_test`). The XInput output is byte-swapped by hand and correct (`input.cpp:876-883`).
- **F13, confirmed: the keyboard binding reads the global settings directly.**
  `settings::GetInt(kEntries[i].id)` runs per control per frame (`input.cpp:645-650`), so a
  mapping isn't a value that can be swapped. The controller mapping is an inline
  `SDL_GamepadButton → XAMINPUT_GAMEPAD_*` table with no test. M8's profiles, presets and
  controller rebinding need both as values.
- **F14, confirmed: the keyboard baseline is pinned already, the controller one isn't.**
  `settings_keys_test` checks the keyboard defaults. Its help text is MW's ("Gas in the game's
  default layout"); the bindings themselves are the baseline and stay.

### Build and packaging
- **F15, confirmed: no CPU tuning flag on macOS arm64.** Only iOS (`-mcpu=apple-a15`) and
  x86-64 get one. Which CPU the compiler then targets is assumed (likely `apple-m1` for the
  arm64-apple-macos triple), and not measured. ARCHITECTURE §3.5 plans `apple-m4` after a
  generic build. The thermal-aware frame-rate policy that §3.5 relies on is present
  (`video/frame_rate_policy.*`).
- **F16, confirmed: packaging is partly there.** The installer's manifest is already Carbon's
  (`install/manifest.inc`: title `0x454107EC`, 78 files). `scripts/macos/` (`make_app.py`,
  `check_app.py`) wasn't imported. The installer tests that assert MW names
  (`install_folder_test`, `portable_test`) weren't ported (`tests/CMakeLists.txt`), so the
  installer has no regression coverage yet.

### Cross-cutting
- **F17, confirmed: the guest data-layout audit is clean apart from M4.8.** In
  `XenonUtils/xbox.h`, the remaining host-endian fields are single bytes (dispatcher header),
  hand-swapped input structs, or the `XXOVERLAPPED` `InternalLow/High/InternalContext` aliases,
  which the runtime doesn't use (it uses the big-endian `Error/Length`). Also confirmed: the
  critical section's `OwningThread` holds r13 (the PCR), not the KTHREAD pointer, and
  `LockCount` isn't maintained. Assumed harmless: not observed to matter.
- **F18, confirmed: hang reports are the strongest diagnostic.** M4.8 was solved from one
  report's guest registers and stacks. A first reading of the report concluded that the decoder
  threads had exited; the raw stacks showed thread 3 alive in `RtlEnterCriticalSection`. Read
  the raw stacks before any summary.
- **F19, confirmed: the harness covers the boot path only.** It runs 40 s from launch, muted,
  with no input. M5–M7 criteria need longer, unmuted, input-scripted runs. Without a scripted
  route, every regression check becomes manual.

## 4. Regression risks

| Risk | Where it would show | Guard today | Gap |
|---|---|---|---|
| A runtime change breaks the boot path | M3/M4 launches | `run_attempt.sh` ×N, `ctest` | None for boot |
| Audio regressions | M5 | none (muted) | Unmuted check with `RECARBON_AUDIO_LOG` thresholds |
| Input default drift | M5–M8 | `settings_keys_test` (keyboard) | Controller table untested |
| Profile/save format change | M6, M8 | none | Round-trip test once M6 writes saves |
| Rare guest-ordering hangs | M6–M7 | hang reports, watchdog | Need long runs to surface |
| Installer and packaging | M8 | none (tests not ported) | Port the adapted installer tests |

## 5. Recommendations (minimal, by impact and urgency)

| # | When | Recommendation | Why | Size |
|---|---|---|---|---|
| R1 | M5, first | Extend `run_attempt.sh` (or a sibling) for unmuted, longer runs with `RECARBON_INPUT_SCRIPT`, keeping today's default | F1, F19: M5–M7 criteria can't be checked otherwise | Small, script only |
| R2 | M5 | Move the controller button table out of `input.cpp` into a header, unchanged, and pin it with a test | F13, F14: the controller baseline; also the controller-mapping seam M8 needs | Small, behaviour-preserving |
| R3 | M5 or M8 start | Have `input.cpp` read keys through a keyboard-mapping value (two scancodes per control), built from `[keys]` as today | F13: profiles and presets then swap a value, with no rewrite | Small; `settings_keys_test` and `key_map_test` must pass unchanged |
| R4 | Before M7 | Decide the frame-time source: Carbon's frame-time hook (M6) or host present times | F7: hidden dependency of an M7 criterion | Decision, then small |
| R5 | M5–M6 | Implement strict stubs only as they're reached: one reviewed commit each, with cited semantics and `gen_import_stubs.py` | F2, F10: keeps the strict policy | Per stub |
| R6 | M6 | Look for Carbon's render-queue fences early, while the free-roam soak runs | F8: the M6 stability criterion | Medium |
| R7 | M8 | Port `scripts/macos/` and the installer tests, adapted to ReCarbon names | F16 | Medium |
| R8 | Any (cheap) | Measure a macOS `-mcpu` choice before adopting it (§3.5) | F15 | Small |
| R9 | M5 | Carbon wording for the key help text, bindings unchanged | F14 | Trivial |

**Not recommended:**
- splitting `hid/` into new modules before M8;
- a profile manager or any customization UI before M8;
- adding fence hooks speculatively without a symptom or a found site.

Each would be refactoring ahead of evidence.

## 6. M8 input customization without rewrites

With R2 and R3, M8 adds, and doesn't change:
- profiles are stored mapping values (one keyboard, one controller);
- presets (WASD, Arrows) are built-in mapping values;
- switching swaps the active value;
- reset-to-default restores the baseline values, which the M5 tests pin;
- `[keys]` migrates by building the first keyboard profile from the same code that builds
  today's mapping.

The existing Settings > Controls page edits the active keyboard mapping as it edits `[keys]`
today. Without R3, M8 would first have to untangle per-frame `settings::GetInt` reads: that's
the one place a rewrite would otherwise be needed.

## 7. Planning documents

No scope or criteria change. One wording fix is enough: ARCHITECTURE §3.6 now says the input
"stays" in three layers, while today they're logical layers in one file (F12). It should say
the layers are to be kept separable, with R2 and R3 as the seams. That edit goes with this
review.
