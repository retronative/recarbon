#!/usr/bin/env python3
# ReCarbon. GPL-3.0-or-later (see COPYING).
#
# The checks and the record of scripts/release_candidate.sh (a local release-candidate build),
# kept here so tests/release_candidate_test.py can test them without a build:
#
#   rc_manifest.py gate-app <ReCarbon.app>         the content-safety gate: fails on anything that looks
#                                                  like game data or generated sources, and on any file
#                                                  outside the bundle layout make_app.py writes
#   rc_manifest.py gate-volume <mounted dmg>       the dmg's top level (the app, the Applications
#                                                  shortcut, the volume icon, the file system's own
#                                                  entries), then gate-app on the app in it
#   rc_manifest.py deps-guard <CMakeCache.txt> <prefix>
#                                                  every bundled library the build found is the deps
#                                                  prefix's (build_deps.sh), none Homebrew's
#   rc_manifest.py write <rc dir> <exit status>    MANIFEST.json from <rc dir>/steps.tsv (name, PASS or
#                                                  FAIL, log, seconds) and fields.tsv (key, value);
#                                                  on a pass, release/SHA256SUMS too
#
# Python 3.9+ (the tests' interpreter may be the system one).
import hashlib, json, os, plistlib, re, stat, subprocess, sys

# Anything that names the game's files or the recompiler's output. Checked on every path in the
# bundle, before the allowlist, so a forbidden file is reported as such wherever it sits.
GAME_EXTENSIONS = {'.xex', '.xexp', '.xcp', '.iso', '.bun', '.big', '.lzc', '.wmv', '.xmv', '.vp6', '.bin',
                   '.ast', '.asf', '.mus', '.abk', '.fsh', '.fng', '.str', '.toml', '.dump', '.ppm', '.log'}
SOURCE_EXTENSIONS = {'.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.inc', '.s', '.o', '.a', '.ll', '.dsym'}
FORBIDDEN_NAMES = re.compile(r'^(zzdata.*|ppc_.*|default\.xex.*|\$systemupdate|pad[ab][12])$', re.I)
FORBIDDEN_DIRS = {'game', 'survey', 'ppc', 'rom_extracted', 'symbols', 'tools', 'build', 'nfs'}

# The bundle make_app.py writes: (pattern, size limit in bytes), the first match decides.
MiB = 1 << 20
LICENSE_FILE = r'(?:[^/]+\.(?:txt|md|patch)|(?:LICENSE|COPYING|NOTICE)[^/]*)'
ALLOWED = [
    (r'Contents/Info\.plist', 64 * 1024),
    (r'Contents/_CodeSignature/CodeResources', 1 * MiB),
    (r'Contents/MacOS/ReCarbon', 256 * MiB),
    (r'Contents/Frameworks/[^/]+\.dylib', 16 * MiB),
    (r'Contents/Resources/AppIcon\.icns', 4 * MiB),
    (r'Contents/Resources/vulkan/icd\.d/[^/]+\.json', 64 * 1024),
    (r'Contents/Resources/licenses/(?:[^/]+/)*' + LICENSE_FILE, 1 * MiB),
]
# A mounted dmg's top level: make_app.py's app, Applications shortcut and volume icon (a regular
# file within its bound: the app's AppIcon.icns), and what macOS itself keeps on a volume (not
# looked into).
VOLUME_ENTRIES = {'ReCarbon.app', 'Applications', '.VolumeIcon.icns'}
VOLUME_ICON_LIMIT = 4 * MiB
VOLUME_SYSTEM = {'.fseventsd', '.Trashes', '.DS_Store', '.Spotlight-V100', '.TemporaryItems', '.background',
                 '.HFS+ Private Directory Data\r', '.journal', '.journal_info_block'}

# The CMake cache entries of what the game links or bundles: each must lie in the deps prefix.
# (RECARBON_MOLTENVK is Homebrew's on purpose, pinned by make_app.py; glslangValidator is a build
# tool, not linked.)
DEPS_KEYS = ['SDL3_DIR', 'glslang_DIR', 'SPIRV-Tools_DIR', 'SPIRV-Tools-opt_DIR', 'Vulkan_INCLUDE_DIR',
             'Vulkan_LIBRARY', 'FFMPEG_INCLUDE_DIR', 'AVCODEC_LIBRARY', 'AVUTIL_LIBRARY']


