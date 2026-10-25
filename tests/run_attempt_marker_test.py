#!/usr/bin/env python3
# ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
#
# scripts/run_attempt.sh's retention marker: a launch writes game/m3/.retention.json
# (owner, class "evidence", status "active") when there is none, and never changes
# an existing one. Runs a copy of the script in a scratch tree, with a stub in
# place of build/main/runtime/ReCarbon, so no game is launched.
# From the repo root: python3 tests/run_attempt_marker_test.py (ctest runs it).
import json, os, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, 'scripts', 'run_attempt.sh')
failures = 0


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print(f'FAIL: {what}')


def launch(tree, name):
    env = {k: v for k, v in os.environ.items() if not k.startswith('RECARBON_')}
    r = subprocess.run(['bash', 'scripts/run_attempt.sh', name, '10'], cwd=tree, env=env,
                       capture_output=True, text=True, timeout=60)
    return r


with tempfile.TemporaryDirectory() as tmp:
    tree = os.path.join(tmp, 'repo')
    os.makedirs(os.path.join(tree, 'scripts'))
    shutil.copy(SCRIPT, os.path.join(tree, 'scripts', 'run_attempt.sh'))
    stub = os.path.join(tree, 'build', 'main', 'runtime', 'ReCarbon')
    os.makedirs(os.path.dirname(stub))
    with open(stub, 'w') as f:
        f.write('#!/bin/sh\necho "stub ran with HOME=$HOME"\nexit 0\n')
    os.chmod(stub, 0o755)
    marker = os.path.join(tree, 'game', 'm3', '.retention.json')

    r = launch(tree, 'first')
    check(r.returncode == 0 and 'exit=0' in r.stdout, f'the stub launch succeeds ({r.returncode}, {r.stdout!r} {r.stderr!r})')
    check(os.path.isfile(marker), 'a launch writes game/m3/.retention.json')
    try:
        m = json.load(open(marker))
    except (OSError, ValueError) as e:
        check(False, f'the marker is readable JSON ({e})')
        m = {}
    check(m.get('owner') == 'recarbon/run_attempt.sh', f'owner names the script ({m.get("owner")!r})')
    check(m.get('project') == 'recarbon', 'project is recarbon')
    check(m.get('class') == 'evidence' and m.get('status') == 'active', f'evidence, active ({m.get("class")!r}, {m.get("status")!r})')
    check(str(m.get('created_at', '')).endswith('Z') and 'T' in str(m.get('created_at', '')), 'created_at is a UTC timestamp')
    log = os.path.join(tree, 'game', 'm3', 'first.log')
    check(os.path.isfile(log) and 'stub ran' in open(log).read(), 'the launch log is still written')

    # A marker someone changed (e.g. released for deletion) is never overwritten.
    m['status'] = 'reviewed'
    m['deletion_approved'] = {'by': 'test', 'at': '2026-10-25'}
    with open(marker, 'w') as f:
        json.dump(m, f)
    before = open(marker).read()
    r = launch(tree, 'second')
    check(r.returncode == 0, 'the second launch succeeds')
    check(open(marker).read() == before, 'an existing marker is left unchanged')

if failures:
    print(f'{failures} check(s) failed')
    sys.exit(1)
print('run_attempt_marker_test: all checks passed')
