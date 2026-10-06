#!/usr/bin/env bash
set -euo pipefail

build="${1:-build/release}"
port="${PULSE_PORT:-39300}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

run_live() {
  "$build/apps/pulse-feed" "$work/session.feed" --port "$port" "$@" > "$work/feed.log" &
  "$build/apps/pulse-book" --port "$port"
  wait
  cat "$work/feed.log"
  echo
}

echo "== machine"
uname -srm
if command -v lscpu > /dev/null; then
  lscpu | grep -E "Model name|^CPU\(s\)"
else
  sysctl -n machdep.cpu.brand_string hw.ncpu
fi
echo

echo "== benchmarks"
"$build/bench/bench_decode"
"$build/bench/bench_book"
"$build/bench/bench_index"
"$build/bench/bench_queue"
echo

echo "== capture and replay"
"$build/apps/pulse-sim" --messages 1000000 --instruments 8 --seed 1 --out "$work/session.feed"
"$build/apps/pulse-replay" "$work/session.feed"
echo

for rate in 100000 500000; do
  echo "== live over loopback TCP at $rate msg/s"
  run_live --rate "$rate"
done

echo "== live over loopback TCP, unpaced"
run_live
