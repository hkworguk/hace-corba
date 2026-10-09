#!/usr/bin/env bash
# Runs the oneway (Ada95) example: starts the server in the
# background, runs the client against it, and makes sure the server is
# cleaned up either way. Run ./build.sh first.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for exe in oneway_server oneway_client; do
  if [ ! -x "$HERE/$exe" ]; then
    echo "error: $HERE/$exe not found -- run ./build.sh first" >&2
    exit 1
  fi
done

"$HERE/oneway_server" &
SERVER_PID=$!
sleep 0.3

if ! "$HERE/oneway_client"; then
  echo "FAILED: oneway client exited non-zero" >&2
  kill "$SERVER_PID" 2>/dev/null || true
  wait "$SERVER_PID" 2>/dev/null || true
  exit 1
fi

wait "$SERVER_PID"
