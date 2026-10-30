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
> (`runtime/kernel/import_stubs.cpp`). The recompiled code (`ppc/`) is never committed to this
> repository. ReCarbon needs your own, legally obtained Xbox 360 copy of the game; game assets and
> disc images are not distributed. Please don't ask for, or share, game files or disc images.

## Status

**Alpha: v0.1.2 released**, an update to the first playable alpha ([Download](docs/download.md)). Statuses come from
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
- **M6 (accepted for the alpha, 2026-10-26):** it boots to free-roam and loads races.
  Free-roam held 15 minutes without a crash in a hand test. On a fresh macOS account a new
  profile was saved, a race loaded, and the save was found again after a restart.
- **M7 (accepted for the alpha, 2026-10-25):** playable racing. The driving controls work on the
  keyboard by hand (including Handbrake, Speedbreaker and Nitrous), a full career race finishes
  against AI and progress survives a restart, and a 9.8-minute race on a MacBook Air M5 averaged
  50.8 fps with no second under 30 fps outside a macOS Space switch. Still open, not recorded as
  passed:
  - a physical controller, deferred and not verified;
  - equivalence with the Xbox 360 version's physics, deferred and unverified;
  - audio and video sync, which rests on the tester's reports, not on measurement;
  - Reverse, Look Back and Pause, not yet tried by hand (gear shifts on K / ; are hand-tested, with automatic transmission).
- **M8 (completed for the alpha, 2026-10-26):** v0.1.0 is released. Alpha packaging,
  validated on a MacBook Air M5: a release candidate installed and played, including a clean
  install on a fresh macOS account. Still open, not recorded as passed:
  - render scaling and widescreen, deferred;
  - a physical controller, Game Mode on a fresh account and an install on a MacBook Air M1,
    not verified.
- **Known issues:** occasional stalls of about 1 s while racing, and shorter GPU-idle stalls
  (both unexplained). See ROADMAP M7.

See [reference/FEASIBILITY.md](reference/FEASIBILITY.md) and
[reference/ROADMAP.md](reference/ROADMAP.md).

| Item | Value |
|---|---|
| Target | Apple Silicon Macs (primary test device: MacBook Air M5). Current builds need macOS 27, because the Homebrew libraries they bundle do |
| App / executable | `ReCarbon.app` (`scripts/macos/make_app.py`; a build against `scripts/macos/build_deps.sh`'s libraries passes `check_app`; v0.1.0 is published as an ad-hoc signed dmg) / `ReCarbon` |
| Bundle ID | `org.retronative.recarbon` |

## Installing

You need *Need for Speed: Carbon* (USA) for Xbox 360, from your own disc image (`.iso`) or
extracted folder. Other versions (PC, PlayStation, Wii, original Xbox, other regions) won't work:
the installer checks every file against the USA disc, and says why when one doesn't match.

ReCarbon is signed ad hoc and not notarized, so macOS blocks a downloaded copy the first time you
open it. After a blocked attempt, choose **System Settings > Privacy & Security > Open Anyway** and
confirm, once, and only for a copy whose checksum you've verified
(`shasum -a 256 -c ReCarbon-0.1.2-macos.dmg.sha256`). Never turn Gatekeeper off. The
full steps, including installing the game from your own disc image, are in
[docs/installation.md](docs/installation.md).

## How it works

ReCarbon is a static recompilation, not an emulator. XenonRecomp translates the
game's PowerPC code into C++ ahead of time, and that C++ compiles to native arm64. A runtime
stands in for the Xbox 360's kernel, GPU (Vulkan on Metal through MoltenVK), XMA audio and
controllers (SDL3).

It builds on [SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker), the
static recompilation of *Need for Speed: Most Wanted* (2005) by project(u). It does not
rewrite SpeedBreaker. There is no Wine, CrossOver or runtime CPU emulation.

## Documents

- [docs/](docs/README.md): the player documentation (also the website's docs pages)
- [reference/FEASIBILITY.md](reference/FEASIBILITY.md): disc and executable analysis, SpeedBreaker audit, risks, verdict
- [reference/ARCHITECTURE.md](reference/ARCHITECTURE.md): proposed architecture and project naming
- [reference/ROADMAP.md](reference/ROADMAP.md): milestones and acceptance criteria
- `reference/analysis/xexinfo.py`: reads the plaintext header of an Xbox 360 executable (XEX2)
- `reference/analysis/xbeinfo.py`: reads the header of an original-Xbox executable (XBE)
- [website/](website/README.md): the project website (Astro); not deployed
- [CONTRIBUTING.md](CONTRIBUTING.md): how to report bugs and contribute
- [SECURITY.md](SECURITY.md): how to report a security problem privately

## Building from source

You need macOS on Apple Silicon and Homebrew, plus your own disc extracted so that
`game/files/default.xex` exists:

```sh
brew install llvm lld cmake ninja python
brew install sdl3 vulkan-loader vulkan-headers glslang spirv-tools molten-vk
```

Then, from the repository root:

```sh
scripts/setup_tools.sh     # XenonRecomp (pinned + patches/xenonrecomp), extract-xiso, xexsurvey,
                           # FFmpeg 7.1.1's XMA decoder (patches/ffmpeg), ds_store and mac_alias
                           # (the dmg's Finder layout) into tools/
scripts/survey.sh          # analyse default.xex, regenerate config/, recompile to ppc/, validate
tests/run_instr_tests.sh   # instruction tests for the patched XenonRecomp

cmake -S . -B build/main -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
ninja -C build/main ReCarbon
```

Then install the game from your disc image (`build/main/runtime/ReCarbon --install <image.iso>`):
see [docs/installation-advanced.md](docs/installation-advanced.md) and [docs/developers.mdx](docs/developers.mdx).

To build the unit tests too, configure with `-DRECARBON_TESTS=ON` (needs `scripts/setup_tools.sh`
first, like the game), then:

```sh
ninja -C build/main
ctest --test-dir build/main
```

`scripts/verify.sh` runs the whole gate in order (build, the unit tests with the timing-sensitive
ones alone at the end, the instruction tests, `validate_recomp.py`) and stops at the first failure;
`ninja -C build/main recarbon_tests` builds the tests alone, without the game.

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
MoltenVK is Homebrew's 1.4.2, pinned by its SHA-256 in `make_app.py`. v0.1.0 is built this way.

`tools/`, `game/`, `ppc/` and `build/` are git-ignored. Never commit or share them.

## Credits and license

ReCarbon is GPL-3.0-or-later (see `COPYING` and `NOTICE`). It is derived from
[SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker) by project(u):
`survey/`, `patches/xenonrecomp/0001–0008`, `tests/` and the build scripts come from it, with
ReCarbon's changes marked. SpeedBreaker in turn builds on Unleashed Recompiled, XenonRecomp
(MIT) and Xenia (BSD-3-Clause). See `LICENSES/`.
