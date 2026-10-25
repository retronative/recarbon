#!/usr/bin/env bash
# ReCarbon. GPL-3.0-or-later (see COPYING).
#
# A release candidate of one commit, built and checked on this Mac only (never in CI: the build
# compiles ppc/, generated from the player's own default.xex, which only the player's own
# machines have; see reference/RELEASE_CANDIDATE.md).
#
#   scripts/release_candidate.sh <commit>
#
# In build/rc/<short sha>-<UTC time>/ (git-ignored, marked "evidence": nothing here is ever
# deleted automatically, a failed run included):
#   src/        a clone of exactly that commit (its own .git; this working tree is untouched),
#               with game/files linked to this repo's extracted disc (read only) and its own
#               tools/, ppc/ and game/survey/ made by its own setup_tools.sh and survey.sh
#   build/      the release build (build_deps.sh's libraries, as build/release), with its tests
#   app/        make_app.py's ReCarbon.app, dmg, .sha256 and symbols/ (kept here: only release/ leaves)
#   extracted/  the app copied back out of the dmg, checked again
#   logs/       one log per step; MANIFEST.json, steps.tsv, fields.tsv
#   release/    on a pass only: the dmg, its .sha256, MANIFEST.json and SHA256SUMS, what
#               scripts/rc_transfer.sh copies to a test Mac
# Exits non-zero on any failure, with the failing step in MANIFEST.json.
#
# The commit's own scripts do the work (setup_tools.sh, survey.sh, run_instr_tests.sh,
# make_app.py, check_app.py: so an older commit is built with its own pipeline); this script
# and scripts/macos/rc_manifest.py (the content gate and the manifest) are taken from the
# working tree running it.
#
# Settings: RC_DEPS=<prefix> (build_deps.sh's, default build/deps), RC_JOBS (6), RC_NICE (10),
# RC_LLVM=<bin dir> (/opt/homebrew/opt/llvm/bin), RC_PYTHON (/opt/homebrew/bin/python3),
# RC_GAME=<extracted disc> (default: where game/files points), MOLTENVK (make_app.py's),
# RC_COMPARE_APP=<app> (check_app.py --same-code-as, default build/app-release/ReCarbon.app if
# present; informational), RC_STAMP (the UTC time in the folder's name, for tests).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd -P)"
HELPER="$REPO/scripts/macos/rc_manifest.py"
die() { echo "release_candidate.sh: $*" >&2; exit 1; }

