#!/usr/bin/env python3
# Check that each row of the settings menu applies, by changing it through the menu as a
# player would and reading the effect. After SpeedBreaker's scripts/verify_settings.py
# (GPL-3.0-or-later). ReCarbon: written anew for ReCarbon's rows (driven by the website's
# generated settings data), its log lines, launches through scripts/run_attempt.sh, and a
# pure analysis that tests/verify_settings_test.py checks offline.
"""Check that every settings row applies, through the menu (RECARBON_UI_KEYS).

    scripts/verify_settings.py run --out <new folder> [--phases menu,persist,relaunch,corrupt]
        [--start S] [--script <RECARBON_INPUT_SCRIPT>] [--resolution 1x|2x|3x] [--fullscreen]
    scripts/verify_settings.py analyze <folder>      # re-read finished runs
    scripts/verify_settings.py plan [--phase menu]   # print a phase's keys and expectations

Each phase is one launch of build/main/runtime/ReCarbon through scripts/run_attempt.sh (its
deadline, an isolated user folder under --out: never the player's), with RECARBON_MUTE=0 so
the audio gain lines exist and SDL_AUDIO_DRIVER=dummy so nothing is heard. The menu keys start
at --start seconds (default 40: the game's front end is up); --script reaches a quieter scene
first (a paused race is the stillest). Every change the menu makes is logged ("[settings]
menu: video.vsync On -> Off (applied now)"), which is how navigation is confirmed before an
effect is judged.

Phases:
  menu      each row changed and put back. Picture rows are captured before, changed and
            after (a scene drifts, so a change is judged against the two captures around it),
            with a control: the menu opened and closed with nothing changed. Other rows are
            judged by the line the change logs (present mode, mipmaps, audio gain, mute in
            the background through window focus events). At the end settings.toml holds the
            defaults again.
  persist   every row changed and left so (no captures): settings.toml holds each value.
  relaunch  persist's user folder again: the file loads, and the values logged at launch
            (deadzones, audio gain, V-Sync) are the stored ones.
  corrupt   a settings.toml that doesn't parse: the game says so, keeps it as .bad, runs.
Fullscreen is only changed with --fullscreen (it takes over the screen). Mute and Internal
Resolution are checked as environment-overridden rows: run_attempt.sh always sets
RECARBON_MUTE and RECARBON_RESOLUTION, so the menu's change must be saved without changing the
run ("saved; the environment keeps this run's value"). Other RECARBON_* variables of the
caller's environment are not passed on.

Verdicts: PASS, FAIL, INCONCLUSIVE (the run didn't reach the state a check needs: the scene
wasn't still, the baseline line never appeared). Results go to stdout and <out>/results.tsv;
the exit status is 1 if anything FAILed. Needs Python 3.11+ (tomllib). Pictures are compared on a sampled grid in pure
Python (no numpy).
"""
import argparse
import json
import os
import pathlib
import re
import shutil
import statistics
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
SETTINGS_JSON = ROOT / 'website' / 'src' / 'data' / 'settings.generated.json'
# The menu's pages (runtime/user/settings.h's Category, in order; Q and E step through them).
PAGES = ['Display', 'Graphics', 'Audio', 'Input', 'Controls', 'Controller', 'Advanced']
HOW_LIVE = 'applied now'
HOW_ENV = "saved; the environment keeps this run's value"
MENU_RE = re.compile(r"^\[settings\] menu: (\w+\.\w+) (.*) -> (.*) \((applied now|at the next launch|"
                     r"saved; the environment keeps this run's value|invalid)\)$")

