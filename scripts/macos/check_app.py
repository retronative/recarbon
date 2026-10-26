#!/usr/bin/env python3
# ReCarbon. GPL-3.0-or-later (see COPYING).
# From SpeedBreaker's scripts/macos/check_app.py (GPL-3.0-or-later). ReCarbon
# changes: the names, a check of the bundle ID (org.retronative.recarbon), of the icon, and of the
# LGPL parts as separate libraries (7).
#
# check_app.py <ReCarbon.app | ReCarbon-macos.dmg> [--min-macos=<version>] [--sdk=<path>]
#              [--same-code-as=<ReCarbon.app | dmg>]
#
# The release's checks of a Mac app (scripts/macos/make_app.py), that it opens on the oldest macOS it
# says it does (default: CMakeLists.txt's SB_OSX_DEFAULT_TARGET, 15.0) and needs nothing outside itself.
# A .dmg is checked against its .sha256 (when there is one) and mounted read-only to check the app in it.
#   1. Info.plist: LSMinimumSystemVersion is that macOS. The icon: CFBundleIconFile names AppIcon, and
#      Contents/Resources/AppIcon.icns has every size macOS asks for (16 to 512 at @1x and @2x); in a
#      dmg, the volume's icon is the same file (.VolumeIcon.icns) with the root's custom-icon flag set.
#      And in a dmg (1e), make_app.py's Finder window: .background holds only background.tiff, the picture at
#      the window's size and twice it (Retina); the volume's .DS_Store opens it in icon view at that size with
#      no toolbar, sidebar, path, status or tab bar, the icons at their size and the app's and Applications'
#      at their places, and the picture's alias names that file on this volume, mounted at /Volumes/ReCarbon
#      (no build machine's folder in the .DS_Store: /private/, /var/folders/, /tmp/, /Users/). Reading it needs
#      ds_store and mac_alias in tools/dmg-python (scripts/setup_tools.sh).
#   2. Every Mach-O in the app: arm64 only, built for that macOS or an older one; the game for exactly it.
#   3. Loads nothing outside the app or the system: every library @rpath/ or /usr/lib/ or /System/, and
#      rpaths only @executable_path/../Frameworks and @loader_path; no build machine's folders in the files
#      (/opt/homebrew, /usr/local/Cellar, /Users/).
#   4. The signature verifies (codesign --deep --strict).
#   5. No system symbol it imports is newer than that macOS, weak imports included. The SDK's .tbd stubs say
#      what the newest macOS exports, not since when, so this asks the SDK's headers: each import of a system
#      library is referenced in an Objective-C file compiled for that macOS with -Werror=unguarded-availability,
#      and an import whose declaration says API_AVAILABLE(macos(x)), x newer, fails with x. Weak imports too:
#      code that calls a newer API without an @available check compiles with only a warning and links it
#      weakly, so it is null (a crash, or a nil class) on the older macOS; an @available check links it weakly
#      as well. A newer weak import passes only when GUARDED below names it, once someone has read that every
#      use of it is behind an @available check. libc++ is left to its own headers, which refuse at compile time
#      what the target lacks (LLVM 18 = macOS 15.0, LLVM 19 = 15.4). What the headers don't declare is listed
#      (in practice what the compiler emits itself: stack protection, ___chkstk_darwin, ARC's objc_* entry
#      points; it emits only what the target has). First, the control: a small library that uses three APIs
#      known to be newer (macOS 15.1 and 15.2: a libSystem function, a CoreGraphics constant, an AppKit class)
#      without @available is built for that macOS and its imports are checked the same way. All three must be
#      caught, as weak imports, or the check itself is broken. Objective-C methods are never imports: the
#      game's build (-Werror=unguarded-availability-new) and scripts/macos/build_deps.sh (any availability
#      warning in a library's build fails it) are what catch those. MoltenVK is outside both: it is
#      Homebrew's bottle (built for macOS 12.0), so its calls to Metal APIs newer than the floor rest on
#      MoltenVK's own runtime checks, and on a run on a real Apple GPU on the oldest macOS.
#   6. The code, for the record: each Mach-O's __TEXT,__text SHA-256 (its machine code; build dates and
#      commits live elsewhere, so a rebuild of the same sources at the same paths gives the same hash).
#      --same-code-as=<app | dmg>: which files' code differs from that build's (the one that was measured
#      or tested, say). Informational: it doesn't fail the check, and it is no proof of speed (a release
#      merges other work, so its game code differs from any earlier build's anyway). That proof is
#      scripts/macos/check_codegen.py, run on the release's own commit: its code for this macOS against
#      its code for macOS 26.0, function by function.
#   7. ReCarbon (alpha gate G2): the LGPL-2.1 parts are separate, replaceable libraries, never inside the
#      executable: FFmpeg's libavcodec.<major>.dylib and libavutil.<major>.dylib and libmspack's LZX decoder
#      librecarbon_lzx.dylib are each in Contents/Frameworks, linked by the executable as @rpath/<file>,
#      and define their own symbols; the executable defines none of theirs (avcodec_*, av_*, avpriv_*,
#      ff_*, lzxd_*), so a build that links any of them statically again fails.
# Needs Apple's command line tools (otool, nm, codesign, clang, the macOS SDK).
import collections, contextlib, hashlib, os, plistlib, re, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
OPTS = dict(a[2:].split('=', 1) if '=' in a else (a[2:], '') for a in sys.argv[1:] if a.startswith('--'))


