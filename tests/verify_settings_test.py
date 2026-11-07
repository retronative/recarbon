#!/usr/bin/env python3
# ReCarbon tests. GPL-3.0-or-later (see COPYING).
#
# scripts/verify_settings.py offline: its menu plans replayed through an independent model of
# the settings menu (pages, rows, how each type steps), and its analysis of synthetic runs
# (logs, captures, settings.toml): a run where everything applied passes, and each kind of
# fault (no effect line, a row that didn't change, a change not put back, the wrong row, a
# moving scene, keys before the UI) gives its own verdict. No game is launched.
# From the repo root: python3 tests/verify_settings_test.py (ctest runs it).
import contextlib, importlib.util, io, json, os, random, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
spec = importlib.util.spec_from_file_location('verify_settings', os.path.join(ROOT, 'scripts', 'verify_settings.py'))
vs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vs)
failures = 0


def silent(f, *args):
    with contextlib.redirect_stdout(io.StringIO()):
        return f(*args)


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print(f'FAIL: {what}')


OPTIONS = json.load(open(os.path.join(ROOT, 'website', 'src', 'data', 'settings.generated.json')))['options']
for o in OPTIONS:
    o['name'] = f"{o['table']}.{o['key']}"
BY_NAME = {o['name']: o for o in OPTIONS}
MENU_PAGES = ['Display', 'Graphics', 'Audio', 'Input', 'Controls', 'Controller', 'Advanced']


# ---- an independent model of the menu (runtime/ui/settings_menu.cpp) ----
class Menu:
    """F1 opens on the last page's first row; Q/E step pages (wrapping); Up/Down move the
    cursor; Left/Right step the row's value (a bool flips, an enum or a number stops at its
    ends); Escape closes. Returns the changes the menu would log."""

    def __init__(self):
        self.page, self.row, self.open = 0, 0, False
        self.values = {}
        for o in OPTIONS:
            if o['type'] == 'Bool':
                self.values[o['name']] = o['default'] == 'On'
            elif o['type'] == 'Enum':
                self.values[o['name']] = next(i for i, v in enumerate(o['values']) if v['token'] == o['defaultToken'])
            else:
                self.values[o['name']] = o['default']
        self.changes = []

    def rows(self):
        return [o for o in OPTIONS if o['category'] == MENU_PAGES[self.page]]

    def press(self, key):
        if key == 'F1':
            self.open = not self.open
            self.row = 0
        elif not self.open:
            return
        elif key == 'Escape':
            self.open = False
        elif key in ('Q', 'E'):
            self.page = (self.page + (1 if key == 'E' else -1)) % len(MENU_PAGES)
            self.row = 0
        elif key in ('Up', 'Down'):
            self.row = max(0, min(len(self.rows()) - 1, self.row + (1 if key == 'Down' else -1)))
        elif key in ('Left', 'Right'):
            o = self.rows()[self.row]
            d = 1 if key == 'Right' else -1
            v = self.values[o['name']]
            if o['type'] == 'Bool':
                n = not v
            elif o['type'] == 'Enum':
                n = max(0, min(len(o['values']) - 1, v + d))
            else:
                n = round(max(o['min'], min(o['max'], v + d * o['step'])), 6)
            if n != v:
                self.changes.append((o['name'], v, n))
                self.values[o['name']] = n


def replay(keys):
    m = Menu()
    times = []
    for item in keys.split(','):
        t, name = item.split(':', 1)
        times.append(float(t))
        if name not in ('Shot', 'FocusLost', 'FocusGained'):
            m.press(name)
    return m, times


for phase in ('menu', 'persist'):
    for fullscreen in (False, True):
        p = vs.PLANS[phase](vs.load_options(), 40.0, fullscreen)
        m, times = replay(p.env())
        want = [(e['option'], a, b) for e in p.expect for a, b in e['steps']]
        check(m.changes == want, f'{phase} (fullscreen {fullscreen}): the keys make exactly the planned changes '
                                 f'({len(m.changes)} vs {len(want)}; first difference '
                                 f'{next(((a, b) for a, b in zip(m.changes, want) if a != b), None)})')
        check(not m.open, f'{phase}: the menu is closed at the end')
        check(times == sorted(times) and times[0] >= 40.0, f'{phase}: the keys are in time order from --start')
        changed = {e['option'] for e in p.expect}
        check(('video.fullscreen' in changed) == fullscreen, f'{phase}: Fullscreen changes only with --fullscreen')
        rows = {o['name'] for o in OPTIONS if fullscreen or o['name'] != 'video.fullscreen'}
        check(changed == rows, f'{phase}: every row is changed ({sorted(rows - changed)} missing)')
        if phase == 'menu':
            check(m.values == Menu().values, 'menu: every row is put back')
            check(all(p.shots.count(s) == 1 for s in p.shots), 'menu: capture labels are unique')

