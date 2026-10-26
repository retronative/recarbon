#!/usr/bin/env bash
# Build the toolchain into tools/ (git-ignored). Safe to re-run: existing
# checkouts and builds are reused, only what's missing or stale is rebuilt.
#
#   scripts/setup_tools.sh
#
# Adapted from SpeedBreaker's scripts/setup_tools.sh (GPL-3.0-or-later).
#
# - XenonRecomp at a pinned upstream commit + patches/xenonrecomp/*.patch
#   (branch "recarbon"), built with Clang: XenonRecomp and XenonAnalyse.
# - extract-xiso (to unpack the disc image).
# - xexsurvey (survey/xexsurvey.cpp), linked against XenonRecomp's libraries,
#   into tools/survey/xexsurvey.
# - FFmpeg's XMA decoder (scripts/build_ffmpeg_xma.sh) into tools/ffmpeg-xma.
# - ds_store and mac_alias (the dmg's Finder layout) into tools/dmg-python.
#
# Needs: git, cmake, ninja, clang 18+ (Homebrew LLVM on macOS, the system
# clang elsewhere; override with LLVM=<bin dir>), make, curl, Python 3 with pip.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$ROOT/tools"
if [ -z "${LLVM:-}" ]; then
  if [ -x /opt/homebrew/opt/llvm/bin/clang ]; then LLVM=/opt/homebrew/opt/llvm/bin
  else LLVM="$(dirname "$(command -v clang)")"; fi
fi
sha256() { if command -v sha256sum >/dev/null; then sha256sum -c -; else shasum -a 256 -c -; fi; }
XENONRECOMP_BASE=ddd128bcca99fe8bfbb99bea583c972351fa6ace   # hedge-dev/XenonRecomp main, 2025

mkdir -p "$TOOLS"

if [ ! -d "$TOOLS/XenonRecomp" ]; then
  git clone --recursive https://github.com/hedge-dev/XenonRecomp.git "$TOOLS/XenonRecomp"
