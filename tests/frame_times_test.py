#!/usr/bin/env python3
# ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
#
# scripts/frame_times.py against synthetic RECARBON_FRAME_LOG CSVs and session
# logs whose answers are known: the window (session seconds, wall clock across
# midnight, placed by the log's first present), the missed-30-fps threshold,
# the counts, the 1 s windows, the [perf] thermal state, and a last line cut
# short at exit. Also that the runtime still prints what the script parses,
# so a changed log or CSV format fails here, not in a hand test's analysis.
# From the repo root: python3 tests/frame_times_test.py (ctest runs it).
import os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, 'scripts', 'frame_times.py')
failures = 0


def check(ok, what):
    global failures
    if not ok:
        print(f'  FAIL: {what}')
        failures += 1


HEADER = 'id,vblank,presented,on_screen,timed,fit_valid,period_ns,grid_ns,rms_ns,ready,multiple\n'
T0 = 5_000_000_000_000  # an arbitrary steady-clock start, ns


def write_csv(path, frame_ms, cut_last=False):
    """One row per present, `presented` advancing by each frame time."""
    t = T0
    rows = [HEADER]
    for i, ms in enumerate([0.0] + frame_ms):
        t += int(ms * 1e6)
        rows.append(f'{i + 1},{t},{t},{t + 2_000_000},1,1,16666666.7,{t},500,{t},1\n')
    text = ''.join(rows)
    if cut_last:
        text = text[:-30]  # the exit cut the last line short
    open(path, 'w').write(text)


def write_log(path, first_present, perf=()):
    lines = ['[    0.0] [build] ReCarbon v0.0.1 (test)\n',
             f'[{first_present:7.1f}] [vblank] presents are timed (VK_KHR_present_wait)\n']
    for t, fps, worst, thermal in perf:
        lines.append(f'[{t:7.1f}] [perf] {fps:.1f} fps (worst frame {worst:.1f} ms) | CP busy 0.2 ms/frame | thermal {thermal}\n')
    open(path, 'w').write(''.join(lines))


def run(*args):
    r = subprocess.run([sys.executable, SCRIPT, *args], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def field(out, pattern):
    m = re.search(pattern, out)
    return m.group(1) if m else None


with tempfile.TemporaryDirectory() as d:
    csv = os.path.join(d, 'frames.csv')
    log = os.path.join(d, '2026-10-23_23-59-50.log')

    print('---- 60 fps with one 600 ms frame ----')
    # 20 s at 60 fps, a 600 ms stall at the middle.
    frames = [1000 / 60] * 600 + [600.0] + [1000 / 60] * 600
    write_csv(csv, frames)
    code, out = run(csv)
    check(code == 0, f'exit 0, got {code}: {out}')
    check(field(out, r'(\d+) frames') == str(len(frames)), f'every frame counted: {out}')
    check(field(out, r'max ([0-9.]+)') == '600.0', f'max 600.0: {out}')
    check(field(out, r'41\.7 ms (\d+)') == '1', f'one frame over 41.7 ms: {out}')
    check(field(out, r'250 ms (\d+)') == '1' and field(out, r'900 ms (\d+)') == '0', f'250/900 buckets: {out}')
    check(field(out, r'median ([0-9.]+)') == '16.7', f'median 16.7: {out}')
    check(field(out, r'under 30 fps (\d+)') == '1', f'the stall second is under 30 fps: {out}')
    mean = float(field(out, r'mean ([0-9.]+)'))
    check(abs(mean - len(frames) / ((1200 / 60 + 0.6))) < 0.2, f'mean fps {mean}')

    print('---- 30 fps cadence: jitter is not a missed frame ----')
    frames = [33.2, 33.4] * 300 + [50.1] * 3
    write_csv(csv, frames)
    code, out = run(csv)
    check(field(out, r'41\.7 ms (\d+)') == '3', f'only the three 50.1 ms frames missed 30 fps: {out}')
    check(field(out, r'under 30 fps (\d+)') == '0', f'no second under 30 fps: {out}')

    print('---- a window by session seconds, placed by the log ----')
    # First present at 2.5 s of the session; a 500 ms stall 1 s in (3.5 s),
    # outside the 5-15 s window, which then holds only 60 fps frames.
    frames = [1000 / 60] * 60 + [500.0] + [1000 / 60] * 1200
    write_csv(csv, frames)
    write_log(log, 2.5, perf=[(4.0, 40.0, 500.0, 'fair'), (8.0, 60.0, 17.0, 'nominal'), (12.0, 60.0, 16.9, 'nominal')])
    code, out = run(csv, '--log', log, '--from', '5', '--to', '15')
    check(code == 0, f'exit 0: {out}')
    check(field(out, r'max ([0-9.]+)') == '16.7', f'the stall outside the window is left out: {out}')
    check(field(out, r'window: ([0-9.]+)-') == '5.0', f'window starts at 5.0: {out}')
    n = int(field(out, r'(\d+) frames'))
    check(abs(n - 600) <= 2, f'about 600 frames in 10 s, got {n}')
    check('thermal nominal 2' in out and 'fair' not in out, f'[perf] lines in the window only: {out}')
    # The same stall inside a window that covers it.
    code, out = run(csv, '--log', log, '--from', '3', '--to', '5')
    check(field(out, r'max ([0-9.]+)') == '500.0', f'the stall at 3.5 s is in 3-5 s: {out}')

    print('---- wall clock across midnight ----')
    # Session started 23:59:50; 23:59:55-00:00:05 is session 5-15 s.
    code, out = run(csv, '--log', log, '--wall-from', '23:59:55', '--wall-to', '00:00:05')
    check(code == 0 and field(out, r'window: ([0-9.]+)-([0-9.]+)') == '5.0', f'wall window at session 5 s: {out}')
    check(field(out, r'window: [0-9.]+-([0-9.]+)') == '15.0', f'wall window ends at session 15 s: {out}')

    print('---- a last line cut short ----')
    write_csv(csv, [1000 / 60] * 100, cut_last=True)
    code, out = run(csv)
    check(code == 0 and field(out, r'(\d+) frames') == '99', f'the cut line is skipped: {out}')

    print('---- too few presents ----')
    write_csv(csv, [16.7])
    code, out = run(csv)
    check(code != 0, f'fewer than 3 presents is an error: {out}')

# What the script parses must still be what the runtime writes.
print('---- the runtime still prints what frame_times.py reads ----')
presenter = open(os.path.join(ROOT, 'runtime', 'video', 'presenter.cpp')).read()
cp = open(os.path.join(ROOT, 'runtime', 'gpu', 'command_processor.cpp')).read()
check(HEADER.strip() in presenter, 'presenter.cpp writes the CSV header frame_times.py reads')
check('"[vblank] %s\\n", s_presentTiming ? "presents are timed' in presenter,
      'presenter.cpp logs "[vblank] presents are timed"')
check('"[perf] %.1f fps (worst frame %.1f ms)' in cp, 'command_processor.cpp logs "[perf] <fps> fps (worst frame <ms> ms)"')
check('" | thermal {}"' in cp, 'command_processor.cpp logs "| thermal <state>"')
check('RECARBON_FRAME_LOG' in presenter, 'presenter.cpp reads RECARBON_FRAME_LOG')

print(f'---- {failures} failed checks ----')
sys.exit(1 if failures else 0)
