#!/usr/bin/env bash
# ReCarbon: check the installer (ReCarbon --install / --verify / --where) end to
# end, in a scratch folder of its own, against the player's disc image and/or
# extracted disc, with cases that must fail. Nothing of the player's is
# written: the sources are only read, every broken source is an APFS clone
# (cp -c: copy-on-write) made in the scratch folder, and every run has its own
# HOME there. The scratch folder must not exist yet; it is removed at the end
# unless KEEP=1.
#
#   scripts/install_check.sh <ReCarbon binary> <disc image|-> <extracted folder|-> <scratch folder>
#
# A "-" skips that source's cases (a Mac without the image). The launch case
# opens a window for 30 s (needs a logged-in desktop); NO_LAUNCH=1 skips it.
# Prints one line per case and "N passed, M failed"; exit 1 if any failed.
set -uo pipefail
[ $# -eq 4 ] || { sed -n 2,15p "$0" >&2; exit 2; }
BIN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
IMAGE="$2" FOLDER="$3" S="$4"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MANIFEST="$ROOT/runtime/install/manifest.inc"
[ -x "$BIN" ] || { echo "not an executable: $BIN" >&2; exit 2; }
abs() { [ "$1" = - ] && echo - || echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; }
IMAGE="$(abs "$IMAGE")" FOLDER="$(abs "$FOLDER")"
[ -e "$S" ] && { echo "the scratch folder exists, left as it is: $S" >&2; exit 2; }
mkdir -p "$S/home/Library/Application Support" "$S/home/Library/Caches"
S="$(cd "$S" && pwd)"
export HOME="$S/home"
# Outside the repository: a development checkout's game/files would be found first.
cd "$S"
unset RECARBON_GAME_DIR
pass=0 fail=0
ok() { pass=$((pass + 1)); echo "PASS  $1"; }
bad() { fail=$((fail + 1)); echo "FAIL  $1: $2"; }

# Every game this script runs, and anything a game starts, are this run's,
# known three ways: the launch's own process group (GAME_PGID); its group and
# descendants as seen while it runs (TREE: pid and start time, so a pid used
# again later is never taken for one of them); and
# RECARBON_TEST_OWNER=$OWNER in the environment of each game this script
# starts, the launch's included. Only those are ever signalled, so a player's
# game, another run's, or the user's session are left alone. On any exit
# (done, Ctrl-C, SIGTERM to this script) they are stopped (SIGTERM, then
# SIGKILL after 10 s) and checked gone. (macOS's ps shows environments with
# -E, its -e meaning every process, and not for Apple's own binaries, such as
# sleep: hence the tree.)
OWNER="install_check-$$-$RANDOM"
GAME_PGID="" TREE=""
started() { ps -o stat=,lstart= -p "$1" 2>/dev/null | awk '$1 !~ /^Z/ { $1 = ""; print substr($0, 2) }'; }
snapshot() {
  [ -n "$GAME_PGID" ] || return 0
  local p
  for p in "$GAME_PGID" $(pgrep -g "$GAME_PGID") $(ps -axo pid=,ppid= |
      awk -v root="$GAME_PGID" '{ up[$1] = $2 } END { for (p in up) { q = p; while (q in up && q != root && q > 1) q = up[q]; if (q == root && p != root) print p } }'); do
    TREE+="$p $(started "$p")"$'\n'
  done
}
# The match is made here, not by a program whose own arguments would hold it.
owned() {
  ps -axww -E -o pid=,command= | while read -r p rest; do
    case " $rest " in *" RECARBON_TEST_OWNER=$OWNER "*) echo "$p" ;; esac
  done
}
leftovers() {
  {
    [ -n "$GAME_PGID" ] && pgrep -g "$GAME_PGID"
    printf '%s' "$TREE" | while read -r p start; do
      [ -n "$start" ] && [ "$(started "$p")" = "$start" ] && echo "$p"
    done
    owned
  } | sort -un
}
stop_game() {
  snapshot
  local left outside; left="$(leftovers)"
  [ -n "$left" ] || return 0
  outside="$(comm -23 <(echo "$left" | sort -u) <([ -n "$GAME_PGID" ] && pgrep -g "$GAME_PGID" | sort -u))"
  [ -n "$outside" ] && echo "note: stopping test-owned processes outside the game's process group: $(echo $outside)" >&2
  kill -TERM $left 2>/dev/null
  for _ in $(seq 20); do [ -z "$(leftovers)" ] && break; sleep 0.5; done
  left="$(leftovers)"
  if [ -n "$left" ]; then
    echo "note: still running 10 s after SIGTERM, killed: $(echo $left)" >&2
    kill -KILL $left 2>/dev/null
    sleep 0.5
  fi
}
# Stops them; true if none is left after that.
cleaned() {
  stop_game
  local left; left="$(leftovers)"
  [ -z "$left" ] && return 0
  kill -KILL $left 2>/dev/null
  echo "left behind: $(echo $left)" >&2
  return 1
}
on_exit() { cleaned || echo "FAIL  cleanup: test-owned processes were still running at exit" >&2; }
trap on_exit EXIT
trap 'echo "interrupted" >&2; exit 130' INT TERM

