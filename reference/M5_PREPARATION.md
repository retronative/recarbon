# M5 preparation — audio, movies, input (read-only audit)

Written 2026-10-22, during M4, before M5 is approved. It plans the work and changes nothing:
no runtime code changes, and no milestone status changes. Every finding is marked **confirmed**
(seen in code, generated code or logs) or **assumed** (inferred; to be checked first in M5).

## 1. How sound reaches the speakers (SpeedBreaker's path, unchanged in ReCarbon)

| Stage | Where | What it does |
|---|---|---|
| Game sound engine | guest code (Carbon names its thread `RWAudioCore Dac`) | Mixes voices in guest code. |
| XMA decode | `runtime/apu/xma.cpp` (hooks `XMA*`) | Kernel-level XMA contexts. One FFmpeg XMA1 decode pass per `XMAEnableContext`, frame by frame, into the guest's output ring (big-endian s16). |
| Movie audio (WMA Pro / WMA v2) | guest code (the XDK's XMV player) | Decoded in software by the player's threads. Confirmed by elimination: the bundled FFmpeg is XMA-only (FEASIBILITY, "Toolchain and inputs"), so there is no host WMA path, yet the mix has sound during movies. |
| XAudio render driver | `runtime/apu/audio.cpp` | Calls the game's render client every 256 samples (5.33 ms) on its own cadence (`audio_cadence.h`). The client returns 5.1 planar big-endian float through `XAudioSubmitRenderDriverFrame`. |
| Output | SDL3 audio stream | 5.1 float at 48 kHz, downmixed by SDL. The queue is held near two device buffers. `RECARBON_MUTE=1` keeps the cadence but opens no device. |

The render callback is the game's audio clock. The XMV player times movie frames against audio
played (measured for MW's player; the same XDK component, assumed for Carbon), so movie A/V sync
depends on this cadence, not on the output device.

## 2. Findings

### Confirmed
- **Carbon's audio imports are all implemented except one.** It imports 22 functions, 16 `XMA*` and 6
  `XAudio*` (generated `ppc/` headers). All are hooked except `XAudioQueryDriverPerformance`,
  which is still a strict stub (`import_stubs.cpp:113`): reaching it stops the game (exit 3).
  It hasn't been reached in any M3/M4 launch. `XamVoice*` are hooked as "no headset".
- **Carbon uses fewer XMA calls than MW.** It doesn't import `XMASetLoopData`,
  `XMASetInputBufferReadOffset` or `XMABlockWhileInUse`, so it must loop and seek by other means
  (rewriting the context data, or software). The loop/seek paths in `xma.cpp` may then be
  unused, or used differently.
- **The render client registers:** `[audio] render client 82A5B6B0 (arg 4000593C) registered`
  in every M4 launch.
- **The game's mix carries sound during movies.** The m4g10 hang report (2026-10-22): "the
  game's mix last had sound 20.5 s ago", that is, until the hang. Movie audio goes through
  XAudio in guest code.
- **Movie audio uses the XAudio voice lock.** The M4.8 hang was XAudio's voice yield
  (`sub_82A4E7A8`) reading a critical section's `RecursionCount`. That fix (big-endian
  `XRTL_CRITICAL_SECTION`) is a prerequisite for any M5 audio verification.
- **No Carbon title hooks exist** (`[title] ... movie pacing ... inactive`). SpeedBreaker's
  movie pacing hooks MW's XMV render call (`sub_826E8CF8`) and D3D swap. Those addresses mean
  nothing in Carbon. Without them, movies still play, but about 7–10% of frames show for 1 or 3
  vblanks instead of 2 (MW's measurement). Carbon's player is the class with vtable
  `0x82010C18` (init `sub_827C9840`, teardown `sub_827C9B30`, decoder threads
  `sub_827C8B78/BA8/BD8/C08`), found during M4.8.
- **Carbon's data is packed.** Everything is in `NFS/ZZDATA0–10.BIN`, indexed by `ZDIR.BIN`,
  so audio assets can't be inspected as loose files. The XMA version and stream layout have to
  be observed at runtime.
- **The input imports are all implemented:** `XamInputGetState`, `XamInputSetState`
  (rumble), `XamInputGetCapabilities` and `XamInputGetKeystrokeEx`.
- **Three XamUser imports are still strict stubs:** `XamUserAreUsersFriends`,
  `XamUserCreateAchievementEnumerator` and `XamUserCreateStatsEnumerator`. The front end may
  call them (profile, achievements and leaderboards screens).
- **Keyboard rebinding works as in SpeedBreaker:** Settings > Controls and the `[keys]` table,
  two keys per control. `settings_keys_test` already pins the keyboard defaults (W/S triggers,
  A/D and arrows steer, Space = A, Backspace = B, Enter = Start, Esc = Back, and so on).
- **The controller mapping isn't pinned.** It's a fixed table inline in
  `hid/input.cpp` (`SDL_GAMEPAD_BUTTON_* → XAMINPUT_GAMEPAD_*`) that no test covers.
- **Back is delayed on the controller.** It's held back from the game until it's clear it
  isn't the Back + Start chord for SpeedBreaker's settings menu (250 ms, or released and
  replayed as a tap).
- **The harness mutes audio.** `scripts/run_attempt.sh` sets `RECARBON_MUTE=1`, so no M3/M4
  launch has opened an output device.

### Assumed (check first in M5)
- **XMA version.** `xma.cpp` defaults to XMA1 because MW (2005) uses it. Carbon (2006) may use
  XMA2 (`RECARBON_XMA=2` switches). The wrong one decodes as noise or fails.
- **EA's stream-player ring semantics.** `xma.cpp` copes with MW's player: it reads write ==
  read with valid clear as full, and queues sub-streams. Carbon's `RWAudioCore` may be a newer
  player with different polling. A mismatch shows up as cut-outs or stalled voices.
- **The default key help text is MW's** ("Gas in the game's default layout"). Carbon's 360
  controls are assumed similar (RT gas, LT brake), not verified in-game. The keys themselves
  stay as they are (they're the baseline); only the descriptions may need Carbon wording.
- **Movie cadence.** At 60 Hz without pacing hooks, the 29.97 fps movies are expected to judder
  slightly. That's polish, not an M5 blocker, unless the acceptance's "correct cadence" is
  judged strictly.

## 3. Risks

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| A1 | `XAudioQueryDriverPerformance` reached on first unmuted or long runs | Medium | Hard stop (exit 3) | Implement from Xenia's semantics as a reviewed import, if reached. |
| A2 | XMA1/XMA2 mismatch | Medium | No music or noise | A/B with `RECARBON_XMA=2`, `RECARBON_XMA_STATS` decode errors, `RECARBON_AUDIO_DUMP`. |
| A3 | Ring semantics tuned to MW | Medium | Cut-outs, stuck streams | `RECARBON_XMA_LOG` on the music context; compare with the comments in `xma.cpp`. |
| A4 | Front-end XamUser stubs | Medium | Hard stop in menus | Log which one is reached, and implement it minimally with cited semantics. |
| A5 | Muted harness hides device problems | High (by design) | M5 acceptance unproven | Unmuted runs for every audio criterion. |
| A6 | Movie judder without pacing hooks | High | Cosmetic | Find Carbon's XMV render call from the vtable at `0x82010C18`; port the pacing logic with new addresses. |
| A7 | More guest-visible kernel structs stored host-endian (like M4.8) | Low | Rare hangs | Done for `xbox.h` (ARCHITECTURE_REVIEW F17). Extend to the guest structs in `runtime/kernel/*.h` if a symptom appears. |
| I1 | The Back + Start chord delays Back in Carbon menus | Low | Feel | Note it; keep the behaviour (it's the existing baseline). |

## 4. Ordered debugging plan (once M5 is approved)

1. **Baseline, unmuted.** One windowed launch without `RECARBON_MUTE`, with `RECARBON_AUDIO_LOG=1`.
   Expect `[audio]` device, gain and level lines, and no strict stub reached. Record the
   underrun and gap counts.
2. **XMA census.** `RECARBON_XMA_STATS=1 RECARBON_XMA_LOG=1` through the boot movies into the
   front end. Record the contexts created, rates, channels and decode errors, and whether loops
   or seeks occur without `XMASetLoopData`.
3. **XMA1 or XMA2.** If step 2 shows decode errors or noise: `RECARBON_XMA=2`, comparing
   `RECARBON_AUDIO_DUMP` captures of the same front-end span. Decide from errors and levels,
   then from listening.
4. **Movie audio.** Confirm guest decoding (no host decoder): mix level during each boot movie
   with `RECARBON_AUDIO_LOG`. Check A/V sync on the intro movie by eye, and with frame dumps
   against the audio dump's timestamps.
5. **Front-end music, 5 minutes.** `RECARBON_AUDIO_LOG` gives no underruns or gaps over 5
   minutes in the front end (the acceptance criterion).
6. **Movie cadence.** Measure the intro movie's frame times. Only if they fail the criterion,
   find Carbon's XMV render call (player vtable `0x82010C18`) and port the pacing, with
   evidence for each address.
7. **Input.** Navigate the menus with `RECARBON_INPUT_SCRIPT` (keyboard path through the
   virtual pad), then by hand with a real controller and the keyboard. Check that `[keys]`
   rebinding still works.
8. **Baseline mapping tests.** Keep `settings_keys_test` for the keyboard. Pin the controller
   table with a test. That needs the table moved out of `input.cpp` into a header, unchanged:
   the one refactor M5 needs (see `ARCHITECTURE_REVIEW.md`).
9. **Stubs reached.** Implement any strict stub reached on the way (A1, A4) as its own
   reviewed commit, with `scripts/gen_import_stubs.py` rerun.

Diagnostics already available: `RECARBON_AUDIO_LOG`, `RECARBON_AUDIO_DUMP[_FROM|_SECONDS]`,
`RECARBON_XMA_LOG[_CTX]`, `RECARBON_XMA_STATS`, `RECARBON_XMA_SILENT`, `RECARBON_XMA=2`,
`RECARBON_MUTE`, `RECARBON_TEST_AUDIO_DEVICE_STALL`, `RECARBON_INPUT_SCRIPT`,
`RECARBON_VIRTUAL_PAD`, `RECARBON_INPUT_LOG`, and the hang report's audio section.
