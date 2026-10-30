#!/usr/bin/env python3
# ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
#
# The benchmark harness's safety: scripts/run_attempt.sh's mandatory deadline and argument
# checks, and scripts/bench/bench_run.sh + bench_watch.sh's refusals and stops. Malformed
# arguments, the deadline and the hard limit, thermal pressure (level 2 = Heavy and above stop,
# 1 doesn't; BENCH_THERMAL_GRACE allows a bounded stay at 2, never at 3), memory and swap, monitor failures, logging failures and a dead watchdog. Runs copies
# of the scripts in a scratch tree with a stub in place of build/main/runtime/ReCarbon and fake
# probes (BENCH_*_CMD reading files the test changes), so no game is launched.
# From the repo root: python3 tests/bench_safety_test.py (ctest runs it).
import os, shutil, signal, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
failures = 0

STUB = r'''#!/bin/sh
# Test stub for the game: STUB_MODE exit:<s> | term | ignore | serious
echo "stub ran res=$RECARBON_RESOLUTION home=$HOME"
echo $$ > "$STUB_DIR/stub.pid"
echo ran >> "$STUB_DIR/ran"
case "$STUB_MODE" in
  exit:*) sleep "${STUB_MODE#exit:}"; exit 0 ;;
  term) trap 'echo stub got TERM; exit 0' TERM; while :; do sleep 0.1; done ;;
  ignore) trap '' TERM; while :; do sleep 0.1; done ;;
  serious) sleep 0.5; echo "[thermal] fair -> serious at 0.5 s"; trap 'exit 0' TERM; while :; do sleep 0.1; done ;;
esac
'''


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print(f'FAIL: {what}')


class Tree:
    def __init__(self, base, name):
        self.root = os.path.join(base, name)
        os.makedirs(os.path.join(self.root, 'scripts', 'bench'))
        shutil.copy(os.path.join(ROOT, 'scripts', 'run_attempt.sh'), os.path.join(self.root, 'scripts'))
        for f in ('bench_run.sh', 'bench_watch.sh', 'lib.sh'):
            shutil.copy(os.path.join(ROOT, 'scripts', 'bench', f), os.path.join(self.root, 'scripts', 'bench'))
        stub = os.path.join(self.root, 'build', 'main', 'runtime', 'ReCarbon')
        os.makedirs(os.path.dirname(stub))
        with open(stub, 'w') as f:
            f.write(STUB)
        os.chmod(stub, 0o755)
        self.probes = os.path.join(self.root, 'probes')
        os.makedirs(self.probes)
        self.set(thermal='0', memfree='60', swap='100.00')

    def set(self, **values):
        for k, v in values.items():
            p = os.path.join(self.probes, k)
            if v is None:
                os.remove(p)
            else:
                with open(p + '.tmp', 'w') as f:
                    f.write(v + '\n')
                os.replace(p + '.tmp', p)

    def env(self, mode='exit:0.5', **extra):
        env = {k: v for k, v in os.environ.items() if not k.startswith(('RECARBON_', 'BENCH_', 'RUN_'))}
        env.update(STUB_MODE=mode, STUB_DIR=self.root,
                   BENCH_THERMAL_CMD=f'cat {self.probes}/thermal', BENCH_MEMFREE_CMD=f'cat {self.probes}/memfree',
                   BENCH_SWAP_CMD=f'cat {self.probes}/swap', BENCH_INTERVAL='0.2', BENCH_COOL_CHECKS='2',
                   BENCH_COOL_TRIES='4', BENCH_COOL_INTERVAL='0.05', BENCH_POLL='0.1', BENCH_KILL_WAIT='1',
                   BENCH_GAME_NAME='recarbon-bench-test-no-such-process')
        env.update(extra)
        return env

    def path(self, *parts):
        return os.path.join(self.root, *parts)

    def ran(self):
        return os.path.exists(self.path('ran'))

    def reset_ran(self):
        for f in ('ran', 'stub.pid'):
            if os.path.exists(self.path(f)):
                os.remove(self.path(f))

    def stub_alive(self):
        try:
            pid = int(open(self.path('stub.pid')).read())
        except (OSError, ValueError):
            return False
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return False
        return True

    def bench(self, args, env, timeout=60):
        # A run that outlives the timeout is a failed check (and is killed), not a crashed test.
        p = subprocess.Popen(['bash', 'scripts/bench/bench_run.sh', *args], cwd=self.root, env=env,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, start_new_session=True)
        try:
            out, err = p.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            out, err = p.communicate()
            check(False, f'bench_run {args} still running after {timeout} s')
        return subprocess.CompletedProcess(p.args, p.returncode, out, err)

    def start(self, args, env):
        return subprocess.Popen(['bash', 'scripts/bench/bench_run.sh', *args], cwd=self.root, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, start_new_session=True)

    def samples(self, name):
        try:
            return open(self.path('game', 'm3', 'bench', name, 'watch.txt')).read()
        except OSError:
            return ''

    def verdict(self, name):
        try:
            return open(self.path('game', 'm3', 'bench', name, 'verdict.txt')).read()
        except OSError:
            return ''