# A row the plan has no check for stops it (a new setting must be added, not skipped).
extra = vs.load_options() + [dict(OPTIONS[0], key='new_row', name='video.new_row')]
try:
    vs.plan_menu(extra)
    check(False, 'a row with no check is refused')
except SystemExit as e:
    check('video.new_row' in str(e), f'a row with no check is named ({e})')


# ---- synthetic runs ----
def fmt(o, v):
    lab = vs.label(o, v)
    return lab if lab is not None else (f'{v:.2f}' if o['type'] == 'Float' else str(v))


EFFECTS = {  # what the game logs right after each forward change
    'video.vsync': ['[video] present mode IMMEDIATE (V-Sync off)'],
    'video.mipmaps': ['[renderer] mipmaps off: 120 textures dropped in 1.0 ms (loaded again as they are drawn)'],
    'audio.master_volume': ['[audio] gain 0.2500 (volume 50%) from 50.00 s of output'],
    'audio.mute_in_background': ['[audio] gain 0.0000 (volume 100%, muted in the background) from 60.00 s of output'],
    'video.frame_rate': ['[perf] 31.0 fps (worst frame 40.0 ms) | ...', '[perf] 30.0 fps (worst frame 34.0 ms) | ...',
                         '[perf] 30.1 fps (worst frame 34.0 ms) | ...'],
    'video.fullscreen': ['[video] fullscreen on (setting)'],
    'video.hdr': ['[video] HDR: Auto chose standard range (the display reports no HDR)'],  # ReCarbon: HDR
}
HEADER = ['[ui] ready (Dear ImGui 1.91): F1 or Back + Start opens the settings',
          '[renderer] internal resolution 2x (2560x1440, Auto; surface pitches 640-1280)',
          '[video] swapchain: SDR (B8G8R8A8_UNORM, sRGB; 60 offered)',  # ReCarbon: HDR's baseline
          '[video] present mode FIFO (V-Sync on)', '[audio] gain 1.0000 (volume 100%) from 0.00 s of output',
          '[perf] 60.0 fps (worst frame 17.0 ms) | ...', '[perf] 60.0 fps (worst frame 17.0 ms) | ...',
          '[perf] 59.9 fps (worst frame 17.0 ms) | ...']


def make_log(p, drop=(), how_override=None, skip_back=None, extra_change=None, header=HEADER):
    lines = list(header)
    for e in p.expect:
        o = BY_NAME[e['option']]
        if e['step'] == 'back' and e['option'] == skip_back:
            continue
        how = (how_override or {}).get(e['option'], e['how'])
        for a, b in e['steps']:  # one line per press, as the menu logs them
            lines.append(f"[settings] menu: {e['option']} {fmt(o, a)} -> {fmt(o, b)} ({how})")
        if e['step'] == 'forward' and e['option'] not in drop:
            lines += EFFECTS.get(e['option'], [])
        if extra_change and e['option'] == extra_change:
            lines.append('[settings] menu: video.anti_aliasing On -> Off (applied now)')
    lines += ['[settings] saved /x/settings.toml', '[run_attempt] deadline: SIGTERM', 'exit=0']
    return '\n'.join(lines) + '\n'


W, H = 96, 64


def write_ppm(path, rng, bump=None, noise=1, shift=0, amp=50):
    px = bytearray()
    for y in range(H):
        for x in range(W):
            v = 60 + (x + y) % 90 + shift + rng.randint(-noise, noise)
            if bump and bump[0] <= x < bump[1] and bump[2] <= y < bump[3]:
                v += amp
            v = max(0, min(255, v))
            px += bytes((v, v, v))
    with open(path, 'wb') as f:
        f.write(f'P6\n{W} {H}\n255\n'.encode() + bytes(px))


