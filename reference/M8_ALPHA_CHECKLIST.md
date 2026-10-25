# M8 alpha packaging checklist (internal, 2026-10-25)

What stands between today's state and a playable alpha build on the user's own Macs, and what more a
public binary would need. Built from ROADMAP M8, `reference/RELEASE_CANDIDATE.md`,
`reference/M7_ACCEPTANCE.md` and the known issues. Nothing here publishes anything.

## 1. Gameplay gate (M7)
- [x] Every M7 criterion completed or accepted (`reference/M7_ACCEPTANCE.md`). Deferred, not
      alpha gates: the physical controller; Xbox 360 physics equivalence.
- [x] The user closes M7 (milestones are gated by the user's decision): closed for the alpha on
      2026-10-25 (ROADMAP M7). Closing M7 is not a release: nothing is published.

## 2. Release-candidate build (local only: the build compiles `ppc/` from the player's own disc)
- [x] `scripts/release_candidate.sh <sha>`: clean checkout, ppc/ from the local disc, recompiler
      validation, config unchanged, ctest, `make_app.py --dmg --min-macos=27.0`, `check_app.py`
      on the app, the dmg and the extracted copy, content gate, manifest and SHA256SUMS. First run
      PASS on 0d60585 (`build/rc/0d60585-20261025T151750Z`).
- [x] Commit the RC tooling (`scripts/release_candidate.sh`, `scripts/rc_transfer.sh`,
      `scripts/macos/rc_manifest.py`, `tests/release_candidate_test.py`, `reference/RELEASE_CANDIDATE.md`)
      after review: committed as 5b1b463.
- [ ] Cut the RC from the commit that will be the alpha (after the docs commits), not from an
      older one.

## 3. Install and acceptance on both Macs (by hand)
- [ ] Transfer the dmg to the M1 with `scripts/rc_transfer.sh` (checksum verified on arrival).
- [ ] On the M1 and on the M5, the checklist in `reference/RELEASE_CANDIDATE.md`: clean first
      launch (record what Gatekeeper does with the ad-hoc signature), game data installed from the
      user's own disc image, Game Mode on, WASD controls, audio, one race to the results, a save
      that survives a restart, a clean exit.
- [ ] M8.4: a clean install on a fresh macOS user account (the user creates the account).
- [ ] M8.11's remaining hand checks: the Arrows preset driving the game, custom profiles surviving
      a restart, switching without a restart (keyboard; the controller is deferred).
- [ ] M6.6: the written new-profile route (new profile, free-roam, race load, restart, save
      recorded), which keeps M6 open.

## 4. Release content and notes
- [ ] Version and build number (Info.plist from the build), CHANGELOG v0.0.1 entry complete.
- [x] Known issues current (`docs/known-issues.md`, 2026-10-25): the 1 s and GPU-idle stalls, the
      M1 frame-rate drop, texture cache growth, the Xbox Live notice needing one OK, non-Latin key
      labels, macOS 27+, movie pacing, the untested physical controller. Recheck against the RC.
- [ ] `docs/download.md` and `docs/installation.md` match what the build actually does.
- [ ] The dSYM kept privately per build (never published), for crash reports.

## 5. Only before a public release (not alpha-on-own-Macs blockers)
- [ ] LGPL source archive (FFmpeg 7.1.1 + patch, libmspack at XenonRecomp's revision, the other
      bundled libraries' sources) and the offer in the app and on the download page.
- [ ] How users can replace the LGPL-2.1 libraries with their own builds.
- [ ] Signing and notarization (today ad hoc; `spctl --assess` rejects the app).
- [ ] Approvals for push, tag, release page and download links (none exist today).

## Shortest path to a playable alpha on the user's Macs
1. ~~Review and commit the pending docs and the RC tooling (local).~~ Done: RC tooling 5b1b463,
   the M7 closure docs after it.
2. Cut an RC at the docs commit; transfer to the M1.
3. Run the acceptance checklist on both Macs, plus M8.4 and the remaining M8.11 and M6.6 checks.
4. The user closes M6 and M8 for the alpha (M7 is closed, 2026-10-25).