def wait_for_samples(tree, name, n, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        if len(tree.samples(name).splitlines()) >= n:
            return True
        time.sleep(0.05)
    return False


def finish(p, timeout=30):
    try:
        out, err = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL)
        out, err = p.communicate()
    return p.returncode, out, err


def mid_run(tree, name, change, mode='term', args=None, settle=3, **extra):
    # Starts a run, waits for some samples, makes a change, returns (exit, out+err, seconds after it).
    p = tree.start(args or [name, '30'], tree.env(mode, **extra))
    ok = wait_for_samples(tree, name, settle)
    check(ok, f'{name}: the watchdog records samples')
    t0 = time.time()
    change()
    rc, out, err = finish(p)
    return rc, out + err, time.time() - t0


with tempfile.TemporaryDirectory() as tmp:
    t = Tree(tmp, 'repo')

    # ---- malformed arguments: exit 2, nothing runs ----
    for args in ([], ['x'], ['a 0', '10'], ['v035-as3x-off 0', '3x'], ['x', '0'], ['x', '-5'], ['x', 'abc'],
                 ['x', '601'], ['x', '10', '4x'], ['x', '10', '3x', 'extra'], ['../x', '10'], ['x', '1.5'], ['x', '']):
        r = t.bench(args, env=t.env())
        check(r.returncode == 2, f'bench_run {args}: exit 2 ({r.returncode}: {r.stderr.strip()!r})')
        check(not t.ran(), f'bench_run {args}: nothing launched')
    check(not os.path.exists(t.path('game', 'm3', 'bench')), 'malformed arguments create no output')

    for args, extra in ((['x', '0'], {}), (['x', 'abc'], {}), (['x', '3601'], {}), (['a b', '5'], {}),
                        (['.hidden', '5'], {}), (['x', '5'], {'RUN_RESOLUTION': '5x'}),
                        (['x', '5'], {'RUN_HOME': 'relative/home'}), (['x', '5'], {'RUN_HOME': '/no/such/folder'})):
        r = subprocess.run(['bash', 'scripts/run_attempt.sh', *args], cwd=t.root, env=t.env(**extra),
                           capture_output=True, text=True, timeout=30)
        check(r.returncode == 2, f'run_attempt.sh {args} {extra}: exit 2 ({r.returncode}: {r.stderr.strip()!r})')
        check(not t.ran() and not os.path.exists(t.path('game', 'm3', 'x.log')), f'run_attempt.sh {args} {extra}: nothing launched')

    # ---- a normal run: exit 0, the settings reach the game ----
    home = os.path.join(tmp, 'isolated-home')
    os.makedirs(home)
    r = t.bench(['ok-2x', '20', '2x'], env=t.env('exit:1', RUN_HOME=home))
    check(r.returncode == 0, f'a normal run: exit 0 ({r.returncode}: {r.stdout!r} {r.stderr!r})')
    check(t.verdict('ok-2x').startswith('verdict=0 '), f'verdict file says 0 ({t.verdict("ok-2x")!r})')
    check(len(t.samples('ok-2x').splitlines()) >= 3, 'samples were recorded')
    log = open(t.path('game', 'm3', 'ok-2x.log')).read()
    check(f'res=2x home={home}' in log, f'the game got 2x and RUN_HOME ({log[:200]!r})')
    r = t.bench(['ok-2x', '20'], env=t.env())
    check(r.returncode == 3, f'a used name is refused, not overwritten ({r.returncode})')
    t.reset_ran()

    # ---- the deadline: run_attempt.sh's SIGTERM, then SIGKILL for a game that ignores it ----
    t0 = time.time()
    r = t.bench(['deadline', '2'], env=t.env('ignore'))
    elapsed = time.time() - t0
    log = open(t.path('game', 'm3', 'deadline.log')).read()
    check('[run_attempt] deadline: SIGTERM' in log, 'the deadline is logged')
    check(elapsed < 15 and not t.stub_alive(), f'a game ignoring SIGTERM is killed ({elapsed:.1f} s)')
    check(r.returncode == 0, f'a deadline stop is a normal end ({r.returncode}: {r.stdout!r})')

    # ---- the hard limit, if run_attempt.sh's deadline fails ----
    h = Tree(tmp, 'hung')
    with open(h.path('scripts', 'run_attempt.sh'), 'w') as f:
        f.write('#!/usr/bin/env bash\nmkdir -p game/m3; echo hung > "game/m3/$1.log"\n'
                'echo $$ > "$STUB_DIR/stub.pid"\ntrap "" TERM\nwhile :; do sleep 0.1; done\n')
    t0 = time.time()
    r = h.bench(['hard', '1'], env=h.env(BENCH_GRACE='1'))
    elapsed = time.time() - t0
    check(r.returncode == 14, f'a launcher that never ends: exit 14 ({r.returncode}: {r.stdout!r} {r.stderr!r})')
    check(elapsed < 15 and not h.stub_alive(), f'... and it is killed ({elapsed:.1f} s)')

    # ---- thermal pressure: 2 (Heavy) and above stop at once, 1 doesn't ----
    for level in ('2', '3'):
        t.set(thermal='0')
        name = f'heavy{level}'
        rc, text, after = mid_run(t, name, lambda: t.set(thermal=level))
        check(rc == 10, f'thermal level {level}: exit 10 ({rc}: {text!r})')
        check(f'thermal pressure level {level}' in t.samples(name), f'thermal level {level}: the STOP line names it')
        check(after < 5 and not t.stub_alive(), f'thermal level {level}: the game is stopped ({after:.1f} s)')
        check(f'[bench_watch] STOP: thermal pressure level {level}' in open(t.path('game', 'm3', f'{name}.log')).read(),
              f'thermal level {level}: the game log records the stop')
        check('the grace' not in t.samples(name), f'thermal level {level}: without a grace the STOP line is as before')
    # ---- BENCH_THERMAL_GRACE: a bounded stay at level 2; level 3 still stops at once ----
    t.set(thermal='0')
    rc, text, after = mid_run(t, 'grace2', lambda: t.set(thermal='2'), BENCH_THERMAL_GRACE='2')
    check(rc == 10, f'grace 2 s, level 2: exit 10 ({rc}: {text!r})')
    check(after >= 1.5, f'grace 2 s: the run went on for the grace ({after:.1f} s)')
    check(after < 2.8 and not t.stub_alive(), f'grace 2 s: then stopped at the grace, not later ({after:.1f} s)')
    check('the grace of 2 s' in t.samples('grace2'), 'grace 2 s: the STOP line names the grace')
    t.set(thermal='0')
    rc, text, after = mid_run(t, 'grace3', lambda: t.set(thermal='3'), BENCH_THERMAL_GRACE='2')
    check(rc == 10 and after < 1.5 and 'thermal pressure level 3' in t.samples('grace3'), f'grace 2 s, level 3: stops at once ({rc}, {after:.1f} s)')
    t.set(thermal='0')
    p = t.start(['gracereset', '30'], t.env('exit:6', BENCH_THERMAL_GRACE='3'))
    wait_for_samples(t, 'gracereset', 3)
    for level in ('2', '0', '2', '0'):
        t.set(thermal=level)
        time.sleep(1.2)
    rc, out, err = finish(p)
    check(rc == 0, f'grace 3 s: two stays of 1.2 s at level 2 with level 0 between: the run ends by itself ({rc}: {out!r} {err!r})')
    t.set(thermal='0')
    t.reset_ran()
    for bad in ('abc', '-1', '601', '1.5'):  # (empty is the default, 0)
        r = t.bench([f'gracebad{len(bad)}{bad[:1]}', '10'], env=t.env(BENCH_THERMAL_GRACE=bad))
        check(r.returncode == 11 and not t.ran(), f'BENCH_THERMAL_GRACE={bad!r}: refused as a monitor failure, nothing ran ({r.returncode})')
        t.reset_ran()

    t.set(thermal='0')
    p = t.start(['moderate', '20'], t.env('exit:1.5'))
    wait_for_samples(t, 'moderate', 2)
    t.set(thermal='1')
    rc, out, err = finish(p)
    check(rc == 0, f'thermal level 1 (Moderate) doesn\'t stop a run ({rc}: {out!r} {err!r})')
    t.set(thermal='0')

    # ---- memory and swap ----
    rc, text, _ = mid_run(t, 'memlow', lambda: t.set(memfree='10'))
    check(rc == 10 and 'memory free 10%' in t.samples('memlow'), f'memory free 10%: exit 10 ({rc})')
    t.set(memfree='60')
    rc, text, _ = mid_run(t, 'swapgrow', lambda: t.set(swap='1200.50'))
    check(rc == 10 and 'swap grew' in t.samples('swapgrow'), f'swap +1.1 GB: exit 10 ({rc})')
    t.set(swap='100.00')
    rc, text, _ = mid_run(t, 'serious', lambda: None, mode='serious')
    check(rc == 10 and 'serious thermal' in t.samples('serious'), f'the game\'s own serious line: exit 10 ({rc})')

    # ---- monitor failures: whatever can't be checked stops the run ----
    for what, change, restore in (('thermal', lambda: t.set(thermal='garbage'), lambda: t.set(thermal='0')),
                                  ('thermal', lambda: t.set(thermal=None), lambda: t.set(thermal='0')),
                                  ('memory', lambda: t.set(memfree=''), lambda: t.set(memfree='60')),
                                  ('swap', lambda: t.set(swap='n/a'), lambda: t.set(swap='100.00'))):
        name = f'mon-{what}-{failures}-{time.time_ns() % 100000}'
        rc, text, after = mid_run(t, name, change)
        check(rc == 11 and f'the {what} probe failed' in t.samples(name), f'{what} probe failure: exit 11 ({rc}: {t.samples(name)[-200:]!r})')
        check(not t.stub_alive(), f'{what} probe failure: the game is stopped')
        restore()
    t.reset_ran()
    t.set(thermal='hot')
    r = t.bench(['pre-thermal', '10'], env=t.env())
    check(r.returncode == 11 and not t.ran(), f'a broken probe before the launch: exit 11, nothing ran ({r.returncode})')
    t.set(thermal='0')

    # ---- logging failures ----
    rc, text, _ = mid_run(t, 'ro-samples', lambda: os.chmod(t.path('game', 'm3', 'bench', 'ro-samples', 'watch.txt'), 0o444))
    check(rc == 12 and not t.stub_alive(), f'the samples file turns read-only: exit 12, game stopped ({rc}: {text!r})')
    sleeper = subprocess.Popen(['sleep', '30'])
    os.makedirs(t.path('a-directory'))
    r = subprocess.run(['bash', 'scripts/bench/bench_watch.sh', t.path('a-directory'), str(sleeper.pid), t.path('none.log')],
                       cwd=t.root, env=t.env(), capture_output=True, text=True, timeout=30)
    check(r.returncode == 12, f'bench_watch with an unwritable samples path: exit 12 ({r.returncode})')
    check(sleeper.wait(timeout=10) is not None, '... and it stops the run it watches')
    sleeper = subprocess.Popen(['sleep', '30'])
    r = subprocess.run(['bash', 'scripts/bench/bench_watch.sh', t.path('w.txt'), str(sleeper.pid), t.path('missing.log')],
                       cwd=t.root, env=t.env(BENCH_LOG_WAIT='0'), capture_output=True, text=True, timeout=30)
    check(r.returncode == 12 and 'no game log' in open(t.path('w.txt')).read(), f'no game log: exit 12 ({r.returncode})')
    check(sleeper.wait(timeout=10) is not None, '... and it stops the run it watches')

    # ---- a dead watchdog stops the game ----
    def kill_watchdog():
        os.kill(int(open(t.path('game', 'm3', 'bench', 'wd-dead', 'watch.pid')).read()), signal.SIGKILL)
    rc, text, after = mid_run(t, 'wd-dead', kill_watchdog)
    check(rc == 13 and not t.stub_alive(), f'the watchdog killed: exit 13, game stopped ({rc}: {text!r})')

    # ---- refusals before the launch: exit 3, nothing runs ----
    t.reset_ran()
    t.set(thermal='1')
    r = t.bench(['warm', '10'], env=t.env())
    check(r.returncode == 3 and 'not cool' in r.stderr and not t.ran(), f'not cool: refused ({r.returncode}: {r.stderr!r})')
    t.set(thermal='0', memfree='25')
    r = t.bench(['lowmem', '10'], env=t.env())
    check(r.returncode == 3 and not t.ran(), f'memory free 25%: refused ({r.returncode})')
    t.set(memfree='60')
    fake = os.path.join(tmp, 'rcbenchfake')
    os.symlink('/bin/sleep', fake)  # named like a game to pgrep, without copying a system binary
    other = subprocess.Popen([fake, '30'])
    try:
        time.sleep(0.2)
        r = t.bench(['busy', '10'], env=t.env(BENCH_GAME_NAME='rcbenchfake'))
        check(r.returncode == 3 and 'already running' in r.stderr and not t.ran(), f'another game running: refused ({r.returncode}: {r.stderr!r})')
    finally:
        other.kill()
        other.wait()

    # A game started during the cooldown (the first check passed) is caught just before the launch.
    t.reset_ran()
    t.set(thermal='1')
    p = t.start(['late-game', '10'], t.env(BENCH_GAME_NAME='rcbenchfake', BENCH_COOL_TRIES='100', BENCH_COOL_INTERVAL='0.1'))
    time.sleep(1)  # in the cooldown: level 1 keeps it waiting
    other = subprocess.Popen([fake, '30'])
    try:
        time.sleep(0.2)
        t.set(thermal='0')
        rc, out, err = finish(p)
        check(rc == 3 and 'already running' in err and not t.ran(), f'a game started during the cooldown: refused ({rc}: {err!r})')
    finally:
        other.kill()
        other.wait()
        t.set(thermal='0')

    for tree in (t, h):
        if tree.stub_alive():
            os.kill(int(open(tree.path('stub.pid')).read()), signal.SIGKILL)
            check(False, f'a stub was left running in {tree.root}')

print(f'---- {failures} failed checks ----')
sys.exit(1 if failures else 0)
