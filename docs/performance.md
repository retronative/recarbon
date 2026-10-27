---
title: Performance
description: Measured ReCarbon frame rates, with dates and conditions. Only measured figures.
---

Only measured figures, with their dates and conditions. Nothing here is a promise for your Mac.

| Measurement | Result | Conditions | Date |
|---|---|---|---|
| Career race (Zone Heat 1, Circuit), hand test 4 | **58.5 fps** average over 118 two-second intervals (about 4 minutes); lowest interval 29.6 fps, just after the race's opening cutscene | MacBook Air M5, 720p internal resolution, fullscreen | 2026-10-23 |
| Scripted Quick Race | 57.6 fps average, no interval under 30 fps (lowest 42.8) | MacBook Air M5, with frame dumps on (they add a stall every 60 frames) | 2026-10-23 |
| Same, without frame dumps (soak run) | 59.5 fps average | MacBook Air M5 | 2026-10-23 |
| Free-roam, hand test 3 | 15 minutes without a crash, hang or watchdog report | MacBook Air M5 | 2026-10-23 |

## Racing by hand, 2026-10-25

The packaged app (one build, `4d521a7`), fullscreen, Game Mode on, audio on, the WASD keys, on
Quick Race > Circuit > Main Street with the stock Mazda RX-8 and the automatic gearbox, driven by
a person. Figures come from the frame-time log (every present) over the racing window, with the
thermal state that macOS reported.

| Measurement | Result | Conditions | Date |
|---|---|---|---|
| Full race, 8 laps (the M7 frame-pacing race) | **50.8 fps** mean over **9.77 minutes** of racing; median frame 16.7 ms, 99th percentile 35.1 ms; no 1-second window under 30 fps except during a 3 s macOS Space switch the tester caused by mistake (frames up to 749 ms then); 47–56 fps from the first lap to the last | MacBook Air M5, AC power, thermal state "fair" throughout | 2026-10-25 |
| Same route, 8 laps (the physics race) | 51.5 fps over 9.0 minutes of racing; 99th percentile 35 ms, longest frame 64 ms; no second under 30 fps | MacBook Air M5, AC power, thermal pressure level 1 at launch and 2 at the end | 2026-10-25 |
| Race 1, 2 laps | 32.8 fps mean over 3.4 minutes; 11 of 203 seconds under 30 fps; longest frame 162 ms | MacBook Air M1, started cool | 2026-10-25 |
| Race 2, 7 laps | 23.0 fps mean over 8.8 minutes: about 30 fps at first, about 21.6 fps from 5 minutes in | MacBook Air M1, after back-to-back sessions, thermal "fair" | 2026-10-25 |
| Race 3, 7 laps | 21.1 fps mean over 14.5 minutes: about 30 fps at first, falling to about 16 fps | MacBook Air M1, after a 25-minute cool-down, thermal "fair" | 2026-10-25 |

Limits of these figures:

