# ReCarbon v0.1.2: release notes (DRAFT, not published)

> **Internal draft, 2026-10-27.** The text for the v0.1.2 release page, from `release/v0.1.2`.
> Checksums of the release candidate `8fb6b5f` (24/24 PASS, `build/rc/8fb6b5f-20261027T134054Z`, the
> commit to tag v0.1.2) and of the third-party source archive built beside it. Nothing is published
> without the user's approval.

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

This is a GPU performance update: 3x (2160p, 4K) still runs at about 30-35 fps on a MacBook Air
M5, not 60. Auto (2x) stays the recommended setting there.

## Tested on MacBook Air M5

Each change was compared on and off in the same session, in a still scene (pixel-identical
captures) and in about 10 minutes of Quick Races driven by hand at 3x: no corruption, no cache
mismatches, no GPU errors. **Not yet tested on a MacBook Air M1 or any other GPU.** For a
comparison, or if something looks wrong, each can be turned off by starting the game from
Terminal with `RECARBON_SCALED_FUSE=0` or `RECARBON_SCALED_WANTED=0`, for example
`RECARBON_SCALED_WANTED=0 /Applications/ReCarbon.app/Contents/MacOS/ReCarbon`.

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
| `ReCarbon-0.1.2-macos.dmg` | `f64507d34cd786c0cb82ef03627d1661c7015ffd5c4d3d45be74c46b0f4ae021` |
| `ReCarbon-0.1.2-third-party-source.tar.xz` | `24531544d1c9524e5fa0d73a89d8bb42a9ae5325d7a5f9248cf1dcc8c648cba9` |

Check the download with `shasum -a 256 -c ReCarbon-0.1.2-macos.dmg.sha256`. ReCarbon is signed ad
hoc and not notarized: macOS asks you to choose **Open Anyway** once (see Installation). The
third-party source archive holds the source of the LGPL parts (FFmpeg's decoders and libmspack's
LZX decoder), which are separate, replaceable libraries in the app.

<!-- 8< ---------------- release page body ends here ---------------- -->

## Before release (internal)

- [x] `chore(release): version 0.1.2` (`CMakeLists.txt`), the CHANGELOG entry, these notes.
- [x] Docs and website on v0.1.2; `scripts/release_candidate.sh 8fb6b5f` on the M5: 24/24 PASS;
      the third-party source archive; checksums here and on the pages.
- [ ] Hand checks (`reference/RELEASE_CANDIDATE.md`) on the M5; the M1 needs the user's approval
      (it is reserved for other work).
- [ ] Merge, tag, push, upload, deploy: the user's approval for each.
