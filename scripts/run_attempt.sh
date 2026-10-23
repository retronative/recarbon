#!/usr/bin/env bash
# One development launch of build/main/runtime/ReCarbon with a hard deadline,
# for bring-up (ROADMAP M3). Runs from the repository root so the game is found
# in game/files; strict stub handling (never RECARBON_STUB_CONTINUE).
#
#   scripts/run_attempt.sh <name> [seconds] [--headless]
#
# Writes game/m3/<name>.log (user data in game/m3/home, see below) (git-ignored: it names game files and addresses)
# and prints the exit status: 3 = an unimplemented import was reached,
# >128 = a signal, 0 = a clean exit. A deadline stop is marked
# "[run_attempt] deadline" in the log (then usually exit 0: SIGTERM quits).
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
name="${1:?usage: run_attempt.sh <name> [seconds] [--headless]}"
secs="${2:-90}"
mkdir -p game/m3
# Muted unless the caller sets RECARBON_MUTE=0 (ReCarbon M5: audio checks need the device).
export RECARBON_RESOLUTION=1x RECARBON_MUTE="${RECARBON_MUTE:-1}" RECARBON_HANG_SECS="${RECARBON_HANG_SECS:-20}"
unset RECARBON_STUB_CONTINUE
# ReCarbon: an isolated user folder (settings, saves, logs, caches) under
# game/m3/home, so automated runs never touch the player's own
# (~/Library/Application Support/ReCarbon). RECARBON_REAL_HOME=1 opts out.
if [ "${RECARBON_REAL_HOME:-0}" != 1 ]; then
  export HOME="$ROOT/game/m3/home"
  mkdir -p "$HOME/Library/Application Support" "$HOME/Library/Caches"
fi
[ "${3:-}" = "--headless" ] && export RECARBON_HEADLESS=1
# At the deadline: SIGTERM (the game quits cleanly, so the next run starts
# without an "ended without quitting" notice), then SIGKILL 5 s later.
perl -e '
  my ($t, $log) = (shift, shift);
  # Emptied here, appended to by both: a game writing at its own offset would
  # write over the deadline line.
  open(my $f, ">", $log) or die "$log: $!"; close $f;
  my $pid = fork() // die "fork: $!";
  if (!$pid) { open(STDOUT, ">>", $log); open(STDERR, ">&STDOUT"); exec @ARGV or die "exec: $!" }
  $SIG{ALRM} = sub { open(my $f, ">>", $log); print $f "[run_attempt] deadline: SIGTERM\n"; close $f;
                     kill "TERM", $pid; sleep 5; kill "KILL", $pid };
  alarm $t; waitpid($pid, 0);
  exit($? & 127 ? 128 + ($? & 127) : $? >> 8)' "$secs" "game/m3/$name.log" build/main/runtime/ReCarbon
status=$?
echo "exit=$status" >> "game/m3/$name.log"
echo "exit=$status  log=game/m3/$name.log  lines=$(wc -l < "game/m3/$name.log")"
