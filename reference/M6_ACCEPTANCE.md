# M6 acceptance record (first boot to free-roam / race load)

The state of each M6 criterion (ROADMAP M6) and its evidence, last updated 2026-10-26. Objective
checks and the tester's reports are kept apart. The criteria are unchanged.

**Decision (user, 2026-10-26): M6 is closed for the alpha.** M6.6, the written new-profile
route, is accepted on the M8.4 fresh-account sessions below. Not recorded as passed: free-roam in
the same session as that route (not done; free-roam stability rests on M6.5), a save hash
across the restart (none taken), and Carbon's frame-time equivalent (not found).

## Status

| Criterion | Status | Evidence |
|---|---|---|
| M6.3 Profile and save: created, written, reloaded intact after a restart | Completed | An alias save created at Start Career (286,764 B) reloads after a restart with its hash unchanged, in an isolated test home (FEASIBILITY §6b). Career progress kept after a restart in hand test 4 (M7.4). |
| M6.4 A race or free-roam loads from the front end to control of the car | Completed | The prologue reaches control of the car (hand test 1); a Quick Race loads from the front end (FEASIBILITY §6c). |
| M6.5 No deadlock or crash for 10 minutes in free-roam | Completed | Hand test 3 (2026-10-23, by the user): 15 minutes in a row in free-roam after the prologue chase; no crash, hang report, strict stub or watchdog report, clean exit. Two 16-minute soaks clean after the render-queue fences (f4cf407). |
| M6.6 The written route (new profile → free-roam → race load, then a restart), logs and save recorded, watchdog reports empty | **Accepted for the alpha** (user decision, 2026-10-26) | The M8.4 sessions below: new profile, race load, restart, save found and opened. Free-roam wasn't part of them and is covered by M6.5. |

## M6.6: the route as run (M8.4, MacBook Air M5, 2026-10-25)

Fresh macOS account `gamtest` (UID 502, created by the user), RC 282557c's app (755b1c5 in the
logs; `reference/M8_ACCEPTANCE.md` explains the ids). The user copied the logs to
`/Library/GameISO/logs/` and the save to `/Library/GameISO/save/`, read here read-only.

- **Session 1** (`2026-10-25_23-47-12.log`): the installer copied 78 files (5.74 GB) from the
  ISO; the game started at 50.4 s. A new profile save was created at 102.0 s
  (`rmcsave:\ALIAS_ABO … (created)`), the race thread ran from 120.6 s, a race was driven, and
  the session ended with Quit Game (349–350 s). The save file was opened again at 330.9 s.
- **Session 2** (`2026-10-25_23-53-06.log`), after a relaunch: no "ended without quitting"
  notice; `[xam] found save "ALIAS_ABO"` at 1.7 s, the save opened at 20.1 s, the race thread ran
  from 68.2 s, and the session ended with Quit Game (334.6–335.6 s), a clean exit.
- **Watchdog and errors:** in neither session a watchdog hang report, strict stub, device loss
  or crash. The only watchdog line beyond the startup banner is a 2.5 s "without a new frame"
  notice at 52.9 s of session 1, while loading right after the install.
- **The save** (the user's copy): `ALIAS_ABO/ALIAS_ABO` 286,764 B (modified 23:52, during
  session 1), with `ALIAS_ABO.xcontent` 308 B. SHA-256 taken on 2026-10-26:
  `f4e7dd051faa265ddc930a129df059a24a5e385812bc30bfe47a955369757232`. No hash was taken between
  the sessions, so "intact" rests on the relaunch finding and opening it.
- **Not in the route:** free-roam. By the user's decision the route is accepted for the alpha
  on this evidence, with free-roam stability covered by M6.5 (hand test 3).

## Still open (not acceptance criteria)

- M6.2's frame-time equivalent: not found in Carbon. The render-queue ARM fence hooks are in
  (f4cf407). M7's frame pacing was accepted on its own evidence (`reference/M7_ACCEPTANCE.md`).
