#!/usr/bin/env bash
# ReCarbon benchmark launch: one bounded, watched run. GPL-3.0-or-later (see COPYING).
#
#   scripts/bench/bench_run.sh <name> <seconds, 1-600> [1x|2x|3x]
#
# A launch through scripts/run_attempt.sh (its isolated user folder, or RUN_HOME), with
# RECARBON_* settings taken from the environment. Samples and the verdict go to
# game/m3/bench/<name>/ (watch.txt, verdict.txt); the game's log to game/m3/<name>.log.
#
# Refuses (exit 3) before launching if another game is running (checked before and after the
# cooldown, and again just before the suspended game is let go), the name was used before, the
# Mac isn't cool (thermal level 0 for BENCH_COOL_CHECKS samples in a row, BENCH_COOL_TRIES at
# most, BENCH_COOL_INTERVAL apart: by default 6, 90 and 10 s) or memory free is under 30%.
# Malformed arguments: exit 2. The game starts suspended and runs only once bench_watch.sh has
# recorded a sample. Then the run stops:
#   - by itself (exit 0), or at run_attempt.sh's deadline (SIGTERM, SIGKILL 5 s later);
#   - on pressure, a monitor or a logging failure (exit 10, 11, 12: bench_watch.sh);
#   - when the watchdog dies while the game runs (exit 13);
#   - at a hard limit BENCH_GRACE (30) s past the deadline, if run_attempt.sh's misses (exit 14).
# Stops reach only this run's process tree. Heavy thermal pressure (level 2) stops a run at once:
# a fanless Mac reaches it within about 80-120 s at 2x or 3x (measured on a MacBook Air M5, October 2026).
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
source scripts/bench/lib.sh

usage() {
  echo "bench_run: $1" >&2
  echo "usage: scripts/bench/bench_run.sh <name> <seconds, 1-600> [1x|2x|3x]" >&2
  exit $BENCH_USAGE
}
refuse() {  # <exit code> <reason>
  echo "bench_run: refused: $2" >&2
  exit "$1"
}