# How each row is changed in the menu phase and judged. presses: Right (+) or Left (-) presses
# from the default. picture: 'strict' (no visible change is a FAIL), 'lenient' (it can depend
# on the scene: INCONCLUSIVE). effect: a line logged between the change and its undo, judged
# only once `baseline` was logged before the change (else INCONCLUSIVE). absent: a line that
# must NOT appear between them. hold: seconds the change stays before it is put back.
SPEC = {
    'video.fullscreen': dict(presses=1, needs_fullscreen=True, hold=4,
                             effect=r'^\[video\] fullscreen on \(setting\)', baseline=r'^\[video\] '),
    'video.vsync': dict(presses=1, hold=6, effect=r'^\[video\] present mode \S+ \(V-Sync off',
                        baseline=r'^\[video\] present mode \S+ \(V-Sync on'),
    'video.frame_rate': dict(presses=1, hold=10, perf=True),
    # Lenient: on a 16:9 window Fill Screen and 16:9 draw the same picture.
    'video.aspect_ratio': dict(presses=1, picture='lenient'),
    'video.brightness': dict(presses=2, picture='strict'),
    'video.xbox_display_gamma': dict(presses=-1, picture='strict'),
    'video.anti_aliasing': dict(presses=1, picture='strict'),
    'video.scaling': dict(presses=1, picture='lenient'),
    'video.sharpening': dict(presses=14, picture='lenient'),
    'video.anisotropic_filtering': dict(presses=1, picture='lenient'),
    'video.mipmaps': dict(presses=1, hold=3, effect=r'^\[renderer\] mipmaps off:',
                          baseline=r'^\[renderer\] internal resolution '),
    # run_attempt.sh sets RECARBON_RESOLUTION: the run keeps it (no rescale), the menu's value is saved.
    'video.internal_resolution': dict(presses=1, hold=4, how=HOW_ENV, absent=r'^\[renderer\] internal resolution \S+ -> ',
                                      baseline=r'^\[renderer\] internal resolution '),
    # ReCarbon: HDR. Turning it on logs Auto's choice on any display (HDR or standard range: the
    # picture depends on the display, so it isn't judged). With HDR off by default, its other rows
    # change nothing visible: their menu lines and saved values are checked.
    'video.hdr': dict(presses=1, hold=4, effect=r'^\[video\] HDR: Auto chose ',
                      baseline=r'^\[video\] swapchain: '),
    'video.hdr_format': dict(presses=1),
    'video.hdr_strength': dict(presses=1),
    'video.hdr_paper_white': dict(presses=1),
    'audio.master_volume': dict(presses=-10, hold=3, effect=r'^\[audio\] gain 0\.2500 \(volume 50%',
                                baseline=r'^\[audio\] gain '),
    'audio.mute': dict(presses=1, hold=3, how=HOW_ENV, absent=r'^\[audio\] gain 0\.0000 \(volume \d+%, muted\)',
                       baseline=r'^\[audio\] gain '),
    'audio.mute_in_background': dict(presses=1, hold=3, focus=True,
                                     effect=r'^\[audio\] gain 0\.0000 \(volume \d+%, muted in the background\)',
                                     baseline=r'^\[audio\] gain '),
    'input.stick_deadzone': dict(presses=5),
    'input.trigger_deadzone': dict(presses=8),
    'input.vibration': dict(presses=1),
    'advanced.async_shaders': dict(presses=1),
    'advanced.performance_overlay': dict(presses=1, picture='strict', settle=2.0),
}
# At launch (the relaunch phase), with the persist phase's values stored.
RELAUNCH_LINES = {
    'input.stick_deadzone': r'^\[input\] stick deadzone {stick:.2f}, ',
    'input.trigger_deadzone': r'^\[input\] stick deadzone \S+, trigger deadzone {trigger:.2f}$',
    'audio.master_volume': r'^\[audio\] gain 0\.2500 \(volume 50%',
    'video.vsync': r'^\[video\] present mode \S+ \(V-Sync off',
}
CORRUPT_TOML = '[video]\nanti_aliasing = false\nsharpening = \n'
STILL_MAX = 8.0    # luma levels: a control above this is a moving scene
FLOOR = 1.0        # luma levels: below this nothing is visible


# --- the rows -------------------------------------------------------------------------------

def load_options(path=SETTINGS_JSON):
    """The menu's rows: website/src/data/settings.generated.json (generated from
    runtime/user/settings.cpp for the macOS build, hidden rows left out), in table order."""
    options = json.load(open(path))['options']
    for o in options:
        o['name'] = f"{o['table']}.{o['key']}"
    return options


def page_rows(options):
    rows = {p: [] for p in PAGES}
    for o in options:
        rows[o['category']].append(o['name'])
    return rows


