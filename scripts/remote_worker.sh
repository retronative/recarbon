#!/usr/bin/env bash
# ReCarbon: build and test a committed SHA on a second Mac over SSH. This Mac
# stays the only one that commits; the worker gets the commit as a git bundle
# (no push or pull) and never changes anything outside its worker folder.
#
#   RECARBON_WORKER=<ssh host> RECARBON_WORKER_DIR=<folder on it> \
#     scripts/remote_worker.sh <command> [sha] ...
#
#   ship  [sha]                       bundle the commit (default HEAD), clone it into src/<sha>
#   put   <sha>                       copy this Mac's build of <sha> (HEAD, clean tree) to src/<sha>
#   build <sha>                       setup_tools, survey, cmake, ninja -j4 (build log in runs/<sha>/)
#   test  <sha>                       validate_recomp, ctest, instruction tests
#   run   <sha> <name> <secs> [VAR=value ...]
#                                     scripts/run_attempt.sh in the background, own HOME in runs/<sha>/<name>
#   status <sha> <name>               the run's result.txt, or that it is still running
#   fetch <sha> <name>                copy runs/<sha>/<name> back to game/m3/remote/<sha>/<name>
#
# Worker layout: data/rom_extracted (the disc, read-only), src/<sha> (one clone
# per commit, never reused), runs/<sha>/<name>, locks/. A build or test holds
# locks/build-<sha>; a run holds locks/game (one game window at a time, and no
# build meanwhile: 8 GB of RAM). A held lock fails the command, never waits.
# Every run writes result.txt: SHA, machine, command, times, exit, counts.
# build and test need the toolchain on the worker (cmake, ninja, Homebrew's
# llvm, Python 3.11+) and say what is missing; without it, put ships a binary
# built here, and run, status and fetch work the same.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
HOST="${RECARBON_WORKER:?set RECARBON_WORKER to the ssh host of the worker}"
DIR="${RECARBON_WORKER_DIR:?set RECARBON_WORKER_DIR to the worker folder (relative to its home)}"
cmd="${1:?usage: remote_worker.sh ship|build|test|run|status|fetch ...}"
shift

remote() { ssh -o BatchMode=yes "$HOST" "bash -s" -- "$DIR" "$@"; }

# Shared by every remote script: paths, Homebrew's tools, and locks.
PRELUDE='set -euo pipefail
W="$HOME/$1"; shift
export PATH="/opt/homebrew/bin:/opt/homebrew/opt/llvm/bin:$PATH"
lock() { mkdir "$W/locks/$1" 2>/dev/null || { echo "locked: $1 ($(cat "$W/locks/$1/owner" 2>/dev/null))" >&2; exit 75; }
         echo "pid $$ sha ${2:-} since $(date "+%F %T")" > "$W/locks/$1/owner"; }
unlock() { rm -rf "$W/locks/$1"; }
clean_tree() { [ -z "$(git -C "$1" status --porcelain --untracked-files=no)" ] || { echo "tree not clean: $1" >&2; exit 1; }; }'

case "$cmd" in
ship)
  sha="$(git rev-parse "${1:-HEAD}^{commit}")"
  bundle="$(mktemp -d "${TMPDIR:-/tmp}/remote_worker.XXXXXX")/recarbon-$sha.bundle"
  git bundle create "$bundle" "$sha" main >/dev/null 2>&1 || git bundle create "$bundle" main
  git bundle verify -q "$bundle" 2>/dev/null
  ssh -o BatchMode=yes "$HOST" "mkdir -p '$DIR/bundles'"
  scp -q "$bundle" "$HOST:$DIR/bundles/"
  rm -rf "$(dirname "$bundle")"
  remote "$sha" <<EOF
$PRELUDE
sha="\$1"; src="\$W/src/\$sha"
[ -e "\$src" ] && { echo "exists, left as it is: \$src"; exit 0; }
git clone -q "\$W/bundles/recarbon-\$sha.bundle" "\$src" 2>/dev/null
git -C "\$src" -c advice.detachedHead=false checkout -q "\$sha"
mkdir -p "\$src/game" && ln -s "\$W/data/rom_extracted" "\$src/game/files"
echo "shipped \$(git -C "\$src" rev-parse HEAD) to \$src"
EOF
  ;;
