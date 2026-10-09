#!/usr/bin/env bash
# Builds and tests every component of hace-corba: the C runtime (CMake),
# the Rust bindings + idlc skeleton + Rust examples (cargo), and the
# Ada95 bindings + examples (gnatmake). Run from anywhere; paths are
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
echo "== C examples (Phase 5: rpc_basic, oneway, async_ami, observer_attach) =="

EXAMPLES_BIN="$ROOT/build/examples/c"

run_example_pair() {
  local name="$1"
  local server_bin="$2"
  local client_bin="$3"
  echo "-- $name --"
  "$EXAMPLES_BIN/$server_bin" &
  local server_pid=$!
  sleep 0.3
  if ! "$EXAMPLES_BIN/$client_bin"; then
    echo "FAILED: $name client exited non-zero" >&2
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    exit 1
  fi
  wait "$server_pid"
}

run_example_pair "rpc_basic" rpc_basic_server rpc_basic_client
run_example_pair "oneway" oneway_server oneway_client
run_example_pair "async_ami" async_ami_server async_ami_client
run_example_pair "observer_attach" observer_attach_subject_server \
  observer_attach_observer_client

echo
echo "== Rust examples (Phase 6: rpc_basic, oneway, async_ami, observer_attach) =="

RUST_EXAMPLES="$ROOT/examples/rust"

run_rust_example_pair() {
  local name="$1"
  local crate_dir="$2"
  local server_bin="$3"
  local client_bin="$4"
  echo "-- $name (rust) --"
  (cd "$RUST_EXAMPLES/$crate_dir" && cargo build)
  local bin_dir="$RUST_EXAMPLES/$crate_dir/target/debug"
  "$bin_dir/$server_bin" &
  local server_pid=$!
  sleep 0.3
  if ! "$bin_dir/$client_bin"; then
    echo "FAILED: $name (rust) client exited non-zero" >&2
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    exit 1
  fi
  wait "$server_pid"
}

run_rust_example_pair "rpc_basic" rpc_basic rpc_basic_server rpc_basic_client
run_rust_example_pair "oneway" oneway oneway_server oneway_client
run_rust_example_pair "async_ami" async_ami async_ami_server async_ami_client
run_rust_example_pair "observer_attach" observer_attach \
  observer_attach_subject_server observer_attach_observer_client

echo
echo "== Ada95 examples (Phase 6: rpc_basic, oneway, async_ami, observer_attach) =="

ADA_EXAMPLES="$ROOT/examples/ada95"

run_ada_example_pair() {
  local name="$1"
  local dir="$2"
  local server_src="$3"
  local server_bin="$4"
  local client_src="$5"
  local client_bin="$6"
  echo "-- $name (ada95) --"
  (cd "$ADA_EXAMPLES/$dir" && \
    gnatmake -gnatwa -I../../../bindings/ada95/hace_corba -I../common "$server_src" \
      -o "$server_bin" -largs "$ROOT/build/runtime/libhace_corba_core.a" -lpthread && \
    gnatmake -gnatwa -I../../../bindings/ada95/hace_corba -I../common "$client_src" \
      -o "$client_bin" -largs "$ROOT/build/runtime/libhace_corba_core.a" -lpthread)
  local bin_dir="$ADA_EXAMPLES/$dir"
  "$bin_dir/$server_bin" &
  local server_pid=$!
  sleep 0.3
  if ! "$bin_dir/$client_bin"; then
    echo "FAILED: $name (ada95) client exited non-zero" >&2
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    exit 1
  fi
  wait "$server_pid"
}

run_ada_example_pair "rpc_basic" rpc_basic server.adb rpc_basic_server \
  client.adb rpc_basic_client
run_ada_example_pair "oneway" oneway server.adb oneway_server client.adb oneway_client
run_ada_example_pair "async_ami" async_ami server.adb async_ami_server \
  client.adb async_ami_client
run_ada_example_pair "observer_attach" observer_attach subject_server.adb \
  observer_attach_subject_server observer_client.adb observer_attach_observer_client

echo
echo "== All builds and tests passed =="
