# ReCarbon v0.0.1 alpha: release notes (DRAFT, not published)

> **Internal draft, 2026-10-26.** No release exists. This text is for the release page and for
> `docs/download.md` once gates G1–G4 are decided (`reference/ALPHA_RELEASE_GATES.md`).
> Placeholders are marked **[GATE …]**. Until then the public pages say there is no release.
>
> **Lifting it into `docs/download.md`:** everything between the two `8<` lines below is written
> in the docs' conventions (GitHub alerts, links relative to `docs/`, license links to `../`), so
> it can replace that page's body as is. Keep the page's front matter, change its `title` to
> "Download" and its `description`, and fill or remove every **[GATE …]** first. The links resolve
> only from `docs/`, not from `reference/`.

<!-- 8< ---------------- docs/download.md body starts here ---------------- -->

> [!WARNING]
> **Alpha**
>
> ReCarbon v0.0.1 is the first playable alpha: races, career progress and saves work, but it is
> not finished. Read [Known issues](known-issues.md) before you play.

## What this is

ReCarbon is an unofficial native port of *Need for Speed: Carbon* (Xbox 360) to Apple Silicon
Macs: the game's own executable, from **your** disc, statically recompiled to arm64 and run on
SpeedBreaker's runtime with Vulkan on Metal (MoltenVK). This is the **first playable alpha**:
races, career progress and saves work; it is not finished, and the issues below are known.

**No game files are included.** You install the game from your own disc image.

## What works (measured, and on which Mac)

All measurements on a MacBook Air M5 (24 GB, macOS 27.0.1) unless marked M1.

- **Install from your disc image:** the first-run installer copied and verified 78 files
  (5.74 GB) from an ISO in 49–86 s, then started the game; also on a brand-new macOS user account.
- **Racing:** full Quick Races driven by hand on the keyboard. A 9.8-minute, 8-lap race averaged
  50.8 fps with no slowdown over the race; 1st place against 7 AI opponents who all finished.
  Driving physics feel normal to the tester (equivalence with the Xbox 360 version isn't
  verified).
- **Career and saves:** a career race won, with its progress kept after a restart; a new profile
  created, then found and loaded after a relaunch.
- **Audio and movies:** music, engine and movies with sound; no audio underruns in the race logs.
- **Game Mode** turns on in fullscreen.
- **Clean exit** with Quit Game; no crash, hang report or GPU device loss in any of the release
  candidate's sessions. More numbers: [Performance](performance.md), [Supported hardware](hardware.md).
- **MacBook Air M1** (supplementary): three full races finished; they start at about 30 fps and
  slow down the longer you play (below).

## Requirements

- An **Apple Silicon** Mac (measured: MacBook Air M5; MacBook Air M1 with lower frame rates).
- **macOS 27 or newer.**
- Your own Xbox 360 disc of *Need for Speed: Carbon* as a disc image (`.iso`) or extracted folder.
- About **5.7 GB** free for the installed game (5.74 GB as Finder counts it), plus the app (about
  70 MB) and the shader cache.

## Install

1. Download `ReCarbon-macos.dmg` and check it: `shasum -a 256 -c ReCarbon-macos.dmg.sha256`
   (the expected SHA-256 is **[GATE G4: the chosen RC's dmg SHA-256; the final RC e8eb933's is
   `6ba57adb4471a05a29d5c5d7a7c35b5fa812b8c2deda01222bd8b2ab01bdb510`]**). The `.dmg` file shows
   macOS's generic disk-image icon; ReCarbon's icon appears once it is open.
2. Open the dmg and drag **ReCarbon** onto **Applications**.
3. **First launch:** ReCarbon is signed ad hoc and **not notarized** (G3, decided 2026-10-26). If
   macOS says it can't verify the app, open **System Settings > Privacy & Security**, find the
   message about ReCarbon and choose **Open Anyway**, then confirm. You do this once. Don't turn
   Gatekeeper off.
4. ReCarbon's installer opens: pick your disc image (or browse for it). It checks the image,
   copies and verifies the files, then starts the game.
5. At the main menu the game says **"Cannot connect to Xbox Live!"**: press **A** (**Space** on
   the keyboard) once to continue. It does this at every launch.

The full steps, the command-line installer and where ReCarbon keeps things are in
[Installation](installation.md).