put)
  sha="$(git rev-parse "${1:?sha}^{commit}")"
  bin=build/main/runtime/ReCarbon
  [ "$(git rev-parse HEAD)" = "$sha" ] || { echo "HEAD is not $sha: check it out and build it here first" >&2; exit 1; }
  [ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "tree not clean" >&2; exit 1; }
  # The build stamps its commit (report/build_info.cmake), "<short>-dirty" from a changed tree.
  strings -a "$bin" | grep -cx "${sha:0:7}" >/dev/null || { echo "$bin was not built from ${sha:0:7} (run ninja)" >&2; exit 1; }
  hash="$(shasum -a 256 "$bin" | cut -d' ' -f1)"
  here="$(scutil --get ComputerName 2>/dev/null || hostname)"
  remote "$sha" <<EOF
$PRELUDE
src="\$W/src/\$1"
[ -d "\$src" ] || { echo "not shipped: \$1" >&2; exit 1; }
[ -e "\$src/$bin" ] && { echo "exists, left as it is: \$src/$bin" >&2; exit 1; }
mkdir -p "\$src/build/main/runtime" "\$W/runs/\$1"
EOF
  scp -q "$bin" "$HOST:$DIR/src/$sha/$bin"
  remote "$sha" "$hash" "$here" <<EOF
$PRELUDE
got="\$(shasum -a 256 "\$W/src/\$1/$bin" | cut -d' ' -f1)"
[ "\$got" = "\$2" ] || { echo "copy differs: \$got" >&2; rm -f "\$W/src/\$1/$bin"; exit 1; }
echo "binary: $bin sha256 \$2, built on \$3 from \$1" > "\$W/runs/\$1/binary.txt"
cat "\$W/runs/\$1/binary.txt"
EOF
  ;;
build|test)
  sha="${1:?sha}"
  remote "$sha" "$cmd" <<EOF
$PRELUDE
sha="\$1"; what="\$2"; src="\$W/src/\$sha"; out="\$W/runs/\$sha"
mkdir -p "\$out"
lock "build-\$sha" "\$sha"; trap 'unlock "build-\$sha"' EXIT
[ -e "\$W/locks/game" ] && { echo "a game run holds the machine" >&2; exit 75; }
cd "\$src"; clean_tree .
[ "\$(git rev-parse HEAD)" = "\$sha" ] || { echo "HEAD is not \$sha" >&2; exit 1; }
missing=""
for t in cmake ninja /opt/homebrew/opt/llvm/bin/clang++; do command -v "\$t" >/dev/null || missing="\$missing \$t"; done
/opt/homebrew/bin/python3 -c 'import tomllib' 2>/dev/null || missing="\$missing python3.11+"
[ -z "\$missing" ] || { echo "missing on the worker:\$missing (not installed by this script)" >&2; exit 69; }
rc=0
if [ "\$what" = build ]; then
  { scripts/setup_tools.sh && scripts/survey.sh \
    && cmake -S . -B build/main -G Ninja -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
    && ninja -C build/main -j4; } > "\$out/build.log" 2>&1 || rc=\$?
  echo "build exit=\$rc log=\$out/build.log"
  shasum -a 256 config/*.toml > "\$out/config.sha256"
  find ppc -type f -print0 | sort -z | xargs -0 shasum -a 256 | shasum -a 256 | sed 's/ .*/  ppc (all files)/' >> "\$out/config.sha256"
else
  { /opt/homebrew/bin/python3 survey/validate_recomp.py game/survey/recomp.log \
    && cmake -S . -B build/tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DRECARBON_TESTS=ON \
         -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
    && ninja -C build/tests -j4 && ctest --test-dir build/tests \
    && tests/run_instr_tests.sh; } > "\$out/test.log" 2>&1 || rc=\$?
  echo "test exit=\$rc log=\$out/test.log"; tail -3 "\$out/test.log"
fi
exit \$rc
EOF
  ;;
run)
  sha="${1:?sha}"; name="${2:?name}"; secs="${3:?seconds}"; shift 3
  remote "$sha" "$name" "$secs" "$@" <<EOF
