#!/usr/bin/env python3
# ReCarbon. GPL-3.0-or-later (see COPYING).
# From SpeedBreaker's scripts/macos/make_app.py (GPL-3.0-or-later). ReCarbon
# changes: the names, the bundle ID org.retronative.recarbon, the notices,
# and ReCarbon's own icon (SpeedBreaker's is not used): scripts/macos/AppIcon.png,
# made from the website's mark (website/public/favicon.svg) by
# scripts/macos/make_icon.swift, as the app's icon and the dmg's volume icon.
#
# make_app.py [build dir] [output dir] [--dmg] [--min-macos=<version>] [--test]:
# ReCarbon.app for macOS (Apple silicon) from a build (default
# build/main), in build/app:
#   Contents/MacOS/ReCarbon      the game
#   Contents/Frameworks/             the libraries it links (SDL3, the Vulkan
#                                    loader, glslang, SPIRV-Tools, and what
#                                    they link; ReCarbon, gate G2: FFmpeg's
#                                    libavcodec and libavutil and libmspack's
#                                    LZX decoder, librecarbon_lzx, the LGPL
#                                    parts, as separate replaceable files),
#                                    named @rpath/<file>, and
#                                    MoltenVK, the Vulkan driver (Homebrew's
#                                    1.4.2: it runs on macOS 12+; another
#                                    version only with MOLTENVK_VERSION=<it>)
#   Contents/Resources/vulkan/icd.d/ MoltenVK's manifest (the Vulkan loader
#                                    looks for drivers in the app's bundle)
#   Contents/Resources/AppIcon.icns  from scripts/macos/AppIcon.png, every
#                                    size macOS asks for (16 to 512 @1x/@2x)
#   Contents/Resources/licenses/     COPYING, NOTICE, THIRD_PARTY_NOTICES.md,
#                                    LICENSES/, and each bundled library's
#                                    own license files (from build_deps.sh's
#                                    prefix, with what each was built from, or
#                                    from the library's Homebrew keg)
#   Contents/Info.plist              a game (macOS Game Mode when fullscreen),
#                                    versioned from the build: CFBundleShort-
#                                    VersionString is project()'s version,
#                                    CFBundleVersion the commit's number in
#                                    the history, LSMinimumSystemVersion the
#                                    build's deployment target, which every
#                                    binary in the app must match (checked)
#
# The macOS the app runs on: the build's deployment target (CMakeLists.txt's
# default, 15.0, unless the build was configured with another). Every library
# in the app must be built for it
# or an older macOS; a newer one is refused, with the two ways out:
#   - the libraries built for it by scripts/macos/build_deps.sh <prefix>, the
#     build configured with -DCMAKE_PREFIX_PATH=<prefix>
#     -DFFMPEG_XMA=<prefix>/ffmpeg-xma (how the release is built);
#   - or Homebrew's libraries, which are built for the macOS they were made
#     on (26.0 on macOS 26), with the build configured for that macOS:
#     -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0. Fine for an app of your own; it
#     won't open on an older macOS, and this script says so.
# --min-macos=<version> (release scripts): refuse an app for any other macOS.
# scripts/macos/check_app.py checks the app or dmg it makes (the release's
# gate: the floor, every binary's, what it loads and imports).
# signed ad hoc (Apple silicon runs nothing unsigned, and rewriting a
# library's names breaks its signature). Nothing of Homebrew is needed to
# run it. No game data: the game installs from the player's disc image on
# the first run, into ~/Library/Application Support/ReCarbon/game.
#
# The game's binary has its debug map stripped (the paths of the objects it
# was linked from); its symbol table stays, so crash reports name host
# functions, and they name the recompiled ones from the game's own table
# (report/crash.cpp) either way. The full symbols go to
# <output>/symbols/ReCarbon-<version>-<commit>-macos.dSYM, for us: never
# published, they map a player's report back to the exact build.
#
# --test: an app to try a build with, named so it can't be taken for a
# release: "ReCarbon (test).app", its displayed name likewise, and its
# bundle identifier with ".test" (not with --dmg: a test app is never a
# download). Its game, saves and settings are the release's (user/paths.h).
#
# --dmg: also <output>/ReCarbon-macos.dmg (the app and a shortcut to
# Applications, to drag it onto) with its .sha256, the release download, and
# a copy named by the version (ReCarbon-<version>-macos.dmg, a hard link).
# The mounted volume shows the app's icon: .VolumeIcon.icns at its root and
# the custom-icon flag in the root folder's Finder info (set on a read-write
# image, then converted). The .dmg file itself gets no icon of its own: that
# would be a resource fork, which leaves its SHA-256 alone but is lost on the
# way to a player (a browser download, scp), so the dmg shows macOS's own.
# The volume opens in a Finder window laid out for the drag (DMG_WINDOW and
# DMG_ICONS): the app on the left, Applications on the right, a dark
# background with an arrow between them (.background/background.tiff, from
# DmgBackground.png and its @2x, drawn by scripts/macos/make_dmg_background.swift),
# no toolbar, sidebar, path or status bar. That's the volume's .DS_Store,
# written on the read-write image by ds_store and mac_alias (dmgbuild's
# libraries, pinned in scripts/macos/dmg-requirements.txt, installed by
# scripts/setup_tools.sh into tools/dmg-python): no Finder scripting, nothing
# but Finder needed to install.
# The dmg is an HFS+ volume (Mac OS Extended) on a GPT disk, compressed
# (UDZO), made by hdiutil (read-write, then converted) while it works:
# macOS 27 deprecates hdiutil's create, attach, detach and imageinfo (they
# still work; -quiet hides the warning) in favour of `diskutil image`. Where hdiutil is missing or fails,
# `diskutil image` makes the same image (its "create from <folder>" makes
# APFS only, so this goes by a blank image, partitioned and filled, then
# converted): the same files, modes, owners and extended attributes. What
# differs, none of it visible in Finder: the GPT partition's name, the
# volume's size (the content + 10% + 8 MB, against hdiutil's estimate), the
# volume root's owner on disk (root, which a mount ignores; hdiutil's is the
# builder), and the compression (diskutil has no zlib level to set; on
# v0.1.1's app its dmg came out 4% smaller than hdiutil's level 9).
# DMG_TOOL=hdiutil or DMG_TOOL=diskutil uses only that one.
import hashlib, json, os, plistlib, re, shutil, stat, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
OPTIONS = [a for a in sys.argv[1:] if a.startswith('--')]
DMG = '--dmg' in OPTIONS
TEST = '--test' in OPTIONS
MIN_MACOS = next((a.split('=', 1)[1] for a in OPTIONS if a.startswith('--min-macos=')), None)
DMG_TOOL = os.environ.get('DMG_TOOL', '')  # '' (hdiutil, else diskutil), 'hdiutil' or 'diskutil'
if DMG_TOOL not in ('', 'hdiutil', 'diskutil'):
    raise SystemExit(f'make_app.py: DMG_TOOL={DMG_TOOL}: use hdiutil or diskutil, or leave it unset')
