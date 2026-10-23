# Changelog

What changed in each ReCarbon version, newest first. The game shows the current version's entry
once, the first time that version starts (it's built into the game: runtime/CMakeLists.txt).

## v0.0.1

### What's new

- A development snapshot, not a release: playable in parts, not finished.
- Need for Speed: Carbon's `default.xex`, from your own disc, is recompiled to native arm64 code
  with XenonRecomp and SpeedBreaker's pipeline, on SpeedBreaker's runtime.
- It boots to the menus, plays the movies with sound, drives in free-roam and finishes career races
  against the AI, and keeps your career progress.
- Known issues: occasional stalls of about a second while racing, and shorter ones. A physical
  controller hasn't been tested yet. It needs an Apple silicon Mac with macOS 27 or later.