$PRELUDE
sha="\$1"; name="\$2"; secs="\$3"; shift 3
src="\$W/src/\$sha"; out="\$W/runs/\$sha/\$name"
[ -x "\$src/build/main/runtime/ReCarbon" ] || { echo "not built: \$sha" >&2; exit 1; }
[ -e "\$out" ] && { echo "run exists, left as it is: \$out" >&2; exit 1; }
ls "\$W/locks" | grep -q '^build-' && { echo "a build holds the machine" >&2; exit 75; }
mkdir -p "\$out/home/Library/Application Support" "\$out/home/Library/Caches"
cat > "\$out/job.sh" <<'JOB'
set -uo pipefail
W="\$1"; sha="\$2"; name="\$3"; secs="\$4"; shift 4
src="\$W/src/\$sha"; out="\$W/runs/\$sha/\$name"
trap 'rm -rf "\$W/locks/game"' EXIT
start="\$(date "+%F %T %z")"
cd "\$src"
env "\$@" HOME="\$out/home" RECARBON_REAL_HOME=1 caffeinate -dims scripts/run_attempt.sh "\$name" "\$secs"
status=\$?
mv "game/m3/\$name.log" "\$out/run.log"
log="\$out/run.log"
{
  echo "sha: \$(git rev-parse HEAD) (tree \$( [ -z "\$(git status --porcelain --untracked-files=no)" ] && echo clean || echo DIRTY))"
  echo "build: \$(grep -m1 '^\[build\]' "\$log")"
  echo "machine: \$(scutil --get ComputerName 2>/dev/null) | \$(sysctl -n hw.model) \$(sysctl -n machdep.cpu.brand_string) | \$(( \$(sysctl -n hw.memsize) / 1073741824 )) GB | macOS \$(sw_vers -productVersion) (\$(sw_vers -buildVersion))"
  echo "command: \$* scripts/run_attempt.sh \$name \$secs (HOME=\$out/home)"
  echo "start: \$start"
  echo "end: \$(date "+%F %T %z")"
  echo "exit: \$status"
  echo "deadline reached: \$(grep -c '^\[run_attempt\] deadline' "\$log")"
  echo "not a recompiled function: \$(grep -c 'not a recompiled function' "\$log")"
  echo "unimplemented/stub: \$(grep -c -i -E 'unimplemented|strict stub' "\$log")"
  echo "hang reports: \$(grep -c '^\[watchdog\] hang report written' "\$log")"
  echo "crash/hang files: \$(ls "\$out/home/Library/Application Support/ReCarbon/logs" 2>/dev/null | grep -c -E '^(crash|hang)-')"
  echo "frames over 900 ms: \$(grep '\[hitch\]' "\$log" | awk '{for(i=1;i<=NF;i++) if(\$i=="took" && \$(i+1)+0>900) n++} END{print n+0}')"
  grep '\[perf\]' "\$log" | grep -o -E '[0-9.]+ fps' | awk '{s+=\$1;n++; if(m==""||\$1<m)m=\$1} END{if(n) printf "fps: mean %.1f, min %.1f over %d samples\n", s/n, m, n}'
  echo "thermal: \$(grep -o -E 'thermal [a-z]+' "\$log" | sort | uniq -c | tr '\n' ' ')"
} > "\$out/result.txt"
JOB
lock game "\$sha"
nohup bash "\$out/job.sh" "\$W" "\$sha" "\$name" "\$secs" "\$@" > "\$out/job.out" 2>&1 < /dev/null &
echo "started \$name on \$sha (pid \$!): \$out"
EOF
  ;;
status)
  sha="${1:?sha}"; name="${2:?name}"
  remote "$sha" "$name" <<EOF
$PRELUDE
out="\$W/runs/\$1/\$2"
if [ -f "\$out/result.txt" ]; then cat "\$out/result.txt"; else echo "running or not started: \$out"; cat "\$W/locks/game/owner" 2>/dev/null || true; fi
EOF
  ;;
fetch)
  sha="${1:?sha}"; name="${2:?name}"
  mkdir -p "game/m3/remote/$sha"
  scp -q -r "$HOST:$DIR/runs/$sha/$name" "game/m3/remote/$sha/"
  rm -rf "game/m3/remote/$sha/$name/home/Library/Caches"
  echo "fetched to game/m3/remote/$sha/$name"
  ;;
*)
  echo "unknown command: $cmd" >&2; exit 2 ;;
esac
