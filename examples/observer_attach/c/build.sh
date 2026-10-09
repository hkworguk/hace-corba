#!/usr/bin/env bash
# Builds the observer_attach (C) example: configures/builds the
# runtime via CMake if it hasn't been already, then builds just this
# pair's two executables.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"

if [ ! -d "$ROOT/build" ]; then
  echo "== configuring build (cmake) =="
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/ubuntu.cmake"
fi

echo "== building observer_attach (C) =="
cmake --build "$ROOT/build" --target observer_attach_subject_server \
  observer_attach_observer_client
