#!/usr/bin/env bash
# Builds and tests every component of hace-corba: the C runtime (CMake),
# the Rust bindings + idlc skeleton (cargo), and the Ada95 bindings
# (gnatmake), then every example pair under examples/<name>/<lang>/ via
# that pair's own build.sh/run.sh. Run from anywhere; paths are
# resolved relative to this script.
#
# Requires: cmake, a C compiler, cargo/rustc, gnatmake. See README.md.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "== C runtime (CMake) =="
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/ubuntu.cmake"
cmake --build "$ROOT/build"
ctest --test-dir "$ROOT/build" --output-on-failure

echo
echo "== hace-idlc (Rust, skeleton) =="
(cd "$ROOT/idlc" && cargo build && cargo test)

echo
echo "== Rust bindings (bindings/rust/hace-corba) =="
(cd "$ROOT/bindings/rust/hace-corba" && cargo build && cargo test)

echo
echo "== Rust example (examples/cdr_udp_echo/rust) =="
(cd "$ROOT/examples/cdr_udp_echo/rust" && cargo build && cargo run)

echo
echo "== Ada95 bindings + unit tests (bindings/ada95) =="
(cd "$ROOT/bindings/ada95/tests" && \
  gnatmake -gnatwa -I../hace_corba test_hace_corba.adb -o test_hace_corba \
    -largs "$ROOT/build/runtime/libhace_corba_core.a" -lpthread && \
  ./test_hace_corba)

echo
echo "== Ada95 example (examples/cdr_udp_echo/ada95) =="
(cd "$ROOT/examples/cdr_udp_echo/ada95" && \
  gnatmake -I../../../bindings/ada95/hace_corba main.adb -o cdr_udp_echo \
    -largs "$ROOT/build/runtime/libhace_corba_core.a" -lpthread && \
  ./cdr_udp_echo)

echo
echo "== ORB-based example pairs (Phase 5/6: rpc_basic, oneway, async_ami, observer_attach) =="
echo "== each pair's own build.sh/run.sh (examples/<name>/<lang>/) orchestrates its build + run =="

for pair in rpc_basic oneway async_ami observer_attach; do
  for lang in c rust ada95; do
    dir="$ROOT/examples/$pair/$lang"
    echo
    echo "-- $pair ($lang) --"
    "$dir/build.sh"
    "$dir/run.sh"
  done
done

echo
echo "== All builds and tests passed =="
