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

# expect <case> <exit code> <error name or -> <ReCarbon arguments...>
expect() {
  local name=$1 code=$2 error=$3; shift 3
  local out; out="$("$BIN" "$@" 2>&1)"; local got=$?
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
  "$BIN" --install "$IMAGE" --dest "$S/d_cancel" > "$S/cancel.out" 2>&1 &
  pid=$!
  sleep 1
  kill -INT $pid 2>/dev/null
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
    (cd "$S" && RECARBON_GAME_DIR="$S/game" RECARBON_MUTE=1 RECARBON_RESOLUTION=1x "$BIN" > "$S/launch.log" 2>&1 &
     pid=$!; sleep 30; kill -TERM $pid; wait $pid 2>/dev/null)  # SIGTERM: the game quits as its window's close button does
    if grep -q '\[perf\]' "$S/launch.log" && ! grep -q -i -E 'not a recompiled function|strict stub' "$S/launch.log" &&
       grep -q "$S/game" "$S/launch.log"; then ok launch_from_install
    else bad launch_from_install "see $S/launch.log"; fi
  fi
fi

echo "$pass passed, $fail failed"
if [ "${KEEP:-0}" != 1 ]; then chmod -R u+w "$S" 2>/dev/null; rm -rf "$S"; fi
[ "$fail" = 0 ]
