#!/usr/bin/env python3
# ReCarbon tests. GPL-3.0-or-later (see COPYING).
#
# The app's icon and the dmg's volume icon, without a build: scripts/macos/make_app.py's
# make_icns (the committed AppIcon.iconset: every size macOS asks for, each drawn at its own pixel
# size and dpi, refused when one is missing or the wrong size) and set_custom_icon (the
# volume root's custom-icon flag, the rest of its Finder info kept), and scripts/macos/
# check_app.py's checks of both (CFBundleIconFile and AppIcon.icns, .VolumeIcon.icns and the
# flag). macOS only (sips, iconutil, xattr). Everything runs in a scratch folder.
# From the repo root: python3 tests/app_icon_test.py (ctest runs it).
import importlib.util, os, plistlib, shutil, subprocess, sys, tempfile

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
make_app = load('make_app')
check_app = load('check_app')


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print(f'FAIL: {what}')


def finder_info(path):
    out = subprocess.run(['xattr', '-px', 'com.apple.FinderInfo', path], capture_output=True, text=True).stdout
    return bytes.fromhex(''.join(out.split()))


def icon_tests(tmp):
    iconset = make_app.ICON
    check(os.path.isdir(iconset), f'{iconset} is committed')
    check(sorted(os.listdir(iconset)) == sorted(make_app.ICON_FILES), f'the iconset holds exactly {make_app.ICON_FILES}')
    for name, (pixels, dpi) in make_app.ICON_FILES.items():
        dims = subprocess.run(['sips', '-g', 'pixelWidth', '-g', 'pixelHeight', '-g', 'dpiWidth', '-g', 'hasAlpha',
                               os.path.join(iconset, name)], capture_output=True, text=True).stdout.split()
        check(dims[-8:] == ['pixelWidth:', str(pixels), 'pixelHeight:', str(pixels), 'dpiWidth:', f'{dpi}.000',
                            'hasAlpha:', 'yes'], f'{name} is {pixels} x {pixels} at {dpi} dpi with alpha ({dims[-8:]})')
    check(make_app.iconset_problems(iconset) == [], f'make_app.py accepts it ({make_app.iconset_problems(iconset)})')
    icns = os.path.join(tmp, 'AppIcon.icns')
    make_app.make_icns(iconset, icns)
    types = check_app.icns_types(icns) if os.path.isfile(icns) else None
    check(types is not None and set(check_app.ICNS_TYPES) <= types,
          f'make_icns gives every size, 16 to 512 @1x/@2x ({sorted(types or [])})')
    check(len(check_app.ICNS_TYPES) == len(make_app.ICON_FILES), 'check_app.py expects make_app.py\'s sizes')

    # A set missing a size, or with one at the wrong size, is refused before iconutil sees it.
    broken = os.path.join(tmp, 'broken.iconset')
    shutil.copytree(iconset, broken)
    os.remove(os.path.join(broken, 'icon_16x16.png'))
    check(any('icon_16x16.png' in p for p in make_app.iconset_problems(broken)), 'a missing size is refused')
    subprocess.run(['sips', '-z', '30', '30', os.path.join(iconset, 'icon_32x32.png'), '--out',
                    os.path.join(broken, 'icon_16x16.png')], capture_output=True)
    check(any('icon_16x16.png' in p for p in make_app.iconset_problems(broken)), 'a wrong size is refused')
    try:
        make_app.make_icns(broken, os.path.join(tmp, 'broken.icns'))
        check(False, 'make_icns refuses a broken set')
    except SystemExit:
        pass

    # check_app.py's icon check, on an app holding that icon.
    app = os.path.join(tmp, 'ReCarbon.app')
    os.makedirs(os.path.join(app, 'Contents/Resources'))
    if os.path.isfile(icns):
        shutil.copy(icns, os.path.join(app, 'Contents/Resources/AppIcon.icns'))
    plist = {'CFBundleIconFile': 'AppIcon'}
    check(check_app.icon_problems(app, plist) == [], f'the icon passes ({check_app.icon_problems(app, plist)})')
    check(check_app.icon_problems(app, {}) != [], 'no CFBundleIconFile fails')
    small = os.path.join(tmp, 'small.iconset')
    os.makedirs(small)
    shutil.copy(os.path.join(iconset, 'icon_32x32.png'), os.path.join(small, 'icon_32x32.png'))
    subprocess.run(['iconutil', '-c', 'icns', small, '-o', os.path.join(app, 'Contents/Resources/AppIcon.icns')],
                   capture_output=True)
    check(any('ic10' in p for p in check_app.icon_problems(app, plist)), 'an icns missing sizes fails')
    with open(os.path.join(app, 'Contents/Resources/AppIcon.icns'), 'wb') as f:
        f.write(b'\x89PNG not an icns')
    check(check_app.icon_problems(app, plist) != [], 'a file that is no icns fails')
    os.remove(os.path.join(app, 'Contents/Resources/AppIcon.icns'))
    check(check_app.icon_problems(app, plist) != [], 'a missing AppIcon.icns fails')
    if os.path.isfile(icns):
        shutil.copy(icns, os.path.join(app, 'Contents/Resources/AppIcon.icns'))

    # The volume icon: a folder stands in for the volume's root (the same Finder info).
    vol = os.path.join(tmp, 'volume')
    os.makedirs(vol)
    check(not make_app.has_custom_icon(vol) and not check_app.has_custom_icon(vol), 'a new folder has no custom icon')
    if os.path.isfile(icns):
        shutil.copy(icns, os.path.join(vol, '.VolumeIcon.icns'))
    check(any('custom-icon flag' in p for p in check_app.volume_icon_problems(vol, app)), 'a volume without the flag fails')
    # Finder info already there (a window position, another flag) is kept.
    before = bytes(range(1, 9)) + b'\x00\x01' + bytes(range(11, 33))
    subprocess.run(['xattr', '-wx', 'com.apple.FinderInfo', before.hex(), vol], check=True)
    make_app.set_custom_icon(vol)
    after = finder_info(vol)
    check(len(after) == 32 and after[8:10] == b'\x04\x01' and after[:8] == before[:8] and after[10:] == before[10:],
          f'set_custom_icon sets the flag and keeps the rest ({after.hex()})')
    check(make_app.has_custom_icon(vol) and check_app.has_custom_icon(vol), 'both scripts read the flag back')
    check(check_app.volume_icon_problems(vol, app) == [], f'the volume icon passes ({check_app.volume_icon_problems(vol, app)})')
    with open(os.path.join(vol, '.VolumeIcon.icns'), 'ab') as f:
        f.write(b'x')
    check(any('not the app' in p for p in check_app.volume_icon_problems(vol, app)), 'another icon on the volume fails')
    os.remove(os.path.join(vol, '.VolumeIcon.icns'))
    check(check_app.volume_icon_problems(vol, app) != [], 'a volume without .VolumeIcon.icns fails')
    plain = os.path.join(tmp, 'plain')
    os.makedirs(plain)
    make_app.set_custom_icon(plain)
    check(finder_info(plain) == bytes(8) + b'\x04\x00' + bytes(22), 'on a folder with no Finder info, only the flag')


if sys.platform != 'darwin':
    print('app_icon_test: macOS only')
    sys.exit(1)
with tempfile.TemporaryDirectory() as tmp:
    icon_tests(tmp)
if failures:
    print(f'{failures} check(s) failed')
    sys.exit(1)
print('app_icon_test: all checks passed')