def make_shots(d, p, no_effect=(), not_back=(), moving=False):
    """The change: +50 in one area of the row's capture. Not put back: +15 elsewhere in the
    capture after. Moving: the control's captures (only) shift, as a scene that doesn't hold."""
    os.makedirs(d, exist_ok=True)
    rng = random.Random(7)
    for i, name in enumerate(p.shots):
        row = name if name in vs.SPEC else None
        path = os.path.join(d, f'ui_shot_{i}.ppm')
        shift = (i * 13) % 40 if moving and name.startswith('noop') else 0
        if row and row not in no_effect:
            write_ppm(path, rng, bump=(8, 40, 8, 30), shift=shift)
        elif name.endswith('_back') and name[:-5] in not_back:
            write_ppm(path, rng, bump=(40, 96, 30, 64), shift=shift, amp=15)
        else:
            write_ppm(path, rng, shift=shift)


def results_of(tmp, p, phase, log, shots=True, toml=None, **shot_args):
    d = os.path.join(tmp, f'{phase}-{len(os.listdir(tmp))}')
    os.makedirs(d)
    plan = vs.plan_dict(phase, p, 40.0, '')
    if shots:
        make_shots(os.path.join(d, 'shots'), p, **shot_args)
    tp = os.path.join(d, 'settings.toml')
    if toml is not None:
        open(tp, 'w').write(toml)
    return vs.analyze_phase(plan, log, os.path.join(d, 'shots'), tp, vs.load_options())


def verdicts(results, row=None, check_=None):
    return [r[2] for r in results if (row is None or r[0] == row) and (check_ is None or r[1] == check_)]


def toml_for(values, default_for=()):
    tables = {}
    for name, v in values.items():
        o = BY_NAME[name]
        if name in default_for:
            v = vs.default_value(o)
        t = vs.toml_value(o, v)
        tables.setdefault(o['table'], []).append(f"{o['key']} = {json.dumps(t) if isinstance(t, (str, bool)) else t}")
    return ''.join(f'[{t}]\n' + '\n'.join(rows) + '\n' for t, rows in tables.items())


