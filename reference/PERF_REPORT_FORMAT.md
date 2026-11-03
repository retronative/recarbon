# Performance report format (`recarbon-perf-report/1`)

What **Settings > Advanced > Export Performance Report** writes, for maintainers comparing
reports from different Macs. The player's side is in `docs/troubleshooting.md`
("Export a performance report"). The code is `runtime/report/perf_report.h` (format),
`runtime/report/perf_scrub.h` (privacy) and `runtime/report/perf_report.cpp` (collection);
`tests/perf_report_test.cpp` checks them, and `scripts/perf_report_check.py <file>...`
checks a real report.

## The file

`recarbon-perf-report-v<version>-<YYYY-MM-DD>_<HH-MM-SS>.md`, on the Desktop (else the
game's data folder, `~/Library/Application Support/ReCarbon`, as the bug report);
`RECARBON_PERF_REPORT_DIR=<dir>` puts it there instead (tests and scripts). It is written only
when the player asks: nothing is collected in the background beyond the counters the game
already keeps, and nothing is ever sent anywhere.

1. A plain-text summary of about a dozen lines, for pasting into an issue as is.
2. A line `--- machine-readable (recarbon-perf-report/1) ---`.
3. A fenced block ```` ```json ```` with the document. Parsers take the first such fence; the
   JSON is strict (no comments, no trailing commas, no NaN), so `json.loads` reads it.

The schema's number (`/1`) changes when a field is removed or changes meaning. New fields may
be added under the same number; a reader ignores keys it doesn't know.

## Metrics

Every metric is an object:

```json
{ "value": 16.67, "status": "measured", "source": "present-ring", "unit": "ms" }
```

- `status`: `measured` (read from a counter or a clock that measures it), `estimated`
  (derived, or from fewer samples than the name implies), `unavailable` (this Mac, build or
  session has none: `value` is `null` and `why` says what is missing). Never a silent 0.
- `source`: where it came from (below). Optional: `unit`, `note`, `window`, and fields of
  the metric's own (`threshold_ms`, `rule`, `name`).

The top-level keys: `schema`, `generated` (UTC, ISO 8601), `trigger` (`settings-row` or
`ui-key`), `session_s` (seconds since launch), `build` (`version`, `commit`, `built`), and
the sections `system`, `display`, `resolution`, `frames`, `display_events`, `cp`, `gpu`,
`hitches`, `thermal`, `cpu`, `warnings`, `flags`.

| Section | Metrics | Source |
|---|---|---|
| `system` | `machine` (`hw.model`), `chip`, `cores` (performance, efficiency), `memory_gb`, `os`, `gpu` (device, driver, Vulkan) | sysctl; the Vulkan device |
| `display` | `mode` (w, h, Hz), `window_px` (w, h, fullscreen), `swapchain`, `vsync` (applied, with the setting), `present_timing`, `frame_rate` (setting, now, Auto's note) | the presenter, the settings |
| `resolution` | `setting` (Internal Resolution), `applied` (scale, width, height at 1280x720 guest, `fixed_by_env` for `RECARBON_RENDER_SCALE`, `dynamic`), `capped`, `anti_aliasing` | the renderer at its last `[perf]` tick |
| `frames` | `frames_presented`, `intervals`, `duration_s`, `fps_mean`, `interval_ms` (p50, p95, p99, max), `over_25ms`, `over_33ms` (> 33.4), `over_50ms` | the present ring |
| `display_events` | `dropped`, `shown`, `repeated_refreshes` (needs `VK_KHR_present_wait`), since launch | the presenter's counters |
| `cp` | `guest_fps`, `worst_frame_ms`, `busy_ms_per_frame`, `draws_per_frame` (last 120 swaps); `busy_ms_total`, `wait_ms_total` by kind (regmem, throttle, shadow, flush, slot, pipeline, suspended) since launch | the command processor's `[perf]` tick |
| `gpu` | `busy_ms_per_frame`, `executing_ms_per_frame` (GPU timestamps), `post_processing_ms` (`RECARBON_GPU_TIMING=1`) | the `[perf]` tick, the presenter |
| `hitches` | `swaps`, `late_frames` (the `[hitch]` rule: swap to swap >= 50 ms, or > the median of the last 63 + 3 ms), `d3d_wait`, `world_step_cadence`, `tiled_draw_drops` | the renderer's counters |
| `thermal` | `state_now` (NSProcessInfo, with `simulated` for `RECARBON_THERMAL_SIM`), `history` (each change, at most 32), `pressure_level` (0 nominal, 1 moderate, 2 heavy, 3 trapping, 4 sleeping) | the thermal poll; `notify_get_state` on `com.apple.system.thermalpressurelevel` |
| `cpu` | `process_ms` (user, system), `threads` (each runtime thread's name and CPU ms) | `getrusage`; `thread_info` once at export |
| `warnings` | log lines that warn or fail, counted by their `[tag]` (`estimated`: the log's in-memory tail only, and a word match) | the session log |
| `flags` | `env` (every `RECARBON_*` variable set), `settings_not_default`, `overridden_by_env` | the environment, the settings |

`flags.env` gives the value of the Settings options' own variables and of a fixed list of
experimental and tuning switches (`kValued` in `perf_report.h`), when the value is a plain
number or word (no `/`, no spaces, at most 40 characters). Every other variable that is set
reads `"(set)"`, and those that carry paths, URLs or scripts (`RECARBON_FRAME_LOG`,
`RECARBON_GAME_DIR`, `RECARBON_UI_KEYS`...) always do.

## Frame times

The present ring holds the host present times (steady clock) of the last 16,383 new game
frames, a redraw of the same frame (menus over a paused game) not counted: about 4.5 minutes
at 60 fps, 9 at 30. With presents timed or not, so every Mac has it.

- interval = the difference of consecutive present times, in ms; no clamp (a 749 ms frame
  during a Space switch stays the worst);
- quantile `p` = `sorted[min(n-1, int(p/100*(n-1)+0.5))]`, the nearest rank, rounded (the
  `quantile` field repeats it; `scripts/frame_times.py`'s `int(p/100*n)` can be one rank higher);
- `over_25ms` counts `> 25`, `over_33ms` `> 33.4`, `over_50ms` `> 50`;
- `fps_mean` = intervals / their sum, host clock (the `[perf]` line's fps, `cp.guest_fps`,
  is the game clock over 120 swaps);
- status: `measured` from 600 intervals (10 s at 60), `estimated` from 60 (with a `note`),
  `unavailable` below.

Cost: one relaxed store and one release store per new frame (the ring's `Push`;
`perf_report_test` prints its time), 128 KB. Everything else is read at export, on a thread of
its own: a copy of the ring, a sort, a few sysctls and one file write.

## Privacy

The report is built from typed fields, never from the log or the system summary as text: no
paths, user or host names, save or profile names, settings file, game files, screenshots or
guest addresses go in by construction. The whole text then passes two checks
(`perf_scrub.h`) before anything is written:

1. **Scrub:** the home folder (and its real path) as `~`; a path under a home or user folder
   (`~/`, `/Users/`, `/home/`, `/Volumes/`, `/private/`, `/var/folders/`) as `<path>`; a guest
   address (`0x8` and 7 hex digits, `sub_` and 8) as `<guest-address>`; the login and host
   names, and the account's full name and its words of 3 letters or more, as whole words in
   any case, as `<user>`.
2. **Denylist (fails closed):** any path left (a `/` starting a word and followed by a name and
   another `/`), `~/`, a guest address, the home folder, any of those names, or `.sav`,
   `settings.toml`, `profiles/`, `.xex`, `.iso`. A hit and **nothing is written**: the row
   says "Failed", and the toast and the log say what was found (its kind, not the text).

   Known limit: a word of the account's full name that is also a hardware word ("Max", "Pro",
   "Air" against the chip "Apple M1 Pro") refuses every report on that Mac with "a user or host
   name". That is the check failing closed; such a tester can still send the bug report.

## Comparing reports

`scripts/perf_report_check.py <file>...` validates each file (the fence, the schema, every
metric's status, the denylist) and prints one line per report: machine, chip, macOS, display,
resolution, fps and p50/p95/p99, so reports from several Macs line up.

## History

- `/1` (v0.1.4): first version.
