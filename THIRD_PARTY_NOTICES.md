# Third-party notices

ReCarbon is free software under the GNU General Public License, version 3 or (at your option) any
later version (GPL-3.0-or-later; [`COPYING`](COPYING), [`NOTICE`](NOTICE)). It is derived from
SpeedBreaker, Copyright (C) 2026 project(u) and SpeedBreaker contributors (GPL-3.0-or-later). It is
built from software by others under their own licenses, listed here. The license texts are in
[`LICENSES/`](LICENSES).

ReCarbon's download is the Mac app (`ReCarbon.app` in `ReCarbon-<version>-macos.dmg`, Apple silicon, macOS
27 or later), published on the project's GitHub Releases page
(`https://github.com/retronative/recarbon/releases`) and built by
[`scripts/macos/make_app.py`](scripts/macos/make_app.py). The app bundles SDL3, the Vulkan loader,
glslang, SPIRV-Tools, MoltenVK, FFmpeg's libavcodec and libavutil, and libmspack's LZX decoder, and
carries these notices, the license texts and each bundled library's license files in
`Contents/Resources/licenses/`. Every ReCarbon release carries them too.

## Compiled into ReCarbon (as into SpeedBreaker)

| Project | What it does here | License | Text |
|---|---|---|---|
| **Unleashed Recompiled**: hedge-dev and contributors | Parts of the kernel, XAM, memory, heap and threading code are adapted from it (each such file says so at the top). | GPL-3.0-or-later | [`LICENSES/GPL-3.0.txt`](LICENSES/GPL-3.0.txt) |
| **Xenia**: Copyright Ben Vanik and Xenia contributors | The Xenos GPU register, microcode and format headers are ported from it (`runtime/gpu/xenos/`), as is the NT status table (`runtime/kernel/imports_error.cpp`). Its kernel, XMA and GPU behaviour is the reference throughout. | BSD-3-Clause | [`LICENSES/Xenia-BSD-3-Clause.txt`](LICENSES/Xenia-BSD-3-Clause.txt) |
| **freedreno**: Copyright (c) 2012 Rob Clark | Shader instruction formats, through Xenia's `ucode.h`. | MIT | [`LICENSES/freedreno-MIT.txt`](LICENSES/freedreno-MIT.txt) |
| **XenonRecomp** (XenonUtils): Copyright (c) 2025 hedge-dev and contributors | Recompiles the game's PowerPC code into C++ (with SpeedBreaker's patches, `patches/xenonrecomp/`); its XEX loader is compiled into the runtime. | MIT | [`LICENSES/XenonRecomp-MIT.md`](LICENSES/XenonRecomp-MIT.md) |
| **tiny-AES-c** | XEX decryption (through XenonRecomp). | Unlicense | [`LICENSES/tiny-AES-c-Unlicense.txt`](LICENSES/tiny-AES-c-Unlicense.txt) |
| **TinySHA1**: Copyright (c) 2012-22 Saurav Mohapatra | SHA-1, in XenonRecomp's XEX patcher. | ISC-style | [`LICENSES/TinySHA1-ISC.txt`](LICENSES/TinySHA1-ISC.txt) |
| **toml++**: Copyright (c) Mark Gillard | Reads and writes `settings.toml` (through XenonRecomp). | MIT | [`LICENSES/tomlplusplus-MIT.txt`](LICENSES/tomlplusplus-MIT.txt) |
| **SIMDe**: Copyright (c) 2017 Evan Nemerson and contributors | Vector instructions in the recompiled code (through XenonRecomp). | MIT | [`LICENSES/SIMDe-MIT.txt`](LICENSES/SIMDe-MIT.txt) |
| **Dear ImGui** 1.92.9: Copyright (c) 2014-2026 Omar Cornut | The installer, the settings menu and the overlays (`runtime/thirdparty/imgui`). | MIT | [`LICENSES/DearImGui-MIT.txt`](LICENSES/DearImGui-MIT.txt) |
| **stb_truetype, stb_rect_pack, stb_textedit**: Copyright (c) 2017 Sean Barrett | Font rasterising and text editing, inside Dear ImGui. | MIT (or public domain) | [`LICENSES/stb-MIT.txt`](LICENSES/stb-MIT.txt) |
| **Roboto** (Medium): Copyright 2011 Google Inc. | The menus' font, embedded in the program. Roboto is a trademark of Google. | Apache-2.0 | [`LICENSES/Apache-2.0.txt`](LICENSES/Apache-2.0.txt) |
| **o1heap**: Copyright (c) 2020 Pavel Kirienko | The game's heap allocator (`runtime/thirdparty/o1heap`). | MIT | [`LICENSES/o1heap-MIT.txt`](LICENSES/o1heap-MIT.txt) |
| **QR Code generator library** (C) 1.8.0: Copyright (c) 2022 Project Nayuki | The QR code of the download page in Settings > Advanced > Check for Updates (`runtime/thirdparty/qrcodegen`). | MIT | [`LICENSES/qrcodegen-MIT.txt`](LICENSES/qrcodegen-MIT.txt) |
| **AMD FidelityFX Super Resolution 1** (FSR 1, `ffx_a.h` and `ffx_fsr1.h` at commit a21ffb8): Copyright (c) 2021 Advanced Micro Devices, Inc. | Settings > Graphics > Upscaling's FSR 1: its EASU and RCAS shader code, compiled into the game's shaders, and its constant setup (`runtime/thirdparty/fsr1`, unmodified). | MIT | [`LICENSES/FidelityFX-FSR-MIT.txt`](LICENSES/FidelityFX-FSR-MIT.txt) |

