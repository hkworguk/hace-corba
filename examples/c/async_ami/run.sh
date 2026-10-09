#!/usr/bin/env bash
# Runs the async_ami (C) example: starts the server in the background,
# runs the client against it, and makes sure the server is cleaned up
# either way. Run ./build.sh first.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
BIN="$ROOT/build/examples/c"

for exe in async_ami_server async_ami_client; do
  if [ ! -x "$BIN/$exe" ]; then
    echo "error: $BIN/$exe not found -- run ./build.sh first" >&2
    exit 1
  fi
done

"$BIN/async_ami_server" &
SERVER_PID=$!
sleep 0.3

if ! "$BIN/async_ami_client"; then
  echo "FAILED: async_ami client exited non-zero" >&2
  kill "$SERVER_PID" 2>/dev/null || true
  wait "$SERVER_PID" 2>/dev/null || true
  exit 1
fi

wait "$SERVER_PID"