def run(*args, check=True):
    return subprocess.run(args, check=check, capture_output=True, text=True).stdout


def version_tuple(v):
    v = tuple(int(x) for x in str(v).split('.'))
    return v + (0,) * (3 - len(v))


def show(v):
    return '.'.join(map(str, v[:2] if v[2] == 0 else v))


def default_target():
    # ReCarbon: as CMakeLists.txt picks it: MACOSX_DEPLOYMENT_TARGET when
    # set, else this Mac's major version (Homebrew's libraries are built for it).
    env = os.environ.get('MACOSX_DEPLOYMENT_TARGET', '')
    if env:
        return env
    return run('sw_vers', '-productVersion').strip().split('.')[0] + '.0'


def macho_files(app):
    files = []
    for d, _, names in os.walk(os.path.join(app, 'Contents')):
        for n in names:
            p = os.path.join(d, n)
            if not os.path.islink(p) and run('file', '-b', p).startswith('Mach-O'):
                files.append(p)
    return sorted(files)


def minos(path):
    """The newest macOS any Mach-O in the file needs (LC_BUILD_VERSION's minos, or the older
    LC_VERSION_MIN_MACOSX), (0, 0, 0) if none says."""
    found, field = [], None
    for line in run('otool', '-l', path).splitlines():
        line = line.strip()
        if line.startswith('cmd '):
            field = {'cmd LC_BUILD_VERSION': 'minos', 'cmd LC_VERSION_MIN_MACOSX': 'version'}.get(line)
        elif field and line.startswith(field + ' '):
            found.append(version_tuple(line.split()[1]))
            field = None
    return max(found) if found else (0, 0, 0)


def libraries(path):
    lines = run('otool', '-L', path).splitlines()[1:]
    own = run('otool', '-D', path).splitlines()[1:]
    return [l.strip().split(' (')[0] for l in lines if l.strip().split(' (')[0] not in own]


def rpaths(path):
    return re.findall(r'cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.+?) \(offset', run('otool', '-l', path))


# --- 5: imports against the SDK's availability annotations -------------------------------------------
LIBSYSTEM_HEADERS = '''stdlib.h stdio.h string.h strings.h stdint.h inttypes.h math.h time.h errno.h signal.h unistd.h
fcntl.h dirent.h dlfcn.h locale.h xlocale.h wchar.h wctype.h ctype.h setjmp.h stdarg.h assert.h pthread.h pthread/qos.h
sched.h spawn.h execinfo.h libgen.h glob.h fnmatch.h getopt.h poll.h syslog.h pwd.h grp.h termios.h utime.h copyfile.h
libproc.h notify.h removefile.h malloc/malloc.h libkern/OSAtomic.h libkern/OSCacheControl.h mach/mach.h mach/mach_time.h
mach/mach_vm.h mach/thread_act.h mach/thread_policy.h mach/vm_map.h mach-o/dyld.h mach-o/getsect.h dispatch/dispatch.h
os/log.h os/lock.h os/signpost.h os/proc.h os/activity.h os/workgroup.h os/os_sync_wait_on_address.h sys/types.h
sys/mman.h sys/sysctl.h sys/stat.h sys/statvfs.h sys/time.h sys/resource.h sys/ioctl.h sys/event.h sys/socket.h
sys/select.h sys/wait.h sys/param.h sys/mount.h sys/attr.h sys/xattr.h sys/uio.h sys/file.h sys/utsname.h sys/un.h
sys/random.h sys/clonefile.h sys/qos.h sys/kdebug_signpost.h netinet/in.h arpa/inet.h netdb.h ifaddrs.h net/if.h
uuid/uuid.h asl.h xpc/xpc.h libunwind.h unwind.h timingsafe.h langinfo.h crt_externs.h CommonCrypto/CommonDigest.h
objc/runtime.h objc/message.h objc/objc-exception.h objc/objc-sync.h'''.split()
FRAMEWORK_HEADERS = {
    'IOKit': ['IOKit/IOKitLib.h', 'IOKit/hid/IOHIDManager.h', 'IOKit/hid/IOHIDLib.h', 'IOKit/pwr_mgt/IOPMLib.h',
              'IOKit/graphics/IOGraphicsLib.h', 'IOKit/IOCFPlugIn.h', 'IOKit/usb/IOUSBLib.h', 'IOKit/ps/IOPowerSources.h',
              'IOKit/ps/IOPSKeys.h', 'IOKit/serial/IOSerialKeys.h']}