## Libraries in the Mac app

| Project | Where | License | Text |
|---|---|---|---|
| **FFmpeg** 7.1.1: the FFmpeg developers | The XMA1, XMA2 and WMA Pro audio decoders: `libavcodec.61.dylib` and `libavutil.59.dylib`, separate, replaceable libraries in the Mac app (below). Built without any GPL parts, with one SpeedBreaker change: [`patches/ffmpeg/0001-xma-single-stream-no-holdback.patch`](patches/ffmpeg/0001-xma-single-stream-no-holdback.patch) (no sample hold-back for single-stream XMA, which the game's audio mixer depends on). Source: `https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz` plus that patch; [`scripts/build_ffmpeg_xma.sh`](scripts/build_ffmpeg_xma.sh) builds it. | LGPL-2.1-or-later | [`LICENSES/FFmpeg-LGPL-2.1.txt`](LICENSES/FFmpeg-LGPL-2.1.txt) |
| **libmspack**: (C) 2003-2023 Stuart Caie | LZX decompression of the XEX: its `lzxd.c` (XenonRecomp's copy, revision `305907723a4e`, unmodified), built as `librecarbon_lzx.dylib`, a separate, replaceable library in the Mac app (below). | LGPL-2.1 | [`LICENSES/libmspack-LGPL-2.1.txt`](LICENSES/libmspack-LGPL-2.1.txt) |
| **SDL3**: Copyright (C) 1997-2026 Sam Lantinga | The window, input, audio and the app's lifecycle. In the Mac app. It includes HIDAPI (controllers; Copyright (c) 2010 Alan Ott, Signal 11 Software; under its BSD-style license, one of the three it offers) and yuv2rgb (Copyright (c) 2016 Adrien Descamps). | zlib; HIDAPI and yuv2rgb: BSD-3-Clause | [`LICENSES/SDL3-Zlib.txt`](LICENSES/SDL3-Zlib.txt); [`LICENSES/HIDAPI-BSD-3-Clause.txt`](LICENSES/HIDAPI-BSD-3-Clause.txt); [`LICENSES/yuv2rgb-BSD-3-Clause.txt`](LICENSES/yuv2rgb-BSD-3-Clause.txt) |
| **glslang**: The Khronos Group Inc. and others | Compiles the game's shaders while it runs. In the Mac app. | BSD-3-Clause and others (its own list) | [`LICENSES/glslang.txt`](LICENSES/glslang.txt) |
| **SPIRV-Tools**: The Khronos Group Inc. and others | Used by glslang (with its optimizer). In the Mac app. | Apache-2.0 | [`LICENSES/Apache-2.0.txt`](LICENSES/Apache-2.0.txt) |
| **MoltenVK** (with SPIRV-Cross): Copyright (c) 2015-2026 The Brenwill Workshop Ltd., The Khronos Group Inc. | Vulkan on Apple's Metal. In the Mac app. It includes cereal (Copyright (c) 2013-2022 Randolph Voorhies, Shane Grant; BSD-3-Clause), which saves its shader conversions, and SPIRV-Tools (below). | Apache-2.0; cereal: BSD-3-Clause | [`LICENSES/Apache-2.0.txt`](LICENSES/Apache-2.0.txt); [`LICENSES/cereal-BSD-3-Clause.txt`](LICENSES/cereal-BSD-3-Clause.txt) |
| **Vulkan Loader**: The Khronos Group Inc., Valve Corporation, LunarG, Inc. | In the Mac app. It includes cJSON (Copyright (c) 2009-2017 Dave Gamble and cJSON contributors). | Apache-2.0; cJSON: MIT | [`LICENSES/Apache-2.0.txt`](LICENSES/Apache-2.0.txt); [`LICENSES/cJSON-MIT.txt`](LICENSES/cJSON-MIT.txt) |
| **Vulkan-Headers** and **SPIRV-Headers**: The Khronos Group Inc. | Headers the game, the Vulkan Loader, glslang and SPIRV-Tools are built with. | Apache-2.0 or MIT (Vulkan-Headers); MIT (SPIRV-Headers) | [`LICENSES/Apache-2.0.txt`](LICENSES/Apache-2.0.txt) or [`LICENSES/Vulkan-Headers-MIT.txt`](LICENSES/Vulkan-Headers-MIT.txt); [`LICENSES/SPIRV-Headers.txt`](LICENSES/SPIRV-Headers.txt) |
| **GNU C Library** (glibc) and **GCC's runtime libraries** (libstdc++, libgcc_s) | **Not shipped by ReCarbon.** SpeedBreaker's SteamOS downloads carry them; ReCarbon builds only for macOS, where the app uses the system's own C and C++ libraries. Listed for completeness. | glibc: LGPL-2.1-or-later. GCC's runtime: GPL-3.0 with the GCC Runtime Library Exception | in SpeedBreaker's SteamOS downloads |

A release build of the Mac app takes its libraries from
[`scripts/macos/build_deps.sh`](scripts/macos/build_deps.sh): SDL3, the Vulkan loader, glslang,
SPIRV-Tools and FFmpeg, built from their projects' own source releases (checked by SHA-256) for
macOS 27; the LZX decoder is built with the game (`runtime/CMakeLists.txt`). MoltenVK is Homebrew's
1.4.2, pinned by its SHA-256. The app's
`Contents/Resources/licenses/README.txt` names each library's project, version, source and
SHA-256 (or, for MoltenVK, its Homebrew bottle), next to its license files. A development build
bundles Homebrew's libraries instead, and its README says so.

## Source code of the LGPL parts, and replacing them

FFmpeg (with [`patches/ffmpeg/`](patches/ffmpeg)) and libmspack's LZX decoder are under the
LGPL-2.1. They are not compiled into the `ReCarbon` executable: they are three separate libraries
in `ReCarbon.app/Contents/Frameworks/` (`libavcodec.61.dylib`, `libavutil.59.dylib`,
`librecarbon_lzx.dylib`), which the game loads by name (`@rpath/<file>`) when it starts.

**Source offer.** With every binary release, ReCarbon publishes the complete source of these parts,
at the versions in that release, on the same GitHub Release page as the binary, for as long as the
binary is offered there: for v0.1.0, `ReCarbon-0.1.0-third-party-source.tar.xz`, with its SHA-256
in `ReCarbon-0.1.0-third-party-source.tar.xz.sha256`. It holds FFmpeg 7.1.1
(`https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz`, SHA-256 in `scripts/setup_tools.sh`), the patch
and [`scripts/build_ffmpeg_xma.sh`](scripts/build_ffmpeg_xma.sh); libmspack at XenonRecomp's pinned
revision `305907723a4e7ab2018e58040059ffb5e77db837`; the source archives of the other bundled
libraries with [`scripts/macos/build_deps.sh`](scripts/macos/build_deps.sh); a README with the build
steps; and `SHA256SUMS`. ReCarbon's own source is this repository, at the release's tag.

**Replacing a library.** You may replace any of the three with your own build, for example a
modified FFmpeg. It must keep the file name and the interface the game uses (FFmpeg 7.1's libavcodec
61 and libavutil 59; libmspack's `lzxd_init`, `lzxd_decompress`, `lzxd_free`), and its install
name, `@rpath/<file>`: `scripts/build_ffmpeg_xma.sh <ffmpeg-7.1.1.tar.xz> <prefix>` builds FFmpeg
that way, and the archive's README shows how to build the LZX decoder. Then, on your copy of the app:

1. Put your file in place of the old one: `ReCarbon.app/Contents/Frameworks/libavcodec.61.dylib`
   (or `libavutil.59.dylib`, `librecarbon_lzx.dylib`).
2. Sign it and the app again, ad hoc: `codesign --force --sign - ReCarbon.app/Contents/Frameworks/<file>`,
   then `codesign --force --sign - ReCarbon.app`.
3. Check: `codesign --verify --deep --strict ReCarbon.app`, and
   `DYLD_PRINT_LIBRARIES=1 ReCarbon.app/Contents/MacOS/ReCarbon --where` lists your file being loaded
   from `Contents/Frameworks`.

Your copy then has a different signature from the released one; if macOS blocks its first launch,
use **Open Anyway** in System Settings > Privacy & Security as for the release.

## Tools used to build it, not shipped

- **XenonRecomp** (above): hedge-dev and contributors, MIT. (**XenosRecomp**, hedge-dev, MIT, which
  SpeedBreaker lists, is not used by ReCarbon: its shaders are translated while the game runs.)
- **extract-xiso**: for developers unpacking their own disc image.
- **Instruction tests** (`tests/`): SpeedBreaker's generated tests and ReCarbon's hand-computed ones.

## Thanks

project(u) and the SpeedBreaker contributors, whose port ReCarbon builds on. The hedge-dev team,
for XenonRecomp and the approach of Unleashed Recompiled that SpeedBreaker follows, and the Xenia
project, for years of Xbox 360 research.
