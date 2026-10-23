#!/usr/bin/env python3
# ReCarbon: frame times over a window of a run, from RECARBON_FRAME_LOG's CSV
# (one line per present: `presented` is the host's steady clock just before
# vkQueuePresentKHR) and, for the window and the thermal state, the session log.
#
#   scripts/frame_times.py <frames.csv> [--log <session.log>]
#       [--from S --to S]                 seconds of the session log
#       [--wall-from HH:MM:SS --wall-to HH:MM:SS]
#                                         wall clock, from the session log's name
#
# The session log stamps seconds since launch (0.1 s); its first "[vblank]
# presents are timed" line is taken as the CSV's first present, so a window is
# placed to about 0.3 s. Without --log the window is seconds from the first
# present. A last line cut short by the exit is skipped.
import argparse, csv, datetime, os, re, sys

ap = argparse.ArgumentParser()
ap.add_argument('csv')
ap.add_argument('--log')
ap.add_argument('--from', dest='start', type=float)
ap.add_argument('--to', dest='end', type=float)
ap.add_argument('--wall-from')
ap.add_argument('--wall-to')
a = ap.parse_args()

pres = []
with open(a.csv) as f:
    for row in csv.DictReader(f):
        if row.get('multiple') and row['presented'].isdigit():
            pres.append(int(row['presented']))
if len(pres) < 3:
    sys.exit(f'{a.csv}: fewer than 3 presents')

ts = re.compile(r'^\[\s*([0-9.]+)\]')
first_present = 0.0  # session seconds of the CSV's first line
perf = []            # (session s, fps, worst ms, thermal)
if a.log:
    for line in open(a.log, errors='replace'):
        m = ts.match(line)
        if not m:
            continue
        t = float(m.group(1))
        if not first_present and '[vblank] presents are timed' in line:
            first_present = t
        elif '[perf]' in line:
            fps = re.search(r'\] \[perf\] ([0-9.]+) fps \(worst frame ([0-9.]+) ms\)', line)
            th = re.search(r'\| thermal ([a-z]+)', line)
            if fps:
                perf.append((t, float(fps.group(1)), float(fps.group(2)), th.group(1) if th else '-'))

start, end = a.start, a.end
if a.wall_from or a.wall_to:
    if not a.log:
        sys.exit('--wall-from/--wall-to need --log')
    m = re.search(r'(\d{4}-\d\d-\d\d)_(\d\d)-(\d\d)-(\d\d)', os.path.basename(a.log))
    if not m:
        sys.exit(f'{a.log}: no session time in its name')
    began = datetime.datetime.strptime(f'{m.group(1)} {m.group(2)}:{m.group(3)}:{m.group(4)}', '%Y-%m-%d %H:%M:%S')

    def wall(hms):
        t = datetime.datetime.strptime(f'{m.group(1)} {hms}', '%Y-%m-%d %H:%M:%S')
        if t < began:
            t += datetime.timedelta(days=1)  # past midnight
        return (t - began).total_seconds()

    start = wall(a.wall_from) if a.wall_from else start
    end = wall(a.wall_to) if a.wall_to else end

t0 = pres[0]
sec = [first_present + (p - t0) / 1e9 for p in pres]  # session seconds of each present
lo = start if start is not None else sec[0]
hi = end if end is not None else sec[-1]
idx = [i for i in range(1, len(pres)) if lo <= sec[i - 1] and sec[i] <= hi]
if not idx:
    sys.exit(f'no frames in {lo:.1f}-{hi:.1f} s (the CSV covers {sec[0]:.1f}-{sec[-1]:.1f} s)')
ms = sorted((pres[i] - pres[i - 1]) / 1e6 for i in idx)
span = sec[idx[-1]] - sec[idx[0] - 1]


def pct(p):
    return ms[min(len(ms) - 1, int(p / 100 * len(ms)))]


print(f'window: {lo:.1f}-{hi:.1f} s of the session ({span / 60:.2f} min), {len(ms)} frames')
print(f'fps: mean {len(ms) / span:.1f}')
print(f'frame time ms: median {pct(50):.1f}, p95 {pct(95):.1f}, p99 {pct(99):.1f}, max {ms[-1]:.1f}')
# Over 41.7 ms (two and a half 60 Hz refreshes): a frame that missed 30 fps.
over = [41.7, 50, 100, 250, 900]
print('frames over: ' + ', '.join(f'{o:g} ms {sum(x > o for x in ms)}' for o in over))
print(f'time in frames over 41.7 ms: {sum(x for x in ms if x > 41.7) / 1e3:.1f} s')
# Whole seconds of the window, and those that showed fewer than 30 frames.
per_s = {}
for i in idx:
    per_s.setdefault(int(sec[i] - lo), 0)
    per_s[int(sec[i] - lo)] += 1
full = [n for s, n in per_s.items() if s < int(span)]
if full:
    print(f'1 s windows: {len(full)}, under 30 fps {sum(n < 30 for n in full)}, lowest {min(full)} fps')
inside = [p for p in perf if lo <= p[0] <= hi]
if inside:
    th = {}
    for p in inside:
        th[p[3]] = th.get(p[3], 0) + 1
    print(f'[perf] lines in the window: {len(inside)}, fps min {min(p[1] for p in inside):.1f}, '
          f'worst frame max {max(p[2] for p in inside):.1f} ms; thermal ' + ', '.join(f'{k} {v}' for k, v in sorted(th.items())))
