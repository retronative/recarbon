#!/usr/bin/env python3
# ReCarbon tests. GPL-3.0-or-later (see COPYING).
#
# Alpha gate G2: the LGPL-2.1 parts (FFmpeg's libavcodec and libavutil, libmspack's LZX decoder)
# are separate, replaceable libraries in the app's Contents/Frameworks, never inside the
# executable. scripts/macos/check_app.py's check 7 on small apps built here with xcrun clang (a
# stand-in executable and stand-in libraries with the real names and one real symbol each): the
# layout make_app.py writes passes, and each planted bug fails it (the LZX decoder or FFmpeg
# linked statically again, a library missing from Frameworks or not linked, a library that isn't
# the one it's named as). And make_app.py's license files for the libraries it makes itself.
# macOS only (clang, nm, otool). Everything runs in a scratch folder.
# From the repo root: python3 tests/lgpl_dylibs_test.py (ctest runs it).
import importlib.util, os, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
failures = 0


def load(name):
    argv = sys.argv
    sys.argv = [name + '.py']  # both parse their command line when loaded
    try:
        spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, 'scripts', 'macos', name + '.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        sys.argv = argv


os.environ.pop('DMG_TOOL', None)
check_app = load('check_app')
make_app = load('make_app')


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print(f'FAIL: {what}')


# The stand-ins: each library's file name and the one symbol check 7 looks for in it.
LIBS = {'libavcodec.61.dylib': 'int avcodec_send_packet(void) { return 1; }\n',
        'libavutil.59.dylib': 'int av_frame_alloc(void) { return 2; }\n',
        'librecarbon_lzx.dylib': 'int lzxd_decompress(void) { return 3; }\n'}
USES = 'int avcodec_send_packet(void); int av_frame_alloc(void); int lzxd_decompress(void);\n'
MAIN = 'int main(void) { return avcodec_send_packet() + av_frame_alloc() + lzxd_decompress(); }\n'


def clang(tmp, source, out, *args):
    src = out + '.c'
    with open(src, 'w') as f:
        f.write(source)
    subprocess.run(['xcrun', 'clang', '-arch', 'arm64', src, '-o', out, *args], check=True, capture_output=True)


def make(tmp, name, libs=LIBS, static='', link=tuple(LIBS)):
    """A stand-in ReCarbon.app: `libs` in Frameworks; the executable links `link` from there and
    has `static` compiled into it."""
    app = os.path.join(tmp, name, 'ReCarbon.app')
    frameworks = os.path.join(app, 'Contents/Frameworks')
    os.makedirs(frameworks)
    os.makedirs(os.path.join(app, 'Contents/MacOS'))
    for lib, source in libs.items():
        clang(tmp, source, os.path.join(frameworks, lib), '-dynamiclib', '-install_name', '@rpath/' + lib)
    # The executable links the real-named libraries (built on the side when Frameworks lacks one).
    side = os.path.join(tmp, name, 'side')
    os.makedirs(side)
    flags = []
    for lib in link:
        clang(tmp, LIBS[lib], os.path.join(side, lib), '-dynamiclib', '-install_name', '@rpath/' + lib)
        flags.append(os.path.join(side, lib))
    exe = os.path.join(app, 'Contents/MacOS/ReCarbon')
    clang(tmp, (USES if link else '') + static + MAIN, exe, *flags, '-Wl,-rpath,@executable_path/../Frameworks')
    return app, exe


def problems(app_exe):
    return check_app.lgpl_problems(*app_exe)


def tests(tmp):
    good = problems(make(tmp, 'good'))
    check(good == [], f'the layout make_app.py writes passes ({good})')

    # The LZX decoder compiled into the executable again (as before gate G2), its library gone.
    found = problems(make(tmp, 'static-lzx', libs={k: v for k, v in LIBS.items() if 'lzx' not in k},
                          static=LIBS['librecarbon_lzx.dylib'], link=('libavcodec.61.dylib', 'libavutil.59.dylib')))
    check(any('defines' in p and '_lzxd_decompress' in p for p in found), f'a static LZX decoder fails ({found})')
    check(any(p.startswith('libmspack LZX: 0 files') for p in found), f'a missing librecarbon_lzx fails ({found})')

    # FFmpeg linked statically as well as its libraries bundled (a static archive back in the build).
    found = problems(make(tmp, 'static-ffmpeg', static='int ff_xma2_decoder = 4; int avpriv_report_missing_feature(void) { return 5; }\n'))
    check(any('defines 2 symbols' in p and '_ff_xma2_decoder' in p for p in found), f'FFmpeg linked statically fails ({found})')

    # A library in Frameworks that the executable doesn't link (and so doesn't import from).
    found = problems(make(tmp, 'unlinked', link=('libavcodec.61.dylib', 'librecarbon_lzx.dylib'),
                          static=LIBS['libavutil.59.dylib']))
    check(any('libavutil' in p and 'doesn\'t link' in p for p in found), f'an unlinked libavutil fails ({found})')
    check(any('defines' in p and '_av_frame_alloc' in p for p in found), f'libavutil\'s code in the executable fails ({found})')

    # A file named as the library that isn't it.
    found = problems(make(tmp, 'impostor', libs=dict(LIBS, **{'libavcodec.61.dylib': 'int something_else(void) { return 0; }\n'})))
    check(any('libavcodec.61.dylib doesn\'t define _avcodec_send_packet' in p for p in found), f'a wrong libavcodec fails ({found})')

    # Two versions of one library side by side.
    app, exe = make(tmp, 'two')
    shutil.copy(os.path.join(app, 'Contents/Frameworks/libavutil.59.dylib'), os.path.join(app, 'Contents/Frameworks/libavutil.60.dylib'))
    found = problems((app, exe))
    check(any(p.startswith('FFmpeg libavutil: 2 files') for p in found), f'two libavutil files fail ({found})')

    # make_app.py's license files for the LGPL libraries that come from neither a prefix nor a keg.
    lzx = make_app.own_library('librecarbon_lzx.dylib')
    check(lzx and lzx[0] == 'libmspack' and ('LICENSES/libmspack-LGPL-2.1.txt', 'COPYING.LIB') in lzx[1],
          f'librecarbon_lzx gets libmspack\'s LGPL-2.1 ({lzx})')
    for lib in ('libavcodec.61.dylib', 'libavutil.59.dylib'):
        ff = make_app.own_library(lib)
        check(ff and ff[0] == 'FFmpeg' and ('LICENSES/FFmpeg-LGPL-2.1.txt', 'COPYING.LGPLv2.1') in ff[1]
              and any(name.endswith('.patch') for _, name in ff[1]), f'{lib} gets FFmpeg\'s LGPL-2.1 and patches ({ff})')
    check(make_app.own_library('libSDL3.0.dylib') is None, 'SDL3 is not one of them')
    for _, _, files in make_app.OWN_LIBRARIES:
        for path, _ in files:
            check(os.path.isfile(os.path.join(ROOT, path)), f'{path} exists')


def main():
    if sys.platform != 'darwin':
        print('lgpl_dylibs_test: macOS only, skipped')
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        tests(tmp)
    print('lgpl_dylibs_test: ' + ('all checks passed' if not failures else f'{failures} failed'))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
