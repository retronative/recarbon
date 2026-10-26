---
title: Known issues
description: ReCarbon alpha's known issues, from the project's roadmap. Kept in sync per release.
---

From ReCarbon's `reference/ROADMAP.md` (M7, "Known issues for the alpha") and the alpha packaging
checks (M8, 2026-10-26). Each is still open.

| Issue | Details |
|---|---|
| Occasional stalls of about 1 s | The presenter waits for a free drawable while holding the GPU queue lock. Being investigated; to revisit before a public release. |
| GPU-idle stalls of 250–600 ms | No lock wait involved. Cause unknown. |
| Hitches in the first race after a fresh install | With an empty shader cache, the first race compiles its pipelines as it goes: on a fresh macOS account the worst frame was 450 ms (a MacBook Air M5). The second session's worst was 100 ms. The cache is kept in `~/Library/Caches/ReCarbon/`, so deleting it brings the hitches back once. |
| Frame rate falls during long races on a MacBook Air M1 | On the M1, races start at about 30 fps and fall as play goes on: to about 21 fps in a 9-minute race and about 16 fps in a 14-minute one, even after a cool-down. The M1 is limited by its GPU from the start, and the GPU's time per frame rises with time under load while macOS reports the thermal state as "fair". GPU throttling on the fanless M1 is the leading explanation but isn't confirmed. A sustained 30 fps isn't claimed on the M1. |
| Texture cache grows during play | ReCarbon keeps every texture it has made until a setting that reloads them changes: nothing is evicted while playing. Driving grows it by roughly 500 MB over 7 laps or 20 minutes of free-roam, on every Mac tested. On current evidence it doesn't cause the frame-rate drop above, and no crash or corruption has been tied to it, but very long sessions use more memory, which matters most on 8 GB Macs. Restarting the game clears it. An experimental eviction (textures unused for a while, once the GPU is done with them) is built in but off by default: a test with a budget well below what the scene needed kept the cache at its budget with no wrong textures, but had more slow frames, so it isn't used until a realistic budget is tested. |
| Physical controller untested | Deferred: no real pad has been tested. The default mapping is pinned by a test, and a virtual controller drives the menus and a race. |
| Development builds link Homebrew's libraries | A build against `scripts/macos/build_deps.sh`'s libraries passes `check_app` (M8). Development builds still name `/opt/homebrew` paths, so Vulkan layers installed there could be picked up. The v0.1.0 release is built against `build_deps.sh`'s libraries. |
| macOS 27 or newer only | Current builds link libraries built for macOS 27.0. |
| No render scaling or widescreen options | Render scaling and widescreen options are not in this alpha. |

## Tested for v0.1.1

On a MacBook Air M5, on top of the v0.1.0 tests below:

- **Played by hand (the published v0.1.1, 2026-10-26):** two Quick Races to the finish, one won and
  one lost, about 9 minutes of driving. The tester found gameplay smooth, with no noticeable
  issues. The logs show mostly 55–60 fps, 1.5% of movie frames off the even two-refresh cadence,
  and a clean exit with Quit Game.
- **Movies:** in the same 120 s of the boot and attract movies, frames shown for 1 or 3 refreshes
  instead of 2 fell from 18.5% to 1.7%; the movies played for the same length.
- **The release candidate:** every automated check passes (the unit tests, the app and disk-image
  checks, the replaceable LGPL libraries, the checksums), and the disk image's window was checked
  in Finder, with ReCarbon copied onto its Applications shortcut.
- **Rebinding under a non-Latin layout:** unit tests only (above).

## Tested for v0.1.0

All on a MacBook Air M5 (24 GB, macOS 27.0.1) unless marked M1.

- **Install from your disc image:** the first-run installer copied and verified 78 files
  (5.74 GB) from an ISO in 49–86 s, then started the game; also on a brand-new macOS user account.
- **Keyboard presets:** the races below were driven on the WASD keys, and the tester reports playing the game with the Arrows preset by hand too, and that it worked (2026-10-26). That rests on the report alone: no log of that session was kept, and the Mac wasn't recorded.
- **Racing:** full Quick Races driven by hand on the keyboard. A 9.8-minute, 8-lap race averaged
  50.8 fps with no slowdown over the race; 1st place against 7 AI opponents who all finished.
  Driving physics feel normal to the tester (equivalence with the Xbox 360 version isn't
  verified).
- **Career and saves:** a career race won, with its progress kept after a restart; a new profile
  created, then found and loaded after a relaunch.
- **Audio and movies:** music, engine and movies with sound; no audio underruns in the race logs.
- **Game Mode** turns on in fullscreen.
- **Clean exit** with Quit Game; no crash, hang report or GPU device loss in any of the release
  candidate's sessions. More numbers: [Performance](performance.md), [Supported hardware](hardware.md).
- **MacBook Air M1** (supplementary): three full races finished; they start at about 30 fps and
  slow down the longer you play (above).

## Still to be tested

Testing for the first alpha ended on 2026-10-26. These weren't verified, and are left as they are:

- **A MacBook Air M1 install of the release candidate:** the M1 ran three full races of an earlier build (above), but the release candidate wasn't installed and played there.
- **Game Mode on a fresh account:** Game Mode turned on in the tester's main account; on the fresh test account it wasn't captured.
- **Rebinding under a non-Latin keyboard layout** (new after v0.1.0): Myanmar, Thai and other layouts in
  another script can now rebind keys. This is covered by unit tests only; it hasn't been tried in the game.
- **Keyboard profiles by hand:** a custom profile surviving a restart, and switching profiles without a restart, were checked by scripts and unit tests only. (The Arrows preset was played by hand: above.)
- **Keyboard controls in a race:** the driving controls are hand-tested on the WASD keys: Throttle (W), Brake (S), Steering (A/D), Change Camera (E), Handbrake (Space), Speedbreaker (Left Shift) and Nitrous (Backspace, on a car that has nitrous) (2026-10-25). Once, the first Backspace press didn't trigger nitrous; why isn't known. Gear Down (K) and Gear Up (;) are hand-tested too, observed with automatic transmission. Still to test by hand: Reverse, Look Back and Pause (so far seen only with the virtual controller).
- **Vehicle physics** against the Xbox 360 version: the car drives normally by hand (1st place over 8 laps against 7 AI opponents who all finished, on a MacBook Air M5), but no lap time or top speed has been compared with the console. Equivalence with the Xbox 360 version isn't verified.
- **Audio and video sync:** reported fine by hand on a MacBook Air M1 and an M5. That rests on the tester's reports alone: the audio logs show no underruns or gaps, but sync hasn't been measured with instruments.
