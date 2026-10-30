#!/usr/bin/env bash
# The automated verification gate (AGENTS.md "Before each commit"), in one run and in order,
# stopping at the first failure. GPL-3.0-or-later (see COPYING).
#
#   scripts/verify.sh [--build-dir <dir>] [--jobs N] [--test-jobs N] [--wait] [--no-build] [--fresh]
#
#   1. configure (only a folder that isn't, or has RECARBON_TESTS off) and build the game and
#      the tests (ninja -jN; N is 8 as in AGENTS.md, --jobs changes it)
#   2. ctest: the parallel-safe tests (label unit) --test-jobs at a time (8 by default), then
#      the timing-sensitive ones one at a time, with the machine to themselves
#      (tests/CMakeLists.txt says which is which)
#   3. tests/run_instr_tests.sh (the recompiler's instruction tests)
#   4. survey/validate_recomp.py (the recompiled game's structural check)
# A launch of the game (scripts/run_attempt.sh) is not part of this: it needs the disc and a GPU.
#
# Steps 3 and 4 depend on nothing the build changes, so a pass is kept with a key over every
# input (the test sources and script, tools/XenonRecomp's commit and diff, the compilers;
# ppc/, the image, the config, the survey scripts and the log) in <logs>/<step>.ok, and the
# step is skipped while the key is the same. --fresh runs them again. A failure is never kept.
#
# Refuses to start while the game or a benchmark runs (a ReCarbon process, or
# scripts/bench/bench_run.sh): the gate's CPU load would corrupt the measurement and the
# tests' own timing checks. --wait polls until they are gone instead.
# Each step's wall and CPU time go to the summary at the end; VERIFY_LOG=<dir> (default
# <build dir>/verify) keeps every step's full output.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BUILD=build/main JOBS=8 TEST_JOBS=8 WAIT=0 DO_BUILD=1 FRESH=0
usage() { sed -n '4,5p' "$0" | sed 's/^# *//' >&2; exit 2; }
while [ $# -gt 0 ]; do
  case $1 in
    --build-dir) BUILD=${2:?}; shift 2 ;;
    --jobs) JOBS=${2:?}; shift 2 ;;
    --test-jobs) TEST_JOBS=${2:?}; shift 2 ;;
    --wait) WAIT=1; shift ;;
    --no-build) DO_BUILD=0; shift ;;
    --fresh) FRESH=1; shift ;;
    *) usage ;;
  esac
done
[[ "$JOBS" =~ ^[1-9][0-9]?$ ]] && [[ "$TEST_JOBS" =~ ^[1-9][0-9]?$ ]] || usage
PY=${PY:-/opt/homebrew/bin/python3}
LLVM=${LLVM:-/opt/homebrew/opt/llvm/bin}
LOGS=${VERIFY_LOG:-$BUILD/verify}
mkdir -p "$LOGS"

# The game or a benchmark running: scripts/bench/bench_run.sh's own check, plus the harness itself
# (its cooldown runs before the game starts).
busy() { pgrep -x ReCarbon >/dev/null || pgrep -f 'scripts/bench/bench_run\.sh' >/dev/null; }
if busy; then
  if [ $WAIT = 1 ]; then
    echo "verify: a ReCarbon process or a benchmark is running: waiting"
    while busy; do sleep 30; done
  else
    echo "verify: refused: a ReCarbon process or a benchmark is running (--wait polls until it ends)" >&2
    exit 3
  fi
fi

now() { perl -MTime::HiRes=time -e 'printf "%.2f\n", time'; }
sha() { shasum -a 256 | cut -c1-16; }
# What tests/run_instr_tests.sh's result depends on.
instr_key() {
  {
    shasum -a 256 tests/run_instr_tests.sh tests/instr/*.s
    [ -d tools/xenia-tests ] && shasum -a 256 tools/xenia-tests/*.s
    git -C tools/XenonRecomp rev-parse HEAD; git -C tools/XenonRecomp diff HEAD --no-ext-diff
    "$LLVM/clang" --version | head -1; "${LLD:-$LLVM/ld.lld}" --version 2>/dev/null | head -1
  } 2>/dev/null | sha
}
# What survey/validate_recomp.py's result depends on (it reads ppc/ whole, and layout.py the survey report).
validate_key() {
  {
    shasum -a 256 survey/validate_recomp.py survey/layout.py config/recarbon.toml \
      game/survey/image.bin game/survey/xexsurvey.txt game/survey/recomp.log
    find ppc/ -type f -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256
    "$PY" --version
  } 2>/dev/null | sha
}

SUMMARY=()
# step <name> <key or -> <command...>: runs it with its output in $LOGS/<name>.log (its tail
# shown on failure), records wall and CPU seconds, and stops the gate on failure. With a key,
# a pass is kept in <name>.ok and the step skipped while the key holds.
step() {
  local name=$1 key=$2 start st=0 log="$LOGS/$1.log" wall cpu; shift 2
  if [ "$key" != - ] && [ $FRESH = 0 ] && [ "$(cat "$LOGS/$name.ok" 2>/dev/null)" = "$key" ]; then
    SUMMARY+=("$(printf '%-16s %s' "$name" "PASS (kept: unchanged inputs $key, from $(date -r "$LOGS/$name.ok" '+%Y-%m-%d %H:%M'); --fresh reruns)")")
    echo "== $name: unchanged inputs ($key), the earlier pass holds"
    return
  fi
  rm -f "$LOGS/$name.ok"
  echo "== $name"
  start=$(now)
  set +e
  /usr/bin/time -p "$@" > "$log" 2> "$log.time"; st=$?
  set -e
  wall=$(perl -e 'printf "%.1f", $ARGV[0] - $ARGV[1]' "$(now)" "$start")
  cpu=$(awk '/^(user|sys) /{s+=$2} END{printf "%.1f", s}' "$log.time")
  grep -v -E '^(real|user|sys) ' "$log.time" >> "$log" || true
  rm -f "$log.time"
  SUMMARY+=("$(printf '%-16s %s %7s s wall %7s s cpu' "$name" "$([ $st = 0 ] && echo PASS || echo FAIL)" "$wall" "$cpu")")
  if [ $st != 0 ]; then
    tail -n 40 "$log" >&2
    echo "verify: FAIL: $name (exit $st), log: $log" >&2
    printf '%s\n' "${SUMMARY[@]}"
    exit 1
  fi
  [ "$key" = - ] || echo "$key" > "$LOGS/$name.ok"
}

T0=$(now)
if [ $DO_BUILD = 1 ]; then
  if ! grep -q '^RECARBON_TESTS:BOOL=ON' "$BUILD/CMakeCache.txt" 2>/dev/null; then
    step configure - cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="$LLVM/clang" -DCMAKE_CXX_COMPILER="$LLVM/clang++" -DRECARBON_TESTS=ON
  fi
  step build - ninja -C "$BUILD" -j"$JOBS"
fi
step ctest-unit - ctest --test-dir "$BUILD" --no-tests=error --output-on-failure -L unit -j"$TEST_JOBS"
step ctest-serial - ctest --test-dir "$BUILD" --no-tests=error --output-on-failure -LE unit -j1
step instr-tests "$(instr_key)" tests/run_instr_tests.sh
step validate-recomp "$(validate_key)" "$PY" survey/validate_recomp.py game/survey/recomp.log
printf '%s\n' "${SUMMARY[@]}"
printf 'verify: PASS in %s s (logs: %s)\n' "$(perl -e 'printf "%.0f", $ARGV[0] - $ARGV[1]' "$(now)" "$T0")" "$LOGS"