with tempfile.TemporaryDirectory() as tmp:
    menu = vs.plan_menu(vs.load_options())
    defaults_toml = toml_for({e['option']: None for e in menu.expect}, default_for={e['option'] for e in menu.expect})

    # ---- everything applied: no FAIL; the scene-dependent rows may be INCONCLUSIVE only ----
    good = results_of(tmp, menu, 'menu', make_log(menu), toml=defaults_toml)
    bad = [r for r in good if r[2] != 'PASS']
    check(not bad, f'a run where everything applied: all PASS ({bad[:4]})')
    # Eight: Expanded View (the aspect_ratio row) is shown again, judged leniently.
    check(len(verdicts(good, check_='picture')) == 8, 'menu: eight picture rows are judged')
    check(len(verdicts(good, check_='effect')) == 8, 'menu: eight rows are judged by a log line')

    # ---- one effect line missing: exactly that row FAILs ----
    for row in ('video.mipmaps', 'video.vsync', 'audio.master_volume', 'audio.mute_in_background'):
        r = results_of(tmp, menu, 'menu', make_log(menu, drop=(row,)), toml=defaults_toml)
        fails = [x for x in r if x[2] == 'FAIL']
        check([x[0] for x in fails] == [row], f'{row} logs no effect: only it FAILs ({fails})')
    r = results_of(tmp, menu, 'menu', make_log(menu, drop=('video.frame_rate',)), toml=defaults_toml)
    check(verdicts(r, 'video.frame_rate', 'effect') == ['INCONCLUSIVE'] and 'FAIL' not in verdicts(r),
          'Frame Rate with no [perf] change: INCONCLUSIVE (lenient)')
    still60 = make_log(menu).replace('[perf] 31.0 fps', '[perf] 60.0 fps').replace('[perf] 30.0 fps', '[perf] 59.8 fps') \
                            .replace('[perf] 30.1 fps', '[perf] 60.1 fps')
    r = results_of(tmp, menu, 'menu', still60, toml=defaults_toml)
    check(verdicts(r, 'video.frame_rate', 'effect') == ['INCONCLUSIVE'], 'Frame Rate 30 while the game stays at 60: not a PASS')

    # ---- no baseline before the change: INCONCLUSIVE, not FAIL ----
    no_base = [l for l in HEADER if not l.startswith('[renderer] internal resolution')]
    r = results_of(tmp, menu, 'menu', make_log(menu, header=no_base), toml=defaults_toml)
    check(verdicts(r, 'video.internal_resolution', 'effect') == ['INCONCLUSIVE'] and verdicts(r, 'video.mipmaps', 'effect') == ['INCONCLUSIVE'],
          f'no renderer line: Internal Resolution and Mipmaps INCONCLUSIVE ({verdicts(r, "video.internal_resolution")})')
    # Internal Resolution is overridden by run_attempt.sh's RECARBON_RESOLUTION: a rescale is a FAIL.
    rescaled = make_log(menu).replace("Auto -> HD (720p) (saved; the environment keeps this run's value)\n",
                                      "Auto -> HD (720p) (saved; the environment keeps this run's value)\n"
                                      '[renderer] internal resolution 2x -> 1x (1280x720, 1x): 40 targets replaced in 2.0 ms\n', 1)
    check(rescaled != make_log(menu), 'the rescale case changed the synthetic log (its label still matches the menu)')
    r = results_of(tmp, menu, 'menu', rescaled, toml=defaults_toml)
    check(verdicts(r, 'video.internal_resolution', 'effect') == ['FAIL'], 'Internal Resolution rescaled a RECARBON_RESOLUTION run: FAIL')
    r = results_of(tmp, menu, 'menu', make_log(menu, how_override={'video.internal_resolution': 'applied now'}), toml=defaults_toml)
    check('FAIL' in verdicts(r, 'video.internal_resolution', 'menu forward'), 'Internal Resolution applied despite RECARBON_RESOLUTION: FAIL')
    no_audio = [l for l in HEADER if not l.startswith('[audio]')]
    r = results_of(tmp, menu, 'menu', make_log(menu, drop=('audio.master_volume',), header=no_audio), toml=defaults_toml)
    check(verdicts(r, 'audio.master_volume', 'effect') == ['INCONCLUSIVE'], 'no audio gain before: Master Volume INCONCLUSIVE')
    slow = [l for l in HEADER if not l.startswith('[perf]')] + ['[perf] 31.0 fps | ...'] * 3
    r = results_of(tmp, menu, 'menu', make_log(menu, header=slow), toml=defaults_toml)
    check(verdicts(r, 'video.frame_rate', 'effect') == ['INCONCLUSIVE'], 'no steady 60 before: Frame Rate INCONCLUSIVE')

    # ---- Mute overridden by the environment ----
    r = results_of(tmp, menu, 'menu', make_log(menu, how_override={'audio.mute': 'applied now'}), toml=defaults_toml)
    check('FAIL' in verdicts(r, 'audio.mute', 'menu forward'), 'Mute applied despite RECARBON_MUTE: FAIL')
    log = make_log(menu).replace("audio.mute Off -> On (saved; the environment keeps this run's value)\n",
                                 "audio.mute Off -> On (saved; the environment keeps this run's value)\n"
                                 '[audio] gain 0.0000 (volume 100%, muted) from 70.00 s of output\n')
    r = results_of(tmp, menu, 'menu', log, toml=defaults_toml)
    check(verdicts(r, 'audio.mute', 'effect') == ['FAIL'], 'Mute changed the overridden run: FAIL')

    # ---- navigation faults ----
    r = results_of(tmp, menu, 'menu', make_log(menu, skip_back='input.vibration'), toml=defaults_toml)
    check(verdicts(r, 'input.vibration', 'menu back') == ['FAIL'], 'Vibration never put back: FAIL')
    r = results_of(tmp, menu, 'menu', make_log(menu, extra_change='input.vibration'), toml=defaults_toml)
    check(verdicts(r, '(menu)') == ['FAIL'], 'a row nothing planned changed: FAIL')
    wrong = make_log(menu).replace('advanced.async_shaders On -> Off', 'advanced.async_shaders On -> On')
    r = results_of(tmp, menu, 'menu', wrong, toml=defaults_toml)
    check(verdicts(r, 'advanced.async_shaders', 'menu forward') == ['FAIL'], 'a change that shows the old value: FAIL')
    back_wrong = make_log(menu).replace('video.anisotropic_filtering Off -> Auto', 'video.anisotropic_filtering Off -> 2x')
    r = results_of(tmp, menu, 'menu', back_wrong, toml=defaults_toml)
    check('FAIL' in verdicts(r, 'video.anisotropic_filtering', 'menu back'), 'put back to another value: FAIL')
    num_back = make_log(menu).replace('video.brightness 1 -> 0 (', 'video.brightness 1 -> -1 (')
    r = results_of(tmp, menu, 'menu', num_back, toml=defaults_toml)
    check(verdicts(r, 'video.brightness', 'menu back') == ['FAIL'], 'a number put back to another value: FAIL')
    r = results_of(tmp, menu, 'menu', make_log(menu, header=['[ui] RECARBON_UI_KEYS: unknown key Bogus'] + HEADER))
    check(len(r) == 1 and r[0][2] == 'FAIL' and 'unknown key' in r[0][3], 'an unknown key: one launch FAIL')
    r = results_of(tmp, menu, 'menu', make_log(menu, header=HEADER[1:]))
    check(len(r) == 1 and r[0][2] == 'FAIL' and 'never became ready' in r[0][3], 'no UI: one launch FAIL')

    # ---- pictures ----
    r = results_of(tmp, menu, 'menu', make_log(menu), toml=defaults_toml, no_effect=('video.brightness', 'video.sharpening'))
    check(verdicts(r, 'video.brightness', 'picture') == ['FAIL'], 'Brightness with no visible change: FAIL (strict)')
    check(verdicts(r, 'video.sharpening', 'picture') == ['INCONCLUSIVE'], 'Sharpening with no visible change: INCONCLUSIVE (lenient)')
    r = results_of(tmp, menu, 'menu', make_log(menu), toml=defaults_toml, not_back=('video.anti_aliasing',))
    check(verdicts(r, 'video.anti_aliasing', 'picture') == ['FAIL'], 'a picture not put back: FAIL')
    r = results_of(tmp, menu, 'menu', make_log(menu), toml=defaults_toml, moving=True)
    check(set(verdicts(r, check_='picture')) == {'INCONCLUSIVE'}, 'a moving scene: every picture row INCONCLUSIVE')
    r = results_of(tmp, menu, 'menu', make_log(menu), shots=False, toml=defaults_toml)
    check(set(verdicts(r, check_='picture')) == {'INCONCLUSIVE'}, 'no captures: picture rows INCONCLUSIVE')
    rng = random.Random(1)
    a, b = os.path.join(tmp, 'a.ppm'), os.path.join(tmp, 'b.ppm')
    write_ppm(a, rng)
    write_ppm(b, rng, bump=(0, 32, 0, 32))
    check(vs.difference(vs.read_ppm(a), vs.read_ppm(a)) == 0, 'difference of a capture with itself is 0')
    check(vs.difference(vs.read_ppm(a), vs.read_ppm(b)) > 20, 'a small corner change registers (top blocks)')

    # ---- settings.toml ----
    r = results_of(tmp, menu, 'menu', make_log(menu), toml=defaults_toml.replace('anti_aliasing = true', 'anti_aliasing = false'))
    check(verdicts(r, 'video.anti_aliasing', 'put back in settings.toml') == ['FAIL'], 'a row left changed in settings.toml: FAIL')
    persist = vs.plan_persist(vs.load_options())
    stored = toml_for(persist.values)
    r = results_of(tmp, persist, 'persist', make_log(persist), shots=False, toml=stored)
    check([x for x in r if x[2] != 'PASS'] == [], f'persist: every value stored: PASS ({[x for x in r if x[2] != "PASS"][:3]})')
    check('sharpening = 1.0' in stored and 'frame_rate = "30"' in stored and 'master_volume = 50' in stored,
          f'persist: the targets are the stepped values ({stored[:200]!r})')
    r = results_of(tmp, persist, 'persist', make_log(persist), shots=False, toml=stored.replace('frame_rate = "30"', 'frame_rate = "60"'))
    check(verdicts(r, 'video.frame_rate') == ['PASS', 'FAIL'], 'persist: a value not stored: FAIL')
    r = results_of(tmp, persist, 'persist', make_log(persist), shots=False, toml=stored.replace('sharpening = 1.0', 'sharpening = 0.5'))
    check(verdicts(r, 'video.sharpening') == ['PASS', 'FAIL'], 'persist: a number not stored: FAIL')
    r = results_of(tmp, persist, 'persist', make_log(persist), shots=False, toml=None)
    check('FAIL' in verdicts(r, '(settings.toml)'), 'persist: no settings.toml: FAIL')
    r = results_of(tmp, persist, 'persist', make_log(persist), shots=False, toml='[video\n')
    check('FAIL' in verdicts(r, '(settings.toml)'), 'persist: a settings.toml that does not parse: FAIL')

    # ---- relaunch and corrupt ----
    quiet = vs.plan_quiet(vs.load_options())
    relaunch = vs.PLANS['relaunch'](vs.load_options())
    check(relaunch.values == persist.values and all(i.endswith(':Shot') for i in relaunch.items), 'relaunch: no keys but one capture, and the values persist stored')
    rel = vs.plan_dict('relaunch', relaunch, 40.0, '')
    launch = [HEADER[0], '[settings] /h/Library/Application Support/ReCarbon/settings.toml: 19 values from the file',
              '[input] stick deadzone 0.13, trigger deadzone 0.10', '[audio] gain 0.2500 (volume 50%) from 0.00 s of output',
              '[video] present mode IMMEDIATE (V-Sync off)']
    d = os.path.join(tmp, 'rel')
    os.makedirs(d)
    open(os.path.join(d, 'settings.toml'), 'w').write(stored)
    r = vs.analyze_phase(rel, '\n'.join(launch), d, os.path.join(d, 'settings.toml'), vs.load_options())
    check([x for x in r if x[2] != 'PASS'] == [], f'relaunch: the stored values run: PASS ({[x for x in r if x[2] != "PASS"]})')
    for i in range(1, len(launch)):
        r = vs.analyze_phase(rel, '\n'.join(launch[:i] + launch[i + 1:]), d, os.path.join(d, 'settings.toml'), vs.load_options())
        check(verdicts(r).count('FAIL') == (2 if 'deadzone' in launch[i] else 1), f'relaunch without {launch[i][:40]!r}: its row(s) FAIL')
    cor = vs.plan_dict('corrupt', quiet, 40.0, '')
    d = os.path.join(tmp, 'cor')
    os.makedirs(d)
    tp = os.path.join(d, 'settings.toml')
    open(tp + '.bad', 'w').write(vs.CORRUPT_TOML)
    clog = [HEADER[0], f"[settings] {tp} doesn't parse: expected a value (line 3, column 14); defaults (kept as {tp}.bad)",
            '[run_attempt] deadline: SIGTERM', 'exit=0']
    r = vs.analyze_phase(cor, '\n'.join(clog), d, tp, vs.load_options())
    check(verdicts(r) == ['PASS'] * 3, f'corrupt: reported, kept, ran: PASS ({r})')
    r = vs.analyze_phase(cor, '\n'.join(clog[:2] + ['exit=139']), d, tp, vs.load_options())
    check(verdicts(r) == ['PASS', 'PASS', 'FAIL'], 'corrupt: a crash: FAIL')
    r = vs.analyze_phase(cor, '\n'.join(clog[:3] + ['exit=137']), d, tp, vs.load_options())
    check(verdicts(r) == ['PASS', 'PASS', 'FAIL'], 'corrupt: killed at the deadline (no clean quit): FAIL')
    os.remove(tp + '.bad')
    r = vs.analyze_phase(cor, '\n'.join(clog), d, tp, vs.load_options())
    check(verdicts(r) == ['PASS', 'FAIL', 'PASS'], 'corrupt: no .bad copy: FAIL')

    # ---- analyze() over a run folder, and the exit status ----
    out = os.path.join(tmp, 'out')
    md = os.path.join(out, 'menu')
    home = os.path.join(md, 'home')
    os.makedirs(os.path.join(home, 'Library', 'Application Support', 'ReCarbon'))
    plan = vs.plan_dict('menu', menu, 40.0, '')
    plan['home'] = home
    json.dump(plan, open(os.path.join(md, 'plan.json'), 'w'))
    open(os.path.join(md, 'game.log'), 'w').write(make_log(menu))
    open(vs.toml_path_of(home), 'w').write(defaults_toml)
    make_shots(os.path.join(md, 'shots'), menu)
    res = vs.analyze(out)
    check(res and all(x[3] == 'PASS' for x in res), 'analyze(): a good run folder passes')
    check(silent(vs.report, res, out) == 0 and os.path.exists(os.path.join(out, 'results.tsv')), 'report: exit 0 and results.tsv')
    open(os.path.join(md, 'game.log'), 'w').write(make_log(menu, drop=('video.mipmaps',)))
    check(silent(vs.report, vs.analyze(out), out) == 1, 'report: a FAIL exits 1')
    check(silent(vs.report, [], out) == 1, 'report: nothing analysed exits 1')
    check(silent(vs.main, ['plan', '--phase', 'persist']) == 0, 'the plan command prints a plan')

print(f'---- {failures} failed checks ----')
sys.exit(1 if failures else 0)
