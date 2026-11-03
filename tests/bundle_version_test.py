#!/usr/bin/env python3
# ReCarbon tests. GPL-3.0-or-later (see COPYING).
#
# scripts/macos/bundle_version.py: CFBundleVersion made from the version, with values worked out
# by hand from MAJOR * 1000000 + MINOR * 10000 + PATCH * 100 + STAGE (alpha.N: N, beta.N: 33 + N,
# rc.N: 66 + N, release: 99); every future version above v0.1.3's commit-count build 368; the order
# of the numbers the order of the versions (Semantic Versioning's); make_app.py writing it (no git
# in it any more) and check_app.py's check of an Info.plist. No build, no network.
# From the repo root: python3 tests/bundle_version_test.py (ctest runs it).
import importlib.util, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
failures = 0


def load(name):
    argv = sys.argv
    sys.argv = [name + '.py']  # make_app.py and check_app.py parse their command line when loaded
    try:
        spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, 'scripts', 'macos', name + '.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        sys.argv = argv


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print(f'FAIL: {what}')


bv = load('bundle_version')
make_app = load('make_app')
check_app = load('check_app')

# Hand-computed.
EXPECTED = {
    '0.1.0': '10099',
    '0.1.3': '10399',
    '0.1.4': '10499',
    '0.1.5-alpha': '10500',
    '0.1.5-alpha.1': '10501',
    '0.1.5-alpha.32': '10532',
    '0.1.5-beta': '10533',
    '0.1.5-beta.2': '10535',
    '0.1.5-rc.1': '10567',
    '0.1.5-rc.32': '10598',
    '0.1.5': '10599',
    '0.2.0': '20099',
    '0.99.99': '999999',
    '1.0.0-rc.2': '1000068',
    '1.0.0': '1000099',
    '12.3.4': '12030499',
    '0.0.3': '399',
}
for version, number in EXPECTED.items():
    try:
        got = bv.bundle_version(version)
    except ValueError as e:
        got = f'ValueError {e}'
    check(got == number, f'{version}: {got}, want {number}')

# One whole number (Apple: at most three period-separated integers; a dotted 0.1.4 would compare
# below 368), above v0.1.3's 368 for every version from 0.0.3 up.
for version in EXPECTED:
    n = bv.bundle_version(version)
    check(n.isdigit() and not n.startswith('0'), f'{version}: {n} is one integer')
    check(int(n) > 368, f'{version}: {n} above v0.1.3\'s 368')
check(bv.LAST_COMMIT_COUNT_BUILD == 368, 'v0.1.3 shipped 368')

# Refused: not a version, outside the scheme's room, or at or below 368.
for bad in ('', '0.1', '0.1.4.1', 'v0.1.4', '0.1.4-dirty', '0.1.4+build.1', '0.1.4-alpha.0', '0.1.4-alpha.33',
            '0.1.4-gamma.1', '0.1.4-Alpha.1', '0.1.04', '0.100.0', '0.1.100', '0.0.2', '0.0.0', '0.0.3-rc.1'):
    try:
        bv.bundle_version(bad)
        check(False, f'{bad!r} is refused')
    except ValueError:
        pass

# The numbers sort as the versions do (Semantic Versioning's order).
ORDER = ['0.1.3', '0.1.4', '0.1.5-alpha', '0.1.5-alpha.1', '0.1.5-alpha.2', '0.1.5-alpha.10', '0.1.5-beta',
         '0.1.5-beta.1', '0.1.5-rc.1', '0.1.5-rc.2', '0.1.5', '0.1.6', '0.2.0-alpha.1', '0.2.0', '0.10.0',
         '1.0.0-alpha', '1.0.0-rc.32', '1.0.0', '1.0.1', '2.0.0']
numbers = [int(bv.bundle_version(v)) for v in ORDER]
for (a, x), (b, y) in zip(zip(ORDER, numbers), zip(ORDER[1:], numbers[1:])):
    check(x < y, f'{a} ({x}) < {b} ({y})')

# make_app.py writes it from the version and no longer counts commits.
src = open(os.path.join(ROOT, 'scripts', 'macos', 'make_app.py')).read()
check("'CFBundleVersion': bundle_version(version)," in src, 'make_app.py writes bundle_version(version)')
check('rev-list' not in src and 'build_number' not in src, 'make_app.py counts no commits')
check(make_app.bundle_version is bv.bundle_version or make_app.bundle_version('0.1.4') == '10499', 'make_app.py uses bundle_version.py')

# check_app.py's check of an Info.plist.
good = {'CFBundleShortVersionString': '0.1.4', 'CFBundleVersion': '10499'}
check(check_app.bundle_version_problem(good) == ('10499', ''), f'0.1.4 / 10499 passes: {check_app.bundle_version_problem(good)}')
for plist, why in (({'CFBundleShortVersionString': '0.1.4', 'CFBundleVersion': '313'}, 'a commit count'),
                   ({'CFBundleShortVersionString': '0.1.3', 'CFBundleVersion': '368'}, "v0.1.3's own"),
                   ({'CFBundleShortVersionString': '0.1.4'}, 'none'),
                   ({'CFBundleShortVersionString': '0.1.4', 'CFBundleVersion': '0.1.4'}, 'a dotted one'),
                   ({'CFBundleVersion': '10499'}, 'no version'),
                   ({'CFBundleShortVersionString': '0.1.4-dirty', 'CFBundleVersion': '10499'}, 'a version outside the scheme')):
    check(check_app.bundle_version_problem(plist)[1] != '', f'{why} fails: {plist}')

print('bundle_version_test:', 'FAILED' if failures else 'passed', f'({failures} failures)')
sys.exit(1 if failures else 0)
