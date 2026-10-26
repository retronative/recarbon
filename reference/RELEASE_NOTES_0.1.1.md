# ReCarbon v0.1.1: release notes (DRAFT, not published)

> **Internal draft, 2026-10-26.** The text for the v0.1.1 release page, with the checksums of the
> release build `80a7f3f` (24/24 PASS, `build/rc/80a7f3f-20261026T151956Z`; the tag v0.1.1) and of
> the third-party source archive built beside it. Nothing is published without the user's approval.

<!-- 8< ---------------- release page body starts here ---------------- -->

ReCarbon v0.1.1 is an update to the first playable alpha of ReCarbon, an unofficial native port of
*Need for Speed: Carbon* (Xbox 360) to Apple Silicon Macs. **No game files are included**: you
install the game from your own *Need for Speed: Carbon* (USA) disc image for Xbox 360.

## What's new

- **Smoother movies.** Each frame of the intro and the other movies now shows for an even two
  refreshes: on a MacBook Air M5, the frames shown for one or three refreshes instead fell from
  18.5% to 1.7%, with the same playback length.
- **Keyboard layouts in another script** (Myanmar, Thai and others): Settings > Controls names keys
  by their position ("W") instead of "?", and you can rebind them. A binding is the key's
  position, as on any layout, so existing bindings don't change. This is covered by automated tests;
  it hasn't been tried in the game yet.
- **Clearer installer messages** when it turns a disc image or folder down: another platform's disc,
  another game, or another region or version.
- **A new installer window** in the download: the app, an arrow and Applications, to drag onto.
- **A new look in racing red and silver:** a new app icon (sharp at every size), the installer
  window and the website.
- The **"Cannot connect to Xbox Live!"** notice is the game's own and not a ReCarbon error: press
  **A** (**Space**) once at the main menu and play on.

## Known issues

Occasional stalls of about a second while racing, and shorter GPU-idle ones; the slowdown in long
races on a MacBook Air M1; memory use that grows during long sessions (an experimental fix is
included but off); hitches in the first race after a fresh install while the shader cache fills.
A physical controller hasn't been tested. See the Known issues page.

## Requirements

An Apple silicon Mac with macOS 27 or later, and your own disc image.

## Downloads

| File | SHA-256 |
|---|---|
| `ReCarbon-0.1.1-macos.dmg` | `7a43e99189cd809614c68c7dec0dc5dc046b0acd3f250a4c561123f590a55941` |
| `ReCarbon-0.1.1-third-party-source.tar.xz` | `74926bf1918b9853aecbe000bec48c2830d49dd431496d53ff09d4d881ada6a8` |

Check the download with `shasum -a 256 -c ReCarbon-0.1.1-macos.dmg.sha256`. ReCarbon is signed ad
hoc and not notarized: macOS asks you to choose **Open Anyway** once (see Installation). The
third-party source archive holds the source of the LGPL parts (FFmpeg's decoders and libmspack's
LZX decoder), which are separate, replaceable libraries in the app.

<!-- 8< ---------------- release page body ends here ---------------- -->
