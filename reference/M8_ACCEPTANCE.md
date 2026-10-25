# M8 acceptance record (alpha packaging)

The state of each M8 criterion (ROADMAP M8) and its evidence, last updated 2026-10-26. Objective
checks and the tester's reports are kept apart. Testing ended on 2026-10-26 by the user's
decision: what wasn't verified by then is listed as a limitation, not as passed.

**State:** the alpha packaging is validated on the MacBook Air M5 (two release candidates pass
every automated check; one was installed and played by hand in the main account and in a fresh
macOS account). **Nothing is published yet.** Publication follows the gates in
`reference/ALPHA_RELEASE_GATES.md` (G1–G4). M8 stays in progress until the user closes it.

## Criteria

| Criterion | Status | Evidence |
|---|---|---|
| M8.1 Installer parametrized for Carbon (manifest, title ID, ReCarbon names) | **Completed** | Carbon's manifest (title 0x454107EC, 78 files). RC 282557c on the M5: the first-run installer copied and verified 78 files (5.74 GB) from the user's ISO into `~/Library/Application Support/ReCarbon/game`, in the main account (86.2 s, session 2026-10-25_23-13-21) and in the fresh account `gamtest` (48.7 s, session 2026-10-25_23-47-12); the game started each time. Earlier: `ReCarbon --install` 78 files, and the game found in the default folder (2026-10-23). |
| M8.2 `ReCarbon.app` with license notices and LGPL sources | **In progress** | The notices, license texts and each library's provenance are in `Contents/Resources/licenses/` (make_app.py; checked by the RC's gate). Bundle ID `org.retronative.recarbon`, version 0.0.1. **Not done:** the LGPL source archive and offer (gate G2). |
| M8.3 Optional: render scaling and widescreen hooks | **Deferred** (optional) | Not started. Not an alpha gate. |
| M8.4 Clean install from a disc image on a fresh macOS account | **Completed on the M5** | Account `gamtest` (UID 502, created by the user), RC 282557c's app, no quarantine attribute. Session 1: ISO install, new profile save `ALIAS_ABO`, a race (3D mean 56.4 fps), Quit Game. Session 2: no "ended without quitting" notice, save `ALIAS_ABO` found and opened, a race (mean 54.5 fps, worst frame 100 ms), Quit Game. No hang report, strict stub, device loss or crash. **Not captured:** Game Mode in that account. **Seen:** the first race after the install hitched up to 450 ms while the empty shader cache compiled pipelines. Not done on the M1. |
| M8.5 `check_app.py` passes | Completed | Both RCs: `check-app`, `check-dmg` and `check-extracted` PASS (and on the release app cd7dc02 / d780e34 before). |
| M8.6 The unit tests run in ctest | Completed | The RCs' `ctest` step: 52/52 at 94750af. |
| M8.7 Keyboard presets WASD and Arrows | Completed | `profiles_test` (ROADMAP M8, B2). |
| M8.8 Persistent keyboard and controller profiles | Completed | Scripted restarts in isolated homes; `profile_store_test`. |
| M8.9 Custom mappings, controller rebinding, switching, reset | Completed | Scripted runs and the virtual pad. The physical controller is not verified (deferred). |
| M8.10 `[keys]` migrated, never dropped | Completed | `profile_store_test`; scripted first launch. |
| M8.11 Presets drive the game; custom profiles survive a restart; switching without a restart | **In progress** (automated evidence only) | Scripted: custom profiles survive a restart, switching takes effect at once; each preset's D-pad keys resolve to D-pad buttons (`profiles_test`). The WASD preset drove every hand-tested race. **Unverified by hand:** the Arrows preset driving the game, and a custom profile surviving a restart. |
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

### What differs between the two RCs

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

## Signing

Both RCs are signed ad hoc (`Signature=adhoc`, no Team ID) and not notarized; `spctl --assess`
rejects them. The copies used so far (built on the M5, or sent with `scp`) carried no quarantine
attribute, so Gatekeeper never prompted. A browser download would carry it (not tested): then
macOS blocks the first launch until the user allows it in System Settings > Privacy & Security
(Open Anyway). Gate G3.

## M6.6 (for the user's decision; M6 is not closed here)

M6.6 asks for the written new-profile route (new profile → free-roam/race load → restart → save
reloaded, logs and save recorded, watchdog reports empty). The M8.4 sessions in `gamtest` give:
a new profile save created (`ALIAS_ABO`, session 1, 102.0 s), a race loaded and driven, Quit Game,
then a restart that found and opened the same save, with no watchdog hang report in either
session. Free-roam wasn't part of that route (M6.5 covered free-roam). Whether this closes M6.6,
and so M6, is the user's decision.

## Limitations carried into the alpha

See `reference/M8_ALPHA_CHECKLIST.md` ("Known limitations") and `docs/known-issues.md`. New in
M8: the first race after a fresh install can hitch (up to ~450 ms) while the shader cache warms;
Game Mode unverified in the fresh account; the M1 not install-validated for either RC; M8.11's
hand checks; M8.3 deferred.