def default_value(o):
    """The default as the menu steps it: a bool, an enum index or a number."""
    if o['type'] == 'Bool':
        return o['default'] == 'On'
    if o['type'] == 'Enum':
        return [v['token'] for v in o['values']].index(o['defaultToken'])
    return o['default']


def step(o, value, presses):
    """settings::StepValue applied `presses` times (one sign): a bool flips on each press, an
    enum and a number stop at their ends."""
    for _ in range(abs(presses)):
        d = 1 if presses > 0 else -1
        if o['type'] == 'Bool':
            value = not value
        elif o['type'] == 'Enum':
            value = min(max(value + d, 0), len(o['values']) - 1)
        else:
            value = round(min(max(value + d * o['step'], o['min']), o['max']), 6)
    return value


def label(o, value):
    """What the menu shows for a bool or enum value (numbers aren't compared by label)."""
    if o['type'] == 'Bool':
        return 'On' if value else 'Off'
    if o['type'] == 'Enum':
        return o['values'][value]['label']
    return None


def toml_value(o, value):
    """The value as tomllib reads settings.toml back."""
    if o['type'] == 'Enum':
        return o['values'][value]['token']
    if o['type'] == 'Int':
        return int(value)
    if o['type'] == 'Float':
        return float(value)
    return bool(value)


# --- the plan: RECARBON_UI_KEYS as a player would press them --------------------------------

class Program:
    """The keys with the menu's state tracked: its page survives closing it, and it opens on
    the page's first row."""

    def __init__(self, options, start):
        self.options = {o['name']: o for o in options}
        self.rows = page_rows(options)
        self.t = float(start)
        self.items = []
        self.page = 'Display'
        self.row = 0
        self.open = False
        self.shots = []         # labels, in the order of build/ui_shot_<n>.ppm
        self.expect = []        # dicts: option, step (forward/back), presses, how, from/to values
        self.values = {name: default_value(o) for name, o in self.options.items()}

    def key(self, name, gap=0.3):
        self.items.append(f'{self.t:.2f}:{name}')
        self.t += gap

    def wait(self, seconds):
        self.t += seconds

    def shot(self, name):
        self.shots.append(name)
        self.key('Shot', 0.6)

    def menu(self, page):
        if not self.open:
            self.key('F1', 1.2)
            self.open = True
            self.row = 0
        steps = (PAGES.index(page) - PAGES.index(self.page)) % len(PAGES)
        if steps:
            name, count = ('E', steps) if steps <= len(PAGES) // 2 else ('Q', len(PAGES) - steps)
            for _ in range(count):
                self.key(name, 0.8)
            self.page = page
            self.row = 0

    def select(self, option):
        page = self.options[option]['category']
        self.menu(page)
        target = self.rows[page].index(option)
        while self.row != target:
            self.key('Down' if target > self.row else 'Up', 0.3)
            self.row += 1 if target > self.row else -1

    def change(self, option, presses, kind, how=HOW_LIVE):
        """Each press that changes the value logs its own line: `steps` holds them."""
        o = self.options[option]
        self.select(option)
        before = value = self.values[option]
        steps = []
        for _ in range(abs(presses)):
            self.key('Right' if presses > 0 else 'Left', 0.25)
            nxt = step(o, value, 1 if presses > 0 else -1)
            if nxt != value:
                steps.append((value, nxt))
            value = nxt
        self.values[option] = value
        self.expect.append(dict(option=option, step=kind, presses=presses, how=how,
                                before=before, after=value, steps=steps))
        self.wait(0.3)

    def close(self, settle=1.2):
        if self.open:
            self.key('Escape', settle)
            self.open = False

    def env(self):
        return ','.join(self.items)


def rows_for(options, fullscreen):
    names = [o['name'] for o in options]
    unknown = [n for n in names if n not in SPEC]
    if unknown:  # a new row: say so rather than skip it
        raise SystemExit(f'verify_settings: no check for the row(s) {unknown}: add them to SPEC')
    return [n for n in names if fullscreen or not SPEC[n].get('needs_fullscreen')]