# The control's three APIs, newer than 15.0: (name, library, macOS version, a pointer to it).
CONTROLS = [('timingsafe_enable_if_supported', 'libSystem', '15.2', '(const void *)&timingsafe_enable_if_supported'),
            ('kCGUseLegacyHDREcosystem', 'CoreGraphics', '15.1', '(const void *)&kCGUseLegacyHDREcosystem'),
            ('NSWritingToolsCoordinatorContext', 'AppKit', '15.2',
             '(__bridge const void *)[NSWritingToolsCoordinatorContext class]')]
# Weak imports newer than the floor that are known to sit behind an @available check in every use:
# {name: (macOS version, where the checks are)}. Add one only after reading each use. (None in v0.1.1.)
GUARDED = {}


def system_imports(path):
    """[(symbol, weak, library short name)] of a file's imports from system libraries."""
    system = set()
    for p in libraries(path):
        if p.startswith(('/usr/lib/', '/System/')):
            leaf = os.path.basename(p)
            system.add(re.sub(r'(\.[A-Za-z0-9]+)*\.dylib$', '', leaf) if leaf.endswith('.dylib') else leaf)
    res = []
    for line in run('nm', '-m', '-u', path).splitlines():
        m = re.match(r'\s*\(undefined\) (weak )?external (\S+) \(from ([^)]+)\)', line)
        if m and m.group(3) in system:
            res.append((m.group(2), bool(m.group(1)), m.group(3)))
    return res


def reference(sym):
    m = re.match(r'_OBJC_(?:META)?CLASS_\$_(\w+)$', sym)
    if m:
        return m.group(1), f'(void)[{m.group(1)} class];'
    if sym.startswith(('_OBJC_EHTYPE_$_', '_OBJC_IVAR_$_', '__Z')) or not sym.startswith('_'):
        return None
    name = sym[1:].split('$')[0]  # _fopen$DARWIN_EXTSN -> fopen
    return (name, f'(void)&{name};') if re.fullmatch(r'[A-Za-z_]\w*', name) else None


def probe(sdk, minimum, wanted, libs):
    """Compiles references to `wanted` ({name: statement}) for `minimum`: ({name: newer version}, {undeclared})."""
    includes = list(LIBSYSTEM_HEADERS)
    for lib in sorted(libs):
        if lib not in ('libSystem', 'libobjc', 'libc++', 'libc++abi'):
            includes += FRAMEWORK_HEADERS.get(lib, [f'{lib}/{lib}.h'])
    lines = []
    for h in includes:
        lines += [f'#if __has_include(<{h}>)', f'#import <{h}>', '#endif']
    lines.append('void probe(void) {')
    at = {}
    for name, stmt in wanted.items():
        lines.append(f'  {stmt}')
        at[len(lines)] = name
    lines.append('}')
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'probe.m')
        with open(src, 'w') as f:
            f.write('\n'.join(lines) + '\n')
        r = subprocess.run(['xcrun', 'clang', '-x', 'objective-c', '-fsyntax-only', '-fobjc-arc', '-fmodules', '-arch', 'arm64',
                            '-isysroot', sdk, f'-mmacosx-version-min={minimum}', '-Wunguarded-availability',
                            '-Wunguarded-availability-new', '-Werror=unguarded-availability',
                            '-Werror=unguarded-availability-new', '-Wno-deprecated-declarations', '-ferror-limit=0', src],
                           capture_output=True, text=True)
    newer, undeclared = {}, set()
    for line in r.stderr.splitlines():
        m = re.match(r'.*probe\.m:(\d+):\d+: (?:error|warning): (.*)', line)
        name = at.get(int(m.group(1))) if m else None
        if not name:
            continue
        a = re.search(r'is only available on macOS ([0-9.]+) or newer', m.group(2))
        if a:
            newer[name] = a.group(1)
        elif re.search(r'undeclared|must be imported|unknown receiver', m.group(2)):
            undeclared.add(name)
    return newer, undeclared


