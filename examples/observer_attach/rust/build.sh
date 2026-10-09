#!/usr/bin/env bash
# Builds the observer_attach (Rust) example. cargo's build.rs compiles
# the certified C runtime sources directly into the hace-corba crate,
# so there's no separate "build the runtime first" step to
# orchestrate -- a plain `cargo build` here already does that as part
# of its own dependency graph.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

echo "== building observer_attach (Rust) =="
cargo build
