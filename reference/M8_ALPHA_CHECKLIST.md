# M8 alpha checklist (internal, updated 2026-10-26)

The alpha release checklist: what was validated, what is known and left open, and what still
stands between the validated release candidate and a publication. Testing ended on 2026-10-26 by
the user's decision ("testing is over; finish and release"): unverified items are limitations,
not gates. Nothing here publishes anything.

Records: `reference/M8_ACCEPTANCE.md` (criteria, both RCs, the evidence),
`reference/ALPHA_RELEASE_GATES.md` (G1–G4), `reference/ALPHA_RELEASE_NOTES.md` (draft notes),
`reference/RELEASE_CANDIDATE.md` (the RC tooling), `reference/M7_ACCEPTANCE.md`.

## 1. Completed validation

- [x] **M7 closed for the alpha** (user, 2026-10-25; 3cc36ad, eb6f321): M7.1–M7.7 under the alpha
      criteria; `reference/M7_ACCEPTANCE.md`.
- [x] **RC tooling** committed (5b1b463): clean checkout, `ppc/` from the local disc, validation,
      ctest, packaging, `check_app.py` on the app, the dmg and the extracted copy, content gate,
      manifest, checksums; `rc_transfer.sh` with checksum verification on arrival.
- [x] **RC 282557c**: 24/24 PASS (`build/rc/755b1c5-20261025T155507Z`; dmg `94086c84…dc51e`,
      binary `97491602…5b88`).
- [x] **RC 94750af** (app icon 0994d60, dmg volume icon 94750af): 24/24 PASS
      (`build/rc/b06e341-20261025T163359Z`; dmg `e4234d65…33e1dd`, binary `b9c1a740…f4ed`; the
      same compiled code as 282557c: `M8_ACCEPTANCE.md`).
- [x] **M5, main account, RC 282557c by hand:** clean reinstall (old data moved aside, kept),
      ISO install 78 files / 5.74 GB, Game Mode on, a race to the results, save `ALIAS_IHG` kept
      after a relaunch, Quit Game clean (`game/m7/accept-m5-755b1c5/`, WORKLOG).
- [x] **M8.4, fresh macOS account `gamtest` on the M5, RC 282557c:** ISO install, new profile
      save `ALIAS_ABO`, a race, the save found on relaunch, Quit Game clean.
- [x] **M8.1** installer for Carbon (the installs above), **M8.5** `check_app.py`, **M8.6** ctest
      52/52, **M8.7–M8.10, M8.12** input profiles.
- [x] Transfers to the M1 with verified checksums (both RCs); RC 282557c installed in its
      `/Applications`, identical to the dmg.
- [x] Version and build: 0.0.1 (193) for 282557c, 0.0.1 (196) for 94750af (`Info.plist`,
      `MANIFEST.json`); the CHANGELOG v0.0.1 entry as of each commit is compiled into it.
- [x] Release documentation brought in line with the build (2026-10-26): `docs/installation.md`,
      `docs/known-issues.md`, `docs/download.md` (still "no release yet"), the Progress page, the
      CHANGELOG, the draft release notes.
- [x] The 94750af RC folder carries its `.retention.json` (class `evidence`); its dSYM stays in
      `app/symbols/`, never published. The 282557c folder left `build/rc/` on 2026-10-26 outside
      this work (`M8_ACCEPTANCE.md`, retention note).

## 2. Known limitations (carried into the alpha)

Kept in `docs/known-issues.md` and the release notes:
- ~1 s presenter/queue-lock stalls; 250–600 ms GPU-idle stalls (both unresolved).
- The first race after a fresh install can hitch (up to ~450 ms) while the shader cache warms.
- MacBook Air M1: frame rate falls in long races (thermal throttling a hypothesis, unconfirmed);
  no sustained 30 fps claimed on the M1.
- Texture cache growth (no eviction; fix deferred until after the alpha).
- The Xbox Live notice needs one OK (A / Space) at every launch.
- Non-Latin keyboard layouts: key labels show "?".
- macOS 27 or newer only.
- Movie frame pacing.
- Physical controller untested; Xbox 360 physics equivalence unverified.
- Keys not tried by hand: Reverse, Look Back, Pause, the manual-gearbox gear keys; the K / ;
  observation unreconciled; the unexplained first Backspace press.
- Game Mode verified in the main account, not captured in the fresh account.
- **The M1 is not install-validated for either RC** (the M1 tests were cancelled, 2026-10-26).
- M8.11's hand checks (the Arrows preset driving in game; a custom profile surviving a restart by
  hand) unverified; the automated evidence is in ROADMAP M8.
- M8.3 (render scaling, widescreen hooks): optional, deferred.
- Only 282557c was install-tested by a person; 94750af differs in packaging only.
- A downloaded `.dmg` shows macOS's generic disk-image icon (user decision; the icons are inside).

## 3. Publication gates

From `reference/ALPHA_RELEASE_GATES.md`:
- [ ] **G1** binary: publish the dmg and its checksum.
- [ ] **G2** LGPL parts (FFmpeg 7.1.1 and libmspack's `lzxd.c`, both static): the source archive,
      the written offer in the app and on the download page, the relinking route.
- [ ] **G3** Developer ID + notarization, or ad hoc with the documented "Open Anyway".
- [ ] **G4** approvals: push, website redeploy, tag, release page, binary upload.
- [ ] Which RC is the alpha (recommended: 94750af; only 282557c was human install-tested).
- [ ] Whether the gamtest evidence closes M6.6 and so M6 (`M8_ACCEPTANCE.md`); and closing M8.

## Shortest path to publishing

1. The user picks the RC (94750af recommended) and decides M6.6/M6.
2. Decide G1 (the dmg).
3. Decide G3 (ad hoc needs nothing new; Developer ID needs a new RC and a launch
   test). Prepare G2's archive from `build/release-src/MANIFEST-draft.tsv`, and write the offer.
4. Fill the release notes' **[GATE …]** placeholders; switch `docs/download.md`, the home page's
   M8 card, the Progress page and the README status lines to the release, in one change.
5. With approval, in order: push, tag the RC's commit, redeploy the website, create the release
   page, upload the dmg, its `.sha256` and (per G2) the source archive.
