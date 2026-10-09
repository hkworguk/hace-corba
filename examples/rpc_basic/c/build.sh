#!/usr/bin/env bash
# Builds the rpc_basic (C) example: configures/builds the runtime via
# CMake if it hasn't been already, then builds just this pair's two
# executables.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"

if [ ! -d "$ROOT/build" ]; then
  echo "== configuring build (cmake) =="
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/ubuntu.cmake"
fi

echo "== building rpc_basic (C) =="
cmake --build "$ROOT/build" --target rpc_basic_server rpc_basic_client