## Controls (keyboard, the WASD preset)

| Action | Key |
|---|---|
| Throttle / Brake | W / S |
| Steer | A / D (or Left / Right) |
| Handbrake (A) | Space |
| Nitrous (B), on a car that has it | Backspace |
| Speedbreaker (X) | Left Shift |
| Change camera (RB) / Look back (LB) | E / Q |
| Start, Pause / Back | Return / Escape |
| Menus | Arrow keys, Space to accept, Backspace to go back |
| ReCarbon's Settings | F1 (Fn+F1) or Cmd+, |

An **Arrows** preset and custom profiles are in Settings > Controls ([Controls](controls.mdx)). A controller is read by
position (a PlayStation pad's Cross is A), but **no physical controller has been tested**.
Hand-tested: throttle, brake, steering, camera, handbrake, Speedbreaker and nitrous. Reverse,
Look Back, Pause and the manual-gearbox gear keys were seen only on a virtual controller.

## Known limitations

The full list, with details: [Known issues](known-issues.md).


- Occasional stalls of about **1 s** (the presenter waiting for a drawable while holding the GPU
  queue lock), and shorter **250–600 ms** GPU-idle stalls with no known cause.
- The **first race after a fresh install can hitch** (up to about 450 ms) while the shader cache
  fills; later sessions are much smoother.
- **MacBook Air M1:** races start at about 30 fps and fall to about 16–22 fps in long races. GPU
  throttling is suspected, not confirmed. A sustained 30 fps isn't claimed on the M1, and this
  release wasn't install-tested on an M1.
- **Memory grows in long sessions:** textures aren't evicted while playing (about 500 MB over 7
  laps). Restarting clears it. A fix is planned after the alpha.
- The **Xbox Live notice** needs one press of A (Space) at every launch.
- **Physical controllers** are untested. Keys not yet tried by hand: Reverse, Look Back, Pause,
  and the gear keys with a manual gearbox (with an automatic car the tester saw K and ; shift
  gears, not yet reconciled with the virtual-controller result). Once, the first Backspace press
  didn't fire the nitrous; why isn't known.
- The **Arrows preset** wasn't driven by hand, and a custom profile surviving a restart was
  checked by scripts only.
- **Non-Latin keyboard layouts** (e.g. Myanmar): Settings > Controls shows letter keys as "?".
  Gameplay input is unaffected.
- **Movies:** correct speed, uneven frame pacing.
- **Physics** equivalence with the Xbox 360 version isn't verified.
- Game Mode was verified in the tester's main account, not captured on the fresh account.
- **macOS 27 or newer** only; render scaling and widescreen options are not in this alpha.

## Reporting a problem

Open Settings (F1), **Advanced > Save Bug Report** ([Troubleshooting](troubleshooting.md)): it writes one `.zip` to your Desktop (logs,
crash or hang reports, version, system summary, settings, a screenshot); nothing is sent
anywhere. Open an issue at `https://github.com/retronative/recarbon/issues` with what you did,
what happened, your Mac and macOS version, and attach the `.zip` if you are comfortable sharing
it. Never attach game files.

## Licenses and source

ReCarbon is free software under the GPL-3.0-or-later; the app carries every component's notice
and license text in `ReCarbon.app/Contents/Resources/licenses/` ([License](../COPYING),
[Notices](../NOTICE), [Third-party notices](../THIRD_PARTY_NOTICES.md)).

**Source.** FFmpeg's libavcodec and libavutil and libmspack's LZX decoder (LGPL-2.1) are separate
libraries in `ReCarbon.app/Contents/Frameworks/` that you can replace with your own builds; their
complete source is on this page as `ReCarbon-0.0.1-third-party-source.tar.xz` (SHA-256
`8635a65fbcbdf655fd6238ae0e954231a5be50e5f0aa587db5529debd432dade`, also in its `.sha256`), and
the steps to replace one are in [Third-party notices](../THIRD_PARTY_NOTICES.md).
**[GATE G4: ReCarbon's own source for this build: the tag.]**

ReCarbon is not affiliated with or endorsed by Electronic Arts. "Need for Speed" and "Carbon" are
trademarks of Electronic Arts Inc.

<!-- 8< ---------------- docs/download.md body ends here ---------------- -->
