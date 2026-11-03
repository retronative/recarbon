#!/usr/bin/env python3
# ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
#
# scripts/perf_report_check.py, the check a real performance report gets
# (reference/PERF_REPORT_FORMAT.md), against synthetic reports: a good one
# passes; each planted fault (a bare metric, a null value that isn't
# unavailable, an unknown status, a missing why, an unknown top-level key, a
# path, a guest address, a name, a save file, no fence) fails it.
# From the repo root: python3 tests/perf_report_check_test.py (ctest runs it).
import copy, importlib.util, json, os, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
spec = importlib.util.spec_from_file_location('perf_report_check', os.path.join(ROOT, 'scripts', 'perf_report_check.py'))
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)
failures = 0


def check(ok, what):
    global failures
    if not ok:
        print(f'  FAIL: {what}')
        failures += 1


def metric(value, status='measured', source='test', **more):
    m = {'value': None if status == 'unavailable' else value, 'status': status, 'source': source}
    if status == 'unavailable':
        m['why'] = 'none here'
    m.update(more)
    return m


GOOD = {
    'schema': 'recarbon-perf-report/1', 'generated': '2026-10-31T01:40:12Z', 'trigger': 'ui-key', 'session_s': 61.0,
    'build': {'version': '0.1.4', 'commit': 'abc1234', 'built': 'v0.1.4'},
    'system': {'machine': metric('Mac16,13'), 'chip': metric('Apple M5')},
    'display': {'mode': metric({'width': 2560, 'height': 1664, 'hz': 60.0})},
    'resolution': {'setting': metric('Auto'), 'capped': metric(None, 'unavailable')},
    'frames': {'window': 'last presents', 'fps_mean': metric(59.9, unit='fps'),
               'interval_ms': metric({'p50': 16.67, 'p95': 17.0, 'p99': 33.0, 'max': 64.0})},
    'display_events': {'dropped': metric(0)}, 'cp': {'busy_ms_per_frame': metric(4.1)}, 'gpu': {'busy_ms_per_frame': metric(9.8)},
    'hitches': {'late_frames': metric(2)}, 'thermal': {'state_now': metric('nominal')},
    'cpu': {'threads': metric([{'name': 'recarbon-cp', 'cpu_ms': 10}])},
    'warnings': metric({'renderer': 1}, 'estimated'),
    'flags': {'env': {'RECARBON_GPU_WAIT_BACKOFF': '0'}, 'settings_not_default': {}, 'overridden_by_env': []},
}
NAMES = {'alice', 'Alice Example', 'Example'}
HOMES = {'/Users/alice'}


def run(doc, summary='ReCarbon performance report\n', raw=None):
    with tempfile.NamedTemporaryFile('w', suffix='.md', delete=False) as f:
        f.write(raw if raw is not None else summary + '\n' + checker.SEPARATOR + json.dumps(doc, indent=2) + '\n```\n')
    try:
        return checker.check(f.name, NAMES, HOMES)[0]
    finally:
        os.unlink(f.name)


def fails(mutate, what, summary='ReCarbon performance report\n'):
    doc = copy.deepcopy(GOOD)
    mutate(doc)
    errors = run(doc, summary)
    check(errors, f'{what}: passed')


errors = run(GOOD)
check(not errors, f'the good report: {errors}')
check(checker.summary(GOOD).startswith('Mac16,13 | Apple M5 |'), checker.summary(GOOD))

fails(lambda d: d['system'].update(chip='Apple M5'), 'a bare value')
fails(lambda d: d['system']['chip'].update(value=None), 'a null value, measured')
fails(lambda d: d['resolution']['capped'].update(value=1), 'a value, unavailable')
fails(lambda d: d['system']['chip'].update(status='guessed'), 'an unknown status')
fails(lambda d: d['resolution']['capped'].pop('why'), 'unavailable without why')
fails(lambda d: d['system']['chip'].pop('source'), 'no source')
fails(lambda d: d.update(extra=1), 'an unknown top-level key')
fails(lambda d: d.pop('cpu'), 'a missing section')
fails(lambda d: d.update(schema='recarbon-perf-report/2'), 'another schema')
fails(lambda d: d['system']['chip'].update(value='Apple M5 (/Applications/ReCarbon.app)'), 'a path')
fails(lambda d: d['system']['chip'].update(value='at ~/Desktop'), 'a ~ path')
fails(lambda d: d['system']['chip'].update(value='sub_82001234'), 'a guest function')
fails(lambda d: d['system']['chip'].update(value='at 0x82ABCDEF'), 'a guest address')
fails(lambda d: d['system']['chip'].update(value='ALICE'), 'the login name')
fails(lambda d: None, 'a name in the summary', summary='by Alice Example\n')
fails(lambda d: d['cpu']['threads']['value'].append({'name': 'slot0.sav', 'cpu_ms': 1}), 'a save file')
fails(lambda d: d['flags']['env'].update(RECARBON_X='settings.toml'), 'the settings file')
check(run(None, raw='no fence here\n'), 'no fence: passed')
check(run(None, raw=checker.SEPARATOR + '{ "schema": \n```\n'), 'broken JSON: passed')
# Not a path: a ratio, a unit, the quantile formula.
check(not run(GOOD, summary='p50 16.7 / p95 17.0 ms, ms/frame, int(p/100*(n-1)+0.5)\n'), 'slashes that are no path failed')
check(not run(GOOD, summary='malice and 0x82ABCDEF0\n'), 'a longer word or number failed')

print(f'{failures} FAILED' if failures else 'all passed')
sys.exit(1 if failures else 0)
