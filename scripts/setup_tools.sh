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
#
# Needs: git, cmake, ninja, clang 18+ (Homebrew LLVM on macOS, the system
# clang elsewhere; override with LLVM=<bin dir>).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$ROOT/tools"
if [ -z "${LLVM:-}" ]; then
  if [ -x /opt/homebrew/opt/llvm/bin/clang ]; then LLVM=/opt/homebrew/opt/llvm/bin
  else LLVM="$(dirname "$(command -v clang)")"; fi
fi
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

# FFmpeg (XMA audio) and Xenia's instruction tests aren't needed until later milestones.

echo "tools ready: $(git -C "$TOOLS/XenonRecomp" log --oneline -1)"
