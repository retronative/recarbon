# Changelog

What changed in each ReCarbon version, newest first. The game shows the current version's entry
once, the first time that version starts (it's built into the game: runtime/CMakeLists.txt).

## v0.1.4

### What's new

- Expanded View, on by default: on a screen that isn't 16:9, such as a MacBook's 16:10, the 3D
  view fills the screen and shows more above and below, while the HUD, mirror and movies keep
  their 16:9 shape. It draws a little more each frame. Settings > Display > Expanded View: Off
  brings back the original 16:9 view with bars.
- Settings in plain words: Internal Resolution is named HD (720p), Full HD (1080p), QHD (1440p)
  and 4K UHD (2160p), and its row says what is drawn when that differs; Upscaling is Sharp, Soft
  or FSR 1; simpler help lines. Saved settings read as before.
- 4K UHD renders at 4K only on a display with at least 3840x2160 pixels; below that it renders
  at QHD and keeps your choice.
- Export Performance Report (Settings > Advanced): a file you can attach to a bug report, made
  only when you ask, with personal paths removed.
- Steadier rendering: the game's render queue handshake and its publishes are fenced, so the
  main thread never reads a half-written entry.
- Display changes are handled in one step (moving the window to another display, connecting or
  disconnecting a monitor, changing resolution or scaling); a saved window size opens within
  the display it opens on; a fullscreen request macOS refuses leaves the window as it was, with
  a message.
- Stepping through Internal Resolution in the open menu rebuilds once, after you stop.
- Controllers: the default mapping was hand-tested on a physical controller (8BitDo Ultimate 2
  Wireless): every current button and mapping is correct.
- Known issues: as in v0.1.3. With Expanded View on, the very first boot movie may show
  stretched, and a fade to a loading screen may leave a strip above or below the 16:9 picture
  for a moment. Measured on a MacBook Air M5. It needs an Apple silicon Mac with macOS 27 or
  later. Experimental switches in this build stay off.

## v0.1.3

### What's new

- Smoother races and a cooler Mac. The game's own busy-waits for the GPU (every 3D scene) no
  longer spin a CPU core: they sleep between polls. On a MacBook Air M5 at 2x this cut the main
  thread's CPU use in a race by more than half; in a 9-minute play-test the races ran 52-60 fps
  where the same Mac ran v0.1.2 at 31-37 fps earlier the same day. At 3x the main thread fell from
  a full core to an eighth and thermal throttling came about two and a half minutes later.
  `RECARBON_GPU_WAIT_BACKOFF=0` turns it off.
- Fewer stalls while racing: vertex data up to 128 KB is copied with its draw instead of guarded
  against the game rewriting it, which removed a stall of 2.7 ms (2x) to 17 ms (3x) about once a
  frame on the M5 (v0.1.2 measured; 3x +2.8 fps). `RECARBON_VERTEX_COPY_128=0` restores 64 KB.
- Settings > Display > Frame Rate explains each choice under the row: what Auto does, what 60
  costs when the Mac can't hold it, and what 30 gives.
- Known issues: a fanless Mac still throttles within a few minutes at 2x and above; once it does,
  races at 2x run at about 52-56 fps with visible judder (frames shown twice) until it cools.
  Otherwise as in v0.1.2: occasional stalls of about a second while racing, and shorter
  ones; the slowdown in long races on the M1; memory use that grows during long sessions; the
  first race after a fresh install can hitch while the shader cache fills. Measured on a MacBook
  Air M5 only, not yet on an M1 or other Macs. It needs an Apple silicon Mac with macOS 27 or
  later. Experimental switches in this build stay off: the AA downscale gate, the select
  multiply, dynamic internal resolution, the pre-race Auto frame rate.

## v0.1.2

### What's new

- Less GPU work at higher internal resolutions, with the same picture. A resolve the game renders
  at a higher resolution now also goes straight into the texture that reads it, and only what
  something reads is kept at that resolution. On a MacBook Air M5 at 3x (2160p), measured while
  driving one change after the other: about 1.7 ms and then a further 1.6 ms less GPU time a
  frame, and 1.5-2 fps more with each. At 2x frame times didn't change; at 1x neither applies.
- Known issues: as in v0.1.1. Occasional stalls of about a second while racing, and shorter
  ones; the slowdown in long races on the M1; memory use that grows during long sessions; the
  first race after a fresh install can hitch while the shader cache fills. The GPU changes were
  measured on a MacBook Air M5 only: not yet on an M1 or other Macs. It needs an Apple silicon
  Mac with macOS 27 or later.

## v0.1.1

### What's new

- When the installer turns a disc image or folder down, it says why: another platform's disc (PC,
  PlayStation, Wii, original Xbox), another game, or another region or version of *Need for Speed:
  Carbon*. ReCarbon needs your own *Need for Speed: Carbon* (USA) disc for Xbox 360.
- Settings > Controls works under a keyboard layout in another script, such as Myanmar or Thai:
  it names keys by their position ("W") instead of showing "?", and you can rebind them. A
  binding is the key's position, as on any layout, so your existing bindings stay as they are.
- Smoother movies: each frame of the intro and other movies now shows for an even two refreshes
  (on a MacBook Air M5, uneven frames fell from 18.5% to 1.7%).
- The download opens on a ReCarbon window: the app on the left, Applications on the right and an
  arrow between them, so you drag one onto the other to install it.
- A new look in racing red and silver: ReCarbon's icon, the disk image's window and the website.
- The "Cannot connect to Xbox Live!" notice at the main menu is the game's own notice that it
  can't reach Xbox Live, not a ReCarbon error: press A (Space) once and play on. The FAQ explains it.
- Known issues: occasional stalls of about a second while racing, and shorter ones; the slowdown
  in long races on the M1; memory use that grows during long sessions; the first race after a
  fresh install can hitch while the shader cache fills. Rebinding under a Myanmar or Thai layout
  hasn't been tried in the game yet, and a physical controller hasn't been tested. It needs an
  Apple silicon Mac with macOS 27 or later.

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