for a in OPTIONS:
    if a not in ('--dmg', '--test') and not a.startswith('--min-macos='):
        raise SystemExit(f'make_app.py: unknown option {a} (usage: make_app.py [build dir] [output dir] [--dmg] '
                         f'[--min-macos=<version>] [--test])')
if TEST and DMG:
    raise SystemExit('make_app.py: --test and --dmg: a test app is never a download')
# The app's name (the bundle's, the displayed one and the menu bar's), bundle
# identifier and symbols' name; --test's are its own.
NAME = 'ReCarbon (test)' if TEST else 'ReCarbon'
BUNDLE_ID = 'org.retronative.recarbon' + ('.test' if TEST else '')
FILE = 'ReCarbon-test' if TEST else 'ReCarbon'
BUILD = os.path.abspath(ARGS[0] if len(ARGS) > 0 else os.path.join(ROOT, 'build/main'))
OUT = os.path.abspath(ARGS[1] if len(ARGS) > 1 else os.path.join(ROOT, 'build/app'))
BIN = os.path.join(BUILD, 'runtime/ReCarbon')
MOLTENVK = os.environ.get('MOLTENVK', '/opt/homebrew/opt/molten-vk/lib/libMoltenVK.dylib')
# The MoltenVK the releases ship (v0.1.0 and on): the Vulkan driver, so another
# version can change how the game runs and how fast. A `brew upgrade` must not
# change it silently: another is taken only when MOLTENVK_VERSION names it.
MOLTENVK_PINNED = '1.4.2'
# ReCarbon (M8): the file itself is pinned too, the keg's libMoltenVK.dylib as
# Homebrew pours it (byte-identical on the M5 and the M1). Provenance: the
# molten-vk 1.4.2 formula's arm64_golden_gate bottle, SHA-256 BOTTLE below,
# whose dylib Homebrew relocates and re-signs on pouring into this file.
# Another file is taken only when MOLTENVK_SHA256 names it.
MOLTENVK_SHA256_PINNED = 'e21bceade8d0638bb2bc041f3166fbd54da19fa0958d8bbaff584afd7958d542'
MOLTENVK_BOTTLE = 'molten-vk 1.4.2 bottle arm64_golden_gate, SHA-256 0de19dbda466d4dabcfdd57963f81764e6f33447b723623683a0a3ddde954031'
# Projects compiled into a bundled library that come with their own license:
# their files go next to the library's (LICENSES/ has them).
EMBEDDED = {'molten-vk': [('cereal, BSD-3-Clause', 'LICENSES/cereal-BSD-3-Clause.txt', 'cereal-LICENSE.txt')]}
# ReCarbon (gate G2): the LGPL-2.1 libraries that come from neither a build_deps.sh prefix nor a
# Homebrew keg: libmspack's LZX decoder, which the game's build makes (runtime/CMakeLists.txt), and
# in a development build FFmpeg from tools/ffmpeg-xma (scripts/setup_tools.sh). Their license files
# come from this repository: (file name, project, [(file in the repository, name in licenses/<project>/)]).
OWN_LIBRARIES = [
    (r'librecarbon_lzx\.dylib', 'libmspack', [('LICENSES/libmspack-LGPL-2.1.txt', 'COPYING.LIB')]),
    (r'lib(?:avcodec|avutil)\.\d+\.dylib', 'FFmpeg', [('LICENSES/FFmpeg-LGPL-2.1.txt', 'COPYING.LGPLv2.1')]
     + [(os.path.join('patches/ffmpeg', f), f) for f in sorted(os.listdir(os.path.join(ROOT, 'patches/ffmpeg')))
        if f.endswith('.patch')]),
]
ICON = os.path.join(ROOT, 'scripts/macos/AppIcon.png')
ICON_SIZES = (16, 32, 128, 256, 512)  # each at @1x and @2x
# A folder's Finder info: 32 bytes, its Finder flags the big-endian 16 bits at
# offset 8, kHasCustomIcon among them.
FINDER_INFO = 'com.apple.FinderInfo'
HAS_CUSTOM_ICON = 0x0400
# The dmg's Finder window (its layout in the volume's .DS_Store): the content's size in points,
# where it opens on screen, the icons' size, and each icon's centre from the content's top left.
# scripts/macos/make_dmg_background.swift draws the background (DmgBackground.png, @2x for
# Retina) to these: the arrow between the two icons. check_app.py checks a dmg against them.
DMG_WINDOW = (640, 400)
DMG_WINDOW_ORIGIN = (200, 120)
DMG_TITLE_BAR = 32  # Finder's window bounds include its title bar (32 points on macOS 27)
DMG_ICON_SIZE = 128
DMG_ICONS = {'ReCarbon.app': (170, 190), 'Applications': (470, 190)}
DMG_BACKGROUND = '.background/background.tiff'
# Where the read-write volume is mounted while its layout is written: where a player's Mac mounts
# the dmg. The picture's alias and bookmark record the volume's path, so it's this one, not a
# folder of the build machine's.
DMG_MOUNT = '/Volumes/ReCarbon'
DMG_BACKGROUND_PNGS = [os.path.join(ROOT, 'scripts/macos', f) for f in ('DmgBackground.png', 'DmgBackground@2x.png')]
# ds_store and mac_alias (scripts/macos/dmg-requirements.txt), from scripts/setup_tools.sh.
DMG_PYTHON = os.path.join(ROOT, 'tools/dmg-python')


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def make_icns(png, icns):
    """The .icns of `png` at every size macOS asks for (icon_<n>x<n>[@2x].png)."""
    with tempfile.TemporaryDirectory() as tmp:
        iconset = os.path.join(tmp, 'AppIcon.iconset')
        os.makedirs(iconset)
        for size in ICON_SIZES:
            for scale in (1, 2):
                px = size * scale
                name = f'icon_{size}x{size}' + ('@2x' if scale == 2 else '') + '.png'
                run('sips', '-z', str(px), str(px), png, '--out', os.path.join(iconset, name))
        run('iconutil', '-c', 'icns', iconset, '-o', icns)


