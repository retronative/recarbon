#!/usr/bin/env bash
# ReCarbon benchmark watchdog. GPL-3.0-or-later (see COPYING).
#
#   scripts/bench/bench_watch.sh <samples file> <run pid> <game log>
#
# Started by bench_run.sh. Every BENCH_INTERVAL seconds (default 5) it records the thermal
# pressure level, memory free %, swap and load, and stops the run (its process tree only) on:
#   - pressure (exit 10): thermal level >= 2 (Heavy), memory free < 20%, swap +1 GB, or the
#     game's own "serious"/"critical" thermal line. BENCH_THERMAL_GRACE=<s> (default 0) lets a
#     run stay at level 2 for that many consecutive seconds (0-600) first; level 3 (Trapping)
#     always stops at once;
#   - a monitor failure (exit 11): a probe that doesn't print a number;
#   - a logging failure (exit 12): the samples file can't be written, or the game log is still
#     missing BENCH_LOG_WAIT (10) s after the start.
# Exits 0 when the run ends by itself. Fails closed: whatever it can't check, it stops.
set -uo pipefail
source "$(dirname "$0")/lib.sh"
out=${1:?usage: bench_watch.sh <samples file> <run pid> <game log>}
run_pid=${2:?run pid}
game_log=${3:?game log}
interval=${BENCH_INTERVAL:-5}

stop() {  # <exit code> <reason>
  local line="STOP: $2"
  echo "$line" >> "$out" 2>/dev/null
  echo "[bench_watch] $line" >> "$game_log" 2>/dev/null
  echo "[bench_watch] $line" >&2
  bench_stop_tree "$run_pid"
  exit "$1"
}

echo "t thermal memfree swapMB load" > "$out" 2>/dev/null || stop $BENCH_STOP_LOGGING "can't write the samples file $out"
grace=${BENCH_THERMAL_GRACE:-0}
[[ "$grace" =~ ^[0-9]{1,3}$ ]] && [ "$grace" -le 600 ] || stop $BENCH_STOP_MONITOR "bad BENCH_THERMAL_GRACE [$grace] (whole seconds, 0-600)"
heavy_since=""
swap0=$(bench_probe "$BENCH_SWAP_CMD" number) || stop $BENCH_STOP_MONITOR "the swap probe failed"
start=$SECONDS
while kill -0 "$run_pid" 2>/dev/null; do
  t=$((SECONDS - start))
  thermal=$(bench_probe "$BENCH_THERMAL_CMD" integer) || stop $BENCH_STOP_MONITOR "the thermal probe failed at $t s"
  memfree=$(bench_probe "$BENCH_MEMFREE_CMD" integer) || stop $BENCH_STOP_MONITOR "the memory probe failed at $t s"
  swap=$(bench_probe "$BENCH_SWAP_CMD" number) || stop $BENCH_STOP_MONITOR "the swap probe failed at $t s"
  load=$(sysctl -n vm.loadavg 2>/dev/null | awk '{print $2}')
  echo "$t $thermal $memfree $swap ${load:-?}" >> "$out" 2>/dev/null || stop $BENCH_STOP_LOGGING "can't write the samples file at $t s"
  if [ "$thermal" -ge 3 ]; then
    stop $BENCH_STOP_PRESSURE "thermal pressure level $thermal (3 = Trapping) at $t s"
  elif [ "$thermal" -ge $BENCH_THERMAL_STOP ]; then
    [ "$grace" -eq 0 ] && stop $BENCH_STOP_PRESSURE "thermal pressure level $thermal (2 = Heavy) at $t s"
    [ -n "$heavy_since" ] || heavy_since=$t
    [ $((t - heavy_since)) -ge "$grace" ] && stop $BENCH_STOP_PRESSURE "thermal pressure level $thermal (2 = Heavy) for $((t - heavy_since)) s, the grace of $grace s, at $t s"
  else
    heavy_since=""
  fi
  [ "$memfree" -lt $BENCH_MEMFREE_STOP ] && stop $BENCH_STOP_PRESSURE "memory free $memfree% at $t s"
  awk -v a="$swap" -v b="$swap0" -v m=$BENCH_SWAP_STOP 'BEGIN { exit !(a - b > m) }' &&
    stop $BENCH_STOP_PRESSURE "swap grew from $swap0 to $swap MB at $t s"
  if [ -f "$game_log" ]; then
    grep -qE '\[thermal\].*-> (serious|critical)' "$game_log" && stop $BENCH_STOP_PRESSURE "the game reported serious thermal state at $t s"
  elif [ "$t" -ge "${BENCH_LOG_WAIT:-10}" ]; then
    stop $BENCH_STOP_LOGGING "no game log at $game_log after $t s"
  fi
  sleep "$interval"
done
exit $BENCH_OK