def bundle_problems(app):
    """What in the app at `app` fails the gate, as 'path: reason' lines (none: it passes)."""
    problems, seen = [], 0
    for top, dirs, files in os.walk(app):
        for name in sorted(dirs + files):
            path = os.path.join(top, name)
            rel = os.path.relpath(path, app)
            st = os.lstat(path)
            seen += 1
            parts = rel.split('/')
            low = [p.lower() for p in parts]
            ext = os.path.splitext(low[-1])[1]
            if stat.S_ISLNK(st.st_mode):
                problems.append(f'{rel}: a symbolic link (the bundle has none)')
            elif ext in GAME_EXTENSIONS or FORBIDDEN_NAMES.match(parts[-1]):
                problems.append(f'{rel}: looks like game data or recompiler output')
            elif ext in SOURCE_EXTENSIONS:
                problems.append(f'{rel}: looks like source code or build output')
            elif stat.S_ISDIR(st.st_mode):
                if low[-1] in FORBIDDEN_DIRS:
                    problems.append(f'{rel}: a folder of game data, generated code, tools or symbols')
            elif not stat.S_ISREG(st.st_mode):
                problems.append(f'{rel}: not a regular file')
            else:
                limit = next((lim for pat, lim in ALLOWED if re.fullmatch(pat, rel)), None)
                if limit is None:
                    problems.append(f'{rel}: outside the bundle layout make_app.py writes')
                elif st.st_size > limit:
                    problems.append(f'{rel}: {st.st_size} bytes, over its bound of {limit}')
    if not seen:
        problems.append('.: empty or missing')
    elif not os.path.isfile(os.path.join(app, 'Contents/MacOS/ReCarbon')):
        problems.append('Contents/MacOS/ReCarbon: missing')
    return problems


def volume_problems(volume):
    problems = []
    for name in sorted(os.listdir(volume)):
        path = os.path.join(volume, name)
        if name in VOLUME_SYSTEM:
            continue
        if name == 'Applications':
            if not (os.path.islink(path) and os.readlink(path) == '/Applications'):
                problems.append('Applications: not the shortcut to /Applications')
        elif name == '.VolumeIcon.icns':
            st = os.lstat(path)
            if not stat.S_ISREG(st.st_mode):
                problems.append('.VolumeIcon.icns: not a regular file')
            elif st.st_size > VOLUME_ICON_LIMIT:
                problems.append(f'.VolumeIcon.icns: {st.st_size} bytes, over its bound of {VOLUME_ICON_LIMIT}')
        elif name == 'ReCarbon.app' and os.path.isdir(path) and not os.path.islink(path):
            problems += ['ReCarbon.app/' + p for p in bundle_problems(path)]
        else:
            problems.append(f'{name}: not part of the download (only ReCarbon.app and Applications)')
    if 'ReCarbon.app' not in os.listdir(volume):
        problems.append('ReCarbon.app: missing')
    return problems


def deps_problems(cache, prefix):
    values = {}
    with open(cache) as f:
        for line in f:
            m = re.match(r'([^#/:][^:]*):[A-Z]+=(.*)$', line.rstrip('\n'))
            if m:
                values[m.group(1)] = m.group(2)
    root = os.path.realpath(prefix) + os.sep
    problems = []
    for key in DEPS_KEYS:
        v = values.get(key)
        if not v or v.endswith('-NOTFOUND'):
            problems.append(f'{key}: not set')
        elif not os.path.realpath(v).startswith(root):
            problems.append(f'{key}={v}: not in {prefix} (Homebrew\'s or another library\'s, not the release\'s)')
    return problems


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def first_line(*cmd):
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return (out.stdout or out.stderr).strip().splitlines()[0]
    except (OSError, IndexError, subprocess.SubprocessError):
        return 'unavailable'


def toolchain(fields):
    t = {'macos': first_line('sw_vers', '-productVersion') + ' (' + first_line('sw_vers', '-buildVersion') + ')',
         'sdk': first_line('xcrun', '--show-sdk-version') + ' ' + first_line('xcrun', '--show-sdk-path'),
         'command_line_tools': first_line('xcode-select', '-p') + ', '
         + next((l for l in first_line_all('pkgutil', '--pkg-info=com.apple.pkg.CLTools_Executables')
                 if l.startswith('version:')), 'version: unavailable')}
    for key, flag in (('cc', '--version'), ('cmake', '--version'), ('ninja', '--version'), ('python', '--version')):
        if fields.get(key):
            t[key] = fields[key] + ': ' + first_line(fields[key], flag)
    return t