fi
cd "$TOOLS/XenonRecomp"
if ! git rev-parse -q --verify recarbon >/dev/null; then
  git checkout -q -b recarbon "$XENONRECOMP_BASE"
  git submodule update --init --recursive
  # The patches carry their own authors; the committer needs any name (a fresh
  # machine or a CI runner has none configured).
  git -c user.name="ReCarbon setup" -c user.email=setup@recarbon.invalid \
    am -q "$ROOT"/patches/xenonrecomp/*.patch
fi
# The bundled fmt predates libc++ dropping transitive <cstdlib>.
cmake -Wno-deprecated -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$LLVM/clang" -DCMAKE_CXX_COMPILER="$LLVM/clang++" \
  "-DCMAKE_CXX_FLAGS=-include cstdlib" > /dev/null
ninja -C build XenonRecomp XenonAnalyse

if [ ! -x "$TOOLS/extract-xiso/build/extract-xiso" ]; then
  [ -d "$TOOLS/extract-xiso" ] || git clone -q --depth 1 https://github.com/XboxDev/extract-xiso.git "$TOOLS/extract-xiso"
  cmake -Wno-deprecated -S "$TOOLS/extract-xiso" -B "$TOOLS/extract-xiso/build" -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
  ninja -C "$TOOLS/extract-xiso/build"
fi

# xexsurvey reuses XenonRecomp's XEX loader; still in tools/XenonRecomp.
SURVEY_SRC="$ROOT/survey/xexsurvey.cpp"
SURVEY_BIN="$TOOLS/survey/xexsurvey"
SURVEY_LIBS=(build/XenonUtils/libXenonUtils.a build/thirdparty/fmt/libfmt.a
             build/thirdparty/disasm/libdisasm.a build/thirdparty/xxHash/cmake_unofficial/libxxhash.a)
survey_stale=0
[ -x "$SURVEY_BIN" ] || survey_stale=1
for f in "$SURVEY_SRC" "${SURVEY_LIBS[@]}"; do
  if [ "$f" -nt "$SURVEY_BIN" ]; then survey_stale=1; fi
done
if [ $survey_stale = 1 ]; then
  mkdir -p "$TOOLS/survey"
  "$LLVM/clang++" -std=c++20 -O2 -include cstdlib -IXenonUtils -Ithirdparty -Ithirdparty/fmt/include \
    -Ithirdparty/xxHash -Ithirdparty/TinySHA1 -Ithirdparty/libmspack/libmspack/mspack -Ithirdparty/tiny-AES-c \
    "$SURVEY_SRC" "${SURVEY_LIBS[@]}" -o "$SURVEY_BIN"
  echo "built tools/survey/xexsurvey"
fi

# FFmpeg for XMA audio: a minimal libavcodec (XMA1/XMA2/WMAPro decoders
# only) with patches/ffmpeg applied (scripts/build_ffmpeg_xma.sh), as shared
# libraries (ReCarbon, gate G2: a static tools/ffmpeg-xma of an earlier
# commit is rebuilt). On macOS the game loads them, so it is built for the game's
# deployment target. ReCarbon change: that is the same default as
# CMakeLists.txt's, MACOSX_DEPLOYMENT_TARGET when set, else this Mac's major
# version (<major>.0, like Homebrew's bottles), not SpeedBreaker's fixed 15.0;
# one built for another macOS is rebuilt.
FFMPEG_VERSION=7.1.1
FFMPEG_SHA256=733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1
ffmpeg_ok=1
[ -f "$TOOLS/ffmpeg-xma/lib/libavcodec.dylib" ] && [ -f "$TOOLS/ffmpeg-xma/lib/libavutil.dylib" ] || ffmpeg_ok=0
if [ "$(uname -s)" = Darwin ]; then
  export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-$(sw_vers -productVersion | cut -d. -f1).0}"
  if [ $ffmpeg_ok = 1 ] && [ "$(otool -l "$TOOLS/ffmpeg-xma/lib/libavcodec.dylib" "$TOOLS/ffmpeg-xma/lib/libavutil.dylib" \
       | awk '/LC_BUILD_VERSION/{b=1} b&&/minos/{print $2; b=0}' | sort -u)" != "$MACOSX_DEPLOYMENT_TARGET" ]; then
    echo "tools/ffmpeg-xma isn't built for macOS $MACOSX_DEPLOYMENT_TARGET: rebuilding it"; ffmpeg_ok=0
  fi
fi
if [ $ffmpeg_ok = 0 ]; then
  mkdir -p "$TOOLS/ffmpeg-src"
  cd "$TOOLS/ffmpeg-src"
  # Into a temporary file first: an interrupted download must not stay behind
  # as the archive that the next run checks (and fails on).
  if [ ! -f "ffmpeg-$FFMPEG_VERSION.tar.xz" ]; then
    curl -sfL -o "ffmpeg-$FFMPEG_VERSION.tar.xz.part" "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz"
    mv "ffmpeg-$FFMPEG_VERSION.tar.xz.part" "ffmpeg-$FFMPEG_VERSION.tar.xz"
  fi
  echo "$FFMPEG_SHA256  ffmpeg-$FFMPEG_VERSION.tar.xz" | sha256
  "$ROOT/scripts/build_ffmpeg_xma.sh" "$TOOLS/ffmpeg-src/ffmpeg-$FFMPEG_VERSION.tar.xz" "$TOOLS/ffmpeg-xma"
  # FFmpeg's license texts, kept next to the source for packaging.
  tar -xf "ffmpeg-$FFMPEG_VERSION.tar.xz" "ffmpeg-$FFMPEG_VERSION/COPYING.LGPLv2.1" "ffmpeg-$FFMPEG_VERSION/LICENSE.md"
  cd "$ROOT"
fi

# The dmg's Finder layout (make_app.py --dmg): ds_store and mac_alias, pure Python, at the
# versions and hashes in scripts/macos/dmg-requirements.txt, into tools/dmg-python (make_app.py
# and check_app.py import them from there). Reinstalled when that file changes.
DMG_PY="$TOOLS/dmg-python"
if ! cmp -s "$ROOT/scripts/macos/dmg-requirements.txt" "$DMG_PY/requirements.txt"; then
  PY="${PY:-$(command -v /opt/homebrew/bin/python3 || command -v python3)}"
  rm -rf "$DMG_PY.part"
  "$PY" -m pip install -q --disable-pip-version-check --no-deps --only-binary=:all: --require-hashes \
    --target "$DMG_PY.part" -r "$ROOT/scripts/macos/dmg-requirements.txt"
  cp "$ROOT/scripts/macos/dmg-requirements.txt" "$DMG_PY.part/requirements.txt"
  rm -rf "$DMG_PY"
  mv "$DMG_PY.part" "$DMG_PY"
  echo "installed tools/dmg-python (ds_store, mac_alias)"
fi

# Xenia's instruction tests aren't needed until later milestones.

echo "tools ready: $(git -C "$TOOLS/XenonRecomp" log --oneline -1)"