def control_imports(sdk, minimum):
    """The control library: CONTROLS' APIs used without @available, built for `minimum` the way any library of
    the app is (clang only warns, and links them weakly). Its system imports, or None and the compiler's output."""
    src = ['#import <AppKit/AppKit.h>', '#import <CoreGraphics/CoreGraphics.h>', '#include <timingsafe.h>',
           f'const void *sb_control[{len(CONTROLS)}];', 'void sb_control_fill(void) {']
    src += [f'  sb_control[{i}] = {expr};' for i, (_, _, _, expr) in enumerate(CONTROLS)] + ['}']
    with tempfile.TemporaryDirectory() as tmp:
        m, lib = os.path.join(tmp, 'control.m'), os.path.join(tmp, 'libcontrol.dylib')
        with open(m, 'w') as f:
            f.write('\n'.join(src) + '\n')
        r = subprocess.run(['xcrun', 'clang', '-x', 'objective-c', '-dynamiclib', '-fobjc-arc', '-arch', 'arm64',
                            '-isysroot', sdk, f'-mmacosx-version-min={minimum}', '-Wno-unguarded-availability-new',
                            '-framework', 'AppKit', '-framework', 'CoreGraphics', '-o', lib, m],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return None, r.stderr.strip()
        return system_imports(lib), ''


def assess(imports, minimum, sdk):
    """imports: [(user, symbol, weak, library)]. -> (names checked, {name: users}, {name: every import of it
    weak}, {name: newer version}, {undeclared names})."""
    wanted, users, weak, libs = {}, collections.defaultdict(set), {}, set()
    for user, sym, is_weak, lib in imports:
        r = reference(sym)
        if r:
            wanted[r[0]] = r[1]
            users[r[0]].add(f'{user} ({lib})')
            weak[r[0]] = weak.get(r[0], True) and is_weak
            libs.add(lib)
    newer, undeclared = probe(sdk, minimum, wanted, libs)
    return wanted, users, weak, newer, undeclared


def check_imports(files, minimum, sdk):
    ok = True
    # ReCarbon: the control's APIs are newer than SpeedBreaker's 15.0 target.
    # A target at or past all of them (ReCarbon's default is this Mac's macOS)
    # leaves nothing for the control to catch: say so instead of failing.
    if all(version_tuple(minimum) >= version_tuple(v) for _, _, v, _ in CONTROLS):
        print(f'  control: not applicable for macOS {minimum} (its APIs are from macOS '
              f'{", ".join(sorted({v for _, _, v, _ in CONTROLS}))}); the scan below is not proven by it')
        control = None
    else:
        control, err = control_imports(sdk, minimum)
        if control is None:
            print(f'  FAIL: the control library didn\'t build: {err}')
            return False
    if control is not None:
        ok = check_control(control, minimum, sdk) and ok
        if not ok:
            return False
    return scan_imports(files, minimum, sdk, ok)


def check_control(control, minimum, sdk):
    _, _, cweak, cnewer, _ = assess([('control', s, w, l) for s, w, l in control], minimum, sdk)
    caught = {n: (cnewer.get(n), cweak.get(n)) for n, _, _, _ in CONTROLS}
    if any(caught[n] != (v, True) for n, _, v, _ in CONTROLS):
        print(f'  FAIL: the check doesn\'t catch the control\'s newer weak imports (it found {caught}): fix check_app.py')
        return False
    print(f'  control: {", ".join(f"{n} (macOS {v}, weak)" for n, (v, _) in caught.items())} caught')
    return True


def scan_imports(files, minimum, sdk, ok):
    imports = [(os.path.basename(f), s, w, l) for f in files for s, w, l in system_imports(f)
               if l not in ('libc++', 'libc++abi')]
    wanted, users, weak, newer, undeclared = assess(imports, minimum, sdk)
    print(f'  {len(imports)} system imports ({sum(w for _, _, w, _ in imports)} weak); {len(wanted)} distinct names, '
          f'{len(wanted) - len(undeclared)} checked against their declarations, newer than macOS {minimum}: {len(newer)}')
    for name, v in sorted(newer.items()):
        who, g = ', '.join(sorted(users[name])), GUARDED.get(name)
        if weak[name] and g and version_tuple(g[0]) == version_tuple(v):
            print(f'  guarded: {name} (macOS {v}, weak), imported by {who}: {g[1]}')
        elif weak[name]:
            print(f'  FAIL: {name} is macOS {v}+, imported weakly by {who}: null before macOS {v}, so every use must be '
                  f'behind an @available check (once read, list it in check_app.py\'s GUARDED)')
            ok = False
        else:
            print(f'  FAIL: {name} is macOS {v}+, imported by {who} (not weakly: the app wouldn\'t open before {v})')
            ok = False
    for name in sorted(set(GUARDED) - set(newer)):
        print(f'  note: GUARDED names {name}, which no file imports as newer than macOS {minimum} (remove it?)')
    if undeclared:
        print(f'  not declared in the SDK\'s headers (the compiler\'s own entry points; not checked): '
              f'{", ".join(sorted(undeclared))}')
    return ok


# --- 7: the LGPL parts as separate libraries ----------------------------------------------------------
# (what, the file in Contents/Frameworks, a symbol it must define and the executable import)
LGPL_LIBRARIES = [('FFmpeg libavcodec', r'libavcodec\.\d+\.dylib', '_avcodec_send_packet'),
                  ('FFmpeg libavutil', r'libavutil\.\d+\.dylib', '_av_frame_alloc'),
                  ('libmspack LZX', r'librecarbon_lzx\.dylib', '_lzxd_decompress')]
LGPL_SYMBOL = re.compile(r'_(?:avcodec|avutil|avpriv|av|ff|swr|lzxd)_\w')


def defined_symbols(path):
    """The names of the symbols a Mach-O file defines, local ones included."""
    return run('nm', '-U', '-j', path).split()


def lgpl_problems(app, exe):
    """What fails check 7, as lines (none: the LGPL parts are separate libraries)."""
    problems = []
    frameworks = os.path.join(app, 'Contents/Frameworks')
    present = sorted(os.listdir(frameworks)) if os.path.isdir(frameworks) else []
    linked = [os.path.basename(l) for l in libraries(exe) if l.startswith('@rpath/')]
    imported = set(run('nm', '-u', '-j', exe).split())
    for what, pattern, symbol in LGPL_LIBRARIES:
        files = [f for f in present if re.fullmatch(pattern, f) and not os.path.islink(os.path.join(frameworks, f))]
        if len(files) != 1:
            problems.append(f'{what}: {len(files)} files matching {pattern} in Contents/Frameworks, not 1')
            continue
        if files[0] not in linked:
            problems.append(f'{what}: the executable doesn\'t link @rpath/{files[0]}')
        if symbol not in defined_symbols(os.path.join(frameworks, files[0])):
            problems.append(f'{what}: {files[0]} doesn\'t define {symbol}')
        if symbol not in imported:
            problems.append(f'{what}: the executable doesn\'t import {symbol}')
    inside = sorted({s for s in defined_symbols(exe) if LGPL_SYMBOL.match(s)})
    if inside:
        problems.append(f'the executable defines {len(inside)} symbols of the LGPL parts (linked statically?): '
                        + ', '.join(inside[:8]) + (' ...' if len(inside) > 8 else ''))
    return problems


# --- 6: the code ---------------------------------------------------------------------------------------
def text_section(path):
    """(SHA-256, size, address) of a thin 64-bit Mach-O's __TEXT,__text, or None."""
    with open(path, 'rb') as f:
        data = f.read()
    if len(data) < 32 or struct.unpack_from('<I', data, 0)[0] != 0xfeedfacf:
        return None
    ncmds, off = struct.unpack_from('<I', data, 16)[0], 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, off)
        if cmd == 0x19:  # LC_SEGMENT_64: 72 bytes, then nsects section_64s of 80
            for i in range(struct.unpack_from('<I', data, off + 64)[0]):
                s = off + 72 + 80 * i
                if data[s:s + 16].rstrip(b'\0') == b'__text' and data[s + 16:s + 32].rstrip(b'\0') == b'__TEXT':
                    addr, n = struct.unpack_from('<QQ', data, s + 32)
                    at = struct.unpack_from('<I', data, s + 48)[0]
                    return hashlib.sha256(data[at:at + n]).hexdigest(), n, addr
        off += size
    return None