[ $# -eq 2 ] || [ $# -eq 3 ] || usage "expected 2 or 3 arguments, got $#"
name=$1 secs=$2 res=${3:-1x}
[[ "$name" =~ ^[A-Za-z0-9_-]{1,64}$ ]] || usage "bad name [$name] (letters, digits, - and _)"
[[ "$secs" =~ ^[1-9][0-9]{0,2}$ ]] && [ "$secs" -le 600 ] || usage "bad duration [$secs] (whole seconds, 1-600)"
[[ "$res" =~ ^[123]x$ ]] || usage "bad resolution [$res] (1x, 2x or 3x)"

out="game/m3/bench/$name"
game_log="game/m3/$name.log"
game_running() { pgrep -x "${BENCH_GAME_NAME:-ReCarbon}" >/dev/null; }
game_running && refuse $BENCH_REFUSED "a ${BENCH_GAME_NAME:-ReCarbon} is already running"
[ -e "$out" ] || [ -e "$game_log" ] && refuse $BENCH_REFUSED "$name was used before ($out or $game_log exists)"
mkdir -p "$out" 2>/dev/null && : > "$out/verdict.txt" 2>/dev/null || refuse $BENCH_STOP_LOGGING "can't write $out"

for probe in "$BENCH_THERMAL_CMD integer" "$BENCH_MEMFREE_CMD integer" "$BENCH_SWAP_CMD number"; do
  bench_probe "${probe% *}" "${probe##* }" >/dev/null || refuse $BENCH_STOP_MONITOR "a probe failed before the launch: ${probe% *}"
done
cool=0
for ((i = 0; i < ${BENCH_COOL_TRIES:-90}; i++)); do
  thermal=$(bench_probe "$BENCH_THERMAL_CMD" integer) || refuse $BENCH_STOP_MONITOR "the thermal probe failed during the cooldown"
  [ "$thermal" -eq 0 ] && cool=$((cool + 1)) || cool=0
  [ $cool -ge "${BENCH_COOL_CHECKS:-6}" ] && break
  sleep "${BENCH_COOL_INTERVAL:-10}"
done
[ $cool -ge "${BENCH_COOL_CHECKS:-6}" ] || refuse $BENCH_REFUSED "not cool: thermal level $thermal after the cooldown"
memfree=$(bench_probe "$BENCH_MEMFREE_CMD" integer) || refuse $BENCH_STOP_MONITOR "the memory probe failed"
[ "$memfree" -ge 30 ] || refuse $BENCH_REFUSED "memory free $memfree% (30% needed)"
# Again after the cooldown, which can last minutes: a game started meanwhile is refused too.
game_running && refuse $BENCH_REFUSED "a ${BENCH_GAME_NAME:-ReCarbon} is already running (started during the cooldown)"

samples() { { wc -l < "$out/watch.txt"; } 2>/dev/null || echo 0; }
run_pid="" wd=""
on_signal() {
  [ -n "$run_pid" ] && { kill -CONT "$run_pid" 2>/dev/null; bench_stop_tree "$run_pid"; }
  [ -n "$wd" ] && bench_kill_tree "$wd" TERM
  echo "verdict=130 interrupted" > "$out/verdict.txt"
  exit 130
}
trap on_signal INT TERM HUP

# Suspended until the watchdog has a sample, so no moment of the run goes unwatched.
RUN_RESOLUTION=$res bash -c 'kill -STOP $$; exec bash scripts/run_attempt.sh "$1" "$2"' _ "$name" "$secs" &
run_pid=$!
for ((i = 0; i < 50; i++)); do
  [[ "$(ps -o stat= -p "$run_pid" 2>/dev/null)" == T* ]] && break
  sleep 0.1
done
[[ "$(ps -o stat= -p "$run_pid" 2>/dev/null)" == T* ]] || { bench_kill_tree "$run_pid" KILL; refuse $BENCH_STOP_WATCHDOG "the run didn't start suspended"; }
scripts/bench/bench_watch.sh "$out/watch.txt" "$run_pid" "$game_log" &
wd=$!
echo "$wd" > "$out/watch.pid"
for ((i = 0; i < 100; i++)); do
  [ "$(samples)" -ge 2 ] && break
  kill -0 "$wd" 2>/dev/null || break
  sleep 0.1
done
if [ "$(samples)" -lt 2 ]; then
  bench_kill_tree "$run_pid" KILL
  kill -CONT "$run_pid" 2>/dev/null
  wait "$wd" 2>/dev/null; ws=$?
  [ "$ws" -ge 10 ] && [ "$ws" -le 12 ] || ws=$BENCH_STOP_WATCHDOG
  echo "verdict=$ws the watchdog never recorded a sample; nothing ran" > "$out/verdict.txt"
  refuse "$ws" "the watchdog never recorded a sample; nothing ran"
fi
if game_running; then  # the suspended run is a shell, not the game: any match is another game
  bench_kill_tree "$run_pid" KILL
  kill -CONT "$run_pid" 2>/dev/null
  bench_kill_tree "$wd" TERM
  echo "verdict=$BENCH_REFUSED another game started before the launch; nothing ran" > "$out/verdict.txt"
  refuse $BENCH_REFUSED "a ${BENCH_GAME_NAME:-ReCarbon} is already running (started before the launch)"
fi
kill -CONT "$run_pid"

start=$SECONDS
limit=$((secs + ${BENCH_GRACE:-30}))
verdict="" reason=""
while kill -0 "$run_pid" 2>/dev/null; do
  if ! kill -0 "$wd" 2>/dev/null; then
    wait "$wd"; ws=$?
    if [ "$ws" -ge 10 ] && [ "$ws" -le 12 ]; then
      verdict=$ws reason="stopped by the watchdog"
    else
      bench_stop_tree "$run_pid"
      verdict=$BENCH_STOP_WATCHDOG reason="the watchdog exited ($ws) while the game ran"
    fi
    wd=""
    break
  fi
  if [ $((SECONDS - start)) -gt "$limit" ]; then
    bench_stop_tree "$run_pid"
    verdict=$BENCH_STOP_TIMEOUT reason="hard limit: still running $limit s after the start"
    break
  fi
  sleep "${BENCH_POLL:-1}"
done
wait "$run_pid"; rs=$?
if [ -n "$wd" ]; then
  if [ -n "$verdict" ]; then
    bench_kill_tree "$wd" TERM
    wait "$wd" 2>/dev/null
  else
    wait "$wd"; ws=$?
    case $ws in
      0) verdict=$BENCH_OK reason="the run ended" ;;
      10|11|12) verdict=$ws reason="stopped by the watchdog" ;;
      *) verdict=$BENCH_STOP_WATCHDOG reason="the watchdog exited ($ws)" ;;
    esac
  fi
fi
stopline=$(grep -m1 '^STOP:' "$out/watch.txt" 2>/dev/null)
echo "verdict=$verdict $reason${stopline:+ ($stopline)}; run_attempt exit $rs" > "$out/verdict.txt"
echo "bench_run: $name: verdict $verdict: $reason${stopline:+ ($stopline)}; run_attempt exit $rs; samples $out/watch.txt"
exit "$verdict"
