#!/usr/bin/env python3
# ReCarbon tests. GPL-3.0-or-later (see COPYING).
#
# The local release-candidate tools, without a build: scripts/release_candidate.sh's preflight
# (each missing prerequisite stops it before anything is written, an existing output folder is
# never touched, and a run that fails later keeps its evidence and records the failing step),
# scripts/macos/rc_manifest.py's content gate, deps guard and manifest, and
# scripts/rc_transfer.sh's refusals and remote checksum check. Everything runs in a scratch
# tree: a fake repository (git init), fake libraries, game folder and compilers, and local
# stand-ins for ssh and scp. No network, no game files.
# From the repo root: python3 tests/release_candidate_test.py (ctest runs it).
import hashlib, importlib.util, json, os, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RC_SH = os.path.join(ROOT, 'scripts', 'release_candidate.sh')
TRANSFER_SH = os.path.join(ROOT, 'scripts', 'rc_transfer.sh')
HELPER = os.path.join(ROOT, 'scripts', 'macos', 'rc_manifest.py')
# The release's dmg, by its published, versioned name (make_app.py's hard link).
DMG = 'ReCarbon-0.0.1-macos.dmg'
spec = importlib.util.spec_from_file_location('rc_manifest', HELPER)
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)
failures = 0


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print(f'FAIL: {what}')


def write(path, text='', mode=0o644):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write(text)
    os.chmod(path, mode)