def finder_info(path):
    """`path`'s Finder info (32 bytes, zeros when it has none)."""
    r = subprocess.run(['xattr', '-px', FINDER_INFO, path], capture_output=True, text=True)
    return bytes.fromhex(''.join(r.stdout.split())).ljust(32, b'\0')[:32] if r.returncode == 0 else bytes(32)


def has_custom_icon(path):
    return int.from_bytes(finder_info(path)[8:10], 'big') & HAS_CUSTOM_ICON != 0


def set_custom_icon(path):
    """The custom-icon flag on a folder (a volume's root: it shows .VolumeIcon.icns), the rest of its
    Finder info kept."""
    info = bytearray(finder_info(path))
    info[8:10] = (int.from_bytes(info[8:10], 'big') | HAS_CUSTOM_ICON).to_bytes(2, 'big')
    run('xattr', '-wx', FINDER_INFO, info.hex(), path)


def deps(path):
    """The libraries `path` links (not the system's), as written in it."""
    lines = run('otool', '-L', path).splitlines()[1:]
    names = [l.strip().split(' (')[0] for l in lines]
    own = run('otool', '-D', path).splitlines()[1:]  # a library lists itself first
    return [n for n in names if n not in own and not n.startswith(('/usr/lib/', '/System/'))]


def rpaths(path):
    out = run('otool', '-l', path)
    return re.findall(r'cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.+?) \(offset', out)


def resolve(ref, referrer):
    """A dependency's file: an absolute path, or @rpath/@loader_path ones."""
    if ref.startswith('@rpath/'):
        for rp in rpaths(referrer):
            rp = rp.replace('@loader_path', os.path.dirname(referrer)).replace('@executable_path', os.path.dirname(BIN))
            cand = os.path.join(rp, ref[len('@rpath/'):])
            if os.path.exists(cand):
                return cand
        raise SystemExit(f'make_app.py: {ref} (from {referrer}) not found in its rpaths')
    if ref.startswith('@loader_path/'):
        return os.path.join(os.path.dirname(referrer), ref[len('@loader_path/'):])
    return ref


def cache_value(key):
    """A value from the build's CMakeCache.txt, or ''."""
    try:
        with open(os.path.join(BUILD, 'CMakeCache.txt')) as f:
            for line in f:
                if line.startswith(key + ':'):
                    return line.split('=', 1)[1].strip()
    except OSError:
        pass
    return ''


def build_identity():
    """(version, commit, dirty) as the build compiled them in (build_info.inc)."""
    info = {}
    try:
        with open(os.path.join(BUILD, 'runtime/generated/build_info.inc')) as f:
            for m in re.finditer(r'#define RECARBON_BUILD_(\w+) "?([^"\n]*)"?', f.read()):
                info[m.group(1)] = m.group(2)
    except OSError:
        pass
    version = info.get('VERSION') or cache_value('CMAKE_PROJECT_VERSION')
    if not re.fullmatch(r'\d+\.\d+\.\d+', version or ''):
        raise SystemExit(f'make_app.py: no version in {BUILD} (project() in CMakeLists.txt; build first)')
    return version, info.get('COMMIT', 'unknown'), info.get('DIRTY', '0') == '1'


def build_number(commit):
    """CFBundleVersion: the commit's number in its history (an integer that
    grows from one release to the next), 0 when git can't tell."""
    out = subprocess.run(['git', '-C', cache_value('CMAKE_HOME_DIRECTORY') or ROOT, 'rev-list', '--count', commit],
                         capture_output=True, text=True).stdout.strip()
    return out if out.isdigit() else '0'


def version_tuple(v):
    """'15.0' -> (15, 0, 0), comparable."""
    v = tuple(int(x) for x in str(v).split('.'))
    return v + (0,) * (3 - len(v))


def minos_all(path):
    """The oldest macOS each Mach-O in a file runs on (LC_BUILD_VERSION or the
    older LC_VERSION_MIN_MACOSX): one for a binary or library, one per member
    of a static archive."""
    found, field = [], None
    for line in run('otool', '-l', path).splitlines():
        line = line.strip()
        if line.startswith('cmd '):
            field = {'cmd LC_BUILD_VERSION': 'minos', 'cmd LC_VERSION_MIN_MACOSX': 'version'}.get(line)
        elif field and line.startswith(field + ' '):
            found.append(version_tuple(line.split()[1]))
            field = None
    return found


def minos(path):
    """The oldest macOS a Mach-O file runs on, as a tuple ((0, 0, 0) if it
    doesn't say)."""
    found = minos_all(path)
    return max(found) if found else (0, 0, 0)


def project_default_target():
    """CMakeLists.txt's default deployment target (SB_OSX_DEFAULT_TARGET), or None."""
    try:
        with open(os.path.join(cache_value('CMAKE_HOME_DIRECTORY') or ROOT, 'CMakeLists.txt')) as f:
            m = re.search(r'set\(SB_OSX_DEFAULT_TARGET "([0-9.]+)"\)', f.read())
            return m.group(1) if m else None
    except OSError:
        return None


def moltenvk_version():
    """The version of the MoltenVK to bundle (its Homebrew keg's, or
    MOLTENVK_VERSION's), refused unless it's the pinned one or
    MOLTENVK_VERSION names it."""
    k = keg(MOLTENVK)
    found = k.split('/')[-1] if k else None
    wanted = os.environ.get('MOLTENVK_VERSION')
    if found and wanted and found != wanted:
        raise SystemExit(f'make_app.py: {MOLTENVK} is MoltenVK {found}, not MOLTENVK_VERSION={wanted}')
    version = found or wanted
    if version != MOLTENVK_PINNED and not wanted:
        raise SystemExit(f'make_app.py: {MOLTENVK} is MoltenVK {version or "of an unknown version (not a Homebrew keg)"}, '
                         f'not {MOLTENVK_PINNED}, the one the releases ship (the Vulkan driver: another can change how the '
                         f'game runs). To bundle it anyway, set MOLTENVK_VERSION=<its version>.')
    return version