def plan_menu(options, start=40.0, fullscreen=False):
    p = Program(options, start)
    p.shot('base0')
    p.wait(1.0)
    p.shot('base1')
    # The control: the menu open as long as for a change, nothing changed.
    p.shot('noop_ref')
    for name in ('noop', 'noop_back'):
        p.menu('Graphics')
        p.wait(2.5)
        p.close()
        p.shot(name)
    for name in rows_for(options, fullscreen):
        s = SPEC[name]
        how = s.get('how', HOW_LIVE)
        if s.get('picture'):
            p.shot(f'{name}_ref')
            p.change(name, s['presses'], 'forward', how)
            p.close(s.get('settle', 1.2))
            p.shot(name)
            p.change(name, -s['presses'], 'back', how)
            p.close(s.get('settle', 1.2))
            p.shot(f'{name}_back')
            continue
        p.change(name, s['presses'], 'forward', how)
        p.close()
        if s.get('focus'):
            p.key('FocusLost', s['hold'])
            p.key('FocusGained', 1.5)
        else:
            p.wait(s.get('hold', 0))
        p.change(name, -s['presses'], 'back', how)
        p.close()
    p.shot('end')
    return p


def plan_persist(options, start=40.0, fullscreen=False):
    p = Program(options, start)
    for name in rows_for(options, fullscreen):
        p.change(name, SPEC[name]['presses'], 'forward', SPEC[name].get('how', HOW_LIVE))
    p.close(3.0)
    p.shot('end')
    return p


def plan_quiet(options, start=40.0, fullscreen=False):
    p = Program(options, start)
    p.shot('end')
    return p


def plan_relaunch(options, start=40.0, fullscreen=False):
    """No keys: the values to find are those the persist phase stored."""
    p = plan_quiet(options, start, fullscreen)
    p.values = plan_persist(options, start, fullscreen).values
    return p


PLANS = {'menu': plan_menu, 'persist': plan_persist, 'relaunch': plan_relaunch, 'corrupt': plan_quiet}


def plan_dict(phase, p, start, script):
    return dict(phase=phase, start=start, keys=p.env(), shots=p.shots, expect=p.expect, script=script or '',
                end=p.t, values={k: v for k, v in p.values.items()})


# --- pictures -------------------------------------------------------------------------------

