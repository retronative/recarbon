---
title: Supported hardware
description: The Macs ReCarbon has been measured on, and what is untested.
---

ReCarbon targets **Apple Silicon Macs**. It states only what has been measured, on the machines it
was measured on.

| Mac | macOS | What ran | Source |
|---|---|---|---|
| MacBook Air, Apple M5 (4P+6E), 24 GB | 27.0.1 | The main test machine: builds, every milestone run, the hand tests, and the alpha release candidate installed and played in the main account and on a fresh macOS account (2026-10-25) | ReCarbon FEASIBILITY §3–§6c, M7 and M8 acceptance records |
| MacBook Air, Apple M1 | 27.0.1 | The packaged app from its bundle (2026-10-23), and three full races by hand that start at about 30 fps and slow down in long races (2026-10-25). The alpha release candidate wasn't installed and played there. | FEASIBILITY §6b, M7 acceptance record |

**Everything else is untested**: other Apple Silicon Macs, Macs without Homebrew installed, and
any macOS before 27 (current builds need 27 or newer: see [Download](download.md)).

## Controllers

- **Keyboard:** hand-tested in menus (M5 hand test 2), and in races Throttle, Brake and Steering
  (hand tests 1, 3 and 4, and the races of 2026-10-25), Handbrake (Space), Speedbreaker (Left
  Shift) and Nitrous (Backspace) (2026-10-25), on the WASD keys (the Default profile). Reverse,
  Look Back, Pause and the gear keys with a manual gearbox are still to try by hand.
- **Controllers:** read through SDL3's gamepad support, by position: a PlayStation pad's Cross acts
  as A. **No physical controller has been tested yet:** that test is deferred. The default mapping is
  pinned by unit tests and checked with SDL's virtual controller only; see the [Garage](https://retronative.github.io/recarbon/garage/).
- Back paddles and extra buttons are **not mapped** by ReCarbon.

Measured performance: see [Performance](performance.md).
