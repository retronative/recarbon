---
title: ReCarbon docs
description: Install, set up and play ReCarbon, the native Apple Silicon recompilation of Need for Speed Carbon (Xbox 360). Unofficial fan project.
---

ReCarbon is a **static recompilation** of *Need for Speed: Carbon* (Xbox 360) for Apple Silicon Macs.
The game's PowerPC code is translated to C++ ahead of time with XenonRecomp and compiled to native
arm64. A runtime, adapted from [SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker),
stands in for the console's kernel, GPU (Vulkan on Metal through MoltenVK), XMA audio and
controllers (SDL3). There is no emulator, no Wine or CrossOver, and no runtime CPU emulation.

:::caution[Alpha]
ReCarbon has **no public release yet**. These pages describe the current development build. Read
[Known issues](known-issues/) before anything else.
:::

## Where things stand

| Milestone | What it covers | Status |
|---|---|---|
| M0–M2 | Disc check, full recompilation (72,864 functions, 0 errors), native arm64 build | Done (2026-10-22) |
| M3–M4 | Boot, GPU bring-up, first frames, movies | Passed (2026-10-22) |
| M5 | Audio, movies, input | In progress: keyboard hand test passed; physical controller deferred |
| M6 | Profile, saves, free-roam stability | Every criterion has evidence; closing is up to the maintainer |
| M7 | Playable racing | In progress: a career race finished by hand; controls, physics and 10-minute frame pacing pending |
| M8 | Polish and distribution | Packaging work started early; not done |

Source: ReCarbon's `reference/ROADMAP.md`.

## Start here

- [Download](download/): what a release will contain, and what you need.
- [Installation](installation/): installing the game from your own disc image.
- [Controls](controls/) and the interactive [Garage](../garage/).
- [Settings](settings/): every option of the in-game Settings overlay (F1).
- [Troubleshooting](troubleshooting/): logs and bug reports.

You need your **own legally obtained copy** of the Xbox 360 disc. ReCarbon contains no game code,
assets or executables, and this site never links to game files.