def code_of(app):
    """{path in the app: (SHA-256, size, address) of its code}."""
    return {os.path.relpath(f, app): text_section(f) for f in macho_files(app)}


@contextlib.contextmanager
def opened(target):
    """The app: `target` itself, or the one in a dmg, mounted read-only for the duration."""
    if not target.endswith('.dmg'):
        yield target
        return
    with tempfile.TemporaryDirectory() as mnt:
        run('hdiutil', 'attach', '-readonly', '-nobrowse', '-noautoopen', '-mountpoint', mnt, target)
        try:
            yield os.path.join(mnt, 'ReCarbon.app')
        finally:
            run('hdiutil', 'detach', mnt, check=False)


# --- 1c: the icon ------------------------------------------------------------------------------------
# An .icns's image types, by the pixels each holds: what make_app.py's iconset (16 to 512, @1x and @2x)
# gives. (ic10 is 512@2x, ic11-ic14 16@2x to 256@2x.)
ICNS_TYPES = {'ic04': 16, 'ic11': 32, 'ic05': 32, 'ic12': 64, 'ic07': 128, 'ic13': 256, 'ic08': 256, 'ic14': 512,
              'ic09': 512, 'ic10': 1024}


def icns_types(path):
    """The image types in an .icns file (its 'info' and 'TOC ' entries left out), or None if it isn't one."""
    with open(path, 'rb') as f:
        data = f.read()
    if len(data) < 8 or data[:4] != b'icns' or struct.unpack_from('>I', data, 4)[0] != len(data):
        return None
    found, off = set(), 8
    while off + 8 <= len(data):
        kind, size = data[off:off + 4].decode('latin-1'), struct.unpack_from('>I', data, off + 4)[0]
        if size < 8:
            return None
        if kind not in ('info', 'TOC '):
            found.add(kind)
        off += size
    return found if off == len(data) else None


