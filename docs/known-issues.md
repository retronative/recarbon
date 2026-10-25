---
title: Known issues
description: ReCarbon alpha's known issues, from the project's roadmap. Kept in sync per release.
---

From ReCarbon's `reference/ROADMAP.md` (M7, "Known issues for the alpha"). Each is still open.

| Issue | Details |
|---|---|
| Occasional stalls of about 1 s | The presenter waits for a free drawable while holding the GPU queue lock. Being investigated; to revisit before a public release. |
| GPU-idle stalls of 250–600 ms | No lock wait involved. Cause unknown. |
| Frame rate falls during long races on a MacBook Air M1 | On the M1, races start at about 30 fps and fall as play goes on: to about 21 fps in a 9-minute race and about 16 fps in a 14-minute one, even after a cool-down. The M1 is limited by its GPU from the start, and the GPU's time per frame rises with time under load while macOS reports the thermal state as "fair". GPU throttling on the fanless M1 is the leading explanation but isn't confirmed. A sustained 30 fps isn't claimed on the M1. |
| Texture cache grows during play | ReCarbon keeps every texture it has made until a setting that reloads them changes: nothing is evicted while playing. Driving grows it by roughly 500 MB over 7 laps or 20 minutes of free-roam, on every Mac tested. On current evidence it doesn't cause the frame-rate drop above, and no crash or corruption has been tied to it, but very long sessions use more memory, which matters most on 8 GB Macs. Restarting the game clears it. A fix (eviction once the GPU is known to be done with a texture) is planned after the alpha. |
| Physical controller untested | Deferred: no real pad has been tested. The default mapping is pinned by a test, and a virtual controller drives the menus and a race. |
| Development builds link Homebrew's libraries | A build against `scripts/macos/build_deps.sh`'s libraries passes `check_app` (M8). Development builds still name `/opt/homebrew` paths, so Vulkan layers installed there could be picked up. No release is published yet. |
| Key labels on a non-Latin keyboard layout | Under Myanmar or another non-Latin layout, Settings > Controls shows letter keys as '?', because the menu's font lacks the script. Binding such a key is refused safely and nothing is saved. Gameplay input is unaffected. |
| "Cannot connect to Xbox Live!" at every launch | Once per launch, on first entering the main menu, the game shows this notice because Xbox Live isn't available. Accept it with **A** (**Space** on the keyboard) to continue; nothing is lost. ReCarbon doesn't dismiss it for you: it can't yet tell reliably when this notice is the one on screen, and it won't press buttons blindly. |
| macOS 27 or newer only | Current builds link libraries built for macOS 27.0. |
| Movie frame pacing | Movies play at the right speed, but 20–28% of frames show for 1 or 3 refreshes instead of 2. Smoothing is deferred to polish. |

## Still to be tested

- **Keyboard controls in a race:** Throttle, Brake and Steering drove three full Quick Races by hand on the WASD keys, and Change Camera (E), Handbrake (Space) and Speedbreaker (Left Shift) worked (2026-10-25). Still to test by hand: Nitrous (the stock RX-8 has none), Reverse, Look Back, gear shifts and Pause (so far seen only with the virtual controller).
- **Vehicle physics** against the Xbox 360 version: the car drove well by hand, but no lap time or top speed has been compared yet.
- **Audio and picture during racing:** reported fine by hand on the M1. Audio sync rests on that report alone.
