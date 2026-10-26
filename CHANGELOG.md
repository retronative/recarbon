# Changelog

What changed in each ReCarbon version, newest first. The game shows the current version's entry
once, the first time that version starts (it's built into the game: runtime/CMakeLists.txt).

## Unreleased

### What's new

- When the installer turns a disc image or folder down, it says why: another platform's disc (PC,
  PlayStation, Wii, original Xbox), another game, or another region or version of *Need for Speed:
  Carbon*. ReCarbon needs your own *Need for Speed: Carbon* (USA) disc for Xbox 360.
- Settings > Controls works under a keyboard layout in another script, such as Myanmar or Thai:
  it names keys by their position ("W") instead of showing "?", and you can rebind them. A
  binding is the key's position, as on any layout, so your existing bindings stay as they are.
- The download opens on a ReCarbon window: the app on the left, Applications on the right and an
  arrow between them, so you drag one onto the other to install it.

## v0.1.0

### What's new

- The first alpha release: playable, not finished.
- Need for Speed: Carbon's `default.xex`, from your own disc, is recompiled to native arm64 code
  with XenonRecomp and SpeedBreaker's pipeline, on SpeedBreaker's runtime.
- It boots to the menus, plays the movies with sound, drives in free-roam and finishes career races
  against the AI, and keeps your career progress.
- Fewer long stalls when other apps keep the Mac busy: the game threads that drive each frame now
  run at a higher macOS priority (measured on a MacBook Air M1: stalls of 100 ms or more went from
  5.3 to 0.3 per run under load).
- Full Quick Races play by hand on the keyboard. On the WASD keys, Space is the handbrake, Left
  Shift is Speedbreaker and Backspace fires the nitrous (on a car that has it), as the Controls
  page now documents.
- The first-run installer copies and verifies the game's 78 files (5.74 GB) from your own disc
  image, also on a brand-new macOS account, and the game starts.
- ReCarbon has its own icon, on the app and on the opened disk image.
- FFmpeg's audio decoders and libmspack's LZX decoder are separate libraries in the app
  (`Contents/Frameworks`) that you can replace with your own builds; the third-party notices say how,
  and the release page has their source.
- On a MacBook Air M5 a 9.8-minute race ran at about 51 fps on average. On a MacBook Air M1 races
  start at about 30 fps and slow down the longer you play.
- Known issues: occasional stalls of about a second while racing, and shorter ones; the slowdown
  in long races on the M1; memory use that grows during long sessions; the "Cannot connect to Xbox
  Live!" notice needs one press of A (Space) at every launch; the first race after a fresh
  install can hitch while the shader cache fills. A physical controller hasn't been tested yet.
  It needs an Apple silicon Mac with macOS 27 or later.