def sha256(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def tree(top):
    return sorted(os.path.relpath(os.path.join(d, n), top) for d, ds, fs in os.walk(top) for n in ds + fs)


def python311():
    for p in (sys.executable, '/opt/homebrew/bin/python3', shutil.which('python3') or ''):
        if p and subprocess.run([p, '-c', 'import tomllib'], capture_output=True).returncode == 0:
            return p
    return None


# The bundle make_app.py writes (build/app-release's, 2026-10-25, the icon, and the LGPL dylibs of gate G2): the gate's "clean" fixture.
CLEAN_APP = '''Contents/Info.plist
Contents/_CodeSignature/CodeResources
Contents/MacOS/ReCarbon
Contents/Frameworks/libSDL3.0.dylib
Contents/Frameworks/libMoltenVK.dylib
Contents/Frameworks/libSPIRV.16.dylib
Contents/Frameworks/libglslang-default-resource-limits.16.dylib
Contents/Frameworks/libSPIRV-Tools-opt.dylib
Contents/Frameworks/libvulkan.1.dylib
Contents/Frameworks/libSPIRV-Tools.dylib
Contents/Frameworks/libglslang.16.dylib
Contents/Frameworks/libavcodec.61.dylib
Contents/Frameworks/libavutil.59.dylib
Contents/Frameworks/librecarbon_lzx.dylib
Contents/Resources/AppIcon.icns
Contents/Resources/licenses/THIRD_PARTY_NOTICES.md
Contents/Resources/licenses/NOTICE
Contents/Resources/licenses/COPYING
Contents/Resources/licenses/README.txt
Contents/Resources/licenses/FFmpeg/LICENSE.md
Contents/Resources/licenses/FFmpeg/0001-xma-single-stream-no-holdback.patch
Contents/Resources/licenses/FFmpeg/COPYING.LGPLv2.1
Contents/Resources/licenses/libmspack/COPYING.LIB
Contents/Resources/licenses/LICENSES/GPL-3.0.txt
Contents/Resources/licenses/LICENSES/XenonRecomp-MIT.md
Contents/Resources/licenses/SPIRV-Tools/LICENSE
Contents/Resources/licenses/molten-vk/cereal-LICENSE.txt
Contents/Resources/licenses/Vulkan-Headers/LICENSES/MIT.txt
Contents/Resources/licenses/SDL3/src/hidapi/LICENSE-gpl3.txt
Contents/Resources/licenses/SDL3/src/video/yuv2rgb/LICENSE
Contents/Resources/vulkan/icd.d/MoltenVK_icd.json'''.split()


def make_app(top):
    app = os.path.join(top, 'ReCarbon.app')
    for rel in CLEAN_APP:
        write(os.path.join(app, rel), 'x')
    return app


def gate_tests(tmp):
    app = make_app(os.path.join(tmp, 'clean'))
    check(rc.bundle_problems(app) == [], f'a clean bundle passes ({rc.bundle_problems(app)})')
    # Each planted file fails the gate, and the problem names it.
    planted = {
        'Contents/Resources/default.xex': 'game data',
        'Contents/Resources/vulkan/ZZDATA1.BIN': 'game data',
        'Contents/Frameworks/ppc_recomp.12.cpp': 'game data',
        'Contents/Resources/licenses/ppc_context.h': 'game data',
        'Contents/Resources/title_hooks.cpp': 'source code',
        'Contents/Resources/recarbon.toml': 'game data',
        'Contents/extra.txt': 'outside',
        'README.txt': 'outside',
        'Contents/Resources/game/note.json': 'a folder of game data',
        'Contents/Resources/symbols/x.txt': 'a folder of game data',
        'Contents/MacOS/helper': 'outside',
        'Contents/Resources/.hidden': 'outside',
        'Contents/Resources/AppIcon.png': 'outside',
        'Contents/Resources/licenses/.DS_Store': 'outside',
    }
    for rel, why in planted.items():
        app = make_app(os.path.join(tmp, 'p-' + rel.replace('/', '_')))
        write(os.path.join(app, rel), 'x')
        found = rc.bundle_problems(app)
        where = rel.split('/note.json')[0].split('/x.txt')[0]
        check(any(p.startswith(where) and why in p for p in found),
              f'a planted {rel} ({why}) fails the gate ({found})')
    app = make_app(os.path.join(tmp, 'big'))
    with open(os.path.join(app, 'Contents/Resources/licenses/README.txt'), 'wb') as f:
        f.truncate(2 << 20)
    check(any('over its bound' in p for p in rc.bundle_problems(app)), 'an oversized license file fails the gate')
    app = make_app(os.path.join(tmp, 'link'))
    os.symlink('/etc', os.path.join(app, 'Contents/Resources/licenses/etc'))
    check(any('symbolic link' in p for p in rc.bundle_problems(app)), 'a symbolic link in the bundle fails the gate')
    check(rc.bundle_problems(os.path.join(tmp, 'nothing')) != [], 'a missing app fails the gate')

    vol = os.path.join(tmp, 'vol')
    make_app(vol)
    os.symlink('/Applications', os.path.join(vol, 'Applications'))
    os.makedirs(os.path.join(vol, '.fseventsd'))
    check(rc.volume_problems(vol) == [], f'a clean dmg volume passes ({rc.volume_problems(vol)})')
    write(os.path.join(vol, '.VolumeIcon.icns'), 'icns')
    check(rc.volume_problems(vol) == [], f'a dmg volume with its icon passes ({rc.volume_problems(vol)})')
    write(os.path.join(vol, '.secret'), 'x')
    check(any(p.startswith('.secret') for p in rc.volume_problems(vol)), 'a stray hidden file on the volume fails')
    os.remove(os.path.join(vol, '.secret'))
    with open(os.path.join(vol, '.VolumeIcon.icns'), 'wb') as f:
        f.truncate(5 << 20)
    check(any('.VolumeIcon.icns' in p and 'over its bound' in p for p in rc.volume_problems(vol)),
          'an oversized volume icon fails')
    os.remove(os.path.join(vol, '.VolumeIcon.icns'))
    os.symlink('/etc/hosts', os.path.join(vol, '.VolumeIcon.icns'))
    check(any('.VolumeIcon.icns' in p and 'regular file' in p for p in rc.volume_problems(vol)),
          'a volume icon that is a symbolic link fails')
    os.remove(os.path.join(vol, '.VolumeIcon.icns'))
    write(os.path.join(vol, 'default.xex'), 'x')
    check(any(p.startswith('default.xex') for p in rc.volume_problems(vol)), 'a stray file on the volume fails')
    os.remove(os.path.join(vol, 'default.xex'))
    write(os.path.join(vol, 'ReCarbon.app/Contents/Resources/ppc_func_mapping.cpp'), 'x')
    check(any('ppc_func_mapping' in p for p in rc.volume_problems(vol)), 'the volume gate checks the app in it')

    prefix = os.path.join(tmp, 'deps')
    cache = os.path.join(tmp, 'CMakeCache.txt')
    good = {k: os.path.join(prefix, 'lib', k) for k in rc.DEPS_KEYS}
    write(cache, '\n'.join(f'{k}:PATH={v}' for k, v in good.items()) + '\nRECARBON_MOLTENVK:FILEPATH=/opt/homebrew/x\n')
    check(rc.deps_problems(cache, prefix) == [], f'a build of the prefix\'s libraries passes ({rc.deps_problems(cache, prefix)})')
    bad = dict(good, SDL3_DIR='/opt/homebrew/lib/cmake/SDL3')
    write(cache, '\n'.join(f'{k}:PATH={v}' for k, v in bad.items()) + '\n')
    check(any(p.startswith('SDL3_DIR') for p in rc.deps_problems(cache, prefix)), 'Homebrew\'s SDL3 fails the deps guard')
    write(cache, '\n'.join(f'{k}:PATH={v}' for k, v in good.items() if k != 'AVCODEC_LIBRARY') + '\n')
    check(any(p.startswith('AVCODEC_LIBRARY') for p in rc.deps_problems(cache, prefix)), 'a missing entry fails the deps guard')


def make_rc(top, steps, exit_status, release=True, completed=True):
    os.makedirs(top)
    with open(os.path.join(top, 'steps.tsv'), 'w') as f:
        for name, status in steps:
            f.write(f'{name}\t{status}\t{top}/logs/{name}.log\t3\n')
    with open(os.path.join(top, 'fields.tsv'), 'w') as f:
        f.write(f'commit\t{"a" * 40}\nversion\t0.0.1\ncc\t/usr/bin/true\npython\t{sys.executable}\n')
        if completed:
            f.write('completed\tyes\n')
    if release:
        write(os.path.join(top, f'release/{DMG}'), 'dmg bytes')
        write(os.path.join(top, f'release/{DMG}.sha256'), f'{sha256(os.path.join(top, "release", DMG))}  {DMG}\n')
    return rc.write(top, exit_status)


def manifest_tests(tmp):
    top = os.path.join(tmp, 'pass')
    m = make_rc(top, [('checkout', 'PASS'), ('build', 'PASS')], 0)
    on_disk = json.load(open(os.path.join(top, 'MANIFEST.json')))
    check(on_disk == m, 'MANIFEST.json is what write() returned')
    check(m['result'] == 'PASS' and m['failed_step'] is None, f'a passed run is PASS ({m["result"]}, {m["failed_step"]})')
    check(m['commit'] == 'a' * 40 and m['version'] == '0.0.1', 'commit and version recorded')
    for k in ('macos', 'sdk', 'command_line_tools', 'cc', 'python'):
        check(k in m['toolchain'], f'toolchain records {k}')
    check([s['name'] for s in m['steps']] == ['checkout', 'build'] and m['steps'][0]['log'].endswith('checkout.log')
          and m['steps'][0]['seconds'] == 3, 'steps recorded with their logs and times')
    sums = os.path.join(top, 'release/SHA256SUMS')
    check(os.path.isfile(sums), 'a passed run writes release/SHA256SUMS')
    if os.path.isfile(sums):
        lines = open(sums).read().splitlines()
        names = [l.split('  ', 1)[1] for l in lines]
        check(names == ['MANIFEST.json', DMG, f'{DMG}.sha256'], f'SHA256SUMS lists the release ({names})')
        r = subprocess.run(['shasum', '-a', '256', '-c', 'SHA256SUMS'], cwd=os.path.join(top, 'release'), capture_output=True)
        check(r.returncode == 0, 'shasum -a 256 -c SHA256SUMS passes')

    top = os.path.join(tmp, 'fail')
    m = make_rc(top, [('checkout', 'PASS'), ('survey', 'FAIL')], 1)
    check(m['result'] == 'FAIL' and m['failed_step'] == 'survey', f'a failed step is recorded ({m["result"]}, {m["failed_step"]})')
    check(not os.path.exists(os.path.join(top, 'release/SHA256SUMS')), 'a failed run writes no SHA256SUMS')
    top = os.path.join(tmp, 'died')
    m = make_rc(top, [('checkout', 'PASS')], 130)
    check(m['result'] == 'FAIL' and 'exit status 130' in str(m['failed_step']), f'a non-zero exit with no failed step is FAIL ({m["failed_step"]})')
    top = os.path.join(tmp, 'cut-short')
    m = make_rc(top, [('checkout', 'PASS')], 0, completed=False)
    check(m['result'] == 'FAIL' and 'not completed' in str(m['failed_step']), f'a run that stopped early with status 0 is FAIL ({m["failed_step"]})')
    check(not os.path.exists(os.path.join(top, 'release/SHA256SUMS')), 'an incomplete run writes no SHA256SUMS')
    return os.path.join(tmp, 'pass')


def transfer_tests(tmp, passed_rc):
    bin_dir = os.path.join(tmp, 'bin')
    home = os.path.join(tmp, 'remote-home')
    os.makedirs(home)
    log = os.path.join(tmp, 'ssh.log')
    # ssh <host> <command>: the command, run in the fake remote home. scp -q <files> <host>:<dir>/.
    write(os.path.join(bin_dir, 'ssh'), f'#!/bin/sh\necho "$@" >> {log}\nshift\ncd "{home}" && exec /bin/sh -c "$1"\n', 0o755)
    write(os.path.join(bin_dir, 'scp'), f'''#!/bin/sh
shift
for last; do :; done
dir="{home}/${{last#*:}}"
while [ $# -gt 1 ]; do cp "$1" "$dir"; shift; done
[ -z "$CORRUPT" ] || echo x >> "$dir/{DMG}"
''', 0o755)
    env = dict(os.environ, RC_SSH=os.path.join(bin_dir, 'ssh'), RC_SCP=os.path.join(bin_dir, 'scp'))

    def transfer(rc_dir, remote, **extra):
        return subprocess.run(['bash', TRANSFER_SH, rc_dir, 'm1-air', remote], env=dict(env, **extra),
                              capture_output=True, text=True, timeout=60)

    r = transfer(passed_rc, 'rc/one')
    check(r.returncode == 0 and 'verified' in r.stdout, f'a passed candidate transfers ({r.returncode} {r.stdout!r} {r.stderr!r})')
    check(tree(os.path.join(home, 'rc/one')) == ['MANIFEST.json', DMG, f'{DMG}.sha256', 'SHA256SUMS'],
          f'only the release files are sent ({tree(os.path.join(home, "rc/one"))})')

    write(os.path.join(home, 'rc/two/keep.txt'), 'theirs')
    r = transfer(passed_rc, 'rc/two')
    check(r.returncode != 0 and 'exists' in r.stderr, f'an existing remote dir is refused ({r.returncode} {r.stderr!r})')
    check(tree(os.path.join(home, 'rc/two')) == ['keep.txt'], 'nothing is copied into an existing remote dir')

    r = transfer(passed_rc, 'rc/three', CORRUPT='1')
    check(r.returncode != 0 and 'mismatch' in r.stderr, f'a checksum mismatch on the remote side fails ({r.returncode} {r.stderr!r})')

    extra = os.path.join(tmp, 'extra')
    shutil.copytree(passed_rc, extra)
    os.makedirs(os.path.join(extra, 'release/symbols'))
    open(log, 'w').close()
    r = transfer(extra, 'rc/four')
    check(r.returncode != 0 and 'symbols' in r.stderr, f'a release folder holding anything else is refused ({r.stderr!r})')
    check(open(log).read() == '', 'nothing is sent when the release folder is refused')

    failed = os.path.join(tmp, 'failed-rc')
    shutil.copytree(passed_rc, failed)
    m = json.load(open(os.path.join(failed, 'MANIFEST.json')))
    m['result'] = 'FAIL'
    os.remove(os.path.join(failed, 'release/MANIFEST.json'))  # a hard link to the top-level one
    json.dump(m, open(os.path.join(failed, 'release/MANIFEST.json'), 'w'), indent=2)
    # Consistent checksums, so only the recorded result can refuse it.
    rel = os.path.join(failed, 'release')
    with open(os.path.join(rel, 'SHA256SUMS'), 'w') as f:
        for n in ('MANIFEST.json', DMG, f'{DMG}.sha256'):
            f.write(f'{sha256(os.path.join(rel, n))}  {n}\n')
    r = transfer(failed, 'rc/five')
    check(r.returncode != 0 and 'pass' in r.stderr and not os.path.exists(os.path.join(home, 'rc/five')), f'a candidate that did not pass is refused ({r.stderr!r})')

    # An RC from before the versioned name: the dmg under its unversioned name only.
    old = os.path.join(tmp, 'unversioned-rc')
    shutil.copytree(passed_rc, old)
    rel = os.path.join(old, 'release')
    os.rename(os.path.join(rel, DMG), os.path.join(rel, 'ReCarbon-macos.dmg'))
    os.remove(os.path.join(rel, f'{DMG}.sha256'))
    write(os.path.join(rel, 'ReCarbon-macos.dmg.sha256'), f'{sha256(os.path.join(rel, "ReCarbon-macos.dmg"))}  ReCarbon-macos.dmg\n')
    with open(os.path.join(rel, 'SHA256SUMS'), 'w') as f:
        for n in ('MANIFEST.json', 'ReCarbon-macos.dmg', 'ReCarbon-macos.dmg.sha256'):
            f.write(f'{sha256(os.path.join(rel, n))}  {n}\n')
    r = transfer(old, 'rc/six')
    check(r.returncode != 0 and 'not part of a release' in r.stderr and not os.path.exists(os.path.join(home, 'rc/six')),
          f'a release without the versioned dmg is refused ({r.stderr!r})')


def preflight_tests(tmp, py):
    repo = os.path.join(tmp, 'repo')
    for rel, src in (('scripts/release_candidate.sh', RC_SH), ('scripts/macos/rc_manifest.py', HELPER)):
        os.makedirs(os.path.dirname(os.path.join(repo, rel)), exist_ok=True)
        shutil.copy(src, os.path.join(repo, rel))
    mvk = os.path.join(tmp, 'libMoltenVK.dylib')
    write(mvk, 'moltenvk')
    write(os.path.join(repo, '.gitignore'), 'build/\n/game\n/tools\n/ppc\n')
    write(os.path.join(repo, 'CMakeLists.txt'), 'project(ReCarbon VERSION 0.0.1 LANGUAGES C CXX)\n')
    write(os.path.join(repo, 'scripts/macos/make_app.py'), f"MOLTENVK_PINNED = '1.4.2'\nMOLTENVK_SHA256_PINNED = '{sha256(mvk)}'\n")
    # The commit's own setup_tools.sh: the first heavy step, here a failure.
    write(os.path.join(repo, 'scripts/setup_tools.sh'), '#!/bin/sh\necho "stub setup_tools"\n[ -z "$STUB_SLEEP" ] || sleep 30\nexit 1\n', 0o755)
    for rel in ('scripts/survey.sh', 'survey/validate_recomp.py', 'tests/run_instr_tests.sh', 'scripts/macos/check_app.py'):
        write(os.path.join(repo, rel), '#!/bin/sh\nexit 1\n', 0o755)
    git = ['git', '-c', 'user.name=t', '-c', 'user.email=t@t.invalid', '-c', 'init.defaultBranch=main']
    subprocess.run(git + ['init', '-q', repo], check=True)
    subprocess.run(git + ['-C', repo, 'add', '-A'], check=True)
    subprocess.run(git + ['-C', repo, 'commit', '-q', '-m', 'test'], check=True)
    sha = subprocess.run(['git', '-C', repo, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()

    deps = os.path.join(tmp, 'deps')
    write(os.path.join(deps, 'share/recarbon-deps.json'), '{\n  "deployment_target": "27.0"\n}\n')
    for rel in ('lib/libSDL3.0.dylib', 'lib/libvulkan.1.dylib', 'lib/cmake/SDL3/x', 'lib/cmake/glslang/x',
                'lib/cmake/SPIRV-Tools/x', 'ffmpeg-xma/lib/libavcodec.dylib', 'ffmpeg-xma/lib/libavutil.dylib',
                'ffmpeg-xma/include/libavcodec/avcodec.h'):
        write(os.path.join(deps, rel), 'x')
    game = os.path.join(tmp, 'disc')
    write(os.path.join(game, 'default.xex'), 'not a real xex')
    os.chmod(game, 0o555)  # read only: the run must never write there
    llvm = os.path.join(tmp, 'llvm')
    for t in ('clang', 'clang++', 'llvm-mc', 'llvm-objdump', 'ld.lld'):
        write(os.path.join(llvm, t), '#!/bin/sh\necho stub\n', 0o755)
    fake = os.path.join(tmp, 'fakebin')
    for t in ('cmake', 'ninja'):
        write(os.path.join(fake, t), '#!/bin/sh\necho "$0 version 0"\n', 0o755)
    base = {k: v for k, v in os.environ.items() if not k.startswith(('RC_', 'MOLTENVK'))}
    base.update(PATH=fake + ':/usr/bin:/bin:/usr/sbin:/sbin:' + os.path.dirname(shutil.which('git')),
                RC_DEPS=deps, RC_GAME=game, RC_LLVM=llvm, RC_PYTHON=py, MOLTENVK=mvk, RC_COMPARE_APP='/nonexistent')
    rc_root = os.path.join(repo, 'build/rc')

    def run(commit=sha, **extra):
        env = dict(base, **extra)
        for k in [k for k, v in env.items() if v is None]:
            del env[k]
        return subprocess.run(['bash', os.path.join(repo, 'scripts/release_candidate.sh'), commit], env=env,
                              capture_output=True, text=True, timeout=120)

    nodeps = os.path.join(tmp, 'deps-incomplete')
    shutil.copytree(deps, nodeps)
    os.remove(os.path.join(nodeps, 'ffmpeg-xma/lib/libavcodec.dylib'))
    nogame = os.path.join(tmp, 'empty-disc')
    os.makedirs(nogame)
    other_mvk = os.path.join(tmp, 'other-moltenvk.dylib')
    write(other_mvk, 'another build')
    nocmake = os.path.join(tmp, 'fakebin-nocmake')
    os.makedirs(nocmake)
    shutil.copy(os.path.join(fake, 'ninja'), nocmake)
    cases = [
        ('an unknown commit', dict(commit='deadbeef'), 'no such commit'),
        ('a missing deps prefix', dict(RC_DEPS=os.path.join(tmp, 'nowhere')), 'build_deps.sh'),
        ('an incomplete deps prefix', dict(RC_DEPS=nodeps), 'libavcodec.dylib missing'),
        ('missing game files', dict(RC_GAME=nogame), 'default.xex missing'),
        ('a missing compiler', dict(RC_LLVM=os.path.join(tmp, 'nowhere')), 'clang'),
        ('a missing cmake', dict(PATH=base['PATH'].replace(fake, nocmake)), 'missing cmake'),
        ('another MoltenVK', dict(MOLTENVK=other_mvk), 'pins'),
    ]
    for what, extra, message in cases:
        r = run(**extra)
        check(r.returncode != 0 and message in r.stderr, f'{what} stops the preflight ({r.returncode} {r.stderr!r})')
        check(not os.path.exists(rc_root), f'{what}: nothing is written')

    short = sha[:7]
    existing = os.path.join(rc_root, f'{short}-T1')
    write(os.path.join(existing, 'evidence.txt'), 'keep me')
    r = run(RC_STAMP='T1')
    check(r.returncode != 0 and 'refusing to touch it' in r.stderr, f'an existing output folder is refused ({r.returncode} {r.stderr!r})')
    check(tree(existing) == ['evidence.txt'] and open(os.path.join(existing, 'evidence.txt')).read() == 'keep me',
          'an existing output folder is left as it was')

    # Past the preflight: the checkout passes, the first heavy step (the stub setup_tools.sh) fails,
    # and the run keeps everything and records the failure.
    r = run(RC_STAMP='T2')
    out = os.path.join(rc_root, f'{short}-T2')
    check(r.returncode != 0, f'a failing step fails the run ({r.returncode} {r.stdout!r} {r.stderr!r})')
    try:
        marker = json.load(open(os.path.join(out, '.retention.json')))
    except (OSError, ValueError) as e:
        check(False, f'the run writes a readable .retention.json ({e})')
        marker = {}
    check(marker.get('class') == 'evidence' and marker.get('status') == 'active'
          and marker.get('owner') == 'recarbon/release_candidate.sh' and marker.get('project') == 'recarbon',
          f'the marker: evidence, active, owned by the script ({marker})')
    try:
        m = json.load(open(os.path.join(out, 'MANIFEST.json')))
    except (OSError, ValueError) as e:
        check(False, f'a failed run writes MANIFEST.json ({e})')
        m = {'steps': []}
    check(m.get('result') == 'FAIL' and m.get('failed_step') == 'tools', f'the manifest records the failing step ({m.get("failed_step")})')
    check(m.get('commit') == sha, 'the manifest records the full commit')
    check([(s['name'], s['status']) for s in m['steps']] == [('checkout', 'PASS'), ('seed-ffmpeg', 'PASS'), ('tools', 'FAIL')],
          f'the steps so far ({m["steps"]})')
    check(all(os.path.isfile(s['log']) for s in m['steps']), 'each step has its log')
    check(os.path.isfile(os.path.join(out, 'src/scripts/setup_tools.sh')) and os.path.isdir(os.path.join(out, 'src/.git')),
          'the failed run\'s checkout is kept')
    check(os.path.islink(os.path.join(out, 'src/game/files')) and os.path.realpath(os.path.join(out, 'src/game/files')) == os.path.realpath(game),
          'the checkout links the game folder')
    check(not os.path.exists(os.path.join(out, 'release')), 'a failed run has no release folder')
    check(tree(game) == ['default.xex'], 'the game folder is unchanged')

    # Stopped mid-step (SIGTERM to the process group, as a task stop sends): FAIL, never PASS.
    import signal, time
    p = subprocess.Popen(['bash', os.path.join(repo, 'scripts/release_candidate.sh'), sha], start_new_session=True,
                         env=dict(base, RC_STAMP='T3', STUB_SLEEP='1'), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out = os.path.join(rc_root, f'{short}-T3')
    deadline = time.time() + 60
    while time.time() < deadline and not os.path.exists(os.path.join(out, 'logs/tools.log')):
        time.sleep(0.1)
    time.sleep(0.5)
    os.killpg(p.pid, signal.SIGTERM)
    p.communicate(timeout=60)
    try:
        m = json.load(open(os.path.join(out, 'MANIFEST.json')))
    except (OSError, ValueError) as e:
        check(False, f'a stopped run writes MANIFEST.json ({e})')
        m = {}
    check(p.returncode != 0 and m.get('result') == 'FAIL', f'a stopped run is FAIL ({p.returncode}, {m.get("result")}, {m.get("failed_step")})')
    os.chmod(game, 0o755)


with tempfile.TemporaryDirectory() as tmp:
    gate_tests(os.path.join(tmp, 'gate'))
    passed = manifest_tests(os.path.join(tmp, 'manifest'))
    transfer_tests(os.path.join(tmp, 'transfer'), passed)
    py = python311()
    if py:
        preflight_tests(os.path.join(tmp, 'preflight'), py)
    else:
        print('release_candidate_test: no Python 3.11+ for the script\'s preflight: those checks skipped')
        failures += 1

if failures:
    print(f'{failures} check(s) failed')
    sys.exit(1)
print('release_candidate_test: all checks passed')
