---
title: Supported hardware
description: The Macs ReCarbon has been measured on, and what is untested.
---

ReCarbon targets **Apple Silicon Macs**. It states only what has been measured, on the machines it
was measured on.

| Mac | macOS | What ran | Source |
|---|---|---|---|
| MacBook Air, Apple M5 (4P+6E), 24 GB | 27.0.1 | The main test machine: builds, every milestone run, the hand tests | ReCarbon FEASIBILITY §3–§6c |
| MacBook Air, Apple M1 | 27.0.1 | The packaged app ran for 60 s from its bundle, reached frame 900 and exited cleanly (2026-10-23) | FEASIBILITY §6b, packaging check |

**Everything else is untested**: other Apple Silicon Macs, Macs without Homebrew installed, and
any macOS before 27 (current builds need 27 or newer: see [Download](../download/)).

## Controllers

- **Keyboard:** hand-tested in menus (M5 hand test 2), and Throttle, Brake and Steering in races
  (hand tests 1, 3 and 4), on the Default keys. The other racing controls are still to verify.
- **Controllers:** read through SDL3's gamepad support, by position: a PlayStation pad's Cross acts
  as A. **No physical controller has been tested yet:** that test is deferred. The default mapping is
  pinned by unit tests and checked with SDL's virtual controller only; see the [Garage](../../garage/).
- Back paddles and extra buttons are **not mapped** by ReCarbon.

Measured performance: see [Performance](../performance/).