def has_custom_icon(path):
    """The custom-icon flag (kHasCustomIcon) in a folder's Finder info."""
    r = subprocess.run(['xattr', '-px', 'com.apple.FinderInfo', path], capture_output=True, text=True)
    info = bytes.fromhex(''.join(r.stdout.split())) if r.returncode == 0 else b''
    return len(info) >= 10 and info[8] & 0x04 != 0


def icon_problems(app, plist):
    if plist.get('CFBundleIconFile') != 'AppIcon':
        return [f'CFBundleIconFile is {plist.get("CFBundleIconFile")!r}, not AppIcon']
    icns = os.path.join(app, 'Contents/Resources/AppIcon.icns')
    if not os.path.isfile(icns):
        return ['no Contents/Resources/AppIcon.icns']
    types = icns_types(icns)
    if types is None:
        return ['Contents/Resources/AppIcon.icns is not an .icns file']
    missing = sorted(set(ICNS_TYPES) - types, key=lambda t: ICNS_TYPES[t])
    return [f'AppIcon.icns has no {", ".join(f"{t} ({ICNS_TYPES[t]} px)" for t in missing)}'] if missing else []


def volume_icon_problems(volume, app):
    icon = os.path.join(volume, '.VolumeIcon.icns')
    if os.path.islink(icon) or not os.path.isfile(icon):
        return ['no .VolumeIcon.icns at the volume\'s root']
    problems = []
    with open(icon, 'rb') as a, open(os.path.join(app, 'Contents/Resources/AppIcon.icns'), 'rb') as b:
        if a.read() != b.read():
            problems.append('.VolumeIcon.icns is not the app\'s AppIcon.icns')
    if not has_custom_icon(volume):
        problems.append('the volume\'s root has no custom-icon flag (Finder shows the generic disk icon)')
    return problems


# --- 1e: the dmg's Finder window ---------------------------------------------------------------------
# make_app.py's DMG_WINDOW, DMG_TITLE_BAR, DMG_ICON_SIZE, DMG_ICONS and DMG_BACKGROUND (tests/
# dmg_layout_test.py checks they agree).
DMG_WINDOW = (640, 400)
DMG_TITLE_BAR = 32
DMG_ICON_SIZE = 128
DMG_ICONS = {'ReCarbon.app': (170, 190), 'Applications': (470, 190)}
DMG_BACKGROUND = '.background/background.tiff'
DMG_MOUNT = '/Volumes/ReCarbon'
DMG_PYTHON = os.path.join(ROOT, 'tools/dmg-python')
# A build machine's folders, as the alias and bookmark would hold them (UTF-8, and UTF-16 for names).
BUILD_FOLDERS = [p.encode(e) for p in ('/private/', '/var/folders/', '/tmp/', '/Users/') for e in ('utf-8', 'utf-16-be')]
HIDDEN_BARS = ('ShowToolbar', 'ShowSidebar', 'ShowPathbar', 'ShowStatusBar', 'ShowTabView')


def tiff_sizes(path):
    """Each image's (width, height) in a TIFF file, in order; [] if it isn't one."""
    r = subprocess.run(['tiffutil', '-info', path], capture_output=True, text=True)
    return [(int(w), int(h)) for w, h in re.findall(r'Image Width: (\d+) Image Length: (\d+)', r.stdout)]


