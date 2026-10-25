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
- Fewer long stalls when other apps keep the Mac busy: the game threads that drive each frame now
  run at a higher macOS priority (measured on a MacBook Air M1: stalls of 100 ms or more went from
  5.3 to 0.3 per run under load).
- Known issues: occasional stalls of about a second while racing, and shorter ones. A physical
  controller hasn't been tested yet. It needs an Apple silicon Mac with macOS 27 or later.