def first_line_all(*cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=30).stdout.splitlines()
    except (OSError, subprocess.SubprocessError):
        return []


def read_tsv(path, width):
    rows = []
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                cols = line.rstrip('\n').split('\t')
                if len(cols) >= width:
                    rows.append(cols[:width - 1] + ['\t'.join(cols[width - 1:])])
    return rows


def write(rc, exit_status):
    """MANIFEST.json for the run in `rc`; on a pass, also release/SHA256SUMS over the release folder."""
    fields = dict(read_tsv(os.path.join(rc, 'fields.tsv'), 2))
    steps = [{'name': n, 'status': s, 'log': log, 'seconds': int(secs) if secs.isdigit() else secs}
             for n, s, log, secs in read_tsv(os.path.join(rc, 'steps.tsv'), 4)]
    failed = next((s['name'] for s in steps if s['status'] != 'PASS'), None)
    # A pass needs the script's last line too (fields.tsv: completed), not just no failed step.
    ok = exit_status == 0 and bool(steps) and failed is None and fields.get('completed') == 'yes'
    if not ok and failed is None:
        failed = 'script (exit status %d after step %s%s)' % (exit_status, steps[-1]['name'] if steps else 'none',
                                                            '' if exit_status else ', not completed')
    app = fields.get('app')
    version = fields.get('version', '')
    if app and os.path.isfile(os.path.join(app, 'Contents/Info.plist')):
        with open(os.path.join(app, 'Contents/Info.plist'), 'rb') as f:
            info = plistlib.load(f)
        version = info.get('CFBundleShortVersionString', version)
        fields['bundle_version'] = str(info.get('CFBundleVersion', ''))
    manifest = {
        'schema': 1,
        'result': 'PASS' if ok else 'FAIL',
        'failed_step': None if ok else failed,
        'exit_status': exit_status,
        'commit': fields.get('commit', ''),
        'version': version,
        'fields': fields,
        'toolchain': toolchain(fields),
        'steps': steps,
        'not_for_distribution': 'symbols/, src/, build/ and logs/ hold data generated from the player\'s own '
                                'disc and stay on this Mac; only release/ is meant to leave it.',
    }
    path = os.path.join(rc, 'MANIFEST.json')
    with open(path + '.part', 'w') as f:
        json.dump(manifest, f, indent=2, sort_keys=False)
        f.write('\n')
    os.replace(path + '.part', path)
    release = os.path.join(rc, 'release')
    if ok and os.path.isdir(release):
        target = os.path.join(release, 'MANIFEST.json')
        if os.path.exists(target):
            os.remove(target)
        os.link(path, target)
        names = sorted(n for n in os.listdir(release) if n != 'SHA256SUMS')
        with open(os.path.join(release, 'SHA256SUMS'), 'w') as f:
            for n in names:
                f.write(f'{sha256(os.path.join(release, n))}  {n}\n')  # shasum -a 256 -c SHA256SUMS
    return manifest


def report(problems, what):
    for p in problems:
        print(f'{what}: {p}')
    if problems:
        print(f'{what}: {len(problems)} problem(s)')
        return 1
    print(f'{what}: passed')
    return 0


def main(argv):
    if len(argv) == 2 and argv[0] == 'gate-app':
        return report(bundle_problems(argv[1]), 'gate-app')
    if len(argv) == 2 and argv[0] == 'gate-volume':
        return report(volume_problems(argv[1]), 'gate-volume')
    if len(argv) == 3 and argv[0] == 'deps-guard':
        return report(deps_problems(argv[1], argv[2]), 'deps-guard')
    if len(argv) == 3 and argv[0] == 'write':
        m = write(argv[1], int(argv[2]))
        print(f'MANIFEST.json: {m["result"]}' + (f' (failed: {m["failed_step"]})' if m['failed_step'] else ''))
        return 0
    print('usage: rc_manifest.py gate-app <app> | gate-volume <mounted dmg> | deps-guard <CMakeCache.txt> <prefix>'
          ' | write <rc dir> <exit status>', file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