def dmg_layout_problems(volume):
    background = os.path.join(volume, DMG_BACKGROUND)
    folder = os.path.dirname(background)
    if os.path.islink(folder) or not os.path.isdir(folder) or os.listdir(folder) != [os.path.basename(background)]:
        return [f'{os.path.dirname(DMG_BACKGROUND)} doesn\'t hold just {os.path.basename(background)}']
    problems = []
    w, h = DMG_WINDOW
    if os.path.islink(background) or tiff_sizes(background) != [(w, h), (2 * w, 2 * h)]:
        problems.append(f'{DMG_BACKGROUND} is not one TIFF of {w} x {h} and {2 * w} x {2 * h} '
                        f'({tiff_sizes(background)})')
    store = os.path.join(volume, '.DS_Store')
    if os.path.islink(store) or not os.path.isfile(store):
        return problems + ['no .DS_Store at the volume\'s root (Finder opens it with its own defaults)']
    if DMG_PYTHON not in sys.path:
        sys.path.insert(0, DMG_PYTHON)
    try:
        import ds_store, mac_alias
    except ImportError:
        return problems + [f'no ds_store and mac_alias in {DMG_PYTHON} to read .DS_Store: run scripts/setup_tools.sh']
    with ds_store.DSStore.open(store, 'r') as d:
        def record(name, code):
            try:
                return d[name][code]
            except KeyError:
                return None
        bwsp, icvp = record('.', 'bwsp') or {}, record('.', 'icvp') or {}
        bounds = re.fullmatch(r'\{\{-?\d+, -?\d+\}, \{(\d+), (\d+)\}\}', bwsp.get('WindowBounds', ''))
        if not bounds or (int(bounds[1]), int(bounds[2])) != (w, h + DMG_TITLE_BAR):
            problems.append(f'the window is {bwsp.get("WindowBounds")}, not {w} x {h + DMG_TITLE_BAR} '
                            f'(the picture and the title bar)')
        shown = [b for b in HIDDEN_BARS if bwsp.get(b) is not False]
        if shown:
            problems.append(f'the window doesn\'t hide {", ".join(shown)}')
        if (record('.', 'icvl') or (None, None))[1] != b'icnv' or icvp.get('backgroundType') != 2 \
                or icvp.get('iconSize') != DMG_ICON_SIZE or icvp.get('arrangeBy') != 'none':
            problems.append(f'not icon view with a picture, {DMG_ICON_SIZE} pt icons, not arranged '
                            f'({record(".", "icvl")}, {icvp.get("backgroundType")}, {icvp.get("iconSize")}, '
                            f'{icvp.get("arrangeBy")})')
        try:
            alias = mac_alias.Alias.from_bytes(icvp['backgroundImageAlias'])
            named = (alias.volume.name, alias.volume.posix_path, alias.target.filename, alias.target.cnid)
        except Exception as e:  # a missing or malformed alias
            named = f'no readable alias ({e})'
        if named != ('ReCarbon', DMG_MOUNT, os.path.basename(background), os.stat(background).st_ino):
            problems.append(f'the picture\'s alias is {named}, not this volume\'s {DMG_BACKGROUND}')
        for name, position in DMG_ICONS.items():
            if record(name, 'Iloc') != position:
                problems.append(f'{name} is at {record(name, "Iloc")}, not {position}')
    with open(store, 'rb') as f:
        data = f.read()
    if any(p in data for p in BUILD_FOLDERS):
        problems.append(f'.DS_Store names a build machine\'s folder (the layout was written outside {DMG_MOUNT})')
    return problems


