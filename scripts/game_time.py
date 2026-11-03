#!/usr/bin/env python3
# ReCarbon: the world's step cadence from RECARBON_GAME_TIME_LOG's CSV (one line
# per game frame, runtime/game/game_time_log.h has the columns), and with
# --frames RECARBON_FRAME_LOG's CSV, how many world steps each presented frame
# carried. GPL-3.0-or-later (see COPYING).
#
#   scripts/game_time.py <game_time.csv> [--frames <frames.csv>] [--from S --to S]
#
# --from/--to are seconds from the first game frame. Both CSVs stamp the
# host's steady clock in ns (host_ns, presented), so they merge as they are: a
# present's world steps are those of the game frames that started since the
# present before it. The game frame starts a frame or two before its present
# (the render thread and the GPU come between), so a present's count is its
# game frame's only while that latency holds steady, as it does at a held
# rate: then 1 step a present is smooth motion, 0 a repeated world state on a
# new refresh, 2+ a jump. A last line cut short at exit is skipped.
import argparse, csv, sys

ap = argparse.ArgumentParser()
ap.add_argument('csv')
ap.add_argument('--frames')
ap.add_argument('--from', dest='start', type=float)
ap.add_argument('--to', dest='end', type=float)
a = ap.parse_args()

COLUMNS = ['host_ns', 'measured_ms', 'ticks', 'frame_ms', 'steps', 'raw_steps', 'acc_ms']
rows = []
with open(a.csv) as f:
    reader = csv.DictReader(f)
    if reader.fieldnames != COLUMNS:
        sys.exit(f'{a.csv}: not a RECARBON_GAME_TIME_LOG file (columns {reader.fieldnames})')
    for r in reader:
        try:
            rows.append((int(r['host_ns']), float(r['measured_ms']), int(r['ticks']), int(r['steps'])))
        except (TypeError, ValueError):
            continue  # the exit cut the line short
if not rows:
    sys.exit(f'{a.csv}: no frames')

t0 = rows[0][0]
lo = t0 + int((a.start or 0) * 1e9)
hi = t0 + int(a.end * 1e9) if a.end is not None else None
rows = [r for r in rows if r[0] >= lo and (hi is None or r[0] < hi)]
if not rows:
    sys.exit('no frames in the window')


def bucket(steps):
    return min(steps, 3)


def histogram(counts, total):
    names = ['no step', '1', '2', '3+']
    return ', '.join(f'{names[k]} {counts[k]} ({100.0 * counts[k] / total:.2f}%)' for k in range(4)) if total else 'none'


world = [r for r in rows if r[3] >= 0]
span_s = (rows[-1][0] - rows[0][0]) / 1e9
print(f'game frames: {len(rows)} over {span_s:.1f} s, {len(rows) - len(world)} without the world')
if world:
    counts = [0, 0, 0, 0]
    for r in world:
        counts[bucket(r[3])] += 1
    minutes = (world[-1][0] - world[0][0]) / 60e9
    irregular = counts[0] + counts[2] + counts[3]
    print(f'world frames: {len(world)}; steps: {histogram(counts, len(world))}')
    rate = f'{irregular / minutes:.1f} a minute' if minutes > 0 else 'n/a'
    print(f'irregular (0 or 2+ steps): {irregular}, {rate}')
    ticks = {}
    for r in world:
        ticks[r[2]] = ticks.get(r[2], 0) + 1
    common = sorted(ticks.items(), key=lambda kv: -kv[1])[:5]
    print('ticks: mean {:.2f}; most common {}'.format(sum(r[2] for r in world) / len(world), ', '.join(f'{t} x{n}' for t, n in common)))
    print('measured: mean {:.3f} ms, max {:.3f} ms'.format(sum(r[1] for r in world) / len(world), max(r[1] for r in world)))

if a.frames:
    presents = []
    with open(a.frames) as f:
        for r in csv.DictReader(f):
            if r.get('multiple') and (r.get('presented') or '').isdigit():
                presents.append(int(r['presented']))
    if not world:
        sys.exit('no world frames to merge with the presents')
    # (only while the world runs: a present in the front end carries no step by design)
    presents = sorted(p for p in presents if p >= world[0][0] and p <= world[-1][0])
    if len(presents) < 2:
        sys.exit(f'{a.frames}: fewer than 2 presents while the world runs')
    counts = [0, 0, 0, 0]
    i = 0
    starts = [r[0] for r in world]
    steps = [r[3] for r in world]
    # skip the game frames before the first present's interval
    while i < len(starts) and starts[i] <= presents[0]:
        i += 1
    for p in presents[1:]:
        n = 0
        while i < len(starts) and starts[i] <= p:
            n += steps[i]
            i += 1
        counts[bucket(n)] += 1
    total = len(presents) - 1
    print(f'presents: {total}; world steps a present: {histogram(counts, total)}')
