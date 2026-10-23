# ReCarbon

Native recompilation of Need for Speed: Carbon (Xbox 360) for Apple Silicon.

> [!IMPORTANT]
> **ReCarbon is an unofficial fan project. It is not affiliated with, endorsed by or sponsored
> by Electronic Arts Inc. or the Need for Speed brand.** *Need for Speed* and *Carbon* are
> trademarks of Electronic Arts Inc. and are used here only to describe the game.
>
> **No game files are included or distributed.** You need your own legally obtained copy of
> the Xbox 360 disc. The repository contains no game code, assets or executables. It does contain
> metadata from analysing the game's executable: function and jump-table addresses (`config/`),
> the installer's list of disc files with their sizes and SHA-256 hashes
> (`runtime/install/manifest.inc`), and the names of the system functions the game imports
> (`runtime/kernel/import_stubs.cpp`). The recompiled code is generated on your Mac from your own
> copy and is never committed. Please don't ask for, or share, game files or disc images.

## Status

**Early development: playable in parts, not released.** Statuses come from
[reference/ROADMAP.md](reference/ROADMAP.md). A milestone passes only when every criterion is met.

- **M0–M2:** the Xbox 360 executable (title 0x454107EC) is verified, recompiles to C++ with no
  errors, reproducibly, and links with the adapted SpeedBreaker runtime into a native arm64
  `ReCarbon`.
- **M3–M4:** it boots and renders: kernel and CRT init, data loading, the GPU, movies and the
  front end.
- **M5 (accepted, with open limitations):** audio, movies and keyboard input work. Still open:
  - a physical controller, deferred and not verified;
  - movie judder, deferred to polish;
  - changing key bindings needs an English keyboard layout: on other layouts the Controls page
    shows letters as '?' and refuses new keys (gameplay input is unaffected).
- **M6 (open):** it boots to free-roam and loads races. Free-roam held 15 minutes without a
  crash in a hand test. The new-profile route test is still pending.
- **M7 (in progress, current):** a full career race finishes against AI, and progress survives a restart
  (hand test 4). Still pending:
  - the driving controls on the keyboard (a physical controller is deferred, not verified);
  - physics;
  - frame pacing over 10 minutes of racing;
  - audio and picture;
  - the repeated route.
- **Known issues:** occasional stalls of about 1 s while racing, and shorter GPU-idle stalls
  (both unexplained). See ROADMAP M7.

See [reference/FEASIBILITY.md](reference/FEASIBILITY.md) and
[reference/ROADMAP.md](reference/ROADMAP.md).

| Item | Value |
|---|---|
| Target | Apple Silicon Macs (primary test device: MacBook Air M5). Current builds need macOS 27, because the Homebrew libraries they bundle do |
| App / executable | `ReCarbon.app` (`scripts/macos/make_app.py`; a build against `scripts/macos/build_deps.sh`'s libraries passes `check_app`; no release published) / `ReCarbon` |
| Bundle ID | `org.retronative.recarbon` |

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

Packaging (M8): the app's libraries are built from SHA-256-checked sources for macOS 27 with
`scripts/macos/build_deps.sh <prefix>`, about 6 minutes and about 60 MB of downloads. A build
configured with `-DCMAKE_PREFIX_PATH=<prefix> -DFFMPEG_XMA=<prefix>/ffmpeg-xma` in its own build
folder is what `scripts/macos/make_app.py` packages and `scripts/macos/check_app.py` passes.
MoltenVK is Homebrew's 1.4.2, pinned by its SHA-256 in `make_app.py`. No release is published.

`tools/`, `game/`, `ppc/` and `build/` are git-ignored. Never commit or share them.

## Credits and license

ReCarbon is GPL-3.0-or-later (see `COPYING` and `NOTICE`). It is derived from
[SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker) by project(u):
`survey/`, `patches/xenonrecomp/0001–0008`, `tests/` and the build scripts come from it, with
ReCarbon's changes marked. SpeedBreaker in turn builds on Unleashed Recompiled, XenonRecomp
(MIT) and Xenia (BSD-3-Clause). See `LICENSES/`.
