# M8 acceptance record (alpha packaging)

The state of each M8 criterion (ROADMAP M8) and its evidence, last updated 2026-10-26 (M8 closed). Objective
checks and the tester's reports are kept apart. Testing ended on 2026-10-26 by the user's
decision: what wasn't verified by then is listed as a limitation, not as passed.

**Decision (user, 2026-10-26): M8 is Completed for Alpha.** No mandatory alpha criterion
remains. Not recorded as passed:
- **Deferred:** M8.3, render scaling and widescreen (optional).
- **Unverified:** the physical controller (and controller rebinding on it), Game Mode on the
  fresh account (M8.4), and an install of a release candidate on the MacBook Air M1.

**State:** the alpha packaging is validated on the MacBook Air M5, and **v0.1.0 is released**.
Checked against the live GitHub Release on 2026-10-26 (`gh release view v0.1.0`): tag `v0.1.0`
on 598a0ea (an annotated tag), a pre-release, not a draft, published 2026-10-25T19:00:52Z, with
four assets: `ReCarbon-0.1.0-macos.dmg` (22,071,063 B, SHA-256
`abc376fb04550842fe4a5a34d4feb75c8c9dd302a8b45db4f590be3018d60d9b`) and its `.sha256`, and
`ReCarbon-0.1.0-third-party-source.tar.xz` (40,815,220 B, SHA-256 `a0e805b4…e883`) and its
`.sha256`. The release was built as the final RC at 598a0ea (24/24 steps PASS, version 0.1.0,
build 207; its `__TEXT,__text` identical to e8eb933's). Gates: `reference/ALPHA_RELEASE_GATES.md`.

**Commit ids:** the RC folders, their `MANIFEST.json` and `Info.plist` name the commits they were
built from before the unpublished commits' text was revised on 2026-10-26. The code is the same;
this record uses the new ids: 755b1c5 is now 282557c, b06e341 is 94750af, f6ce759 is e8eb933.

## Criteria

| Criterion | Status | Evidence |
|---|---|---|
| M8.1 Installer parametrized for Carbon (manifest, title ID, ReCarbon names) | **Completed** | Carbon's manifest (title 0x454107EC, 78 files). RC 282557c on the M5: the first-run installer copied and verified 78 files (5.74 GB) from the user's ISO into `~/Library/Application Support/ReCarbon/game`, in the main account (86.2 s, session 2026-10-25_23-13-21) and in the fresh account `gamtest` (48.7 s, session 2026-10-25_23-47-12); the game started each time. Earlier: `ReCarbon --install` 78 files, and the game found in the default folder (2026-10-23). |
| M8.2 `ReCarbon.app` with license notices and LGPL sources | **Completed** (released in v0.1.0, with the source archive as a release asset) | The notices, license texts and each library's provenance are in `Contents/Resources/licenses/` (make_app.py; checked by the RC's gate). Bundle ID `org.retronative.recarbon`, version 0.0.1. Gate G2 (decided 2026-10-26): FFmpeg and libmspack's LZX decoder are replaceable dylibs in `Contents/Frameworks/` (check_app.py check 7), the notices carry the source offer and the replacement steps, and the third-party source archive is built locally (`build/release-src/`, `ALPHA_RELEASE_GATES.md`). Replacement verified on a copy of the RC (below). |
| M8.3 Optional: render scaling and widescreen hooks | **Deferred** (optional) | Not started. Not an alpha gate. |
| M8.4 Clean install from a disc image on a fresh macOS account | **Completed on the M5** | Account `gamtest` (UID 502, created by the user), RC 282557c's app, no quarantine attribute. Session 1: ISO install, new profile save `ALIAS_ABO`, a race (3D mean 56.4 fps), Quit Game. Session 2: no "ended without quitting" notice, save `ALIAS_ABO` found and opened, a race (mean 54.5 fps, worst frame 100 ms), Quit Game. No hang report, strict stub, device loss or crash. **Not captured:** Game Mode in that account. **Seen:** the first race after the install hitched up to 450 ms while the empty shader cache compiled pipelines. Not done on the M1. |
| M8.5 `check_app.py` passes | Completed | e8eb933, 282557c and 94750af: `check-app`, `check-dmg` and `check-extracted` PASS (and on the release app cd7dc02 / d780e34 before). |
| M8.6 The unit tests run in ctest | Completed | The RCs' `ctest` step: 52/52 at 94750af, 53/53 at e8eb933 (with `lgpl_dylibs_test`). |
| M8.7 Keyboard presets WASD and Arrows | Completed | `profiles_test` (ROADMAP M8, B2). |
| M8.8 Persistent keyboard and controller profiles | Completed | Scripted restarts in isolated homes; `profile_store_test`. |
| M8.9 Custom mappings, controller rebinding, switching, reset | Completed | Scripted runs and the virtual pad. The physical controller is not verified (deferred). |
| M8.10 `[keys]` migrated, never dropped | Completed | `profile_store_test`; scripted first launch. |
| M8.11 Presets drive the game; custom profiles survive a restart; switching without a restart | **Completed** (presets by hand; persistence and switching automated) | **Human:** the WASD preset drove every hand-tested race; the user reports (2026-10-26) playing the game with the **Arrows** preset by hand and that it worked. No log of that session was collected, so it rests on the user's report. **Automated:** custom keyboard and controller profiles survive a restart and switching takes effect at once, in scripted runs in isolated homes (ROADMAP M8, B2); each preset's D-pad keys resolve to D-pad buttons (`profiles_test`). Not done by hand: a custom profile across a restart, and switching without a restart (ROADMAP's Verify line asks for unit tests and scripted sessions for these). |
| M8.12 Reset to default matches M5's baseline | Completed | Label-only change approved by the user (2026-10-24); the baseline key, mapping and parse checks pass. |

## Release candidates

Built by `scripts/release_candidate.sh` on the M5 (darkBook), libraries from `build/deps`
(record SHA-256 `1926e909…f5a3`), MoltenVK 1.4.2 pinned (`e21bcead…d542`), macOS 27.0.1, Homebrew
clang 23.1.4. Both 24/24 steps PASS. `build/rc/` is git-ignored; nothing in it is published.

| | 282557c | 94750af |
|---|---|---|
| Folder | `build/rc/755b1c5-20261025T155507Z` | `build/rc/b06e341-20261025T163359Z` |
| Version (build) | 0.0.1 (193) | 0.0.1 (196) |
| dmg SHA-256 | `94086c843cfa316114bf5e61237fa8c74f0691ae525efd1771d4534a75fdc51e` (22,503,635 B) | `e4234d650d679b1d33cbc70be365a7668cae1439d3b3e9757f80b7d50433e1dd` (21,924,559 B) |
| Game binary SHA-256 | `97491602da6ec1166f2cd3dffbb70138537776bcd1a83153aec2f5a275457b88` | `b9c1a740252d248621762ed1168541760fe0b8c6173cfcc03e42a65fb3acf4ed` |
| `MANIFEST.json` SHA-256 (release/) | `9a7ffddf54e2f96252b32a5b6cef7c3e0889f90777d90406af4c8045b60d263e` | `8eb7692ebdb5a718bf55c63356ac9fc2e5b7f1522c44ab0a3301d8874093074e` |
| Retention marker | class `evidence`, status `active` (read before the folder left `build/rc/`, below) | class `evidence`, status `active` |
| M5 by hand | **Install-validated:** main account (clean reinstall after the old data was moved aside, ISO install, Game Mode on, a race to the results, save `ALIAS_IHG` kept after a relaunch, Quit Game clean) and M8.4 (above). Evidence: `game/m7/accept-m5-755b1c5/`, WORKLOG. | **Not install-tested by a human.** |
| M1 | Copied with `rc_transfer.sh` (checksums verified) and installed in `/Applications` (identical to the dmg). **Never launched there; the M1 tests were cancelled (2026-10-26).** | Copied with `rc_transfer.sh`, checksums verified; not installed. |

**Retention note (2026-10-26, 00:10 +07):** while this record was written, the folders
`build/rc/755b1c5-20261025T155507Z` and the three `0d60585-*` runs disappeared from the M5's
`build/rc/`, outside this session and not through `retention_cleanup.py` (no audit entry). The
values above for 282557c were read from its `MANIFEST.json` and `SHA256SUMS` before that. Its `release/` was
copied to the M1 on 2026-10-25 (`rc/755b1c5-20261025T155507Z`; not rechecked since). The 94750af folder and its marker
and `release/` are intact, but its `extracted/ReCarbon.app` (the copy taken back out of the dmg)
also left at 00:12, the same way; `SHA256SUMS` verified at 00:14 and 00:23 (a Finder icon was added to the dmg file as extended
attributes at 00:09, which leaves its SHA-256 unchanged).

### The final RC: e8eb933 (2026-10-26)

`scripts/release_candidate.sh e8eb933` with `RC_DEPS=build/deps-lgpl` (build_deps.sh's libraries
with FFmpeg shared; record SHA-256 `43eabd15…e8a3`), MoltenVK 1.4.2 pinned. Folder
`build/rc/f6ce759-20261025T180528Z`, 24/24 steps PASS, version 0.0.1 (build 204), marker class `evidence`.

- dmg SHA-256 `6ba57adb4471a05a29d5c5d7a7c35b5fa812b8c2deda01222bd8b2ab01bdb510` (22,071,083 B);
  game binary `92cfdba7a2a88c1bc9d2a9d47cd7b08ed025bcc1b206b58ccf9c1069f5554d3e` (the dmg's copy
  identical); `MANIFEST.json` `f3226c881d8739cfbeed0532f1cc43a5d59f378cfd54ee617c1da9602be2dcfd`.
- Differs from 94750af by the LGPL dylibs (69faf7f), the release text (968f03f: What's New, the
  notices with the offer and replacement steps) and `release_candidate.sh`'s absolute `RC_DEPS`
  (e8eb933). Game code unchanged (`runtime/` only in `runtime/CMakeLists.txt`'s linking).
- **Linkage:** the executable links `@rpath/libavcodec.61.dylib`, `@rpath/libavutil.59.dylib`,
  `@rpath/librecarbon_lzx.dylib` with the one rpath `@executable_path/../Frameworks`; each dylib
  is named `@rpath/<file>`, rpath `@loader_path`, macOS 27.0, `Signature=adhoc`; no `/opt/homebrew`
  or build path in any binary; `nm`: none of their symbols defined in the executable (16 imported);
  `codesign --verify --deep --strict` passes.
- **Replacement** (on a copy, the RC untouched and re-verified): `libavcodec.61.dylib` rebuilt from
  the source archive (`build_ffmpeg_xma.sh … --optflags=-O2`) and `librecarbon_lzx.dylib` built with
  the archive README's command, put in Frameworks, signed as the notices say (library, then app);
  `codesign --verify --deep --strict` passes and `DYLD_PRINT_LIBRARIES` shows dyld loading the
  replacements' UUIDs (9D23E87E…, 787FB92A…; the RC's are B52D6EC9…, 16C88513…) from the copy's
  Frameworks.
- **Smoke test** (one launch of a copy, isolated home from `game/m7/template-home`, 175 s, SIGTERM):
  the three LGPL dylibs loaded from the app's Frameworks, nothing missing; the xex loaded (LZX) and
  the game booted through the movies to the main menu (profile loaded at 65 s); audio with levels up
  to 0.88 peak and no gaps; no error, strict stub, hang report or crash; menus at 60.0 fps; clean exit
  (status 0). The Quick Race route of `race_m5.args` didn't reach a race in that time (windowed).
  `SDL_AUDIO_DRIVER=dummy` was not honored: the audio went to the system output device.
- Not done (by scope): races, install matrices, M1, benchmarks; no human install test of e8eb933.

### What differs between 282557c and 94750af

- **Source** (`git diff --stat 282557c 94750af`): 9 files, all packaging, tests or docs —
  `scripts/macos/{make_app.py, check_app.py, rc_manifest.py, make_icon.swift, AppIcon.png}`,
  `tests/{CMakeLists.txt, app_icon_test.py, release_candidate_test.py}` and
  `reference/M8_ALPHA_CHECKLIST.md`. Nothing under `runtime/`, `recomp/`, `config/`, `patches/`,
  `survey/` or `CHANGELOG.md` changed.
- **App:** the extracted apps differ in `Contents/Resources/AppIcon.icns` (added),
  `Contents/Info.plist` (`CFBundleIconFile` added, `CFBundleVersion` 193 → 196, `ReCarbonCommit`),
  `_CodeSignature/CodeResources` and `Contents/MacOS/ReCarbon`. Every bundled library is
  byte-identical. The executable's `__TEXT,__text` section is identical in both
  (`46ebbbef…2b90728`, 42,410,268 B, `check_app` section 6): its file hash differs through the
  embedded build identity (commit, build time) and the ad-hoc signature.
- **dmg:** 94750af's volume also carries `.VolumeIcon.icns` and the custom-icon flag. After a
  download the `.dmg` file shows macOS's generic disk-image icon (user decision: no zip wrapper,
  no `make_app.py` change); the app icon and the volume icon are inside the image.

**Recommendation (the user decides):** 94750af as the alpha: the latest commit, with the icons,
passing every automated check, and its compiled code identical to 282557c's. Only 282557c was
install-tested by a person.
(Superseded: v0.1.0 was built as the final RC at 598a0ea, above.)

## Signing

Both RCs are signed ad hoc (`Signature=adhoc`, no Team ID) and not notarized; `spctl --assess`
rejects them. The copies used so far (built on the M5, or sent with `scp`) carried no quarantine
attribute, so Gatekeeper never prompted. A browser download would carry it (not tested): then
macOS blocks the first launch until the user allows it in System Settings > Privacy & Security
(Open Anyway). Gate G3.

## M6.6

Accepted for the alpha on the M8.4 sessions by the user's decision (2026-10-26), and M6 closed:
`reference/M6_ACCEPTANCE.md`.

## Limitations carried into the alpha

See `reference/M8_ALPHA_CHECKLIST.md` ("Known limitations") and `docs/known-issues.md`. New in
M8: the first race after a fresh install can hitch (up to ~450 ms) while the shader cache warms;
Game Mode unverified in the fresh account; the M1 not install-validated for any RC; M8.3 deferred.
M8.11's custom-profile restart and switching checks are automated only (above).
