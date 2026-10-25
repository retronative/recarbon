# Release candidates (internal)

A release candidate (RC) is one commit built, packaged and checked the way a release would be,
on the M5 only, and tried by hand on the M1 and the M5. It is **local by design**: the build
compiles `ppc/`, generated from the player's own `default.xex`. Nothing made from the disc goes
to GitHub or any other service: no `ppc/`, decrypted image, disc contents, symbols or logs, and
no app or dmg outside a release. GitHub Actions stays limited
to checks that need no game files (today only `.github/workflows/website.yml`).

## Build one

```sh
scripts/release_candidate.sh <commit>        # e.g. scripts/release_candidate.sh "$(git rev-parse HEAD)"
```

Needs (checked before any work, nothing is written when one is missing):

- git, cmake, ninja, make, curl, perl; Homebrew's llvm (`clang`, `clang++`, `llvm-mc`,
  `llvm-objdump`) and lld; Python 3.11+ (`/opt/homebrew/bin/python3`); the Command Line Tools
  (`xcrun`, `otool`, `codesign`, `hdiutil`, `ditto`, the macOS SDK).
- The release libraries: `scripts/macos/build_deps.sh build/deps` (or `RC_DEPS=<prefix>`), built
  for macOS 27.0. Homebrew's libraries are never used in their place: the run fails if the build
  found any of them (`deps-guard`).
- Homebrew's MoltenVK at the SHA-256 the commit's `make_app.py` pins.
- The extracted disc: `game/files` (read only), or `RC_GAME=<folder>`, with `default.xex`.
- Network for downloads only: XenonRecomp and extract-xiso from GitHub (`setup_tools.sh`).
  `run_instr_tests.sh` uses Xenia's tests only when `tools/xenia-tests` exists, which no script
  of this commit fetches, so an RC runs `tests/instr/*.s` alone. FFmpeg's archive is copied from this repo's
  `tools/ffmpeg-src/` when there (`setup_tools.sh` checks its pinned SHA-256 either way).
  Nothing is uploaded.

Options: `RC_JOBS` (ninja, default 6) and `RC_NICE` (default 10; the whole run is reniced, since
other tests may share the Mac), `RC_LLVM`, `RC_PYTHON`, `RC_COMPARE_APP`.

Each run gets `build/rc/<short sha>-<UTC time>/` (git-ignored), marked `.retention.json` class
`evidence`, status `active`: it is never deleted automatically, a failed run included. Release
it through `retention_cleanup.py` like any evidence.

| Path | What | Leaves this Mac? |
|---|---|---|
| `src/` | a `git clone --local` of exactly that commit, detached (own `.git`), with its own `tools/`, `ppc/`, `game/survey/` | never |
| `build/` | the release build (as `build/release`, plus `-DRECARBON_TESTS=ON`) | never |
| `app/` | `make_app.py`'s `ReCarbon.app`, `ReCarbon-macos.dmg` (+ `.sha256`), the versioned dmg link, `symbols/` (dSYM) | never (the dmg only via `release/`) |
| `extracted/` | the app copied back out of the dmg | never |
| `logs/`, `steps.tsv`, `fields.tsv` | one log per step | never |
| `MANIFEST.json` | commit, version, toolchain, deps prefix and record hash, MoltenVK pin and file hash, package and binary SHA-256, every step with PASS/FAIL, time and log | |
| `release/` | on a pass only: the dmg, its `.sha256`, `MANIFEST.json`, `SHA256SUMS` | to our own test Macs only |

## What it checks

In order; the first failure stops the run, is recorded in `MANIFEST.json` (`result`,
`failed_step`) and exits non-zero. An interrupted run is recorded as failed.

1. `checkout`: the clone is at the commit and `git status --porcelain` is empty; `game/files`
   links the disc.
2. `tools`: the commit's `scripts/setup_tools.sh` (its own XenonRecomp patches).
3. `survey`: the commit's `scripts/survey.sh` regenerates `ppc/` and the config blocks from scratch.
4. `validate-recomp`, `instr-tests`: `survey/validate_recomp.py`, `tests/run_instr_tests.sh`.
5. `config-unchanged`: the regenerated `config/` is byte-identical to the committed one; then the
   checkout is still clean (checked again after the build and at the end).
6. `configure`, `deps-guard`, `build`, `build-identity` (`build_info.inc` names the commit, not
   dirty), `ctest`.
7. `package`: `make_app.py <build> app --dmg --min-macos=27.0`.
8. `check-app` (with `--same-code-as` the previous `build/app-release`, informational),
   `check-dmg`: `check_app.py`.