def moltenvk_checked():
    """The SHA-256 of the MoltenVK to bundle, refused unless it's the pinned
    file or MOLTENVK_SHA256 names it."""
    with open(os.path.realpath(MOLTENVK), 'rb') as f:
        found = hashlib.sha256(f.read()).hexdigest()
    wanted = os.environ.get('MOLTENVK_SHA256', MOLTENVK_SHA256_PINNED)
    if found != wanted:
        raise SystemExit(f'make_app.py: {MOLTENVK} has SHA-256 {found}, not {wanted}'
                         + ('' if 'MOLTENVK_SHA256' in os.environ else ' (the pinned file). To bundle it anyway, '
                            'set MOLTENVK_SHA256=<its SHA-256>.'))
    return found


def keg(path):
    """The Homebrew keg a library file is in (/opt/homebrew/Cellar/<name>/<version>), or None."""
    m = re.match(r'(.*/Cellar/[^/]+/[^/]+)/', os.path.realpath(path))
    return m.group(1) if m else None


DEPS_RECORD = 'share/recarbon-deps.json'   # written by scripts/macos/build_deps.sh


def deps_prefix(path):
    """The scripts/macos/build_deps.sh prefix a library file is in (<prefix>/lib/<file>, or
    <prefix>/ffmpeg-xma/lib/<file>), or None."""
    d = os.path.dirname(os.path.realpath(path))
    for prefix in (os.path.dirname(d), os.path.dirname(os.path.dirname(d))):
        if os.path.isfile(os.path.join(prefix, DEPS_RECORD)):
            return prefix
    return None


