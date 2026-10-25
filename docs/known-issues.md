---
title: Known issues
description: ReCarbon alpha's known issues, from the project's roadmap. Kept in sync per release.
---

From ReCarbon's `reference/ROADMAP.md` (M7, "Known issues for the alpha"). Each is still open.

| Issue | Details |
|---|---|
| Occasional stalls of about 1 s | The presenter waits for a free drawable while holding the GPU queue lock. Being investigated; to revisit before a public release. |
| GPU-idle stalls of 250–600 ms | No lock wait involved. Cause unknown. |
| Frame rate can drop on a fanless Mac during long sessions | Seen once, on a MacBook Air M1: after about 45 minutes of back-to-back play, a 9-minute race ran at about 30 fps for its first minute, then about 21 fps to the end. The GPU took longer per frame with the same scene load, and macOS reported its thermal state as "fair". The cause isn't confirmed; a slower GPU on a warm Mac is the leading explanation. |
| Physical controller untested | Deferred: no real pad has been tested. The default mapping is pinned by a test, and a virtual controller drives the menus and a race. |
| Development builds link Homebrew's libraries | A build against `scripts/macos/build_deps.sh`'s libraries passes `check_app` (M8). Development builds still name `/opt/homebrew` paths, so Vulkan layers installed there could be picked up. No release is published yet. |
| Key labels on a non-Latin keyboard layout | Under Myanmar or another non-Latin layout, Settings > Controls shows letter keys as '?', because the menu's font lacks the script. Binding such a key is refused safely and nothing is saved. Gameplay input is unaffected. |
| macOS 27 or newer only | Current builds link libraries built for macOS 27.0. |
| Movie frame pacing | Movies play at the right speed, but 20–28% of frames show for 1 or 3 refreshes instead of 2. Smoothing is deferred to polish. |

## Still to be tested

- **Keyboard controls in a race:** Throttle, Brake and Steering drove two full Quick Races by hand, and Change Camera worked (2026-10-25). Still to test by hand: Reverse, Look Back, gear shifts and Pause (so far seen only with the virtual controller), and which keys give Handbrake, Nitrous and Speedbreaker.
- **Vehicle physics** against the Xbox 360 version: the car drove well by hand, but no lap time or top speed has been compared yet.
- **Frame pacing over 10 minutes of racing** on the MacBook Air M5. The M1 races so far are supplementary.
- **Audio and picture during racing:** reported fine by hand on the M1. Audio sync rests on that report alone.
