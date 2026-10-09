#!/usr/bin/env bash
# Runs the rpc_basic (C) example: starts the server in the background,
# runs the client against it, and makes sure the server is cleaned up
# either way. Run ./build.sh first.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
BIN="$ROOT/build/examples/c"

for exe in rpc_basic_server rpc_basic_client; do
  if [ ! -x "$BIN/$exe" ]; then
    echo "error: $BIN/$exe not found -- run ./build.sh first" >&2
    exit 1
  fi
done

"$BIN/rpc_basic_server" &
SERVER_PID=$!
sleep 0.3

if ! "$BIN/rpc_basic_client"; then
  echo "FAILED: rpc_basic client exited non-zero" >&2
  kill "$SERVER_PID" 2>/dev/null || true
  wait "$SERVER_PID" 2>/dev/null || true
  exit 1
fi

wait "$SERVER_PID"
