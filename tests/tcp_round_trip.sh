#!/bin/sh
set -eu

sim="$1"
feed="$2"
book="$3"
port=$((40000 + $$ % 20000))

"$sim" --messages 200000 --instruments 4 --out tcp.feed > /dev/null
"$feed" tcp.feed --port "$port" --rate 500000 > feed.log &
feed_pid=$!

status=0
"$book" --port "$port" || status=$?

kill "$feed_pid" 2> /dev/null || true
wait "$feed_pid" 2> /dev/null || true
exit "$status"