def copy_licenses(resources, libraries):
    """Contents/Resources/licenses: ReCarbon's notices and every bundled
    library's own license files: from the build_deps.sh prefix it was built
    in (with the project, version, source and SHA-256 it was built from),
    from its Homebrew keg, or (OWN_LIBRARIES) from this repository. A
    prefix's projects that the game and the libraries are built with but
    that aren't a library of their own here (the Vulkan and SPIR-V headers)
    get their license files too."""
    licenses = os.path.join(resources, 'licenses')
    os.makedirs(licenses)
    for name in ('COPYING', 'NOTICE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(os.path.join(ROOT, name), licenses)
    shutil.copytree(os.path.join(ROOT, 'LICENSES'), os.path.join(licenses, 'LICENSES'))
    lines = ['ReCarbon is free software under the GPL-3.0-or-later (COPYING, NOTICE). What it is',
             'built from and with, and each license, is in THIRD_PARTY_NOTICES.md and LICENSES/.', '',
             'The libraries in Contents/Frameworks, with their own license files here:']
    records = {}   # prefix -> its build_deps.sh record
    used = set()   # (prefix, project) whose license files are here

    def record(prefix):
        if prefix not in records:
            with open(os.path.join(prefix, DEPS_RECORD)) as f:
                records[prefix] = json.load(f)
        return records[prefix]

    def copy_project(prefix, project):
        src = os.path.join(prefix, 'share/licenses', project)
        dest = os.path.join(licenses, project)
        files = record(prefix)['projects'][project]['licenses']
        if (prefix, project) not in used:
            if os.path.exists(dest):
                raise SystemExit(f'make_app.py: two sources of license files for {project}')
            for f in files:
                os.makedirs(os.path.dirname(os.path.join(dest, f)), exist_ok=True)
                shutil.copy2(os.path.join(src, f), os.path.join(dest, f))
            used.add((prefix, project))
        return files

    def describe(prefix, project, built=True):
        r = record(prefix); j = r['projects'][project]
        built = f', built for macOS {r["deployment_target"]}+ ({r["architecture"]})' if built else ''
        return f'{project} {j["version"]}{built}, from {j["url"]} (SHA-256 {j["sha256"]})'

    missing = []
    for lib, src in sorted(libraries.items()):
        found = []
        prefix = deps_prefix(src)
        k = keg(src)
        if prefix:
            projects = record(prefix)['projects']
            project = next((n for n, j in projects.items()
                            if lib in j['libraries'] or os.path.basename(os.path.realpath(src)) in j['libraries']), None)
            if project:
                found = copy_project(prefix, project)
                lines.append(f'  {lib}: {describe(prefix, project)}; license files in {project}/: {", ".join(found)}')
            else:
                lines.append(f'  {lib}: from {src} (not in {os.path.join(prefix, DEPS_RECORD)})')
        elif k:
            package, version = k.split('/')[-2:]
            dest = os.path.join(licenses, package)
            candidates = [os.path.join(k, f) for f in os.listdir(k)
                          if re.fullmatch(r'(LICENSE|COPYING|NOTICE)([.-](txt|md|rst|LIB|L?GPL\S*))?', f, re.I)]
            share = os.path.join(k, 'share/licenses')
            if os.path.isdir(share):
                for d, _, files in os.walk(share):
                    candidates += [os.path.join(d, f) for f in files]
            for c in candidates:
                if os.path.isfile(c):
                    os.makedirs(dest, exist_ok=True)
                    target = os.path.join(dest, os.path.basename(c))
                    if not os.path.exists(target):
                        shutil.copy2(c, target)
                    found.append(os.path.basename(c))
            embedded = []
            for name, text, file in EMBEDDED.get(package, []) if found else []:
                shutil.copy2(os.path.join(ROOT, text), os.path.join(dest, file))
                embedded.append(f'{name} ({file})')
            provenance = ''
            if package == 'molten-vk':
                with open(os.path.realpath(src), 'rb') as f:
                    provenance = f' [{MOLTENVK_BOTTLE}; this file SHA-256 {hashlib.sha256(f.read()).hexdigest()} before bundling]'
            lines.append(f'  {lib}: {package} {version}, from Homebrew{provenance} '
                         f'({", ".join(sorted(set(found))) or "no license file found"})'
                         + (f'; it includes {", ".join(embedded)}' if embedded else ''))
        elif own_library(lib):
            project, files = own_library(lib)
            for path, name in files:
                os.makedirs(os.path.join(licenses, project), exist_ok=True)
                shutil.copy2(os.path.join(ROOT, path), os.path.join(licenses, project, name))
                found.append(name)
            lines.append(f'  {lib}: {own_description(lib, src)} (LGPL-2.1); license files in {project}/: {", ".join(found)}')
        else:
            lines.append(f'  {lib}: from {src} (neither a build_deps.sh prefix nor a Homebrew keg)')
        if not found:
            missing.append(lib)

    # What the game and those libraries are built with from the same prefixes
    # (the headers of a prefix that a bundled library came from). (A project
    # with libraries of its own that aren't in the app isn't in it at all.)
    library_prefixes = {prefix for prefix, _ in used}
    others = [(prefix, project) for prefix in sorted(records) for project, j in records[prefix]['projects'].items()
              if (prefix, project) not in used and not j['libraries'] and prefix in library_prefixes]
    if others:
        lines += ['', 'Built into the game and the libraries above, with their license files here:']
        for prefix, project in others:
            files = copy_project(prefix, project)
            lines.append(f'  {describe(prefix, project, built=False)}: headers; license files in {project}/: {", ".join(files)}')
    with open(os.path.join(licenses, 'README.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    if missing:
        raise SystemExit('make_app.py: no license file found for ' + ', '.join(missing))


def own_library(lib):
    """(project, [(file in the repository, name)]) of an OWN_LIBRARIES library, or None."""
    return next(((project, files) for pattern, project, files in OWN_LIBRARIES if re.fullmatch(pattern, lib)), None)


def own_description(lib, src):
    """What an OWN_LIBRARIES library is built from, for licenses/README.txt."""
    if lib.startswith('librecarbon_lzx'):
        mspack = os.path.join(cache_value('CMAKE_HOME_DIRECTORY') or ROOT, 'tools/XenonRecomp/thirdparty/libmspack')
        rev = subprocess.run(['git', '-C', mspack, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
        return (f'libmspack\'s LZX decoder (lzxd.c) at {rev or "an unknown revision"} (https://github.com/kyz/libmspack, '
                f'XenonRecomp\'s submodule), built with the game as a library of its own, unmodified')
    return f'FFmpeg (scripts/build_ffmpeg_xma.sh, with patches/ffmpeg), from {os.path.dirname(src)}'


def failure(e):
    """What a failed command said (its last line), for a message."""
    said = (e.stderr or e.stdout or '').strip().splitlines()
    return said[-1] if said else f'exit {e.returncode}'


def diskutil(*args):
    """Runs `diskutil <args>`; a failure ends the script, saying which step."""
    try:
        return run('diskutil', *args)
    except subprocess.CalledProcessError as e:
        raise SystemExit(f'make_app.py: diskutil {" ".join(args)}: {failure(e)}')


def attached_disks(plist_text):
    """`diskutil image attach --plist`'s devices: [(disk, mount point or None)], the whole disk first."""
    entities = plistlib.loads(plist_text.encode()).get('system-entities', [])
    disks = [(e['dev-entry'].removeprefix('/dev/'), e.get('mount-point')) for e in entities if 'dev-entry' in e]
    if not disks or not re.fullmatch(r'disk\d+', disks[0][0]):
        raise SystemExit(f'make_app.py: diskutil image attach named no disk: {entities}')
    return disks


def dmg_libraries():
    """ds_store and mac_alias, from tools/dmg-python."""
    if DMG_PYTHON not in sys.path:
        sys.path.insert(0, DMG_PYTHON)
    try:
        import ds_store, mac_alias
    except ImportError:
        raise SystemExit(f'make_app.py: no ds_store and mac_alias in {DMG_PYTHON} (the dmg\'s Finder layout): '
                         f'run scripts/setup_tools.sh')
    return ds_store, mac_alias


def make_dmg_background(tiff):
    """The window's background: DmgBackground.png and its @2x in one TIFF, which Finder shows at
    the screen's resolution."""
    run('tiffutil', '-cathidpicheck', *DMG_BACKGROUND_PNGS, '-out', tiff)


def finder_layout(volume):
    """The Finder window of the mounted volume at `volume`, in its .DS_Store: icon view at
    DMG_WINDOW's size, no toolbar, sidebar, path or status bar, the background picture, and the
    app and Applications icons at DMG_ICONS. The background's alias and bookmark name the file on
    this volume (its name, creation date and the file's ID), so they're made here, on the
    read-write volume, not in the stage."""
    ds_store, mac_alias = dmg_libraries()
    background = os.path.join(volume, DMG_BACKGROUND)
    (x, y), (w, h) = DMG_WINDOW_ORIGIN, (DMG_WINDOW[0], DMG_WINDOW[1] + DMG_TITLE_BAR)
    with ds_store.DSStore.open(os.path.join(volume, '.DS_Store'), 'w+') as d:
        d['.']['vSrn'] = ('long', 1)
        d['.']['icvl'] = ('type', b'icnv')
        d['.']['vstl'] = ('type', b'icnv')
        d['.']['bwsp'] = {
            'WindowBounds': f'{{{{{x}, {y}}}, {{{w}, {h}}}}}', 'ShowToolbar': False, 'ShowSidebar': False,
            'ContainerShowSidebar': False, 'ShowPathbar': False, 'ShowStatusBar': False, 'ShowTabView': False,
            'PreviewPaneVisibility': False, 'SidebarWidth': 0,
        }
        d['.']['icvp'] = {
            'viewOptionsVersion': 1, 'backgroundType': 2,
            'backgroundImageAlias': mac_alias.Alias.for_file(background).to_bytes(),
            'backgroundColorRed': 0.039, 'backgroundColorGreen': 0.047, 'backgroundColorBlue': 0.059,  # the picture's bottom
            'iconSize': float(DMG_ICON_SIZE), 'textSize': 13.0, 'labelOnBottom': True, 'arrangeBy': 'none',
            'gridSpacing': 100.0, 'gridOffsetX': 0.0, 'gridOffsetY': 0.0, 'showIconPreview': False,
            'showItemInfo': False, 'scrollPositionX': 0.0, 'scrollPositionY': 0.0,
        }
        d['.']['pBBk'] = mac_alias.Bookmark.for_file(background)
        for name, position in DMG_ICONS.items():
            d[name]['Iloc'] = position


def dmg_with_diskutil(stage, dmg, tmp):
    """The dmg hdiutil makes from `stage`, made with `diskutil image`: a blank
    image, a GPT disk with one HFS+ volume named ReCarbon on it, the
    stage copied in, the volume icon's flag and the Finder layout set, then
    converted to UDZO."""
    size = 0
    for root, dirs, files in os.walk(stage):
        for n in dirs + files:
            size += (os.lstat(os.path.join(root, n)).st_size + 4095) // 4096 * 4096
    size = (size + size // 10 + (8 << 20) + (1 << 20) - 1) // (1 << 20) << 20
    blank = os.path.join(tmp, 'blank.dmg')
    diskutil('image', 'create', 'blank', '--fs', 'None', '--size', str(size), blank)
    disk = attached_disks(diskutil('image', 'attach', '--plist', '--noMount', '--nobrowse', blank))[0][0]
    try:
        # Mac OS Extended, not journaled, as hdiutil's -fs HFS+. It mounts the
        # new volume in /Volumes for a moment; it's filled at DMG_MOUNT, out of
        # Finder's sight (nobrowse).
        diskutil('partitionDisk', '/dev/' + disk, '1', 'GPT', 'HFS+', 'ReCarbon', '100%')
        volume = disk + 's1'
        info = plistlib.loads(diskutil('info', '-plist', volume).encode())
        if info.get('FilesystemType') != 'hfs' or info.get('VolumeName') != 'ReCarbon':
            raise SystemExit(f'make_app.py: {volume} is {info.get("FilesystemType")} "{info.get("VolumeName")}", not hfs "ReCarbon"')
        if info.get('MountPoint'):
            diskutil('unmount', volume)
        diskutil('mount', 'nobrowse', volume)
        mnt = plistlib.loads(diskutil('info', '-plist', volume).encode()).get('MountPoint')
        if mnt != DMG_MOUNT:
            raise SystemExit(f'make_app.py: {volume} mounted at {mnt}, not {DMG_MOUNT}')
        run('ditto', stage, mnt)
        os.chmod(mnt, stat.S_IMODE(os.stat(stage).st_mode))  # the volume's root as hdiutil makes it
        set_custom_icon(mnt)
        finder_layout(mnt)
        shutil.rmtree(os.path.join(mnt, '.fseventsd'), ignore_errors=True)  # the event log macOS starts at mount
        diskutil('unmount', volume)
    finally:
        subprocess.run(['diskutil', 'eject', disk], capture_output=True)
    diskutil('image', 'create', 'from', '--format', 'UDZO', blank, dmg)


def dmg_with_hdiutil(stage, dmg, tmp):
    """The dmg of `stage` made by hdiutil: read-write, the volume icon's flag
    and the Finder layout set on it, then converted to UDZO."""
    rw = os.path.join(tmp, 'rw.dmg')
    run('hdiutil', 'create', '-quiet', '-volname', 'ReCarbon', '-srcfolder', stage, '-fs', 'HFS+',
        '-format', 'UDRW', '-ov', rw)
    entities = plistlib.loads(run('hdiutil', 'attach', '-plist', '-nobrowse', '-noautoopen', rw).encode())
    mnt = next(e['mount-point'] for e in entities['system-entities'] if e.get('mount-point'))
    try:
        if mnt != DMG_MOUNT:
            raise SystemExit(f'make_app.py: the read-write volume mounted at {mnt}, not {DMG_MOUNT}')
        set_custom_icon(mnt)
        finder_layout(mnt)
        shutil.rmtree(os.path.join(mnt, '.fseventsd'), ignore_errors=True)  # the event log macOS starts at mount
    finally:
        run('hdiutil', 'detach', '-quiet', mnt)
    run('hdiutil', 'convert', '-quiet', rw, '-format', 'UDZO', '-imagekey', 'zlib-level=9', '-ov', '-o', dmg)


def check_dmg_contents(dmg, app, background, tmp):
    """The dmg, mounted read-only: the app and the Applications shortcut, the
    app's icon as the volume's (.VolumeIcon.icns and the root's custom-icon
    flag), the Finder layout (`background` as .background's only file, the
    .DS_Store placing both icons), and the app's code signature checks every
    file in it."""
    mnt = os.path.join(tmp, 'check')
    os.makedirs(mnt)
    disks = attached_disks(diskutil('image', 'attach', '--plist', '--readOnly', '--nobrowse', '--mountPoint', mnt, dmg))
    try:
        top = sorted(os.listdir(mnt))
        link = os.path.join(mnt, 'Applications')
        if top != ['.DS_Store', '.VolumeIcon.icns', '.background', 'Applications', 'ReCarbon.app'] \
                or not os.path.islink(link) or os.readlink(link) != '/Applications':
            raise SystemExit(f'make_app.py: {dmg} holds {top}, not ReCarbon.app, a shortcut to /Applications, '
                             f'.VolumeIcon.icns, .background and .DS_Store')
        folder = os.path.dirname(os.path.join(mnt, DMG_BACKGROUND))
        with open(os.path.join(mnt, DMG_BACKGROUND), 'rb') as a, open(background, 'rb') as b:
            if os.listdir(folder) != [os.path.basename(DMG_BACKGROUND)] or a.read() != b.read():
                raise SystemExit(f'make_app.py: {dmg}\'s {os.path.dirname(DMG_BACKGROUND)} is not just the background')
        ds_store, _ = dmg_libraries()
        with ds_store.DSStore.open(os.path.join(mnt, '.DS_Store'), 'r') as d:
            placed = {name: d[name]['Iloc'] for name in DMG_ICONS}
        if placed != DMG_ICONS:
            raise SystemExit(f'make_app.py: {dmg}\'s icons are at {placed}, not {DMG_ICONS}')
        with open(os.path.join(mnt, '.VolumeIcon.icns'), 'rb') as a, \
                open(os.path.join(app, 'Contents/Resources/AppIcon.icns'), 'rb') as b:
            if a.read() != b.read():
                raise SystemExit(f'make_app.py: {dmg}\'s .VolumeIcon.icns is not the app\'s AppIcon.icns')
        if not has_custom_icon(mnt):
            raise SystemExit(f'make_app.py: {dmg}\'s volume has no custom-icon flag')
        run('codesign', '--verify', '--deep', '--strict', os.path.join(mnt, 'ReCarbon.app'))
    finally:
        subprocess.run(['diskutil', 'eject', disks[0][0]], capture_output=True)


def check_dmg_with_diskutil(dmg, app, background, tmp):
    """hdiutil verify's place when diskutil made the dmg: a compressed GPT +
    HFS+ image, then its contents."""
    info = plistlib.loads(diskutil('image', 'info', '--plist', dmg).encode())
    hints = [p.get('content-hint') for p in info.get('Partitions', [])]
    if info.get('Image Format') != 'UDZO' or hints != ['GUID_partition_scheme', 'Apple_HFS']:
        raise SystemExit(f'make_app.py: {dmg} is {info.get("Image Format")} {hints}, not UDZO with one HFS+ volume')
    check_dmg_contents(dmg, app, background, tmp)


def make_dmg(app, version):
    """ReCarbon-macos.dmg: the app and a shortcut to Applications, with the
    app's icon as the volume's and the Finder layout (finder_layout). Returns
    it and the tool that made it."""
    dmg = os.path.join(OUT, 'ReCarbon-macos.dmg')
    versioned = os.path.join(OUT, f'ReCarbon-{version}-macos.dmg')
    with tempfile.TemporaryDirectory() as tmp:
        stage = os.path.join(tmp, 'ReCarbon')
        os.makedirs(stage)
        run('ditto', app, os.path.join(stage, 'ReCarbon.app'))
        os.symlink('/Applications', os.path.join(stage, 'Applications'))
        shutil.copyfile(os.path.join(app, 'Contents/Resources/AppIcon.icns'), os.path.join(stage, '.VolumeIcon.icns'))
        os.makedirs(os.path.dirname(os.path.join(stage, DMG_BACKGROUND)))
        make_dmg_background(os.path.join(stage, DMG_BACKGROUND))
        dmg_libraries()  # before anything is made
        if os.path.lexists(DMG_MOUNT):
            raise SystemExit(f'make_app.py: {DMG_MOUNT} exists (a ReCarbon dmg is open?): eject it first, the dmg\'s '
                             f'layout is written there')
        for f in (dmg, versioned):
            if os.path.exists(f):
                os.remove(f)
        tool = None
        if DMG_TOOL != 'diskutil':
            if shutil.which('hdiutil'):
                try:
                    dmg_with_hdiutil(stage, dmg, tmp)
                    tool = 'hdiutil'
                except subprocess.CalledProcessError as e:
                    if DMG_TOOL == 'hdiutil':
                        raise SystemExit(f'make_app.py: hdiutil: {failure(e)}')
                    print(f'make_app.py: hdiutil failed ({failure(e)}): making the dmg with diskutil image',
                          file=sys.stderr)
                    if os.path.exists(dmg):
                        os.remove(dmg)
            elif DMG_TOOL == 'hdiutil':
                raise SystemExit('make_app.py: DMG_TOOL=hdiutil, but this Mac has no hdiutil')
            else:
                print('make_app.py: no hdiutil on this Mac: making the dmg with diskutil image', file=sys.stderr)
        if not tool:
            dmg_with_diskutil(stage, dmg, tmp)
            tool = 'diskutil'
        os.link(dmg, versioned)
        for f in (dmg, versioned):
            digest = run('shasum', '-a', '256', f).split()[0]
            with open(f + '.sha256', 'w') as out:
                out.write(f'{digest}  {os.path.basename(f)}\n')  # sha256sum -c / shasum -a 256 -c
        if tool == 'hdiutil':
            run('hdiutil', 'verify', '-quiet', dmg)
            check_dmg_contents(dmg, app, os.path.join(stage, DMG_BACKGROUND), tmp)
        else:
            check_dmg_with_diskutil(dmg, app, os.path.join(stage, DMG_BACKGROUND), tmp)
    return dmg, tool


def main():
    if sys.platform != 'darwin':
        raise SystemExit('make_app.py: macOS only')
    if not os.access(BIN, os.X_OK):
        raise SystemExit(f'make_app.py: no {BIN}; build first')
    version, commit, dirty = build_identity()
    if dirty:
        print(f'make_app.py: warning: the build is {commit}-dirty (uncommitted changes): not for release', file=sys.stderr)
    minimum = cache_value('CMAKE_OSX_DEPLOYMENT_TARGET')
    if not re.fullmatch(r'\d+(\.\d+)*', minimum):
        raise SystemExit(f'make_app.py: no CMAKE_OSX_DEPLOYMENT_TARGET in {BUILD} (the top-level CMakeLists.txt sets one)')
    if MIN_MACOS and version_tuple(MIN_MACOS) != version_tuple(minimum):
        raise SystemExit(f'make_app.py: {BUILD} is built for macOS {minimum}, not {MIN_MACOS} (--min-macos); '
                         f'configure it with -DCMAKE_OSX_DEPLOYMENT_TARGET={MIN_MACOS}, or in a new build folder')
    default = project_default_target()
    if default and version_tuple(default) != version_tuple(minimum):
        print(f'make_app.py: warning: {BUILD} is configured for macOS {minimum}, not the project\'s {default} '
              f'(CMakeLists.txt): not for release', file=sys.stderr)
    mvk_version = moltenvk_version()
    moltenvk_checked()
    app = os.path.join(OUT, NAME + '.app')
    shutil.rmtree(app, ignore_errors=True)
    contents = os.path.join(app, 'Contents')
    macos, frameworks = os.path.join(contents, 'MacOS'), os.path.join(contents, 'Frameworks')
    resources = os.path.join(contents, 'Resources')
    icd = os.path.join(resources, 'vulkan/icd.d')
    for d in (macos, frameworks, icd):
        os.makedirs(d)
    exe = os.path.join(macos, 'ReCarbon')
    shutil.copy2(BIN, exe)
    # The full symbols for us, then the debug map out of the app's copy.
    symbols = os.path.join(OUT, 'symbols')
    os.makedirs(symbols, exist_ok=True)
    dsym = os.path.join(symbols, f'{FILE}-{version}-{commit}{"-dirty" if dirty else ""}-macos.dSYM')
    shutil.rmtree(dsym, ignore_errors=True)
    subprocess.run(['dsymutil', BIN, '-o', dsym], check=True, capture_output=True)
    run('strip', '-S', exe)

    # The closure of non-system libraries, each copied once by the name it is
    # linked as (libSDL3.0.dylib), from the file behind it. The originals are
    # resolved against the build's own binary, so the copies can be rewritten.
    origin = {exe: BIN}
    copied = {}  # name in Frameworks -> original path
    queue = [exe]
    while queue:
        f = queue.pop()
        for ref in deps(f):
            name = os.path.basename(ref)
            if name in copied:
                continue
            src = os.path.realpath(resolve(ref, origin[f]))
            dst = os.path.join(frameworks, name)
            shutil.copy2(src, dst)
            os.chmod(dst, 0o755)
            copied[name] = src
            origin[dst] = src
            queue.append(dst)
    shutil.copy2(MOLTENVK, os.path.join(frameworks, 'libMoltenVK.dylib'))
    os.chmod(os.path.join(frameworks, 'libMoltenVK.dylib'), 0o755)
    origin[os.path.join(frameworks, 'libMoltenVK.dylib')] = MOLTENVK

    # Names: every reference to a copied library becomes @rpath/<name>; the
    # game finds them in ../Frameworks, the libraries next to themselves.
    # Homebrew's rpaths go, so nothing outside the app is ever loaded.
    for f in [exe] + [os.path.join(frameworks, n) for n in list(copied) + ['libMoltenVK.dylib']]:
        args = []
        if f != exe:
            args += ['-id', '@rpath/' + os.path.basename(f)]
        for ref in deps(f):
            if os.path.basename(ref) in copied:
                args += ['-change', ref, '@rpath/' + os.path.basename(ref)]
        for rp in rpaths(f):
            args += ['-delete_rpath', rp]
        args += ['-add_rpath', '@executable_path/../Frameworks' if f == exe else '@loader_path']
        run('install_name_tool', *args, f)

    with open(os.path.join(icd, 'MoltenVK_icd.json'), 'w') as j:
        j.write('{\n    "file_format_version": "1.0.0",\n    "ICD": {\n'
                '        "library_path": "../../../Frameworks/libMoltenVK.dylib",\n'
                '        "api_version": "1.4.0",\n        "is_portability_driver": true\n    }\n}\n')

    # The icon, every size macOS asks for.
    if not os.path.isfile(ICON):
        raise SystemExit(f'make_app.py: no {ICON} (swift scripts/macos/make_icon.swift makes it)')
    make_icns(ICON, os.path.join(resources, 'AppIcon.icns'))

    # Every binary here runs on the macOS the plist promises.
    def show(v):
        return '.'.join(map(str, v[:2] if v[2] == 0 else v))
    floor = version_tuple(minimum)
    need = {f: minos(f) for f in [exe] + [os.path.join(frameworks, n) for n in os.listdir(frameworks)]}
    newer = [f'{os.path.basename(f)} ({show(v)}, from {origin[f]})' for f, v in sorted(need.items()) if v > floor]
    newest = max(need.values())
    if newer:
        brew = any('/Cellar/' in n or '/opt/homebrew/' in n for n in newer)
        raise SystemExit(
            f'make_app.py: these need a newer macOS than the build\'s {minimum}, so the app would not open on macOS '
            f'{minimum}:\n  ' + '\n  '.join(newer) + '\n'
            + ('Homebrew\'s bottles are built for the macOS they were made on. ' if brew else '')
            + f'Either build the libraries for {minimum} (how the release is built):\n'
            f'  scripts/macos/build_deps.sh <prefix>, then configure the build with -DCMAKE_PREFIX_PATH=<prefix> '
            f'-DFFMPEG_XMA=<prefix>/ffmpeg-xma and rebuild;\n'
            f'or make an app for this Mac\'s macOS and newer only: configure the build with '
            f'-DCMAKE_OSX_DEPLOYMENT_TARGET={show(newest)} and rebuild.')
    if need[exe] != floor:
        raise SystemExit(f'make_app.py: the game was built for macOS {show(need[exe])}, not {minimum}')

    copy_licenses(resources, {**copied, 'libMoltenVK.dylib': MOLTENVK})

    with open(os.path.join(contents, 'Info.plist'), 'wb') as p:
        plistlib.dump({'CFBundleIconFile': 'AppIcon',
            'CFBundleDevelopmentRegion': 'en',
            'CFBundleExecutable': 'ReCarbon',
            'CFBundleIdentifier': BUNDLE_ID,
            'CFBundleInfoDictionaryVersion': '6.0',
            'CFBundleName': NAME,
            'CFBundleDisplayName': NAME,
            'CFBundlePackageType': 'APPL',
            'CFBundleShortVersionString': version,
            'CFBundleVersion': build_number(commit),
            'ReCarbonCommit': commit + ('-dirty' if dirty else ''),
            'LSMinimumSystemVersion': minimum,
            # A game: macOS Game Mode (CPU/GPU priority, lower Bluetooth
            # latency for controllers) when it runs fullscreen.
            'LSApplicationCategoryType': 'public.app-category.racing-games',
            'GCSupportsControllerUserInteraction': True,
            'NSHighResolutionCapable': True,
            'NSHumanReadableCopyright': 'Copyright (C) 2026 ReCarbon contributors; based on SpeedBreaker, Copyright (C) '
                                        '2026 project(u) and SpeedBreaker contributors. Free software (GPL-3.0-or-later). '
                                        'Need for Speed and Carbon are trademarks of Electronic Arts; ReCarbon is not '
                                        'affiliated with Electronic Arts. It needs your own copy of the Xbox 360 game.',
        }, p)

    # Ad hoc signatures, the libraries before the app that seals them.
    for n in sorted(os.listdir(frameworks)):
        run('codesign', '--force', '--sign', '-', os.path.join(frameworks, n))
    run('codesign', '--force', '--sign', '-', app)
    run('codesign', '--verify', '--deep', '--strict', app)

    # Nothing but the system's and the app's own: no library or search path
    # outside it (Homebrew's, or the prefix the libraries were built in).
    binaries = [exe] + [os.path.join(frameworks, n) for n in os.listdir(frameworks)]
    leftover = [f'{os.path.basename(f)}: {r}' for f in binaries for r in deps(f) if not r.startswith('@rpath/')]
    leftover += [f'{os.path.basename(f)}: rpath {r}' for f in binaries for r in rpaths(f) if not r.startswith('@')]
    if leftover:
        raise SystemExit('make_app.py: still linked outside the app: ' + '; '.join(leftover))
    size = int(run('du', '-sk', app).split()[0]) // 1024
    print(f'{app} ({size} MB, v{version}, {commit}{"-dirty" if dirty else ""}, macOS {minimum}+): '
          f'{len(copied)} libraries and MoltenVK {mvk_version} in Frameworks; symbols in {dsym}')
    if DMG:
        dmg, tool = make_dmg(app, version)
        print(f'{dmg} ({os.path.getsize(dmg) // (1 << 20)} MB, made with {tool}) and {os.path.basename(dmg)}.sha256')


if __name__ == '__main__':
    main()
