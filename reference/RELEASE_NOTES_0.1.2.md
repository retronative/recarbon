# ReCarbon v0.1.2: release notes (DRAFT, not published)

> **Internal draft, 2026-10-27.** The text for the v0.1.2 release page, from `perf/scaled-wanted`
> at `066644c`. No release candidate yet: the version (`CMakeLists.txt`, still 0.1.1), the RC run
> (`reference/RELEASE_CANDIDATE.md`), its checksums and the hand checks come with the release
> commit. Nothing is published without the user's approval.

<!-- 8< ---------------- release page body starts here ---------------- -->

ReCarbon v0.1.2 is an update to the playable alpha of ReCarbon, an unofficial native port of
*Need for Speed: Carbon* (Xbox 360) to Apple Silicon Macs. **No game files are included**: you
install the game from your own *Need for Speed: Carbon* (USA) disc image for Xbox 360.

## What's new

- **Less GPU work at higher internal resolutions, with the same picture.** A resolve the game
  renders at a higher resolution now also goes straight into the texture that reads it (from
  SpeedBreaker v0.1.9's SCALED_FUSE), and only what something reads is kept at that resolution
  (SpeedBreaker v0.2.0's ScaledWanted). On a MacBook Air M5 at 3x (2160p), measured while driving
  one change after the other: about 1.7 ms and then a further 1.6 ms less GPU time a frame, and
  1.5-2 fps more with each. At 2x frame times didn't change; at 1x neither applies.

## Tested on MacBook Air M5

Each change was compared on and off in the same session, in a still scene (pixel-identical
captures) and in about 10 minutes of Quick Races driven by hand at 3x: no corruption, no cache
mismatches, no GPU errors. Not yet measured on a MacBook Air M1.

## Known issues

As in v0.1.1: occasional stalls of about a second while racing, and shorter GPU-idle ones; the
slowdown in long races on a MacBook Air M1; memory use that grows during long sessions; hitches in
the first race after a fresh install while the shader cache fills. A physical controller hasn't
been tested. See the Known issues page.

## Requirements

An Apple silicon Mac with macOS 27 or later, and your own disc image.

## Downloads

| File | SHA-256 |
|---|---|
| `ReCarbon-0.1.2-macos.dmg` | (from the release candidate) |
| `ReCarbon-0.1.2-third-party-source.tar.xz` | (from the release candidate) |

<!-- 8< ---------------- release page body ends here ---------------- -->

## Before release (internal)

- [ ] Merge `perf/scaled-wanted` into `main` (approval).
- [ ] `chore(release): version 0.1.2`: `CMakeLists.txt` 0.1.1 -> 0.1.2, this file's header.
- [ ] `scripts/release_candidate.sh <commit>` on the M5: every step PASS.
- [ ] Hand checks (`reference/RELEASE_CANDIDATE.md`): the M5; the M1 needs the user's approval
      (it is reserved for other work).
- [ ] Checksums here and on the download, installation and Progress pages; the README's dmg name.
- [ ] Tag, push and publish: the user's approval for each.
