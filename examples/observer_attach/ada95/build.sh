#!/usr/bin/env bash
# Builds the observer_attach (Ada95) example: builds the runtime's
# static library via CMake if it hasn't been already, then compiles
# this pair's subject server and observer client with gnatmake.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
RUNTIME_LIB="$ROOT/build/runtime/libhace_corba_core.a"

if [ ! -f "$RUNTIME_LIB" ]; then
  echo "== building runtime (cmake) =="
  if [ ! -d "$ROOT/build" ]; then
    cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/ubuntu.cmake"
  fi
  cmake --build "$ROOT/build" --target hace_corba_core
fi

echo "== building observer_attach (Ada95) =="
cd "$HERE"
gnatmake -gnatwa -I../../../bindings/ada95/hace_corba -I../../common/ada95 subject_server.adb \
  -o observer_attach_subject_server -largs "$RUNTIME_LIB" -lpthread
gnatmake -gnatwa -I../../../bindings/ada95/hace_corba -I../../common/ada95 observer_client.adb \
  -o observer_attach_observer_client -largs "$RUNTIME_LIB" -lpthread
