# ReCarbon v0.1.3: release notes (DRAFT, not published)

> **Internal draft, 2026-10-30.** The text for the v0.1.3 release page, from `release/v0.1.3`.
> Checksums of the release candidate `5539dca` (24/24 PASS, `build/rc/5539dca-20261030T142149Z`, the
> commit to tag v0.1.3) and of the third-party source archive built beside it
> (`build/release-src/ReCarbon-0.1.3-third-party-source.tar.xz`). Nothing is published without the user's approval.

<!-- 8< ---------------- release page body starts here ---------------- -->

ReCarbon v0.1.3 is a performance and stability update to the playable alpha of ReCarbon, an
unofficial native port of *Need for Speed: Carbon* (Xbox 360) to Apple Silicon Macs. **No game
files are included**: you install the game from your own *Need for Speed: Carbon* (USA) disc image
for Xbox 360.

## What's new

- **Smoother races and a cooler Mac.** The game's own busy-waits for the GPU, which run in every
  3D scene, no longer spin a CPU core: the thread sleeps between polls. On a MacBook Air M5 at 2x
  (1440p) this cut the main thread's CPU use in a race by more than half (about 860 to 260-340 ms
  a second in the test harness), and in a 9-minute play-test the races ran at 52-60 fps where the
  same Mac had run v0.1.2 at 31-37 fps earlier the same day. At 3x (2160p) the main thread fell from
  a full core to about an eighth of one and macOS's thermal throttling came about two and a half
  minutes later. Frame pacing on a cool Mac is unchanged from v0.1.2 (99 of 100 frames within 19 ms
  in both). `RECARBON_GPU_WAIT_BACKOFF=0` turns it off.
- **Fewer stalls while racing.** Vertex data up to 128 KB is copied with its draw instead of
  guarded against the game rewriting it, which removed a stall of 2.7 ms (2x) to 17 ms (3x) about
  once a frame on the M5. `RECARBON_VERTEX_COPY_128=0` restores the old 64 KB threshold.
- **Settings > Display > Frame Rate explains each choice** under the row: what Auto does, what 60
  costs when the Mac can't hold it, and what 30 gives.

## What this update does not do

It does not hold a sustained 60 fps at 2x on a fanless Mac. A MacBook Air throttles within a few
minutes at 2x and above; once it does, races at 2x run at about 52-56 fps with visible judder
(frames shown twice) until the Mac cools. 3x (2160p) runs at about 40-54 fps depending on heat.
Auto (2x) stays the recommended setting.

## Tested on MacBook Air M5

The CPU change was compared with v0.1.2 on the same Mac in matched 7-minute launches (frame
pacing level, CPU use down), switched on and off inside one session (present timing identical),
and in a play-test by hand (six races, no errors, no audio gaps); 30 fps, 1x, the pause menu and 3x
were checked for correctness. **Not yet tested on a MacBook Air M1 or any other GPU.** If
something looks wrong, start the game from Terminal with the switch off, for example
`RECARBON_GPU_WAIT_BACKOFF=0 /Applications/ReCarbon.app/Contents/MacOS/ReCarbon`.

## Known issues

Thermal judder at 2x and above once the Mac throttles (above). As in v0.1.2: occasional stalls of
about a second while racing, and shorter ones; the slowdown in long races on a MacBook Air M1;
memory use that grows during long sessions; hitches in the first race after a fresh install while
the shader cache fills. See the Known issues page.

## Requirements

An Apple silicon Mac with macOS 27 or later, and your own disc image.

## Downloads

| File | SHA-256 |
|---|---|
| `ReCarbon-0.1.3-macos.dmg` | `b0cb142a90d5fe86ee5bc90234c78a824a1a4ff240fe8ebc5f261913e446a071` |
| `ReCarbon-0.1.3-third-party-source.tar.xz` | `2277ed03badeb2da9402e2c186a2ad6d54c255f9a7284d5051b2d50885e784cb` |

Check the download with `shasum -a 256 -c ReCarbon-0.1.3-macos.dmg.sha256`. ReCarbon is signed ad
hoc and not notarized: macOS asks you to choose **Open Anyway** once (see Installation). The
third-party source archive holds the source of the LGPL parts (FFmpeg's decoders and libmspack's
LZX decoder), which are separate, replaceable libraries in the app.

Provenance: the app was built from the maintainer's release-branch commit `5539dca` (named in the
app's build identity, Settings > Advanced). The tag `v0.1.3` carries the same program source with the
maintainer's internal documents removed and a few comments reworded, so the runtime builds from the
tag reproduce this release apart from the embedded commit name.

<!-- 8< ---------------- release page body ends here ---------------- -->

## Publication checklist (internal; every step needs the user's explicit approval, none is done)

The candidate is `5539dca`; the branch tip carries documentation commits after it. The dmg and
its checksum come from `build/rc/5539dca-20261030T142149Z/release/` and are not rebuilt.

- [x] Version 0.1.3 in `CMakeLists.txt`, the CHANGELOG entry (built into the app), these notes; the
      verification record is kept privately by the maintainer (local, untracked).
- [x] `scripts/release_candidate.sh 5539dca` on the M5 with `RC_DEPS=build/deps-lgpl`: 24/24 PASS;
      the installed app's scripted launch check PASS.
- [ ] The user's hand checks (`reference/RELEASE_CANDIDATE.md`): checksum, install from the dmg,
      first launch, a full race, saves persist, pause and resume, audio, clean exit.
- [x] The third-party source archive `build/release-src/ReCarbon-0.1.3-third-party-source.tar.xz`
      (40,816,040 bytes, SHA-256 `2277ed03badeb2da9402e2c186a2ad6d54c255f9a7284d5051b2d50885e784cb`, its
      `.sha256` beside it): v0.1.2's archive with the README's version and the inner SHA256SUMS regenerated,
      the same 22 entries, no dependency pin changed since v0.1.2 (`git diff v0.1.2..5539dca` on
      `scripts/macos/build_deps.sh`, `scripts/build_ffmpeg_xma.sh`, `patches/ffmpeg`, `scripts/setup_tools.sh`
      is empty); inner `shasum -a 256 -c SHA256SUMS` 14/14 OK. Its SHA-256 goes into `docs/download.md`
      at publication.
- [ ] Docs and website on v0.1.3, one commit (`docs(release): v0.1.3 ...`, as e97fc0f did for v0.1.2):
      `docs/download.md` (link, file name, checksum, the source archive's checksum, the tag),
      `docs/installation.md`, `docs/installation-advanced.md`, `docs/index.md`, `docs/README.md`,
      `docs/known-issues.md` ("Tested for v0.1.3"), `README.md`, `SECURITY.md`,
      `website/src/components/Hero.astro`, `website/src/pages/index.astro`; then `npm run check`
      and `npm run build` in `website/` (generated tables must not drift).
- [ ] Merge `release/v0.1.3` into `main` (no fast-forward loss of the branch history; the website
      deploys from `main` through `.github/workflows/website.yml`).
- [ ] Tag `v0.1.3` on `5539dca` (the commit the dmg was built from), annotated with the release
      page's first paragraph.
- [ ] Push `main` and the tag; create the GitHub release `v0.1.3` with the page body above and the
      four files (`.dmg`, `.dmg.sha256`, the source archive, its `.sha256`, plus `SHA256SUMS`).
- [ ] Verify the published checksums against `build/rc/5539dca-20261030T142149Z/release/SHA256SUMS`
      and the live website's download page.

Rollback after publication: v0.1.2's release stays available; `docs/download.md` can point back to
it in one commit; the tag `v0.1.3` is not deleted.
