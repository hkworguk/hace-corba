# hace-corba

A CORBA ORB implementation using UDP as the inter-ORB transport
("UIOP" — UDP Inter-ORB Protocol), structured for eventual DO-178C Design
Assurance Level C (DAL C) certification.

## Status

Early scaffold. See `docs/roadmap.md` for the full phased plan. Phase 0
(foundations) and Phase 1 (CDR + GIOP message framing) are done — there
is no UIOP transport (actually sending these over UDP, retry/timeout,
fragmentation-in-practice) or ORB/POA yet, so nothing here can make an
actual CORBA RPC/oneway/async/observer call. Ada95 and Rust bindings
exist for the CDR + HAL core (not yet extended to the new GIOP module),
with examples that round-trip a CDR-encoded message over a raw UDP
socket — see `bindings/` and `examples/`. See
`docs/corba-spec-gap-analysis.md` for the full item-by-item check of
this implementation against the OMG CORBA spec.

## What this is

- A CORBA runtime (`runtime/`, C99) implementing the Common Data
  Representation (CDR) and GIOP message encoding from OMG's CORBA
  Interoperability specification (`runtime/include/hace/cdr.h`,
  `runtime/include/hace/giop.h`) — currently as pure encode/decode over
  caller-supplied buffers, with the actual UDP transport (Phase 2,
  "UIOP") not yet built.
- Hand-written Ada95 and Rust bindings (`bindings/`) onto that runtime's
  CDR and HAL layers, for host tooling and non-safety-critical
  application code — not part of the DAL C certification boundary (see
  `docs/architecture/adr-0001-runtime-language.md`). The IDL-generated
  bindings (Ada95, C, C++, Rust) are separate, later work — see `idlc/`
  below.
- An IDL compiler (`idlc/`, Rust) that will generate Ada95, C, C++, and
  Rust bindings from OMG IDL once the ORB core exists (Phase 4, not
  built yet — currently a skeleton).
- Targets: Ubuntu Linux (fully supported) and Green Hills INTEGRITY-178
  tuMP (HAL abstraction in place; the target backend requires the user's
  licensed Green Hills SDK to complete and test — see
  `runtime/src/hal/integrity178/README.md`).
- A process framework for DO-178C DAL C (`docs/do178c/`) — objectives,
  lifecycle data mapping, coding standard. This is not a certification
  submission; see that document for what it does and doesn't cover.

## Standards basis

Built against specifications published by the Object Management Group at
<https://www.omg.org/corba/>. Exact document IDs and versions are pinned
in `docs/standards/spec-references.md`, including the documented
deviations required to run GIOP over UDP instead of TCP.

## Repository layout

```
docs/        Standards references, architecture decision records, DO-178C
             process framework, roadmap.
idlc/        hace-idlc: the IDL compiler (Rust, host tool, not part of the
             DAL C certification boundary).
runtime/     hace-corba-core: the certified ORB runtime (C99).
bindings/    Hand-written Ada95 and Rust bindings onto the runtime's
             CDR/HAL layers (not part of the DAL C certification boundary).
examples/    Example clients/servers, per language: Rust and Ada95 today
             (CDR + UDP round-trip); RPC/oneway/async/observer wait on
             the ORB core (Phase 2-3, see docs/roadmap.md).
cmake/       CMake toolchain files per target.
ci/          Continuous integration configuration; ci/build_all.sh builds
             and tests every component in one pass.
```

## Building everything

```sh
ci/build_all.sh
```

Builds and tests the C runtime (CMake), the Rust bindings + idlc
skeleton + Rust example (cargo), and the Ada95 bindings + example
(gnatmake) in one pass. Requires cmake, a C compiler, cargo/rustc, and
gnatmake.

## Building the C runtime by itself

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/ubuntu.cmake
cmake --build build
ctest --test-dir build
```

## Building the IDL compiler

```sh
cd idlc
cargo build
cargo test
```

## Building the Rust bindings and example

```sh
cd bindings/rust/hace-corba && cargo build && cargo test
cd ../../../examples/rust/cdr_udp_echo && cargo run
```

## Building the Ada95 bindings, tests, and example

Requires the C runtime to be built first (for
`build/runtime/libhace_corba_core.a`). No `.gpr` project is set up yet;
see `bindings/ada95/hace_corba/README.md` for the `gnatmake` invocation,
or just run `ci/build_all.sh`.