def check(app, minimum, sdk):
    """-> (every check passed, the code of each Mach-O)."""
    ok = True
    print(f'{app}: for macOS {minimum}+')
    with open(os.path.join(app, 'Contents/Info.plist'), 'rb') as f:
        plist = plistlib.load(f)
    declared = plist.get('LSMinimumSystemVersion', '')
    good = re.fullmatch(r'\d+(\.\d+)*', declared or '') and version_tuple(declared) == version_tuple(minimum)
    print(f'1. Info.plist: LSMinimumSystemVersion {declared}, v{plist.get("CFBundleShortVersionString")} '
          f'({plist.get("ReCarbonCommit")}): {"ok" if good else "FAIL"}')
    ok &= bool(good)
    # ReCarbon: the canonical bundle ID (".test" for make_app.py --test).
    bundle_id = plist.get('CFBundleIdentifier', '')
    good_id = bundle_id in ('org.retronative.recarbon', 'org.retronative.recarbon.test')
    print(f'1b. Info.plist: CFBundleIdentifier {bundle_id}: {"ok" if good_id else "FAIL (want org.retronative.recarbon)"}')
    ok &= good_id
    icon = icon_problems(app, plist)
    print(f'1c. icon: {"ok (CFBundleIconFile AppIcon, AppIcon.icns 16 to 512 @1x/@2x)" if not icon else "FAIL " + "; ".join(icon)}')
    ok &= not icon
    files = macho_files(app)
    exe = os.path.join(app, 'Contents/MacOS', plist.get('CFBundleExecutable', 'ReCarbon'))
    floor = version_tuple(minimum)
    print(f'2. {len(files)} Mach-O files:')
    for f in files:
        v, archs = minos(f), run('lipo', '-archs', f).strip()
        good = archs == 'arm64' and (v == floor if f == exe else v <= floor)
        print(f'   {show(v):>6}  {archs:<6} {os.path.relpath(f, app)}{"" if good else "   FAIL"}')
        ok &= good
    if exe not in files:
        print(f'   FAIL: no {exe}')
        ok = False
    bad = []
    for f in files:
        bad += [f'{os.path.basename(f)}: {l}' for l in libraries(f) if not l.startswith(('@rpath/', '/usr/lib/', '/System/'))]
        bad += [f'{os.path.basename(f)}: rpath {r}' for r in rpaths(f) if r not in ('@executable_path/../Frameworks', '@loader_path')]
        found = sorted(set(re.findall(r'(/opt/homebrew|/usr/local/Cellar|/Users/[^/\s]+)', run('strings', '-a', f))))
        bad += [f'{os.path.basename(f)}: names {s}' for s in found]
    print(f'3. loads only the app and the system, names no build machine\'s folder: {"ok" if not bad else "FAIL"}')
    for b in bad:
        print(f'   {b}')
    ok &= not bad
    r = subprocess.run(['codesign', '--verify', '--deep', '--strict', app], capture_output=True, text=True)
    print(f'4. codesign --verify --deep --strict: {"ok" if r.returncode == 0 else "FAIL " + r.stderr.strip()}')
    ok &= r.returncode == 0
    print(f'5. imports against the availability in {sdk}:')
    ok &= check_imports(files, minimum, sdk)
    code = code_of(app)
    print('6. code (__TEXT,__text SHA-256, size, address):')
    for rel, c in sorted(code.items()):
        print(f'   {c[0]}  {c[1]:>10}  0x{c[2]:x}  {rel}' if c else f'   (no __text)  {rel}')
    lgpl = lgpl_problems(app, exe) if exe in files else [f'no {exe}']
    print(f'7. the LGPL parts are separate libraries in Contents/Frameworks (libavcodec, libavutil, librecarbon_lzx), '
          f'none inside the executable: {"ok" if not lgpl else "FAIL"}')
    for p in lgpl:
        print(f'   {p}')
    ok &= not lgpl
    return ok, code


def main():
    if len(ARGS) != 1:
        raise SystemExit('usage: check_app.py <ReCarbon.app | ReCarbon-macos.dmg> [--min-macos=<version>] '
                         '[--sdk=<path>] [--same-code-as=<ReCarbon.app | dmg>]')
    minimum = OPTS.get('min-macos') or default_target()
    sdk = OPTS.get('sdk') or run('xcrun', '--sdk', 'macosx', '--show-sdk-path').strip()
    target = os.path.abspath(ARGS[0])
    ok = True
    if target.endswith('.dmg') and os.path.exists(target + '.sha256'):
        want = open(target + '.sha256').read().split()[0]
        got = run('shasum', '-a', '256', target).split()[0]
        print(f'{os.path.basename(target)}: SHA-256 {got}: {"matches" if got == want else "FAIL, not " + want}')
        ok &= got == want
    with opened(target) as app:
        passed, code = check(app, minimum, sdk)
        ok &= passed
        if target.endswith('.dmg'):
            vol = volume_icon_problems(os.path.dirname(app), app)
            print(f'1d. the dmg volume\'s icon: {"ok (.VolumeIcon.icns, the custom-icon flag)" if not vol else "FAIL " + "; ".join(vol)}')
            ok &= not vol
            layout = dmg_layout_problems(os.path.dirname(app))
            print(f'1e. the dmg\'s Finder window: {"ok (the background, the window, the icons\' places)" if not layout else "FAIL " + "; ".join(layout)}')
            ok &= not layout
    if OPTS.get('same-code-as'):
        ref = os.path.abspath(OPTS['same-code-as'])
        with opened(ref) as other:
            theirs = code_of(other)
        print(f'   code compared with {ref} (informational):')
        for rel in sorted(set(code) | set(theirs)):
            a, b = code.get(rel), theirs.get(rel)
            what = ('only here' if rel not in theirs else 'only there' if rel not in code
                    else 'same code' if a and b and a[0] == b[0]
                    else 'same size, different code' if a and b and a[1] == b[1]
                    else f'different code ({b[1] if b else "?"} -> {a[1] if a else "?"} bytes)')
            print(f'   {what:<28} {rel}')
    print('PASS' if ok else 'FAILED')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
