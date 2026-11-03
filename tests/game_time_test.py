#!/usr/bin/env python3
# ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
#
# scripts/game_time.py against synthetic RECARBON_GAME_TIME_LOG and RECARBON_FRAME_LOG CSVs
# whose answers are known: the steps histogram, frames without the world, the irregular rate,
# the window, a last line cut short at exit, and the world steps each present carried. Also
# that the runtime's header (game/game_time_log.h) is what the script reads.
# From the repo root: python3 tests/game_time_test.py (ctest runs it).
import os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, 'scripts', 'game_time.py')
failures = 0


def check(ok, what):
    global failures
    if not ok:
        print(f'  FAIL: {what}')
        failures += 1


HEADER = 'host_ns,measured_ms,ticks,frame_ms,steps,raw_steps,acc_ms\n'
FRAMES_HEADER = 'id,vblank,presented,on_screen,timed,fit_valid,period_ns,grid_ns,rms_ns,ready,multiple\n'
T0 = 7_000_000_000_000
FRAME = 16_666_667


def write_game(path, steps, cut_last=False, ticks=60):
    text = HEADER
    for i, k in enumerate(steps):
        t = T0 + i * FRAME
        text += f'{t},16.667,{ticks if k >= 0 else 0},16.667,{k},{k},{-1 if k < 0 else 20.0}\n'
    if cut_last:
        text = text[:-12]
    open(path, 'w').write(text)


def write_frames(path, presents):
    text = FRAMES_HEADER
    for i, t in enumerate(presents):
        text += f'{i + 1},{t},{t},{t},1,1,16666666.7,{t},500,{t},1\n'
    open(path, 'w').write(text)


def run(*args):
    p = subprocess.run([sys.executable, SCRIPT, *args], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


with tempfile.TemporaryDirectory() as d:
    game, frames = os.path.join(d, 'game.csv'), os.path.join(d, 'frames.csv')

    print('---- the runtime header ----')
    src = open(os.path.join(ROOT, 'runtime', 'game', 'game_time_log.h')).read()
    m = re.search(r'kHeader = "([^"]*)\\n"', src)
    check(m and m.group(1) + '\n' == HEADER, 'game_time_log.h writes the columns the script reads')

    print('---- the histogram ----')
    # 2 front-end frames, then 60 world frames: 54 with 1 step, 3 with none, 2 with 2, 1 with 4
    steps = [-1, -1] + [1] * 54 + [0, 0, 0, 2, 2, 4]
    write_game(game, steps)
    rc, out = run(game)
    check(rc == 0, f'runs: {out}')
    check('game frames: 62 over' in out and '2 without the world' in out, out)
    check('world frames: 60; steps: no step 3 (5.00%), 1 54 (90.00%), 2 2 (3.33%), 3+ 1 (1.67%)' in out, out)
    # 6 irregular over 59 frame intervals of the world
    rate = 6 / (59 * FRAME / 60e9)
    check(f'irregular (0 or 2+ steps): 6, {rate:.1f} a minute' in out, out)
    check('ticks: mean 60.00; most common 60 x60' in out, out)

    print('---- a cut line and the window ----')
    write_game(game, [1] * 10 + [0], cut_last=True)
    rc, out = run(game)
    check(rc == 0 and 'world frames: 10; steps: no step 0' in out, f'the cut last line skipped: {out}')
    write_game(game, [1] * 60 + [0] * 60)
    rc, out = run(game, '--from', '1.0')
    check('world frames: 60; steps: no step 60' in out, f'--from: seconds from the first frame: {out}')
    rc, out = run(game, '--to', '1.0')
    check('world frames: 60; steps: no step 0 (0.00%), 1 60' in out, f'--to: {out}')

    print('---- merged with the presents ----')
    # one present a game frame, a frame later: each present carries its frame's steps
    steps = [1, 1, 0, 1, 2, 1, 1, 3, 1, 1]
    write_game(game, steps)
    write_frames(frames, [T0 + i * FRAME + FRAME // 2 for i in range(len(steps))])
    rc, out = run(game, '--frames', frames)
    # presents 2..10 cover frames 1..8 (frame 9 starts after the last present that is within range)
    check('presents: 8; world steps a present: no step 1 (12.50%), 1 5 (62.50%), 2 1 (12.50%), 3+ 1 (12.50%)' in out, out)
    # two game frames between two presents (a doubled refresh): their steps add up
    write_frames(frames, [T0 + FRAME // 2, T0 + 2 * FRAME + FRAME // 2, T0 + 3 * FRAME + FRAME // 2])
    write_game(game, [1, 1, 1, 1, 1])
    rc, out = run(game, '--frames', frames)
    check('presents: 2; world steps a present: no step 0 (0.00%), 1 1 (50.00%), 2 1 (50.00%)' in out, out)
    write_game(game, [-1] * 5)
    rc, out = run(game, '--frames', frames)
    check(rc != 0 and 'no world frames' in out, f'no world: refused: {out}')

    print('---- not a game-time file ----')
    open(game, 'w').write(FRAMES_HEADER)
    rc, out = run(game)
    check(rc != 0 and 'not a RECARBON_GAME_TIME_LOG file' in out, out)

print(f'{failures} FAILED' if failures else 'all passed')
sys.exit(1 if failures else 0)
