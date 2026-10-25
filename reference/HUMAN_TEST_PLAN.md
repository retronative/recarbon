# Human test plan: M8.11, M6.6 and M7 (2026-10-24)

Five launches cover every pending human criterion of M8.11, M6.6 and M7. Each criterion keeps its own
evidence: a named log per session, and wall-clock times written down for each step, so no
result rests on another's. The physical controller stays Deferred.
Nothing here is PASS until the user has run it and the logs agree.

*Status (2026-10-25):* the M7 part was run as the M7 races of 2026-10-25 (three on the M1, two on
the M5), and M7 is closed for the alpha (`reference/M7_ACCEPTANCE.md`). Sessions 1, 2 and the
M8.11 and M6.6 parts weren't run as written (below).

*Status (2026-10-26):* testing ended by the user's decision, and sessions 1 and 2 weren't run as
written. M6.6 is accepted for the alpha on the M8.4 fresh-account sessions instead (new profile,
race load, restart, save found; free-roam covered by M6.5: `reference/M6_ACCEPTANCE.md`). For
M8.11, the user reports playing with the Arrows preset by hand and that it worked (no log of
that session); switching without a restart and a custom profile across a restart rest on
scripted runs and unit tests. Hand test 5's remaining keys (Reverse, Look Back, Pause, the gear
keys with a manual gearbox) stay untested by hand. M6 and M8 are closed for the alpha.

## Before each session (done by Claude, not the tester)
- Nothing ReCarbon left running; the build commit and the binary's SHA-256 recorded.
- For sessions on the real home (3–5): back up `settings.toml` and the saves, with hashes, as
  for hand tests 3 and 4.
- Afterwards: collect the log, hash the save, and check that no new crash or hang report is in
  the home's reports.

## Session 1: a new profile, both presets (M6.6 route, M8.11)
A fresh, isolated home, so a new profile is created and the real home is never touched:

```sh
H=$PWD/game/m3/home-h06 && mkdir -p "$H/Library/Application Support" "$H/Library/Caches"
HOME=$H RECARBON_REAL_HOME=1 RECARBON_MUTE=0 scripts/run_attempt.sh h06a 3600
```

1. **WASD menus (M8.11):** at the first menu, open Settings (Cmd+, or F1) > Controls and check
   that Keyboard Profile reads **WASD** (a new player's profile), and close it. Move through
   the menus with the **arrow keys** only (WASD's D-pad, which also work its left stick), Return
   to accept and Backspace to go back. Note the time and the screens reached.
2. **New profile (M6.6):** create a new profile and start the career, through to control of the
   car. Note the time.
3. **WASD drives (M8.11):** drive briefly: W throttle, S brake, A/D steering (Left/Right steer
   too).
4. **Arrows, without a restart (M8.11):** open Settings and choose **Arrows**. Move through a
   menu with **W A S D** only, then drive with the arrows (Up throttle, Down brake, Left/Right
   steering). Note the times.
5. **Race load (M6.6):** from the front end, load a race and reach control of the car. Note the time.
6. Quit with Quit Game.

## Session 2: restart and hand test 5 (M6.6 reload, hand test 5)
The same home: `HOME=$H RECARBON_REAL_HOME=1 RECARBON_MUTE=0 scripts/run_attempt.sh h06b 1800`
1. **Reload (M6.6):** the profile and career load intact. Note the time. Claude records the
   save's presence, size and hash from sessions 1 and 2.
2. **Hand test 5 (05/05b/05c):** keep the Arrows preset, which is restored from session 1. Its
   D-pad is W A S D alone, so the D-pad's actions aren't mixed with the left stick's. In
   free-roam, press each remaining key one at a time and write down what the game does:
   - Space, Backspace, Left Shift, Left Ctrl, Q, E;
   - W A S D (the D-pad);
   - O L K ; (the right stick);
   - Z and C (the stick clicks).

   This is the evidence for handbrake, nitrous, Speedbreaker, camera and look back. The left
   stick's up/down has no key in either preset, so leave it unmapped unless it is bound to a
   free key here.

## Sessions 3–5: the written M7 route, 3 times (M7.1, M7.2, M7.5, M7.6, M7.7)
The real home with its warm shader cache, on the **WASD** keys (M7 is judged on WASD, the
Default profile).
For run *n* = 1, 2, 3:

```sh
RECARBON_REAL_HOME=1 RECARBON_MUTE=0 RECARBON_FRAME_LOG=game/m3/h07-$n.frames.csv \
  scripts/run_attempt.sh h07-$n 2400
```

- **The route:** a Quick Race circuit. In run 1, choose the track, car, transmission and number
  of laps, so the race lasts **at least 10 minutes**, and write them down. Runs 2 and 3 use the
  same choices.
- **Note per run:**
  - the wall-clock times of the race start and finish;
  - **M7.5:** the Mac on AC power or battery, and any stutter with its time;
  - **M7.1:** steering, throttle and brake, plus handbrake, nitrous and camera on the keys
    found in session 2;
  - **M7.2:** the top speed on a fixed straight, and the lap times, against the Xbox 360
    version where a reference exists;
  - **M7.6:** audio in sync, and any missing world geometry.
- **Claude, after each run:**
  - `scripts/frame_times.py game/m3/h07-$n.frames.csv --log <session log> --wall-from … --wall-to …`
    over the race (fps, percentiles, missed-30-fps frames, the thermal state);
  - the session log's checks.
- **M7.7** is the three runs together. Each run is also full M7.5 and M7.6 evidence on its
  own.

## Not in this plan
- M8.4 (install on a fresh macOS user account) needs its own account and launch.
- The physical controller (Deferred).
