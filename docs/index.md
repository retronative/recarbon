---
title: ReCarbon docs
description: Install, set up and play ReCarbon, the native Apple Silicon recompilation of Need for Speed Carbon (Xbox 360). Unofficial fan project.
---

ReCarbon is a **static recompilation** of *Need for Speed: Carbon* (Xbox 360) for Apple Silicon Macs.
The game's PowerPC code is translated to C++ ahead of time with XenonRecomp and compiled to native
arm64. A runtime, adapted from [SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker),
stands in for the console's kernel, GPU (Vulkan on Metal through MoltenVK), XMA audio and
controllers (SDL3). There is no emulator, no Wine or CrossOver, and no runtime CPU emulation.

> [!WARNING]
> **Alpha**
>
> ReCarbon **v0.1.0** is the first playable alpha: [Download](download.md). Read
> [Known issues](known-issues.md) before anything else.

## Where things stand

| Milestone | What it covers | Status |
|---|---|---|
| M0–M2 | Disc check, full recompilation (72,864 functions, 0 errors), native arm64 build | Done (2026-10-22) |
| M3–M4 | Boot, GPU bring-up, first frames, movies | Passed (2026-10-22) |
| M5 | Audio, movies, input | Accepted (2026-10-23), with open limitations; physical controller deferred |
| M6 | Profile, saves, free-roam stability | Open: every criterion has evidence; the written new-profile route (a hand test) is pending |
| M7 | Playable racing | Accepted for the alpha (2026-10-25): keyboard controls, full races and a career race by hand, 50.8 fps over a 9.8-minute race on a MacBook Air M5; physical controller deferred, Xbox 360 physics equivalence unverified |
| M8 | Polish and distribution | In progress, the current milestone: alpha packaging, validated on a MacBook Air M5 (a release candidate installed and played, including a clean install on a fresh macOS account). v0.1.0 alpha released 2026-10-26 |

Source: ReCarbon's `reference/ROADMAP.md`.

## Start here

- [Download](download.md): ReCarbon v0.1.0, what you need and how to install it.
- [Installation](installation.md): installing the game from your own disc image.
- [Controls](controls.mdx) and the interactive [Garage](https://retronative.github.io/recarbon/garage/).
- [Settings](settings.mdx): every option of the in-game Settings overlay (F1).
- [Troubleshooting](troubleshooting.md): logs and bug reports.

You need your **own legally obtained copy** of the Xbox 360 disc. ReCarbon contains no game code,
assets or executables, and this site never links to game files.
