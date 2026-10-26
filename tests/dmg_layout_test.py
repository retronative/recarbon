#!/usr/bin/env python3
# ReCarbon tests. GPL-3.0-or-later (see COPYING).
#
# The dmg's Finder window, without a build: scripts/macos/make_app.py's background (the committed
# DmgBackground.png and @2x at the window's size, joined into one TIFF) and finder_layout (the
# .DS_Store) on a small HFS+ volume named ReCarbon, mounted out of Finder's sight at /Volumes/ReCarbon
# as make_app.py mounts it, checked by scripts/macos/check_app.py's dmg_layout_problems, which must
# also catch a layout that is wrong, or written at another mount point (a build machine's folder).
# Both scripts' layout constants must agree. macOS only (hdiutil, tiffutil, sips); needs ds_store
# and mac_alias in tools/dmg-python (scripts/setup_tools.sh), and /Volumes/ReCarbon free (no ReCarbon
# dmg open); skipped (77) otherwise.
# Everything runs in a scratch folder. From the repo root: python3 tests/dmg_layout_test.py (ctest runs it).
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


make_app = load('make_app')
check_app = load('check_app')


def check(ok, what):
    global failures
    if not ok:
        failures += 1
        print(f'FAIL: {what}')


def constants_tests():
    for name in ('DMG_WINDOW', 'DMG_TITLE_BAR', 'DMG_ICON_SIZE', 'DMG_ICONS', 'DMG_BACKGROUND', 'DMG_MOUNT',
                 'DMG_PYTHON'):
        check(getattr(make_app, name) == getattr(check_app, name), f'check_app.py\'s {name} is make_app.py\'s')
    w, h = make_app.DMG_WINDOW
    for png, scale in zip(make_app.DMG_BACKGROUND_PNGS, (1, 2)):
        dims = subprocess.run(['sips', '-g', 'pixelWidth', '-g', 'pixelHeight', png], capture_output=True,
                              text=True).stdout.split()
        check(dims[-4:] == ['pixelWidth:', str(w * scale), 'pixelHeight:', str(h * scale)],
              f'{os.path.basename(png)} is {w * scale} x {h * scale} ({dims[-4:]})')
    for name, (x, y) in make_app.DMG_ICONS.items():
        half = make_app.DMG_ICON_SIZE // 2
        check(half <= x <= w - half and half <= y <= h - half, f'{name}\'s icon lies inside the window')


def layout_tests(tmp):
    tiff = os.path.join(tmp, 'background.tiff')
    make_app.make_dmg_background(tiff)
    w, h = make_app.DMG_WINDOW
    check(check_app.tiff_sizes(tiff) == [(w, h), (2 * w, 2 * h)],
          f'the background is one TIFF at 1x and 2x ({check_app.tiff_sizes(tiff)})')

    image, mnt = os.path.join(tmp, 'layout.dmg'), make_app.DMG_MOUNT
    subprocess.run(['hdiutil', 'create', '-quiet', '-size', '8m', '-fs', 'HFS+', '-volname', 'ReCarbon', image],
                   check=True)
    subprocess.run(['hdiutil', 'attach', '-quiet', '-nobrowse', '-noautoopen', image], check=True)
    try:
        check(os.path.ismount(mnt), f'the scratch volume mounted at {mnt}')
        os.makedirs(os.path.join(mnt, '.background'))
        shutil.copyfile(tiff, os.path.join(mnt, make_app.DMG_BACKGROUND))
        os.makedirs(os.path.join(mnt, 'ReCarbon.app'))
        os.symlink('/Applications', os.path.join(mnt, 'Applications'))
        make_app.finder_layout(mnt)
        problems = check_app.dmg_layout_problems(mnt)
        check(problems == [], f'finder_layout\'s window passes check_app.py ({problems})')

        ds_store, _ = make_app.dmg_libraries()
        store = os.path.join(mnt, '.DS_Store')
        with ds_store.DSStore.open(store, 'r') as d:
            bwsp = d['.']['bwsp']
        check(bwsp['WindowBounds'].endswith(f'{{{w}, {h + make_app.DMG_TITLE_BAR}}}}}'),
              f'the window is the picture\'s size and the title bar ({bwsp["WindowBounds"]})')

        with ds_store.DSStore.open(store, 'r+') as d:
            d['ReCarbon.app']['Iloc'] = (10, 10)
        check(any('ReCarbon.app is at' in p for p in check_app.dmg_layout_problems(mnt)), 'a moved icon fails')
        with ds_store.DSStore.open(store, 'r+') as d:
            d['.']['bwsp'] = dict(bwsp, ShowToolbar=True)
        check(any('ShowToolbar' in p for p in check_app.dmg_layout_problems(mnt)), 'a shown toolbar fails')
        os.remove(store)
        check(any('.DS_Store' in p for p in check_app.dmg_layout_problems(mnt)), 'no .DS_Store fails')

        # The picture's alias names the file by its ID: another file in its place fails.
        make_app.finder_layout(mnt)
        background = os.path.join(mnt, make_app.DMG_BACKGROUND)
        os.rename(background, background + '.old')
        shutil.copyfile(tiff, background)
        os.remove(background + '.old')
        check(any('alias' in p for p in check_app.dmg_layout_problems(mnt)), 'a picture the alias doesn\'t name fails')
        with open(os.path.join(mnt, '.background/notes.txt'), 'w') as f:
            f.write('x')
        check(any('.background' in p for p in check_app.dmg_layout_problems(mnt)),
              'anything else in .background fails')
        os.remove(os.path.join(mnt, '.background/notes.txt'))
    finally:
        subprocess.run(['hdiutil', 'detach', '-quiet', mnt], capture_output=True)

    # Written at a mount point of the build machine's: the alias and bookmark name that folder.
    other = os.path.join(tmp, 'volume')
    os.makedirs(other)
    subprocess.run(['hdiutil', 'attach', '-quiet', '-nobrowse', '-noautoopen', '-mountpoint', other, image], check=True)
    try:
        make_app.finder_layout(other)
        problems = check_app.dmg_layout_problems(other)
        check(any('build machine' in p for p in problems) and any('alias' in p for p in problems),
              f'a layout written outside {mnt} fails ({problems})')
    finally:
        subprocess.run(['hdiutil', 'detach', '-quiet', other], capture_output=True)


def main():
    if sys.platform != 'darwin':
        print('dmg_layout_test: macOS only')
        return 77
    if not os.path.isdir(os.path.join(make_app.DMG_PYTHON, 'ds_store')):
        print(f'dmg_layout_test: skipped: no ds_store and mac_alias in {make_app.DMG_PYTHON} '
              f'(run scripts/setup_tools.sh)')
        return 77
    if os.path.lexists(make_app.DMG_MOUNT):
        print(f'dmg_layout_test: skipped: {make_app.DMG_MOUNT} is in use (a ReCarbon dmg is open?)')
        return 77
    constants_tests()
    with tempfile.TemporaryDirectory() as tmp:
        layout_tests(tmp)
    print('dmg_layout_test: ' + (f'{failures} failure(s)' if failures else 'all checks passed'))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
