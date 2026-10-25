# M7 acceptance record (playable racing)

The state of each M7 criterion (ROADMAP M7) and its evidence, last updated 2026-10-25. Objective
measurements and the tester's reports are kept apart. Nothing here weakens a criterion: M7.5 is
judged on the MacBook Air M5, and the MacBook Air M1 results are supplementary (user decision,
2026-10-25).

## Status

| Criterion | Status | Evidence |
|---|---|---|
| M7.1 Player controls (keyboard; the physical controller is deferred) | In progress | Throttle, brake and steering drove three full races; camera (E), handbrake (Space) and Speedbreaker (Left Shift) by hand. Nitrous untested: the stock RX-8 has none. |
| M7.2 Vehicle physics | Positive human evidence; Xbox 360 equivalence **unverified** | M5 race (2026-10-25): 1st in 9:06.14 over 8 laps; steering, handling, braking and collisions felt normal to the tester; the race clock agrees with the log's wall clock within ~1–2%. No Xbox 360 reference exists, so equivalence isn't claimed. |
| M7.3 AI opponents | Completed | Hand test 4 (2026-10-23). In the M1 races the 7 opponents start and race (screenshots); their finishing wasn't recorded. |
| M7.4 Career race and save | Completed | Hand test 4 (2026-10-23). |
| M7.5 Frame pacing, 10 minutes, M5 | **Accepted** (user decision, 2026-10-25) | M5 race: 9.77 min of racing (14 s short of 10; 8 laps is the game's maximum), 50.8 fps mean, no second under 30 fps outside a 3 s Space switch the tester caused, no degradation over the race, thermal "fair". The tester accepted the length. The M1 races don't meet 30 fps (below). |
| M7.6 Audio and picture | In progress | Tester: picture and sound fine, nothing missing (M1), smooth with audio in sync (M5 M7.5 race); audio and visuals normal (M5 physics race). Audio log: no underruns or gaps over 50 ms after startup in any race, except during the M5 Space switch. Sync rests on the tester's report. |
| M7.7 Written route ×3 | Done on the M1 | Three human races with frame-time and session logs. The M5 race is still needed for M7.5. |

## The route

The packaged `ReCarbon.app` at `4d521a7` (binary sha256 `b5a18da0…`), fullscreen, Game Mode on,
audio on, a fresh isolated home with the test profile and a warm shader cache, the WASD (Default)
keys:
- Main Menu > Quick Race > Circuit > Main Street, Laps 7 (Quick Race Options: four rows down,
  five right), stock Mazda RX-8, automatic transmission, other options at their defaults.
- The menus are scripted (`RECARBON_INPUT_SCRIPT`). Start during the pre-race scene skips it, and
  Start a few seconds after GO pauses the race at the line; Start during the countdown is ignored.
  A screenshot confirms the pause before the tester is told to take over.
- Nothing is injected after that, and there is no deadline: the tester quits with Quit Game.
- Logs per run: the frame log (`RECARBON_FRAME_LOG`), the session log (with `[perf]` GPU time and
  thermal state every 120 frames, and audio reports), the Game Mode log and the power state.
  `scripts/frame_times.py` reports each race window.

## MacBook Air M1 races (supplementary), 2026-10-25

| Race | Length | Start state | fps (mean) | Median / p99 frame | Frames > 100 ms | Seconds under 30 fps |
|---|---|---|---|---|---|---|
| 1 (2 laps) | 3.4 min | cool | 32.8 | 32.9 / 52.0 ms | 2 (max 162 ms) | 11 of 203 |
| 2 (7 laps) | 8.8 min | after back-to-back sessions | 23.0 | 48.8 / 81.4 ms | 9 (max 118 ms) | 463 of 528 |
| 3 (7 laps, keys tested) | 14.5 min | after a 25 min cool-down | 21.1 | 49.2 / 99.5 ms | 158 (max 288 ms) | 704 of 871 |

- Every race exited cleanly (exit 0) with no hang, watchdog, strict-stub or device-loss report and
  no new crash report. Game Mode was on in each.
- The tester finished all laps every time and reported no stutters or problems, and didn't notice
  the drop in races 2 and 3.
- **The drop:** each long race started at about 30 fps and fell with time under load (race 2 to
  21.6 fps from about 5 minutes into the session; race 3 to about 16 fps). GPU time per frame rose
  from 31 to 46–62 ms with draws unchanged, and the thermal state was "fair". Throttling on the
  fanless M1 is a hypothesis; no GPU clock is readable on macOS. Recorded as an M1 limitation.

## MacBook Air M5 race (M7.5), 2026-10-25

The same route at 8 laps (the game's maximum: a ninth Right wraps to 1), after a scripted dry run
that re-timed the handover for the M5. Launched at thermal pressure level 1 (warm from the dry
run); thermal "fair" throughout, level 2 at the end; AC power; Game Mode on; exit 0.

| Window | Length | fps (mean) | Median / p99 frame | Max frame | Seconds under 30 fps |
|---|---|---|---|---|---|
| Whole race (219–805 s) | 9.77 min | 50.8 | 16.7 / 35.1 ms | 748.6 ms | 5 of 585 (all in the Space switch) |
| Before the switch (222–355 s) | 2.2 min | 49.7 | 16.7 / 35.2 ms | 96.3 ms | 0 of 132 (lowest 35) |
| After the switch (362–805 s) | 7.4 min | 51.6 | 16.7 / 35.0 ms | 99.2 ms | 0 of 442 (lowest 38) |

- **The only stall:** the tester pressed Ctrl+Arrow on lap 3, switching macOS Spaces. Game Mode
  paused from 20:46:14.5 to 20:46:17.3, exactly the frames of up to 749 ms and the audio gaps of up
  to 95 ms. Attributed to the tester's action, not to ReCarbon.
- **No degradation:** 47–56 fps and 15–19 ms of GPU time per frame from the first lap to the last,
  while the texture cache grew from 857 to 5,918 entries.
- **Tester:** smooth; audio in sync; the same controls as on the M1; lap times seen but not
  recorded here.
- **Decision (user, 2026-10-25):** M7.5 accepted with 9.77 minutes of racing.

## M7.2 physics reference race (MacBook Air M5), 2026-10-25

The same packaged app, isolated test home and route as the M7.5 race: Main Street, 8 laps, stock
Mazda RX-8, automatic transmission, other Quick Race options at their defaults (difficulty Medium,
7 opponents, catch-up on, nitrous on). Paused a few seconds after GO and handed over; the race
clock already ran for those seconds, so lap 1 includes them. Another session's game (Wine) was
stopped for the race at this session's request; nothing else heavy ran.

| | Result |
|---|---|
| Standings (tester's photo) | 1st 9:06.14 (tester); then 9:07.16, 9:20.49, 9:30.47, 9:34.32, 9:36.63, 9:55.45, 10:07.36 |
| Lap times | Not available (the tester has no per-lap times) |
| Game clock against the log | Race clock 546.1 s; the log puts the racing at about 547–558 s (its ~2.3 s resolution, the pre-pause seconds and the finish camera). They agree within about 1–2%: no sign of the simulation running slow or fast. Not a precise clock-rate measurement. |
| Frame pacing while racing | 51.5 fps, p99 35 ms, max 64 ms, no second under 30 fps (9.0 min) |
| Thermal, power | Pressure level 1 at launch, 2 at the end; AC power; Game Mode never paused while racing |
| Tester | Everything felt normal: steering, handling, braking, collisions, audio and visuals; the car was responsive; no significant gameplay problems. Subjective impression (not a performance or physics comparison): driving felt smoother and more enjoyable than SpeedBreaker. |

What this does and doesn't show:
- The race clock and real time agree, the car completed 8 laps against 7 AI opponents who also
  finished, and the finishing spread is plausible.
- The game's own track screen gives a built-in record of 2:07.00 for the default 2 laps (63.5 s a
  lap). The tester averaged 68.3 s a lap, about 7.5% slower, including the standing start. That is
  consistent, not a measurement of physics fidelity.
- **Decision (user, 2026-10-25):** the completed 8-lap M5 race is accepted as positive human
  gameplay evidence for M7.2. Equivalence with the Xbox 360 version is recorded as **unverified**:
  no reference (a lap time or top speed with the same car and track) exists, and none is assumed.

## Investigation: texture cache growth and the M1 frame-rate drop (read-only, 2026-10-25)

- **Confirmed (code):** the `[perf]` "textures" figure counts live cache entries. Entries are keyed
  by guest address, format and layout, and are removed only by `DropTextures`, when a setting that
  reloads textures changes. There is no age, use or memory-budget eviction.
- **Confirmed (logs):** the cache grows while driving, without a plateau, on both Macs: about
  5,000 entries (about 490 MB) after 7 laps on the M1; 466 → 4,537 over 21 minutes of free-roam on
  the M5. It follows distance driven more than time. Whether the entries are stale addresses or
  duplicates isn't known.
- **Not supported as the frame-rate driver:** race 2's GPU time stayed at 46–47 ms while its
  entries doubled, and at about 700 s race 2 had 4,891 entries at 46.9 ms against race 3's 2,973 at
  57.6 ms. In 18 earlier M1 runs with flat entry counts, GPU time still rose over two hours under
  load.
- **Speedbreaker:** ruled out as necessary (race 2 shows the same growth and drop without it); its
  own effect in race 3 is unverified (no activation times are logged).
- **Ruled out:** more draws, CPU-side cost per entry, bursts of new images, device-memory pressure
  (2.4 of 5.5 GB), and the known stall kinds.
- **Decisions (user, 2026-10-25):** the cache growth is a known alpha limitation and its fix is
  deferred until after the alpha; any eviction must first establish GPU in-flight lifetime, fences
  and safe destruction. No further M1 GPU-clock experiment for the alpha.

## Remaining for M7

1. Nitrous by hand (M7.1; needs a car that has it). Kept separate from M7.2.
2. M7.2's Xbox 360 equivalence stays unverified until a reference exists (a lap time or top speed
   with the stock RX-8 on Main Street); not an open test on the ReCarbon side.
