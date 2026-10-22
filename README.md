# ReCarbon

Native recompilation of Need for Speed: Carbon (Xbox 360) for Apple Silicon.

> [!IMPORTANT]
> **ReCarbon is an unofficial fan project. It is not affiliated with, endorsed by or sponsored
> by Electronic Arts Inc. or the Need for Speed brand.** *Need for Speed* and *Carbon* are
> trademarks of Electronic Arts Inc. and are used here only to describe the game.
>
> **No game files are included or distributed.** You need your own legally obtained copy of
> the Xbox 360 disc. The repository contains no game code, assets or executables, and no code
> generated from them. Please don't ask for, or share, game files or disc images.

## Status

**Early development. Nothing runs yet.**

- **M0:** the Xbox 360 executable (title 0x454107EC) is verified as compatible with the
  SpeedBreaker/XenonRecomp pipeline.
- **M1:** the whole executable recompiles to C++ with no errors, reproducibly.
- **M2:** all of it compiles for arm64 and links with the adapted SpeedBreaker runtime into a
  native `ReCarbon` executable. It hasn't been run yet.
- **M3:** it boots Carbon: kernel and CRT init, data loading, GPU bring-up, and the game's own loading screen rendered. Movie video is still black and nothing is playable.
- **Next, M4:** rendering, not started.

See [reference/FEASIBILITY.md](reference/FEASIBILITY.md) and
[reference/ROADMAP.md](reference/ROADMAP.md).

| Item | Value |
|---|---|
| Target | Apple Silicon Macs, macOS 15 or later (primary test device: MacBook Air M5) |
| App / executable | `ReCarbon.app` / `ReCarbon` (planned) |
| Bundle ID | `io.github.thixpin.recarbon` |

## How it will work

ReCarbon is planned as a static recompilation, not an emulator. XenonRecomp translates the
game's PowerPC code into C++ ahead of time, and that C++ compiles to native arm64. A runtime
stands in for the Xbox 360's kernel, GPU (Vulkan on Metal through MoltenVK), XMA audio and
controllers (SDL3).

The plan builds on [SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker), the
static recompilation of *Need for Speed: Most Wanted* (2005) by project(u). It does not
rewrite SpeedBreaker. There is no Wine, CrossOver or runtime CPU emulation.

## Documents

- [reference/FEASIBILITY.md](reference/FEASIBILITY.md): disc and executable analysis, SpeedBreaker audit, risks, verdict
- [reference/ARCHITECTURE.md](reference/ARCHITECTURE.md): proposed architecture and project naming
- [reference/ROADMAP.md](reference/ROADMAP.md): milestones and acceptance criteria
- `reference/analysis/xexinfo.py`: reads the plaintext header of an Xbox 360 executable (XEX2)
- `reference/analysis/xbeinfo.py`: reads the header of an original-Xbox executable (XBE)

## Building (development only, not playable)

You need macOS on Apple Silicon and Homebrew, plus your own disc extracted so that
`game/files/default.xex` exists:

```sh
brew install llvm lld cmake ninja python
brew install sdl3 vulkan-loader vulkan-headers glslang spirv-tools molten-vk
```

Then, from the repository root:

```sh
scripts/setup_tools.sh     # XenonRecomp (pinned + patches/xenonrecomp), extract-xiso, xexsurvey,
                           # FFmpeg 7.1.1's XMA decoder (patches/ffmpeg) into tools/
scripts/survey.sh          # analyse default.xex, regenerate config/, recompile to ppc/, validate
tests/run_instr_tests.sh   # instruction tests for the patched XenonRecomp

cmake -S . -B build/main -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
ninja -C build/main ReCarbon
```

The build links Homebrew's libraries, whose bottles are built for the macOS they were made on.
So the macOS deployment target defaults to your own macOS (`MACOSX_DEPLOYMENT_TARGET` when set,
else `<major version>.0`, e.g. 27.0), and `setup_tools.sh` builds `tools/ffmpeg-xma` for the same
target (and rebuilds it when that changes). The binary runs only on that macOS or newer. To
choose another target, set `MACOSX_DEPLOYMENT_TARGET` for both `setup_tools.sh` and the first
`cmake` run, or pass `-DCMAKE_OSX_DEPLOYMENT_TARGET=<version>` to `cmake`. A build folder keeps
the target it was first configured with.

Packaging, not done yet: a release for older macOS versions (SpeedBreaker's targets macOS 15.0)
needs the libraries built from source for that target (SpeedBreaker's
`scripts/macos/build_deps.sh`, passed with `-DCMAKE_PREFIX_PATH=<prefix>
-DFFMPEG_XMA=<prefix>/ffmpeg-xma`).

`tools/`, `game/`, `ppc/` and `build/` are git-ignored. Never commit or share them.

## Credits and license

ReCarbon is GPL-3.0-or-later (see `COPYING` and `NOTICE`). It is derived from
[SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker) by project(u):
`survey/`, `patches/xenonrecomp/0001–0008`, `tests/` and the build scripts come from it, with
ReCarbon's changes marked. SpeedBreaker in turn builds on Unleashed Recompiled, XenonRecomp
(MIT) and Xenia (BSD-3-Clause). See `LICENSES/`.
