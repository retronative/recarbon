# M7 acceptance record (playable racing)

The state of each M7 criterion (ROADMAP M7) and its evidence, last updated 2026-10-25. Objective
measurements and the tester's reports are kept apart. Nothing here weakens a criterion: M7.5 is
judged on the MacBook Air M5, and the MacBook Air M1 results are supplementary (user decision,
2026-10-25).

## Status

| Criterion | Status | Evidence |
|---|---|---|
| M7.1 Player controls (keyboard; the physical controller is deferred) | In progress | Throttle, brake and steering drove three full races; camera (E), handbrake (Space) and Speedbreaker (Left Shift) by hand. Nitrous untested: the stock RX-8 has none. |
| M7.2 Vehicle physics | Pending | "Drove well" and "physics OK" by the tester; no lap time or top speed compared with the Xbox 360 version yet. |
| M7.3 AI opponents | Completed | Hand test 4 (2026-10-23). In the M1 races the 7 opponents start and race (screenshots); their finishing wasn't recorded. |
| M7.4 Career race and save | Completed | Hand test 4 (2026-10-23). |
| M7.5 Frame pacing, 10 minutes, M5 | **Open** | Not yet run on the M5. The M1 races don't meet 30 fps (below). |
| M7.6 Audio and picture | In progress | Tester: picture and sound fine, nothing missing. Audio log: no underruns or gaps over 50 ms after startup in any race. Sync rests on the tester's report. |
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

1. M7.5 on the M5: one human race of the route above, at least 10 minutes of racing, with the same
   logs, judged on the existing criterion (a stable 30 fps or better, thermal state noted).
2. Nitrous by hand (needs a car that has it).
3. A physics reference: a lap time on Main Street, or the top speed on a straight, against the
   Xbox 360 version.
