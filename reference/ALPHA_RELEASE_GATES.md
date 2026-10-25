# Alpha publication gates (internal, 2026-10-26)

What must be decided or prepared before the ReCarbon alpha is published. Each gate gives the
facts found in this repository, the options and the decision. Nothing here has been published,
pushed, tagged or uploaded.

Related: `reference/M8_ACCEPTANCE.md` (what was validated), `reference/RELEASE_CANDIDATE.md`
(how an RC is built), `reference/ALPHA_RELEASE_NOTES.md` (the draft notes),
`reference/M8_ALPHA_CHECKLIST.md` (the checklist and the shortest path).

| Gate | Subject | Decision |
|---|---|---|
| G1 | Binary | Publish the dmg and its checksum |
| G2 | LGPL parts: source archive, written offer, replaceable libraries | Replaceable dylibs; the source archive and the offer on the release page |
| G3 | Signing and notarization | Ad hoc, with the documented "Open Anyway" |
| G4 | Publication | Push, website redeploy, tag, release page, upload |

## Decisions (2026-10-26) and their state

- **G1: publish the dmg** (`ReCarbon-macos.dmg` and its `.sha256`) on the GitHub Release page.
- **G2: replaceable dynamic libraries**, done in 69faf7f: FFmpeg's
  `libavcodec.61.dylib` and `libavutil.59.dylib` (built shared by `scripts/build_ffmpeg_xma.sh`) and
  libmspack's LZX decoder `librecarbon_lzx.dylib` (its own CMake target) are separate files in
  `Contents/Frameworks/`, named `@rpath/<file>`, ad-hoc signed like the other libraries;
  `check_app.py` check 7 fails a build with any of their code in the executable
  (`tests/lgpl_dylibs_test.py`). The written offer and the replacement steps are in
  `THIRD_PARTY_NOTICES.md` (968f03f, bundled in the app). The third-party source archive is built
  locally: `build/release-src/ReCarbon-0.0.1-third-party-source.tar.xz` (40,814,628 B, SHA-256
  `8635a65fbcbdf655fd6238ae0e954231a5be50e5f0aa587db5529debd432dade`, with `.sha256`; README and
  `SHA256SUMS` inside), from files already on this Mac; not uploaded. It is published on the
  release page next to the dmg, with the offer (the notices say "for as long as the binary is
  offered there").
- **G3: ad hoc signing** (option 2), with the documented **Open Anyway** step (README,
  `docs/installation.md`, `docs/troubleshooting.md`, `docs/download.md`).
- **The final RC** with all of it: `build/rc/f6ce759-20261025T180528Z` (24/24 PASS), below and in `M8_ACCEPTANCE.md`.
- Still open: **G4** (push, website redeploy, tag, release page, uploads), and the switch
  of `docs/download.md` and the other public pages to "released" (a release branch does that).

## G1. Binary

**Facts**
- The download is `ReCarbon-macos.dmg` (`ReCarbon.app` and a shortcut to Applications) with its
  `.sha256`, as `scripts/release_candidate.sh` puts them in an RC's `release/`.
- The app contains no game data files: the RC's content gate (`rc_manifest.py`) rejects `.xex`,
  `.iso`, `ZZDATA*`, generated sources and the like, and the player installs the game from their
  own disc image.
- The build is reproducible from a clean checkout plus the player's own disc: two runs from
  scratch give byte-identical `config/` and `ppc/` (AGENTS.md), and `release_candidate.sh` builds
  from a fresh clone of one commit.

**Decision:** publish the dmg (2026-10-26).

## G2. LGPL-2.1 parts

**Facts**
- **FFmpeg 7.1.1** (LGPL-2.1-or-later), built by `scripts/build_ffmpeg_xma.sh` (XMA1/XMA2/WMA Pro
  decoders only, no GPL parts) with `patches/ffmpeg/0001-xma-single-stream-no-holdback.patch`.
  The release build takes it from `build_deps.sh`'s `<prefix>/ffmpeg-xma`. Up to 94750af
  `libavcodec.a` and `libavutil.a` were **linked statically** into the executable; since 69faf7f
  they are `libavcodec.61.dylib` and `libavutil.59.dylib` in `Contents/Frameworks/`.
- **libmspack** (LGPL-2.1): only `lzxd.c` is used, from XenonRecomp's submodule; up to 94750af
  it was **compiled into** the executable, since 69faf7f it is `librecarbon_lzx.dylib` in
  `Contents/Frameworks/`. Its source: `https://github.com/kyz/libmspack`
  at `305907723a4e7ab2018e58040059ffb5e77db837` (tag v1.11, 2023-02-24), the revision XenonRecomp's
  pinned base `ddd128bcca99fe8bfbb99bea583c972351fa6ace` records. ReCarbon's XenonRecomp patches
  don't touch it.
- The other bundled libraries are dynamic libraries in `Contents/Frameworks/` (SDL3, Vulkan
  loader, glslang, SPIRV-Tools, MoltenVK), under Zlib, Apache-2.0, BSD and MIT terms
  (`THIRD_PARTY_NOTICES.md`). The release copies of all but MoltenVK are built from source by
  `scripts/macos/build_deps.sh`; MoltenVK 1.4.2 is Homebrew's bottle, pinned by SHA-256. No
  MoltenVK source archive exists on this machine.
- `THIRD_PARTY_NOTICES.md` names the source of the LGPL parts at the versions used;
  `docs/download.md` promises the source offer with a release. SpeedBreaker ships a third-party
  source archive per release.
- LGPL-2.1 section 6 concerns letting the user modify the library and use the modified version
  with the app.

**The source archive, as it would be made from files on this machine** (SHA-256 verified
2026-10-26 against the pins in `build_deps.sh` and `setup_tools.sh`; a draft manifest is in
`build/release-src/MANIFEST-draft.tsv`, git-ignored, never uploaded):

| File | SHA-256 |
|---|---|
| `ffmpeg-7.1.1.tar.xz` | `733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1` |
| `patches/ffmpeg/0001-xma-single-stream-no-holdback.patch` | `bc89077528acbe34d6cce2b69da12738e21cd19fab5885c6187dc487ec246601` |
| `scripts/build_ffmpeg_xma.sh` (at e8eb933, the final RC; `8774f12a…` at 94750af) | `1626fa21cd20d92547959d315c598306423022698983052cfcc1c130fc7d13d6` |
| libmspack at `3059077` (only `lzxd.c` is used: `622183f03bac71a12327bc9f036395dcb5c3b7c6cc7d766785de24ac1354256a`) | `libmspack-305907723a4e….tar.gz`, a `git archive` of that revision: `c018e87f79e8b04012fdb6fae454c600245f649602d77176186671ddf0376db8` |
| `SDL3-3.4.18.tar.gz` | `9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3` |
| `Vulkan-Loader-vulkan-sdk-1.4.363.0.tar.gz` | `941eff558fc74f248745fb7df367640cf89a8457f216dae498a74ae14b6ba6f7` |
| `Vulkan-Headers-vulkan-sdk-1.4.363.0.tar.gz` | `4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e` |
| `SPIRV-Tools-vulkan-sdk-1.4.363.0.tar.gz` | `e6c83a215538fbfd265ccf398d3877985055df1273dbed73912bd05307664688` |
| `SPIRV-Headers-vulkan-sdk-1.4.363.0.tar.gz` | `5b3a0d099f5c7649e29c0938476c462e71d66ad89a0bde42fa1ba3eb476956c9` |
| `glslang-16.6.0.tar.gz` | `9c09b901149c729df745057dafa815278aaa101b84d2b6e14f16a42de52f97f2` |
| `scripts/macos/build_deps.sh` (at e8eb933; `0e746703…` at 94750af) | `afe8c361346122656cb9fb306690c9796c1128658a83c99cfeb866b6b5c0cee1` |
| MoltenVK 1.4.2 | no source here (Homebrew bottle only) |

**Replacing the libraries:** FFmpeg and libmspack are built as replaceable dylibs in
`Contents/Frameworks/`, as the other libraries already are (69faf7f: `runtime/CMakeLists.txt`,
`build_deps.sh`, `make_app.py`, `check_app.py`, with its own RC and tests).

**Fixed inside the earlier RCs (282557c, 94750af):** both bundle the `THIRD_PARTY_NOTICES.md`
of their commit in `Contents/Resources/licenses/`, written before any release: it says there are
no downloads yet and carries no offer text. Likewise their compiled What's New is the CHANGELOG
v0.0.1 entry as of their commit ("A development snapshot, not a release"); the 2026-10-26
CHANGELOG edits reach only a later build. Publishing either RC as is means shipping that text.
**The final RC e8eb933 replaces them**: its notices carry the offer and the replacement steps,
and its What's New calls v0.0.1 the first alpha release.

**Still to publish:** the archive next to the dmg, and the offer on the download page, with the
release change.

**Decision:** replaceable dylibs (2026-10-26), the offer as in `THIRD_PARTY_NOTICES.md`.

## G3. Signing and notarization

**Facts**
- Both RCs are signed ad hoc (`codesign -dv`: `Signature=adhoc`, no Team ID), not notarized;
  `spctl --assess --type execute` rejects them.
- Copies built on the M5 or sent with `scp` carried no `com.apple.quarantine` attribute, so no
  Gatekeeper prompt was ever seen. A browser download sets the attribute (not tested with
  ReCarbon): macOS then blocks the first launch until the user opens System Settings > Privacy &
  Security and chooses **Open Anyway** for ReCarbon. The documentation must never tell users to
  turn Gatekeeper off.

**Options**
1. **Developer ID + notarization:** Apple Developer Program membership (annual fee per Apple's
   current terms; an organization account needs a D-U-N-S number), a Developer ID Application
   certificate, signing with the hardened runtime and a secure timestamp, `xcrun notarytool submit`,
   then `xcrun stapler staple` on the dmg. The certificate names the account holder, which users
   see. The hardened runtime is a signing change in `make_app.py` that needs its own RC and a
   launch test on both Macs. Users then get the normal first-launch prompt.
2. **Ad hoc** (as today): no cost; every user of a downloaded copy has to use Open Anyway once,
   documented in the release notes and `docs/installation.md`.

**Decision:** ad hoc (option 2), 2026-10-26: the current RC ships as built.

## G4. Publication (nothing done)

- **Push** of the local commits to `origin/main` (`git log --oneline origin/main..main`): the 16
  from 716f126 to 94750af (716f126, 4cb161c, fe7a7b9, 868c6c7, 3e34a67, 505fad0, 9a402e2,
  0d60585, 5b1b463, 3cc36ad, eb6f321, b3dfe6a, 282557c, efaf30e, 0994d60, 94750af), plus the
  2026-10-26 documentation commits. Check the list again before pushing.
- **Website redeploy** (GitHub Pages, `.github/workflows/website.yml`), after the push; then
  `node scripts/gen-progress.mjs` again, so the Progress page can link the newly public commits.
- **Tag** (e.g. `v0.0.1-alpha`) on the RC's commit (94750af or 282557c), the commit the binary
  names in Settings > Advanced and `Info.plist` (`ReCarbonCommit`). The documentation commits
  after it don't change the app, except `CHANGELOG.md`: its v0.0.1 entry is compiled into the
  game's What's New, so the RCs show the entry as it was at their commit.
- **Release page** (text: `reference/ALPHA_RELEASE_NOTES.md`) and `docs/download.md` switched
  from "no release yet" to the release, in the same change.
- **Binary upload** of the chosen dmg with its `.sha256` (and, per G2, the source archive), only
  after G1, G2 and G3.

**Decision:** each approval separately, in that order.