[ $# = 1 ] || die "usage: release_candidate.sh <commit>"
DEPS="${RC_DEPS:-$REPO/build/deps}"
JOBS="${RC_JOBS:-6}"
NICE="${RC_NICE:-10}"
LLVM="${RC_LLVM:-/opt/homebrew/opt/llvm/bin}"
PY="${RC_PYTHON:-/opt/homebrew/bin/python3}"
MOLTENVK="${MOLTENVK:-/opt/homebrew/opt/molten-vk/lib/libMoltenVK.dylib}"
MIN_MACOS=27.0

# --- Preflight: everything that can be checked before any work, nothing written yet.
command -v git > /dev/null || die "missing git"
SHA="$(git -C "$REPO" rev-parse --verify --quiet "$1^{commit}")" || die "$1: no such commit in $REPO"
# A clone carries only what a ref reaches.
[ -n "$(git -C "$REPO" for-each-ref --contains "$SHA" --count=1)" ] \
  || die "$SHA isn't on any branch or tag (a clone wouldn't have it)"
SHORT="$(git -C "$REPO" rev-parse --short=7 "$SHA")"
for t in cmake ninja hdiutil ditto codesign shasum xcrun otool curl make perl; do
  command -v "$t" > /dev/null || die "missing $t"
done
for t in clang clang++ llvm-mc llvm-objdump; do
  [ -x "$LLVM/$t" ] || die "missing $LLVM/$t (Homebrew's llvm; RC_LLVM=<bin dir>)"
done
# Where tests/run_instr_tests.sh looks for the linker.
[ -x "$LLVM/ld.lld" ] || command -v ld.lld > /dev/null || [ -x /opt/homebrew/bin/ld.lld ] || die "missing ld.lld (Homebrew's lld)"
[ -x "$PY" ] && "$PY" -c 'import tomllib' 2> /dev/null || die "$PY: need Python 3.11+ (RC_PYTHON=<python>)"
for f in scripts/setup_tools.sh scripts/survey.sh survey/validate_recomp.py tests/run_instr_tests.sh \
         scripts/macos/make_app.py scripts/macos/check_app.py CMakeLists.txt; do
  git -C "$REPO" cat-file -e "$SHA:$f" 2> /dev/null || die "$SHORT has no $f: not a commit this script can build"
done
for f in share/recarbon-deps.json lib/libSDL3.0.dylib lib/libvulkan.1.dylib lib/cmake/SDL3 lib/cmake/glslang \
         lib/cmake/SPIRV-Tools ffmpeg-xma/lib/libavcodec.dylib ffmpeg-xma/lib/libavutil.dylib \
         ffmpeg-xma/include/libavcodec/avcodec.h; do
  [ -e "$DEPS/$f" ] || die "$DEPS/$f missing: build the release libraries first (scripts/macos/build_deps.sh $DEPS), or set RC_DEPS"
done
# Absolute: CMake reads a relative CMAKE_PREFIX_PATH against another folder (and finds Homebrew's).
DEPS="$(cd "$DEPS" && pwd -P)"
grep -q "\"deployment_target\": \"$MIN_MACOS\"" "$DEPS/share/recarbon-deps.json" \
  || die "$DEPS is not built for macOS $MIN_MACOS (share/recarbon-deps.json)"
MVK_PIN="$(git -C "$REPO" show "$SHA:scripts/macos/make_app.py" | sed -n "s/^MOLTENVK_SHA256_PINNED = '\([0-9a-f]*\)'.*/\1/p")"
[ -f "$MOLTENVK" ] || die "missing $MOLTENVK (Homebrew's molten-vk, which make_app.py bundles)"
MVK_SHA="$(shasum -a 256 < "$MOLTENVK" | cut -d' ' -f1)"
[ -z "$MVK_PIN" ] || [ "$MVK_SHA" = "$MVK_PIN" ] || [ -n "${MOLTENVK_SHA256:-}" ] \
  || die "$MOLTENVK has SHA-256 $MVK_SHA, not the $MVK_PIN $SHORT's make_app.py pins"
if [ -n "${RC_GAME:-}" ]; then GAME="$RC_GAME"
else GAME="$(cd "$REPO/game/files" 2> /dev/null && pwd -P)" || die "$REPO/game/files: no extracted disc (or set RC_GAME)"; fi
[ -f "$GAME/default.xex" ] || die "$GAME/default.xex missing: the extracted disc is needed"
OUT_ROOT="$REPO/build/rc"
git -C "$REPO" check-ignore -q "$OUT_ROOT/x" || die "$OUT_ROOT is not git-ignored: refusing to write there"
OUT="$OUT_ROOT/$SHORT-${RC_STAMP:-$(date -u +%Y%m%dT%H%M%SZ)}"
[ ! -e "$OUT" ] || die "$OUT exists: refusing to touch it"

# --- The run. From here on every outcome is recorded and kept.
mkdir -p "$OUT_ROOT"
mkdir "$OUT"
printf '{"class": "evidence", "created_at": "%s", "host": "%s", "note": "release candidate of %s (scripts/release_candidate.sh): src/, build/, app/symbols and logs/ are game-derived, never distributed", "owner": "recarbon/release_candidate.sh", "project": "recarbon", "status": "active"}\n' \
  "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$(hostname -s)" "$SHA" > "$OUT/.retention.json"
SRC="$OUT/src" BUILD="$OUT/build" APP="$OUT/app" LOGS="$OUT/logs" MNT="$OUT/mnt"
mkdir "$LOGS"
renice -n "$NICE" -p $$ > /dev/null || true

field() { printf '%s\t%s\n' "$1" "$2" >> "$OUT/fields.tsv"; }
finish() {
  local status=$?
  if [ -d "$MNT" ] && mount | grep -q " on $MNT "; then hdiutil detach -quiet "$MNT" || hdiutil detach -force -quiet "$MNT" || true; fi
  field finished_at "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  "$PY" "$HELPER" write "$OUT" "$status" || true
  echo "release_candidate.sh: $OUT ($([ $status = 0 ] && echo PASS || echo FAIL))"
  exit $status
}
trap finish EXIT
# An interrupted run is a failed one (the EXIT trap alone would see the last command's status).
trap 'exit 130' INT; trap 'exit 143' TERM; trap 'exit 129' HUP
# step <name> <command...>: runs it with errexit (also inside the functions below), its output in
# logs/<name>.log; a failure stops the run.
step() {
  local name=$1 start=$SECONDS st=0; shift
  echo "== $name"
  set +e; ( set -e; "$@" ) > "$LOGS/$name.log" 2>&1; st=$?; set -e
  printf '%s\t%s\t%s\t%s\n' "$name" "$([ $st = 0 ] && echo PASS || echo FAIL)" "$LOGS/$name.log" $((SECONDS - start)) >> "$OUT/steps.tsv"
  if [ $st != 0 ]; then echo "FAIL: $name (exit $st), log: $LOGS/$name.log" >&2; tail -n 20 "$LOGS/$name.log" >&2; exit 1; fi
}

field commit "$SHA"; field requested "$1"; field short "$SHORT"; field host "$(hostname -s)"
field started_at "$(date -u +%Y-%m-%dT%H:%M:%SZ)"; field deps_prefix "$DEPS"
field deps_record_sha256 "$(shasum -a 256 < "$DEPS/share/recarbon-deps.json" | cut -d' ' -f1)"
field moltenvk "$MOLTENVK"; field moltenvk_sha256 "$MVK_SHA"; field moltenvk_pinned_sha256 "$MVK_PIN"
field moltenvk_pinned_version "$(git -C "$REPO" show "$SHA:scripts/macos/make_app.py" | sed -n "s/^MOLTENVK_PINNED = '\(.*\)'.*/\1/p")"
field game "$GAME"; field jobs "$JOBS"; field nice "$NICE"
field cc "$LLVM/clang"; field cmake "$(command -v cmake)"; field ninja "$(command -v ninja)"; field python "$PY"
field rc_script_commit "$(git -C "$REPO" rev-parse HEAD)$([ -z "$(git -C "$REPO" status --porcelain -- scripts/release_candidate.sh scripts/macos/rc_manifest.py)" ] || echo ' (uncommitted changes)')"

clean_checkout() { [ -z "$(git -C "$SRC" status --porcelain)" ] || { git -C "$SRC" status --porcelain; exit 1; }; }
game_listing() { find "$GAME/" -ls | awk '{$1=$2=""; print}' | LC_ALL=C sort | shasum -a 256; }
checkout() {
  git clone --local --no-checkout "$REPO" "$SRC"
  git -C "$SRC" checkout -q --detach "$SHA"
  [ "$(git -C "$SRC" rev-parse HEAD)" = "$SHA" ]
  clean_checkout
  mkdir "$SRC/game"
  ln -s "$GAME" "$SRC/game/files"
  test -f "$SRC/game/files/default.xex"
  game_listing > "$LOGS/game-files.before"
  clean_checkout
}
step checkout checkout
field version "$(sed -n 's/^project(ReCarbon VERSION \([0-9.]*\).*/\1/p' "$SRC/CMakeLists.txt")"

# FFmpeg's source archive from this repo's tools/, when there: setup_tools.sh checks it against
# the SHA-256 it pins either way, so it is the same file a download would be.
seed_ffmpeg() {
  local v
  v="$(sed -n 's/^FFMPEG_VERSION=\([0-9.]*\).*/\1/p' "$SRC/scripts/setup_tools.sh")"
  if [ -n "$v" ] && [ -f "$REPO/tools/ffmpeg-src/ffmpeg-$v.tar.xz" ]; then
    mkdir -p "$SRC/tools/ffmpeg-src"
    cp "$REPO/tools/ffmpeg-src/ffmpeg-$v.tar.xz" "$SRC/tools/ffmpeg-src/"
    echo "seeded ffmpeg-$v.tar.xz from $REPO/tools/ffmpeg-src (setup_tools.sh verifies its SHA-256)"
  fi
}
step seed-ffmpeg seed_ffmpeg
step tools bash -c 'cd "$1" && LLVM="$2" scripts/setup_tools.sh' _ "$SRC" "$LLVM"
step survey bash -c 'cd "$1" && PY="$2" scripts/survey.sh' _ "$SRC" "$PY"
step validate-recomp bash -c 'cd "$1" && "$2" survey/validate_recomp.py game/survey/recomp.log' _ "$SRC" "$PY"
step instr-tests bash -c 'cd "$1" && LLVM="$2" tests/run_instr_tests.sh' _ "$SRC" "$LLVM"
# AGENTS.md: survey.sh from scratch reproduces the committed config/ byte for byte.
step config-unchanged git -C "$SRC" diff --exit-code --stat -- config/
step checkout-clean clean_checkout

step configure cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$LLVM/clang" -DCMAKE_CXX_COMPILER="$LLVM/clang++" \
  -DCMAKE_PREFIX_PATH="$DEPS" -DFFMPEG_XMA="$DEPS/ffmpeg-xma" -DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN_MACOS \
  -DRECARBON_MOLTENVK="$MOLTENVK" -DRECARBON_TESTS=ON
step deps-guard "$PY" "$HELPER" deps-guard "$BUILD/CMakeCache.txt" "$DEPS"
step build ninja -C "$BUILD" -j"$JOBS"
build_identity() {
  local info="$BUILD/runtime/generated/build_info.inc" commit
  cat "$info"
  commit="$(sed -n 's/^#define RECARBON_BUILD_COMMIT "\(.*\)"/\1/p' "$info")"
  [ -n "$commit" ] && [ "${SHA#"$commit"}" != "$SHA" ] || { echo "built from $commit, not $SHA"; exit 1; }
  grep -q '^#define RECARBON_BUILD_DIRTY 0' "$info" || { echo "the build is dirty"; exit 1; }
}
step build-identity build_identity
step ctest ctest --test-dir "$BUILD" --output-on-failure
step checkout-clean-after-build clean_checkout

step package "$PY" "$SRC/scripts/macos/make_app.py" "$BUILD" "$APP" --dmg --min-macos=$MIN_MACOS
DMG="$APP/ReCarbon-macos.dmg"
field app "$APP/ReCarbon.app"; field package "$DMG"
field package_sha256 "$(shasum -a 256 < "$DMG" | cut -d' ' -f1)"
field app_binary_sha256 "$(shasum -a 256 < "$APP/ReCarbon.app/Contents/MacOS/ReCarbon" | cut -d' ' -f1)"
COMPARE="${RC_COMPARE_APP:-$REPO/build/app-release/ReCarbon.app}"
same_code=()
[ -d "$COMPARE" ] && same_code=(--same-code-as="$COMPARE") && field compared_with "$COMPARE"
step check-app "$PY" "$SRC/scripts/macos/check_app.py" "$APP/ReCarbon.app" --min-macos=$MIN_MACOS ${same_code[@]+"${same_code[@]}"}
step check-dmg "$PY" "$SRC/scripts/macos/check_app.py" "$DMG" --min-macos=$MIN_MACOS
step gate-app "$PY" "$HELPER" gate-app "$APP/ReCarbon.app"

extract() {
  mkdir "$MNT"
  hdiutil attach -readonly -nobrowse -noautoopen -quiet -mountpoint "$MNT" "$DMG"
  ls -la "$MNT"
  local st=0
  "$PY" "$HELPER" gate-volume "$MNT" || st=$?
  [ $st != 0 ] || ditto "$MNT/ReCarbon.app" "$OUT/extracted/ReCarbon.app" || st=$?
  hdiutil detach -quiet "$MNT" || hdiutil detach -force -quiet "$MNT"
  return $st
}
step dmg-extract extract
step check-extracted "$PY" "$SRC/scripts/macos/check_app.py" "$OUT/extracted/ReCarbon.app" --min-macos=$MIN_MACOS
binary_match() {
  local a b
  a="$(shasum -a 256 < "$APP/ReCarbon.app/Contents/MacOS/ReCarbon" | cut -d' ' -f1)"
  b="$(shasum -a 256 < "$OUT/extracted/ReCarbon.app/Contents/MacOS/ReCarbon" | cut -d' ' -f1)"
  echo "built $a"; echo "dmg   $b"
  [ "$a" = "$b" ]
}
step binary-match binary_match
field extracted_binary_sha256 "$(shasum -a 256 < "$OUT/extracted/ReCarbon.app/Contents/MacOS/ReCarbon" | cut -d' ' -f1)"
game_unchanged() { game_listing | diff "$LOGS/game-files.before" -; }
step game-files-unchanged game_unchanged
step checkout-clean-final clean_checkout

release() {
  mkdir "$OUT/release"
  ln "$DMG" "$DMG.sha256" "$OUT/release/"
  (cd "$OUT/release" && shasum -a 256 -c ReCarbon-macos.dmg.sha256)
}
step release release
field completed yes
