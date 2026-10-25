# Alpha publication gates (internal, 2026-10-26)

What must be decided or prepared before the ReCarbon alpha is published. Each gate gives the
facts found in this repository, the options and the decision to make. Nothing here has been
published, pushed, tagged or uploaded.

Related: `reference/M8_ACCEPTANCE.md` (what was validated), `reference/RELEASE_CANDIDATE.md`
(how an RC is built), `reference/ALPHA_RELEASE_NOTES.md` (the draft notes),
`reference/M8_ALPHA_CHECKLIST.md` (the checklist and the shortest path).

| Gate | Subject | Decision needed |
|---|---|---|
| G1 | Binary | What is published: the dmg and its checksum |
| G2 | LGPL parts: source archive, written offer, replaceable libraries | What to publish and how the libraries can be replaced |
| G3 | Signing and notarization | Developer ID + notarization, or ad hoc with "Open Anyway" |
| G4 | Publication | Push, website redeploy, tag, release page, upload |

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

**Options**
1. Publish the dmg (with G2 and G3 done).
2. Defer publication.

**Decision:** which option.

## G2. LGPL-2.1 parts

**Facts**
- **FFmpeg 7.1.1** (LGPL-2.1-or-later), built by `scripts/build_ffmpeg_xma.sh` (XMA1/XMA2/WMA Pro
  decoders only, no GPL parts) with `patches/ffmpeg/0001-xma-single-stream-no-holdback.patch`.
  The release build takes it from `build_deps.sh`'s `<prefix>/ffmpeg-xma`; `libavcodec.a` and
  `libavutil.a` are **linked statically** into the executable (`runtime/CMakeLists.txt` lines
  71–72, 154).
- **libmspack** (LGPL-2.1): only `lzxd.c` is **compiled into** the executable
  (`runtime/CMakeLists.txt` line 11) from XenonRecomp's submodule: `https://github.com/kyz/libmspack`
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
| `scripts/build_ffmpeg_xma.sh` (at the release commit) | `8774f12ac1aa511d97292b1cbccccee0c37209e30b8d5ed4d90c0ca0994618b8` |
| libmspack at `3059077` (only `lzxd.c` is used: `622183f03bac71a12327bc9f036395dcb5c3b7c6cc7d766785de24ac1354256a`) | a `git archive` of that revision, still to make |
| `SDL3-3.4.18.tar.gz` | `9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3` |
| `Vulkan-Loader-vulkan-sdk-1.4.363.0.tar.gz` | `941eff558fc74f248745fb7df367640cf89a8457f216dae498a74ae14b6ba6f7` |
| `Vulkan-Headers-vulkan-sdk-1.4.363.0.tar.gz` | `4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e` |
| `SPIRV-Tools-vulkan-sdk-1.4.363.0.tar.gz` | `e6c83a215538fbfd265ccf398d3877985055df1273dbed73912bd05307664688` |
| `SPIRV-Headers-vulkan-sdk-1.4.363.0.tar.gz` | `5b3a0d099f5c7649e29c0938476c462e71d66ad89a0bde42fa1ba3eb476956c9` |
| `glslang-16.6.0.tar.gz` | `9c09b901149c729df745057dafa815278aaa101b84d2b6e14f16a42de52f97f2` |
| `scripts/macos/build_deps.sh` (at the release commit) | `0e746703371c3f824ccf619e6b7f2972b963185b57b60b031920d9e127d0a0b5` |
| MoltenVK 1.4.2 | no source here (Homebrew bottle only) |

**Replacing the libraries:** build FFmpeg and libmspack as replaceable dylibs in
`Contents/Frameworks/`, as the other libraries already are. A build change
(`runtime/CMakeLists.txt`, `build_deps.sh`, `make_app.py`, `check_app.py`) that needs its own RC
and tests: a later decision, not part of this alpha's RCs.

**Fixed inside the RCs (no rebuild allowed now):** both RCs bundle the `THIRD_PARTY_NOTICES.md`
of their commit in `Contents/Resources/licenses/`, written before any release: it says there are
no downloads yet and carries no offer text. Likewise their compiled What's New is the CHANGELOG
v0.0.1 entry as of their commit ("A development snapshot, not a release"); the 2026-10-26
CHANGELOG edits reach only a later build. Publishing either RC as is means shipping that text;
changing it needs a new RC.

**Still to prepare:** the archive itself (published next to the dmg), the written offer in the
app (Settings or the bundled `licenses/README.txt`) and on the download page. The current RCs
carry the notices and license texts but no offer text.

**Decision:** how the libraries become replaceable, and the offer's wording and duration.

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

**Decision:** which option. Option 1 needs a new RC and the user's tests; option 2 can ship the
current RC.

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
