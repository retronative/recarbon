#!/usr/bin/env bash
# ReCarbon. GPL-3.0-or-later (see COPYING).
#
# Copy a passed release candidate (scripts/release_candidate.sh) to one of the player's own test
# Macs over SSH, and check it arrived intact there.
#
#   scripts/rc_transfer.sh <rc dir> <ssh host> <remote dir>
#   e.g. scripts/rc_transfer.sh build/rc/0d60585-20261025T120000Z m1-air \
#          project/thixpin/nfs/recarbon-worker/rc/0d60585-20261025T120000Z
#
# Sends only <rc dir>/release/: the versioned dmg (ReCarbon-<version>-macos.dmg), its .sha256,
# MANIFEST.json and SHA256SUMS (never
# symbols/, src/, build/ or logs/). Refuses a run that didn't pass, a release folder holding
# anything else, and a remote dir that already exists. Then runs shasum -a 256 -c on the remote
# side. A relative remote dir is relative to the remote home.
# RC_SSH and RC_SCP replace ssh and scp (the tests use local stand-ins).
set -euo pipefail
die() { echo "rc_transfer.sh: $*" >&2; exit 1; }
[ $# = 3 ] || die "usage: rc_transfer.sh <rc dir> <ssh host> <remote dir>"
RC="$1" HOST="$2" REMOTE="$3"
SSH="${RC_SSH:-ssh}" SCP="${RC_SCP:-scp}"
REL="$RC/release"
[ -d "$REL" ] && [ -f "$REL/SHA256SUMS" ] || die "$REL/SHA256SUMS missing: not a passed release candidate"
grep -q '"result": "PASS"' "$REL/MANIFEST.json" 2> /dev/null || die "$REL/MANIFEST.json doesn't record a pass"
case "$REMOTE" in ''|*\'*|*' '*|-*) die "$REMOTE: a remote dir with no quotes, spaces or leading dash, please";; esac

# Exactly the files SHA256SUMS lists, plus itself, all regular files with the expected names.
files=(SHA256SUMS)
dmg=
while read -r _ name; do files+=("$name"); done < "$REL/SHA256SUMS"
for name in "${files[@]}"; do
  if [[ "$name" =~ ^ReCarbon-[0-9][0-9.]*-macos\.dmg$ ]]; then
    [ -z "$dmg" ] || die "$name: a second dmg"
    dmg="$name"
  elif ! [[ "$name" =~ ^(ReCarbon-[0-9][0-9.]*-macos\.dmg\.sha256|MANIFEST\.json|SHA256SUMS)$ ]]; then
    die "$name: not part of a release (SHA256SUMS lists only the versioned dmg, its .sha256 and MANIFEST.json)"
  fi
  [ -f "$REL/$name" ] && [ ! -L "$REL/$name" ] || die "$REL/$name: not a regular file"
done
[ -n "$dmg" ] && printf '%s\n' "${files[@]}" | grep -qxF "$dmg.sha256" || die "$REL: no versioned dmg with its .sha256"
for name in "$REL"/* "$REL"/.[!.]*; do
  [ -e "$name" ] || continue
  printf '%s\n' "${files[@]}" | grep -qxF "$(basename "$name")" || die "$name: in release/ but not in SHA256SUMS"
done
(cd "$REL" && shasum -a 256 -c SHA256SUMS > /dev/null) || die "$REL: local checksums don't match SHA256SUMS"

"$SSH" "$HOST" "if [ -e '$REMOTE' ]; then echo '$REMOTE exists' >&2; exit 17; fi; mkdir -p '$REMOTE'" \
  || die "$HOST:$REMOTE exists or can't be made: refusing (choose a new remote dir)"
paths=()
for name in "${files[@]}"; do paths+=("$REL/$name"); done
"$SCP" -q "${paths[@]}" "$HOST:$REMOTE/" || die "copy to $HOST:$REMOTE failed"
"$SSH" "$HOST" "cd '$REMOTE' && shasum -a 256 -c SHA256SUMS && shasum -a 256 -c '$dmg.sha256'" \
  || die "checksum mismatch on $HOST:$REMOTE (left there for inspection)"
echo "rc_transfer.sh: $HOST:$REMOTE verified ($(head -c 12 "$REL/$dmg.sha256")… $dmg)"