# expect <case> <exit code> <error name or -> <ReCarbon arguments...>
expect() {
  local name=$1 code=$2 error=$3; shift 3
  local out; out="$(RECARBON_TEST_OWNER="$OWNER" "$BIN" "$@" 2>&1)"; local got=$?
  echo "$out" > "$S/$name.out"
  if [ "$got" != "$code" ]; then bad "$name" "exit $got, wanted $code ($(tail -1 "$S/$name.out"))"
  elif [ "$error" != - ] && ! grep -q "^error ($error)" "$S/$name.out"; then bad "$name" "no 'error ($error)' ($(tail -1 "$S/$name.out"))"
  else ok "$name"; fi
}

# Sources that aren't a disc.
expect not_found 1 not_found --install "$S/no-such-image.iso" --dest "$S/d_not_found"
mkdir "$S/empty"
expect empty_folder 1 not_a_disc_image --install "$S/empty" --dest "$S/d_empty"
head -c 1048576 /dev/urandom > "$S/random.iso"
expect random_file 1 not_a_disc_image --install "$S/random.iso" --dest "$S/d_random"
expect usage 2 - --install

if [ "$FOLDER" != - ]; then
  # Broken copies of the extracted disc: one file gone, one byte changed, another default.xex.
  cp -c -R "$FOLDER" "$S/f_missing" && chmod -R u+w "$S/f_missing"
  # A file the game needs (the manifest's), not one it doesn't ($SystemUpdate, .DS_Store).
  victim="$(sed -n 's/^MANIFEST_FILE("\([^"]*\)".*/\1/p' "$MANIFEST" | grep -v -x default.xex | LC_ALL=C sort | head -1)"
  rm "$S/f_missing/$victim"
  expect folder_missing_file 1 missing_file --install "$S/f_missing" --dest "$S/d_missing"
  cp -c -R "$FOLDER" "$S/f_corrupt" && chmod -R u+w "$S/f_corrupt"
  printf '\xff' | dd of="$S/f_corrupt/$victim" bs=1 seek=0 conv=notrunc 2>/dev/null
  expect folder_corrupt_file 1 corrupt_file --install "$S/f_corrupt" --dest "$S/d_corrupt"
  cp -c -R "$FOLDER" "$S/f_xex" && chmod -R u+w "$S/f_xex"
  size=$(stat -f %z "$S/f_xex/default.xex")
  printf '\xff' | dd of="$S/f_xex/default.xex" bs=1 seek=$((size - 1)) conv=notrunc 2>/dev/null
  expect folder_other_xex 1 unsupported_version --install "$S/f_xex" --dest "$S/d_xex"
  for f in f_missing f_corrupt f_xex d_missing d_corrupt d_xex; do
    [ -e "$S/$f/.recarbon-install-partial" ] && bad "$f" "a partial install was left behind"
  done
  # A good folder: verify, then install.
  expect folder_verify 0 - --verify "$FOLDER"
  expect folder_install 0 - --install "$FOLDER" --dest "$S/folder_game"
fi

if [ "$IMAGE" != - ]; then
  expect image_verify 0 - --verify "$IMAGE"
  # The image cut short (an incomplete download, FAT32's 4 GB limit).
  cp -c "$IMAGE" "$S/truncated.iso" && chmod u+w "$S/truncated.iso"
  truncate -s $(( $(stat -f %z "$IMAGE") / 2 )) "$S/truncated.iso"
  expect image_truncated 1 truncated_image --install "$S/truncated.iso" --dest "$S/d_truncated"
  # Destinations that won't do.
  mkdir "$S/d_full" && echo "someone's file" > "$S/d_full/notes.txt"
  expect dest_not_empty 1 destination_not_empty --install "$IMAGE" --dest "$S/d_full"
  mkdir "$S/readonly" && chmod 555 "$S/readonly"
  expect dest_not_writable 1 destination_not_writable --install "$IMAGE" --dest "$S/readonly/game"
  chmod 755 "$S/readonly"
  if hdiutil create -quiet -size 200m -fs APFS -volname rcinstallcheck "$S/small.dmg" &&
     hdiutil attach -quiet -nobrowse -mountpoint "$S/small" "$S/small.dmg"; then
    expect dest_no_space 1 not_enough_space --install "$IMAGE" --dest "$S/small/game"
    hdiutil detach -quiet "$S/small"
  else
    bad dest_no_space "couldn't make a 200 MB volume"
  fi
  # Cancelled part way (Ctrl-C): exit 130, and nothing left at the destination.
  RECARBON_TEST_OWNER="$OWNER" "$BIN" --install "$IMAGE" --dest "$S/d_cancel" > "$S/cancel.out" 2>&1 &
  pid=$!
  sleep 1
  kill -INT $pid 2>/dev/null
  for _ in $(seq 60); do kill -0 $pid 2>/dev/null || break; sleep 0.5; done
  kill -0 $pid 2>/dev/null && kill -KILL $pid  # ignored SIGINT: fails below (exit 137)
  wait $pid; got=$?
  if [ $got = 0 ]; then echo "SKIP  cancel: the install finished within 1 s"
  elif [ $got != 130 ]; then bad cancel "exit $got, wanted 130"
  elif [ -n "$(ls -A "$S/d_cancel" 2>/dev/null)" ]; then bad cancel "files left at the destination"
  else ok cancel; fi
  # The real install, into an empty folder, then found by --where.
  expect image_install 0 - --install "$IMAGE" --dest "$S/game"
  n=$(find "$S/game" -type f ! -name '.recarbon-install*' | wc -l | tr -d ' ')
  want=$(sed -n 's/^MANIFEST_INFO([^,]*, [^,]*, \([0-9]*\),.*/\1/p' "$MANIFEST")
  [ "$n" = "$want" ] && ok "image_install_files ($n)" || bad image_install_files "$n files, wanted $want"
  expect where 0 - --where
  grep -q "$S/game" "$S/where.out" && ok where_names_it || bad where_names_it "$(head -3 "$S/where.out" | tr '\n' ' ')"
  expect reinstall_over_install 0 - --install "$IMAGE" --dest "$S/game"
  if [ "${NO_LAUNCH:-0}" != 1 ]; then
    # The game from the install, started outside the repository, for 30 s.
    # (Started on its own, not as "cd && game &": then $! is a subshell's and
    # the signal never reaches the game, which went on running.)
    # In a process group of its own (perl's setpgrp, then exec: the same pid).
    RECARBON_TEST_OWNER="$OWNER" RECARBON_GAME_DIR="$S/game" RECARBON_MUTE=1 RECARBON_RESOLUTION=1x \
      perl -e 'setpgrp(0, 0); exec @ARGV or die "exec: $!"' "$BIN" > "$S/launch.log" 2>&1 &
    pid=$!
    GAME_PGID=$pid
    sleep 30 & wait $!  # (in the background, so a Ctrl-C is handled at once)
    # SIGTERM: the game quits as its window's close button does.
    snapshot
    kill -TERM -- "-$GAME_PGID" 2>/dev/null
    for _ in $(seq 20); do pgrep -g "$GAME_PGID" > /dev/null || break; sleep 0.5; done
    if pgrep -g "$GAME_PGID" > /dev/null; then bad launch_quits "still running 10 s after SIGTERM"
    elif ! grep -q '^\[runtime\] SIGTERM' "$S/launch.log"; then bad launch_quits "the game never logged the SIGTERM (was another process signalled?)"
    else ok launch_quits; fi
    # Stopped (SIGKILL if need be) before the wait, which would otherwise
    # hang on a game that ignored SIGTERM.
    if cleaned; then ok launch_cleaned_up; else bad launch_cleaned_up "test-owned processes were left running (killed)"; fi
    wait $pid 2>/dev/null
    if grep -q '\[perf\]' "$S/launch.log" && ! grep -q -i -E 'not a recompiled function|strict stub' "$S/launch.log" &&
       grep -q "$S/game" "$S/launch.log"; then ok launch_from_install
    else bad launch_from_install "see $S/launch.log"; fi
  fi
fi

echo "$pass passed, $fail failed"
if [ "${KEEP:-0}" != 1 ]; then chmod -R u+w "$S" 2>/dev/null; rm -rf "$S"; fi
[ "$fail" = 0 ]
