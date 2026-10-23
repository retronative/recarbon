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

The ROADMAP's racing criterion (M7) asks for a stable 30 fps or better over **10 minutes** of
racing, from frame-time logs. That hasn't been measured yet.

## Known stalls

Short stalls still happen: see [Known issues](../known-issues/). They are recorded honestly,
not cut out of measurements.

## Settings that affect performance

All in Settings (F1); see [Settings](../settings/):

- **Internal Resolution:** the 3D scene's resolution. 1x is the console's 720p.
- **Frame Rate:** 60, 30, or Auto (60, or 30 while the device is hot or can't hold 60).
- **Asynchronous Shaders:** skips an object for a frame or two while its shader compiles, instead
  of stuttering.
- **Performance Overlay:** shows the frame rate and frame time in a corner.