9. `gate-app`, and in `dmg-extract` `gate-volume` (`scripts/macos/rc_manifest.py`): nothing that
   looks like game data or generated sources (`.xex`, `.iso`, `ZZDATA*`, `ppc_*`, `.cpp`/`.h`,
   `.toml`, `.log`, `game/`, `survey/`, `symbols/` …), no symbolic link, no file outside the
   layout `make_app.py` writes (`Contents/{MacOS/ReCarbon, Frameworks/*.dylib, Resources/…,
   Info.plist, _CodeSignature}`), each within a size bound; the dmg's top level holds only
   `ReCarbon.app` and the `Applications` shortcut (plus the file system's own entries).
10. `dmg-extract`, `check-extracted`, `binary-match`: the dmg mounted read-only, the app copied
    out with `ditto` and checked again, its main binary's SHA-256 equal to the built one's.
11. `game-files-unchanged`: the disc folder's listing (names, sizes, times) is as before.
12. `release`: `release/` and `SHA256SUMS`.

## Send it to a test Mac

```sh
scripts/rc_transfer.sh build/rc/<id> m1-air project/thixpin/nfs/recarbon-worker/rc/<id>
```

Copies only `release/` (refusing a run that didn't pass, a release folder holding anything else,
and a remote folder that already exists), then runs `shasum -a 256 -c` there.

## Verify, install, first launch

On the test Mac, in the folder it arrived in:

```sh
shasum -a 256 -c SHA256SUMS && shasum -a 256 -c ReCarbon-macos.dmg.sha256
```

Open the dmg and drag `ReCarbon.app` onto `Applications`.

The app is **signed ad hoc** (`codesign -dv`: `Signature=adhoc`, no Team ID), not notarized, so
`spctl --assess --type execute` rejects it (checked on the M5's `build/app-release`, 2026-10-25).
Gatekeeper enforces that assessment on files carrying the `com.apple.quarantine` attribute. A
build made on the Mac itself has none: on the M5 the app lists only `com.apple.provenance`, and
the RC's dmg `com.apple.FinderInfo`, `com.apple.diskimages.recentcksum` and `com.apple.provenance`. Whether
`scp` sets the attribute, and how the copy on the M1 then behaves at first launch, are **not yet
verified**: note what you see (`xattr -l` on the dmg and the installed app) during the first RC's
acceptance. If macOS blocks it, use System Settings > Privacy & Security > Open Anyway (the user's
action); don't turn Gatekeeper off.

A launched app uses the real `~/Library/Application Support/ReCarbon` (game, saves, settings):
the acceptance run is the user's, on their own account.

## Acceptance checklist (by hand, on the M1 and the M5)

- [ ] Checksums verified as above; the version in Settings > Advanced matches `MANIFEST.json`.
- [ ] Clean launch from `/Applications` (no crash dialog, no Homebrew needed).
- [ ] Game data installs from the user's own disc image with the first-run installer
      (`docs/installation.md`), then the game starts.
- [ ] The Xbox Live notice is accepted (A, Space): a known alpha limitation.
- [ ] Fullscreen: Game Mode turns on (menu bar indicator).
- [ ] Controls: the WASD preset drives (throttle, brake, steering), menus respond.
- [ ] Audio: music and engine, no gaps.
- [ ] A full race to the finish.
- [ ] Saves persist: quit, relaunch, the profile and progress are there.
- [ ] Clean exit from the game's menu (no "ended without quitting" notice on the next launch).

Record the results, the Macs and the dmg's SHA-256 next to the RC's evidence.

## Licenses

Read from `COPYING`, `NOTICE`, `THIRD_PARTY_NOTICES.md`, `LICENSES/` and `reference/M8_PLAN.md`
(2026-10-25):

- ReCarbon is GPL-3.0-or-later. The app bundles SDL3 (Zlib; HIDAPI, yuv2rgb BSD-3-Clause), the
  Vulkan loader (Apache-2.0, cJSON MIT), glslang (BSD-3-Clause, MIT, GPL-3.0-or-later parts and
  others), SPIRV-Tools (Apache-2.0), MoltenVK (Apache-2.0, cereal BSD-3-Clause), and the code of
  XenonRecomp (MIT), Xenia (BSD-3-Clause) and the other components `THIRD_PARTY_NOTICES.md`
  lists. `make_app.py` puts their notices in `Contents/Resources/licenses/`.
- **LGPL parts** (gate G2, decided 2026-10-26: since 69faf7f separate, replaceable dylibs in
  `Contents/Frameworks/`, `libavcodec.61.dylib`, `libavutil.59.dylib`, `librecarbon_lzx.dylib`,
  checked by `check_app.py` check 7; the offer and the replacement steps are in
  `THIRD_PARTY_NOTICES.md`; the source archive in `build/release-src/`). **Up to 94750af linked
  statically into the `ReCarbon` executable:** FFmpeg 7.1.1 (LGPL-2.1-or-later,
  `libavcodec.a` and `libavutil.a` from `build_deps.sh`'s `ffmpeg-xma`, linked by
  `runtime/CMakeLists.txt`, with `patches/ffmpeg/0001-…`) and libmspack's `lzxd.c` (LGPL-2.1,
  compiled into the executable from XenonRecomp's submodule). `THIRD_PARTY_NOTICES.md` names
  their complete source at the versions used; SpeedBreaker ships a third-party-source archive
  per release. LGPL-2.1 section 6 also concerns letting users use the work with a modified
  library.
- **Must exist before a public release:**
  1. A third-party source archive (FFmpeg 7.1.1 tarball + patch + `build_ffmpeg_xma.sh`; libmspack
     at XenonRecomp's pinned submodule revision; the other bundled libraries' sources as built by
     `build_deps.sh`, with their SHA-256), published next to the binary, with the offer in the app
     and on the download page (`docs/download.md` promises it).
  2. A way for users to replace the LGPL libraries with their own builds.
  3. A decision on signing and notarization (today ad hoc).
  4. The acceptance checklist above passed on both Macs.
- `release_candidate.sh` and `rc_transfer.sh` publish nothing: they only build and copy to our
  own Macs.
