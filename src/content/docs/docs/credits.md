---
title: Credits
description: The projects and people ReCarbon is built on.
---

ReCarbon is derived from **[SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker)**,
Copyright (C) 2026 project(u) and SpeedBreaker contributors (GPL-3.0-or-later). Files taken from
SpeedBreaker keep its attribution; changes are noted in each file.

## Thanks

- **project(u) and the SpeedBreaker contributors**, whose port ReCarbon builds on.
- **The hedge-dev team**, for XenonRecomp and the approach of Unleashed Recompiled that SpeedBreaker
  follows.
- **The Xenia project**, for years of Xbox 360 research.

## Compiled into ReCarbon

| Project | What it does | License |
|---|---|---|
| Unleashed Recompiled (hedge-dev and contributors) | Parts of the kernel, XAM, memory, heap and threading code | GPL-3.0-or-later |
| Xenia (Ben Vanik and contributors) | Xenos GPU headers, the NT status table; the reference throughout | BSD-3-Clause |
| freedreno (Rob Clark) | Shader instruction formats, through Xenia | MIT |
| XenonRecomp (hedge-dev and contributors) | Recompiles the PowerPC code; its XEX loader | MIT |
| libmspack (Stuart Caie) | LZX decompression of the XEX | LGPL-2.1 |
| tiny-AES-c | XEX decryption | Unlicense |
| TinySHA1 (Saurav Mohapatra) | SHA-1 in XenonRecomp's XEX patcher | ISC-style |
| toml++ (Mark Gillard) | Reads and writes `settings.toml` | MIT |
| SIMDe (Evan Nemerson and contributors) | Vector instructions in the recompiled code | MIT |
| Dear ImGui (Omar Cornut) | The installer, the Settings menu and the overlays | MIT |
| stb (Sean Barrett) | Font rasterising and text editing, inside Dear ImGui | MIT |
| Roboto (Google) | The menus' font | Apache-2.0 |
| o1heap (Pavel Kirienko) | The game's heap allocator | MIT |
| QR Code generator (Project Nayuki) | The QR code in Check for Updates | MIT |
| AMD FidelityFX Super Resolution 1 | FSR 1 upscaling | MIT |
| FFmpeg 7.1.1 | XMA1, XMA2 and WMA Pro audio decoders | LGPL-2.1-or-later |

## In the app

SDL3 (with HIDAPI and yuv2rgb), glslang, SPIRV-Tools, MoltenVK (with SPIRV-Cross and cereal), the
Vulkan Loader (with cJSON), Vulkan-Headers and SPIRV-Headers.

The full texts are on the [Licenses](../licenses/notice/) pages, taken from ReCarbon's
`LICENSES/` folder. Source: ReCarbon's `THIRD_PARTY_NOTICES.md`.

## Source of the LGPL parts

FFmpeg and libmspack (LGPL-2.1) are compiled into the `ReCarbon` executable. ReCarbon distributes
no binaries yet. Whoever distributes a build must also offer the complete source of those parts at
the versions used: FFmpeg 7.1.1 (`https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz`) with ReCarbon's
patch and build script, and libmspack as XenonRecomp's pinned submodule has it.

## This website

Built with Astro and Starlight. Fonts: Chakra Petch, Inter and JetBrains Mono, under the SIL Open
Font License, served from this site. The drawings of the keyboard and controller are original.