- One Mac of each model, one track, one car, one build. Two races on the M5 and three on the M1.
- The M7 criterion names 10 minutes of racing; 8 laps (the game's maximum) took 9.77 minutes, and
  that length was accepted for the alpha.
- On the **M1** the frame rate falls in long races (16–22 fps by the end); the GPU's time per frame
  rises with time under load while draws stay the same. GPU throttling on the fanless M1 is a
  hypothesis, not a confirmed cause. A sustained 30 fps isn't claimed on the M1: see
  [Known issues](known-issues.md).

## Under CPU load from other apps

ReCarbon raises the game threads that drive each frame (the ones that present a frame or read
the controller) to macOS's "user-initiated" priority. It never lowers a thread that already
runs higher. Measured before and after, with the same scripted Quick Race:

| Measurement | Before | After | Conditions | Date |
|---|---|---|---|---|
| Stalls of 100 ms or more per run | 5.3 (3, 9, 4) | **0.3** (0, 0, 1) | MacBook Air M1 (8 GB), 720p internal resolution, six background busy loops, 3 runs each, 185 s of racing per run | 2026-10-25 |
| Longest frame | 137 ms | 99 ms | same | 2026-10-25 |
| Average frame rate | 40.6 fps | 40.6 fps | same (the load, not the priority, sets the frame rate) | 2026-10-25 |
| Game CPU use | 1.75 cores | 1.94 cores | same: the frame threads now run instead of waiting | 2026-10-25 |
| Without background load | no difference (0 stalls of 100 ms or more in 9 runs) | | MacBook Air M1 | 2026-10-25 |
| Packaged app, Game Mode on, no background load | 39.8 fps in the race, 0 stalls of 100 ms or more, no audio gaps | | MacBook Air M1, fullscreen, scripted Quick Race | 2026-10-25 |

Measured on one Mac with a synthetic load. It doesn't fix the stalls below, and it hasn't been
measured on other Macs yet.

## Known stalls

Short stalls still happen: see [Known issues](known-issues.md). They are recorded honestly,
not cut out of measurements.

## Internal resolution on a MacBook Air M5, 2026-10-27

Measured in hand-played sessions of v0.1.1, while driving:

| Internal Resolution | 3D scene | Frame rate |
|---|---|---|
| 2x (1440p), what **Auto** picks on this Mac | 2560×1440 | about **60 fps** |
| 3x (2160p, 4K) | 3840×2160 | about **30 fps** |

- **4K means the 3D scene is drawn at 3840×2160.** The game's own textures and its movies stay at
  their original resolution.
- 4K rendering was checked on an external 4K monitor and on the MacBook's own display. On a
  display with fewer pixels than 3840×2160, the picture is scaled down, which smooths edges.
- At 3x the GPU needs about 30–35 ms a frame, so the game runs near 30 fps. **Auto (2x) is the
  recommended setting on the MacBook Air M5.** Other Macs weren't measured.

## Higher internal resolutions in v0.1.2, 2026-10-27

v0.1.2 does less GPU work when the 3D scene is drawn above 1x: a resolve drawn at a higher
resolution also goes straight into the texture that reads it, and only what something reads is
kept at that resolution. Each change was turned on and off every 10–25 seconds in the same session,
and each "on" stretch was compared with the "off" stretches next to it. MacBook Air M5,
Internal Resolution 3x (2160p):

| Change | GPU time a frame | Frame rate | Conditions |
|---|---|---|---|
| Resolves stored into the textures that read them | **1.7 ms less** (median, 9 of 9 comparisons) | +1.6 fps | Quick Races driven by hand |
| Same, still scene | 3.3 ms less (9 of 10) | | the same picture, byte for byte |
| Then, on top of it: only what's read kept at 3x | **1.6 ms less** (median, 18 of 19) | +1.9 fps | about 10 minutes of Quick Races driven by hand |
| Same, still scene | 1.0 ms less (9 of 11) | | the same picture, byte for byte |

- **3x still runs at about 30–35 fps** on this Mac, not 60. Auto (2x) stays the recommended setting.
- At 2x the first change made no difference to GPU time or frame pacing; at 1x neither applies.
- The two were measured one after the other, so the totals aren't simply added up.
- No corruption, cache mismatches or GPU errors in these sessions.
- **Measured on one MacBook Air M5 only.** Not yet tested on a MacBook Air M1 or any other GPU.

## Settings that affect performance

All in Settings (F1); see [Settings](settings.mdx):

- **Internal Resolution:** the 3D scene's resolution. 1x is the console's 720p; 3x is 2160p (4K).
- **Frame Rate:** 60, 30, or Auto (60, or 30 while the device is hot or can't hold 60).
- **Asynchronous Shaders:** skips an object for a frame or two while its shader compiles, instead
  of stuttering.
- **Performance Overlay:** shows the frame rate and frame time in a corner.
