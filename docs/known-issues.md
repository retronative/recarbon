---
title: Known issues
description: ReCarbon alpha's known issues, from the project's roadmap. Kept in sync per release.
---

From ReCarbon's `reference/ROADMAP.md` (M7, "Known issues for the alpha"). Each is still open.

| Issue | Details |
|---|---|
| Occasional stalls of about 1 s | The presenter waits for a free drawable while holding the GPU queue lock. Being investigated; to revisit before a public release. |
| GPU-idle stalls of 250–600 ms | No lock wait involved. Cause unknown. |
| Physical controller untested | The default mapping is pinned by a test and a virtual controller reaches the menus, but no real pad has been tested yet (M5). |
| Release packaging links Homebrew's libraries | The bundled Vulkan loader names `/opt/homebrew` paths (`check_app` check 3). On a Mac with Homebrew, Vulkan layers installed there could be picked up. Fixed by building the libraries from source (M8). |
| macOS 27 or newer only | Current builds link libraries built for macOS 27.0. |
| Movie frame pacing | Movies play at the right speed, but 20–28% of frames show for 1 or 3 refreshes instead of 2. Smoothing is deferred to polish. |

## Still to be tested

- Controls on a controller and on the keyboard in a race, by hand: Throttle, Brake / Reverse and Steering; Change Camera, Look Back, gear shifts and Pause (so far seen only with the virtual controller); and where Handbrake, Nitrous and Speedbreaker are.
- Vehicle physics against the Xbox 360 version.
- Frame pacing over 10 minutes of racing.
- Audio and picture during racing.