def read_ppm(path, stride=4):
    """A binary PPM as a grid of luma samples (every `stride`th pixel each way)."""
    data = open(path, 'rb').read()
    fields, pos = [], 0
    while len(fields) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        end = pos
        while not data[end:end + 1].isspace():
            end += 1
        fields.append(data[pos:end])
        pos = end
    pos += 1
    if fields[0] != b'P6' or fields[3] != b'255':
        raise ValueError(f'{path}: not a binary 8-bit PPM')
    w, h = int(fields[1]), int(fields[2])
    grid = []
    for y in range(0, h, stride):
        row = data[pos + y * w * 3: pos + (y + 1) * w * 3]
        r, g, b = row[0::3 * stride], row[1::3 * stride], row[2::3 * stride]
        grid.append([(299 * a + 587 * c + 114 * d) // 1000 for a, c, d in zip(r, g, b)])
    return grid


def difference(a, b, block=8):
    """How different two captures are: the mean of the top 5% of blocks' mean absolute luma
    difference (a corner overlay and a whole-picture tone shift both register). Different
    sizes: 255."""
    if len(a) != len(b) or (a and len(a[0]) != len(b[0])):
        return 255.0
    blocks = []
    for by in range(0, len(a), block):
        for bx in range(0, len(a[0]), block):
            total = n = 0
            for y in range(by, min(by + block, len(a))):
                ra, rb = a[y], b[y]
                for x in range(bx, min(bx + block, len(ra))):
                    total += abs(ra[x] - rb[x])
                    n += 1
            blocks.append(total / n)
    blocks.sort(reverse=True)
    top = blocks[:max(1, len(blocks) // 20)]
    return sum(top) / len(top)


def picture_verdict(ref, changed, back, control, strict):
    """(verdict, detail) for one bracket, against the control (noop_ref, noop, noop_back)."""
    drift = max(difference(control[0], control[1]), difference(control[1], control[2]),
                difference(control[0], control[2]))
    if drift > STILL_MAX:
        return 'INCONCLUSIVE', f'the scene moves (control {drift:.1f})'
    restore = difference(ref, back)
    if restore > max(3 * drift, 2 * FLOOR):
        return 'FAIL', f'not put back: before vs after {restore:.1f}, control {drift:.1f}'
    effect = min(difference(changed, ref), difference(changed, back))
    floor = max(drift, restore)
    if effect >= max(3 * floor, FLOOR):
        return 'PASS', f'change {effect:.1f}, control {drift:.1f}, put back {restore:.1f}'
    if strict and effect < max(1.5 * floor, FLOOR):
        return 'FAIL', f'no visible change: {effect:.1f}, control {drift:.1f}'
    return 'INCONCLUSIVE', f'change {effect:.1f} too close to the control {drift:.1f}'


# --- the log --------------------------------------------------------------------------------

def menu_changes(lines):
    """(line index, option, from, to, how) for every [settings] menu change."""
    out = []
    for i, line in enumerate(lines):
        m = MENU_RE.match(line)
        if m:
            out.append((i,) + m.groups())
    return out


def match_expectations(lines, expect, options):
    """Each expectation's log lines (one per press that changed the value), in order:
    {position in expect: (first line index, last line index, first from, last to, the hows)},
    and the changes nothing expected. An expectation short of lines isn't found."""
    changes = menu_changes(lines)
    found, used, at = {}, set(), 0
    for k, e in enumerate(expect):
        mine = []
        for j in range(at, len(changes)):
            if len(mine) == max(1, len(e['steps'])):
                break
            if changes[j][1] == e['option']:
                mine.append(j)
        if len(mine) == max(1, len(e['steps'])):
            used.update(mine)
            at = mine[-1] + 1
            first, last = changes[mine[0]], changes[mine[-1]]
            found[k] = (first[0], last[0], first[2], last[3], {changes[j][4] for j in mine})
    extra = [changes[j] for j in range(len(changes)) if j not in used]
    return found, extra


def phase_problem(lines):
    """A launch whose keys never reached the menu: one diagnosis, not a FAIL per row."""
    unknown = [l for l in lines if l.startswith('[ui] RECARBON_UI_KEYS: unknown key')]
    if unknown:
        return unknown[0]
    if not any(l.startswith('[ui] ready') for l in lines):
        return 'the UI never became ready ([ui] ready)'
    return None


def navigation_results(lines, expect, options):
    """Per row: the forward change logged with its value and how, and (when planned) the undo
    back to the value before."""
    opts = {o['name']: o for o in options}
    found, extra = match_expectations(lines, expect, options)
    results = []
    for k, e in enumerate(expect):
        o = opts[e['option']]
        got = found.get(k)
        if got is None:
            results.append((e['option'], f"menu {e['step']}", 'FAIL', 'no [settings] menu line'))
            continue
        _, _, frm, to, hows = got
        how = ', '.join(sorted(hows))
        want = label(o, e['after'])
        problems = []
        if hows != {e['how']}:
            problems.append(f'({how}), expected ({e["how"]})')
        if want is not None and to != want:
            problems.append(f'shows {to}, expected {want}')
        if frm == to:
            problems.append('no change')
        if e['step'] == 'back':
            fwd = next((found.get(j) for j in range(k - 1, -1, -1)
                        if expect[j]['option'] == e['option'] and expect[j]['step'] == 'forward'), None)
            if fwd is not None and to != fwd[2]:
                problems.append(f'put back to {to}, was {fwd[2]}')
        results.append((e['option'], f"menu {e['step']}", 'FAIL' if problems else 'PASS',
                        '; '.join(problems) or f'{frm} -> {to} ({how})'))
    if extra:
        results.append(('(menu)', 'unexpected changes', 'FAIL',
                        '; '.join(f'{c[1]} {c[2]} -> {c[3]}' for c in extra[:5])))
    return results, found


def effect_results(lines, expect, found):
    """The rows judged by a logged effect, between each forward change and its undo."""
    results = []
    for k, e in enumerate(expect):
        s = SPEC[e['option']]
        if e['step'] != 'forward' or not any(s.get(x) for x in ('effect', 'absent', 'perf')):
            continue
        back = next((j for j in range(k + 1, len(expect)) if expect[j]['option'] == e['option']), None)
        if k not in found or back is None or back not in found:
            results.append((e['option'], 'effect', 'INCONCLUSIVE', 'the change or its undo was not logged'))
            continue
        lo, hi = found[k][1], found[back][0]   # after the last forward press, before the first undo
        window = lines[lo + 1:hi]
        if s.get('perf'):
            results.append((e['option'], 'effect', *perf_verdict(lines[:found[k][0]], window)))
            continue
        if not any(re.match(s['baseline'], l) for l in lines[:found[k][0]]):
            results.append((e['option'], 'effect', 'INCONCLUSIVE', f'no baseline line ({s["baseline"]}) before the change'))
            continue
        if s.get('absent'):
            hit = next((l for l in window if re.match(s['absent'], l)), None)
            results.append((e['option'], 'effect', 'FAIL' if hit else 'PASS',
                            f'changed the run: {hit}' if hit else 'the run kept its value'))
            continue
        hit = next((l for l in window if re.match(s['effect'], l)), None)
        results.append((e['option'], 'effect', 'PASS' if hit else 'FAIL', hit or f'no line matching {s["effect"]}'))
    return results


PERF_RE = re.compile(r'^\[perf\] (\d+(?:\.\d+)?) fps')


def perf_verdict(before, window):
    """Frame Rate 30: the [perf] rate (every 120 frames) falls. The first line in the window
    straddles the change. Lenient: whether the game's own rate follows depends on the scene."""
    base = [float(m.group(1)) for m in map(PERF_RE.match, before) if m][-3:]
    after = [float(m.group(1)) for m in map(PERF_RE.match, window) if m][1:]
    if len(base) < 2 or statistics.median(base) < 50:
        return 'INCONCLUSIVE', f'no steady 60 before the change ({base})'
    if not after:
        return 'INCONCLUSIVE', 'no [perf] line while it was set'
    if statistics.median(after) <= 35:
        return 'PASS', f'{statistics.median(base):.1f} -> {statistics.median(after):.1f} fps'
    return 'INCONCLUSIVE', f'the rate stayed at {statistics.median(after):.1f} fps'


# --- settings.toml --------------------------------------------------------------------------

def toml_results(toml_path, options, values, check, missing_ok=False):
    """Each row's stored value against `values` (the plan's, as the menu stepped them)."""
    import tomllib
    if not os.path.exists(toml_path):
        return [('(settings.toml)', check, 'INCONCLUSIVE' if missing_ok else 'FAIL', f'no {toml_path}')]
    try:
        data = tomllib.load(open(toml_path, 'rb'))
    except tomllib.TOMLDecodeError as e:
        return [('(settings.toml)', check, 'FAIL', f"doesn't parse: {e}")]
    results = []
    for o in options:
        if o['name'] not in values:
            continue
        want = toml_value(o, values[o['name']])
        got = data.get(o['table'], {}).get(o['key'])
        if got is None and want == toml_value(o, default_value(o)):
            continue  # a default needn't be written
        ok = abs(got - want) < 1e-6 if isinstance(want, float) and isinstance(got, (int, float)) else got == want
        if not ok:
            results.append((o['name'], check, 'FAIL', f'settings.toml has {got!r}, expected {want!r}'))
    if not results:
        results.append(('(settings.toml)', check, 'PASS', f'{len(values)} rows as expected'))
    return results


# --- analysis of one phase ------------------------------------------------------------------

def analyze_phase(plan, log_text, shots_dir, toml_path, options):
    phase = plan['phase']
    lines = log_text.splitlines()
    problem = phase_problem(lines)
    if problem:
        return [(f'({phase})', 'launch', 'FAIL', problem)]
    expect = plan['expect']
    opts = {o['name']: o for o in options}
    results = []
    if phase in ('menu', 'persist'):
        nav, found = navigation_results(lines, expect, options)
        results += nav
        if phase == 'menu':
            results += effect_results(lines, expect, found)
            results += picture_results(plan, shots_dir)
            results += toml_results(toml_path, options, {e['option']: default_value(opts[e['option']]) for e in expect},
                                    'put back in settings.toml', missing_ok=True)
        else:
            results += toml_results(toml_path, options, plan['values'], 'stored in settings.toml')
    elif phase == 'relaunch':
        loaded = any(re.match(r'^\[settings\] .*settings\.toml: \d+ values? from the file', l) for l in lines)
        results.append(('(settings.toml)', 'loaded at launch', 'PASS' if loaded else 'FAIL',
                        'read' if loaded else 'no "values from the file" line'))
        values = plan['values']
        fields = dict(stick=values.get('input.stick_deadzone', 0), trigger=values.get('input.trigger_deadzone', 0))
        for name, rx in RELAUNCH_LINES.items():
            if name not in values:
                continue
            rx = rx.format(**fields)
            hit = next((l for l in lines if re.match(rx, l)), None)
            results.append((name, 'at launch', 'PASS' if hit else 'FAIL', hit or f'no line matching {rx}'))
        results += toml_results(toml_path, options, values, 'kept in settings.toml')
    elif phase == 'corrupt':
        said = next((l for l in lines if "doesn't parse" in l and 'defaults (kept as' in l), None)
        results.append(('(settings.toml)', 'corrupt file reported', 'PASS' if said else 'FAIL', said or 'no "doesn\'t parse" line'))
        bad = str(toml_path) + '.bad'
        kept = os.path.exists(bad) and open(bad).read() == CORRUPT_TOML
        results.append(('(settings.toml)', 'kept as .bad', 'PASS' if kept else 'FAIL', bad if kept else f'no copy at {bad}'))
        ran = any(l.startswith('[run_attempt] deadline') for l in lines) and any(re.match(r'^exit=(0|143)$', l) for l in lines)
        results.append(('(game)', 'runs with defaults', 'PASS' if ran else 'FAIL', 'ran to the deadline' if ran else 'ended early'))
    return results


def picture_results(plan, shots_dir):
    shots = plan['shots']
    path = {name: os.path.join(shots_dir, f'ui_shot_{i}.ppm') for i, name in enumerate(shots)}
    missing = [n for n in shots if not os.path.exists(path[n])]
    rows = [e['option'] for e in plan['expect'] if e['step'] == 'forward' and SPEC[e['option']].get('picture')]
    if missing:
        return [(r, 'picture', 'INCONCLUSIVE', f'{len(missing)} capture(s) missing, first {missing[0]}') for r in rows]
    cache = {}

    def img(name):
        if name not in cache:
            cache[name] = read_ppm(path[name])
        return cache[name]
    control = [img('noop_ref'), img('noop'), img('noop_back')]
    out = []
    for r in rows:
        verdict, detail = picture_verdict(img(f'{r}_ref'), img(r), img(f'{r}_back'), control,
                                          SPEC[r]['picture'] == 'strict')
        out.append((r, 'picture', verdict, detail))
    return out


def toml_path_of(home):
    return os.path.join(home, 'Library', 'Application Support', 'ReCarbon', 'settings.toml')


def analyze(out, options=None):
    options = options or load_options()
    results = []
    for phase in PLANS:
        d = os.path.join(out, phase)
        if not os.path.exists(os.path.join(d, 'plan.json')):
            continue
        plan = json.load(open(os.path.join(d, 'plan.json')))
        log = open(os.path.join(d, 'game.log'), encoding='utf-8', errors='replace').read() \
            if os.path.exists(os.path.join(d, 'game.log')) else ''
        home = plan.get('home') or os.path.join(d, 'home')
        for r in analyze_phase(plan, log, os.path.join(d, 'shots'), toml_path_of(home), options):
            results.append((phase,) + r)
    return results


def report(results, out):
    with open(os.path.join(out, 'results.tsv'), 'w') as f:
        f.write('phase\trow\tcheck\tverdict\tdetail\n')
        for r in results:
            f.write('\t'.join(r) + '\n')
    for phase, row, check, verdict, detail in results:
        print(f'{verdict:12s} {phase:8s} {row:30s} {check:26s} {detail}')
    counts = {v: sum(1 for r in results if r[3] == v) for v in ('PASS', 'FAIL', 'INCONCLUSIVE')}
    print(f"---- {counts['PASS']} PASS, {counts['FAIL']} FAIL, {counts['INCONCLUSIVE']} INCONCLUSIVE ----")
    return 1 if counts['FAIL'] or not results else 0


# --- running the game -----------------------------------------------------------------------

def run(args, options):
    out = os.path.abspath(args.out)
    if os.path.exists(out) and os.listdir(out):
        raise SystemExit(f'verify_settings: {out} is not empty (results go to a new folder)')
    shots = sorted(pathlib.Path(ROOT, 'build').glob('ui_shot_*.ppm'))
    if shots:
        raise SystemExit(f'verify_settings: move build/ui_shot_*.ppm away first ({len(shots)} there)')
    if not os.access(ROOT / 'build' / 'main' / 'runtime' / 'ReCarbon', os.X_OK):
        raise SystemExit('verify_settings: no build/main/runtime/ReCarbon')
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, '.retention.json'), 'w') as f:
        json.dump({'owner': 'recarbon/verify_settings.py', 'project': 'recarbon', 'class': 'experiment',
                   'status': 'active', 'created_at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                   'note': 'settings verification: logs, captures, user folders'}, f)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    for phase in args.phases:
        d = os.path.join(out, phase)
        home = os.path.join(out, 'persist', 'home') if phase == 'relaunch' else os.path.join(d, 'home')
        os.makedirs(os.path.join(d, 'shots'), exist_ok=True)
        os.makedirs(os.path.join(home, 'Library', 'Application Support', 'ReCarbon'), exist_ok=True)
        if phase == 'corrupt':
            with open(toml_path_of(home), 'w') as f:
                f.write(CORRUPT_TOML)
        p = PLANS[phase](options, args.start, args.fullscreen)
        plan = plan_dict(phase, p, args.start, args.script)
        plan['home'] = home
        secs = int(p.t + 30 + len(p.shots))
        with open(os.path.join(d, 'plan.json'), 'w') as f:
            json.dump(plan, f, indent=1)
        name = f'vs-{phase}-{stamp}'
        env = {k: v for k, v in os.environ.items() if not k.startswith(('RECARBON_', 'RUN_', 'BENCH_'))}
        env.update(RUN_HOME=home, RUN_RESOLUTION=args.resolution, RECARBON_UI_KEYS=p.env(),
                   RECARBON_MUTE='0', SDL_AUDIO_DRIVER='dummy')
        if args.script:
            env['RECARBON_INPUT_SCRIPT'] = args.script
        print(f'verify_settings: {phase}: {secs} s, {len(p.shots)} captures', flush=True)
        subprocess.run(['bash', 'scripts/run_attempt.sh', name, str(secs)], cwd=ROOT, env=env, check=False)
        log = ROOT / 'game' / 'm3' / f'{name}.log'
        if log.exists():
            shutil.copy(log, os.path.join(d, 'game.log'))
        for s in sorted(pathlib.Path(ROOT, 'build').glob('ui_shot_*.ppm')):
            shutil.move(str(s), os.path.join(d, 'shots', s.name))
    return report(analyze(out, options), out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run')
    r.add_argument('--out', required=True)
    r.add_argument('--phases', default='menu,persist,relaunch,corrupt')
    r.add_argument('--start', type=float, default=40.0)
    r.add_argument('--script', default='')
    r.add_argument('--resolution', default='1x', choices=['1x', '2x', '3x'])
    r.add_argument('--fullscreen', action='store_true')
    a = sub.add_parser('analyze')
    a.add_argument('out')
    pl = sub.add_parser('plan')
    pl.add_argument('--phase', default='menu', choices=list(PLANS))
    pl.add_argument('--start', type=float, default=40.0)
    pl.add_argument('--fullscreen', action='store_true')
    args = ap.parse_args(argv)
    options = load_options()
    if args.cmd == 'plan':
        p = PLANS[args.phase](options, args.start, args.fullscreen)
        print(json.dumps(plan_dict(args.phase, p, args.start, ''), indent=1))
        return 0
    if args.cmd == 'analyze':
        return report(analyze(args.out, options), args.out)
    args.phases = [x for x in args.phases.split(',') if x]
    bad = [x for x in args.phases if x not in PLANS]
    if bad:
        raise SystemExit(f'verify_settings: unknown phase(s) {bad}')
    if 'relaunch' in args.phases and 'persist' not in args.phases:
        raise SystemExit('verify_settings: relaunch reads the persist phase\'s user folder: run them together')
    return run(args, options)


if __name__ == '__main__':
    sys.exit(main())
