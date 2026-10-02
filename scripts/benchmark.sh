#!/usr/bin/env bash
# Runs each configuration on the same scene and prints a results table.
# Usage: scripts/benchmark.sh [path/to/raytracer] [width] [spp]
set -euo pipefail

BIN="${1:-build/raytracer}"
WIDTH="${2:-480}"
SPP="${3:-16}"
CORES="$( (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) )"

run() {  # run <label> <args...>
    local label="$1"; shift
    local out
    out="$("$BIN" --width "$WIDTH" --spp "$SPP" --no-output "$@" | grep '^result')"
    local time speed
    time="$(sed -E 's/.*time=([0-9.]+)s.*/\1/' <<<"$out")"
    speed="$(sed -E 's/.*speed=([0-9.]+) Mrays.*/\1/' <<<"$out")"
    printf "| %-34s | %9s s | %12s |\n" "$label" "$time" "$speed"
}

echo "Benchmark: ${WIDTH}px wide, ${SPP} spp, ${CORES} cores"
echo
printf "| %-34s | %11s | %12s |\n" "Configuration" "Time" "Mrays/s"
echo   "|------------------------------------|-------------|--------------|"
run "naive (list, 1 thread)"        --accel list --threads 1
run "list, $CORES threads" --accel list --threads "$CORES"
run "BVH, 1 thread"               --accel bvh  --threads 1
run "BVH, $CORES threads"         --accel bvh  --threads "$CORES"
