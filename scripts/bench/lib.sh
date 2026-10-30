# ReCarbon benchmark harness: shared by bench_run.sh and bench_watch.sh (sourced, not run).
# GPL-3.0-or-later (see COPYING).
#
# The system probes print one number each. Tests replace them through the BENCH_*_CMD variables
# (tests/bench_safety_test.py); a probe that prints anything else counts as a monitor failure.
#   thermal: macOS thermal pressure level (kOSThermalNotificationPressureLevelName; on macOS
#            0 Nominal, 1 Moderate, 2 Heavy = throttling, 3 Trapping, 4 Sleeping). Not
#            NSProcessInfo.thermalState: the game can report "fair" while this is Heavy.
#   memfree: system-wide memory free percentage.
#   swap:    swap used, MB.
BENCH_THERMAL_CMD=${BENCH_THERMAL_CMD:-"notifyutil -g com.apple.system.thermalpressurelevel | awk '{print \$2}'"}
BENCH_MEMFREE_CMD=${BENCH_MEMFREE_CMD:-"memory_pressure -Q | awk '/free percentage/ {sub(\"%\", \"\", \$NF); print \$NF}'"}
BENCH_SWAP_CMD=${BENCH_SWAP_CMD:-"sysctl -n vm.swapusage | awk '{sub(\"M\", \"\", \$6); print \$6}'"}
BENCH_THERMAL_STOP=2   # Heavy: stop at once
BENCH_MEMFREE_STOP=20  # percent
BENCH_SWAP_STOP=1024   # MB of swap growth during a run

# Exit codes (bench_run.sh's, and bench_watch.sh's for the stops it makes).
BENCH_OK=0 BENCH_USAGE=2 BENCH_REFUSED=3 BENCH_STOP_PRESSURE=10 BENCH_STOP_MONITOR=11
BENCH_STOP_LOGGING=12 BENCH_STOP_WATCHDOG=13 BENCH_STOP_TIMEOUT=14

bench_probe() {  # <cmd> <integer|number>: the probe's value, or status 1
  local v
  v=$(bash -c "$1" 2>/dev/null | head -1 | tr -d '[:space:]')
  case $2 in
    integer) [[ "$v" =~ ^[0-9]+$ ]] ;;
    number) [[ "$v" =~ ^[0-9]+(\.[0-9]+)?$ ]] ;;
  esac || return 1
  printf '%s\n' "$v"
}

# Signals a process and everything under it, children first (a parent that dies first can
# leave them running). Only this tree: never a process found by name.
bench_kill_tree() {  # <pid> <signal>
  local child
  for child in $(pgrep -P "$1" 2>/dev/null); do
    bench_kill_tree "$child" "$2"
  done
  kill -"$2" "$1" 2>/dev/null
}

bench_stop_tree() {  # <pid>: SIGTERM, then SIGKILL after BENCH_KILL_WAIT seconds
  bench_kill_tree "$1" TERM
  local i
  for ((i = 0; i < ${BENCH_KILL_WAIT:-5} * 10; i++)); do
    kill -0 "$1" 2>/dev/null || return 0
    sleep 0.1
  done
  bench_kill_tree "$1" KILL
}
