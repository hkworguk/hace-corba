#!/usr/bin/env bash
# Builds and tests every component of hace-corba: the C runtime (CMake),
# the Rust bindings + idlc skeleton + Rust example (cargo), and the
# Ada95 bindings + example (gnatmake). Run from anywhere; paths are
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
echo "== Rust example (examples/rust/cdr_udp_echo) =="
(cd "$ROOT/examples/rust/cdr_udp_echo" && cargo build && cargo run)

echo
echo "== Ada95 bindings + unit tests (bindings/ada95) =="
(cd "$ROOT/bindings/ada95/tests" && \
  gnatmake -gnatwa -I../hace_corba test_hace_corba.adb -o test_hace_corba \
    -largs "$ROOT/build/runtime/libhace_corba_core.a" -lpthread && \
  ./test_hace_corba)

echo
echo "== Ada95 example (examples/ada95/cdr_udp_echo) =="
(cd "$ROOT/examples/ada95/cdr_udp_echo" && \
  gnatmake -I../../../bindings/ada95/hace_corba main.adb -o cdr_udp_echo \
    -largs "$ROOT/build/runtime/libhace_corba_core.a" -lpthread && \
  ./cdr_udp_echo)

echo
echo "== All builds and tests passed =="
