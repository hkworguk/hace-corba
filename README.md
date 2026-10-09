# hace-corba

A CORBA ORB implementation using UDP as the inter-ORB transport
("UIOP" — UDP Inter-ORB Protocol), structured for eventual DO-178C Design
Assurance Level C (DAL C) certification.

## Status

Early scaffold. See `docs/roadmap.md` for the full phased plan. Phase 0
through Phase 5 are done: CDR + GIOP message framing, the UIOP UDP
transport (send/receive over real sockets, bounded retry/timeout, real
fragmentation), a minimal ORB core (exceptions, object references, a
POA dispatching Requests to activated servants across a bounded,
concurrent worker-task pool), and four real client/server example pairs
in `examples/c/` — `rpc_basic`, `oneway`, `async_ami`, `observer_attach`
— each two separate executables actually talking CORBA over UDP (see
`examples/c/README.md`). What's still missing is `idlc` actually
generating code from IDL (Phase 4) — every example's servants/stubs are
hand-written against the runtime directly. Ada95 and Rust bindings
exist for the CDR + HAL core only (not yet extended to GIOP/UIOP/the
CORBA modules), with their own simpler CDR-round-trip examples — see
`bindings/` and `examples/rust`/`examples/ada95`. See
`docs/corba-spec-gap-analysis.md` for the full item-by-item check of
this implementation against the OMG CORBA spec.

## What this is

- A CORBA runtime (`runtime/`, C99) implementing the Common Data
  Representation (CDR), GIOP message encoding, the UIOP UDP transport,
  and a minimal ORB core from OMG's CORBA specifications
  (`runtime/include/hace/cdr.h`, `giop.h`, `uiop.h`, `corba_exception.h`,
  `corba_ior.h`, `corba_poa.h`): request/reply with bounded
  retry/timeout, oneway, real fragmentation, a chosen subset of system
  exceptions, object references, and a static POA dispatching Requests
  to activated servants across a bounded worker-task pool — all working
  over actual UDP sockets. What's still missing is `idlc` generating
  code from IDL (Phase 4); every example in `examples/c/` hand-writes
  its servants/stubs against the runtime directly instead.
- Hand-written Ada95 and Rust bindings (`bindings/`) onto that runtime's
  CDR and HAL layers, for host tooling and non-safety-critical
  application code — not part of the DAL C certification boundary (see
  `docs/architecture/adr-0001-runtime-language.md`). The IDL-generated
  bindings (Ada95, C, C++, Rust) are separate, later work — see `idlc/`
  below.
- An IDL compiler (`idlc/`, Rust) that will generate Ada95, C, C++, and
  Rust bindings from OMG IDL (Phase 4, not built yet — currently a
  skeleton) — now that the ORB core it would target (Phase 3: POA,
  servants, exceptions) exists.
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
examples/    Example clients/servers. examples/c/: four real ORB-based
             pairs (rpc_basic, oneway, async_ami, observer_attach), see
             examples/c/README.md. examples/rust/, examples/ada95/:
             simpler CDR+UDP round-trip examples for those bindings.
cmake/       CMake toolchain files per target.
ci/          Continuous integration configuration; ci/build_all.sh builds
             and tests every component in one pass.
```

## Building everything

```sh
ci/build_all.sh
```

Builds and tests the C runtime plus the four `examples/c/` ORB-based
example pairs (CMake — actually runs each pair, not just compiles it),
the Rust bindings + idlc skeleton + Rust example (cargo), and the Ada95
bindings + example (gnatmake), in one pass. Requires cmake, a C
compiler, cargo/rustc, and gnatmake.

## Building the C runtime by itself

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/ubuntu.cmake
cmake --build build
ctest --test-dir build
```

This also builds the `examples/c/` pairs (disable with
`-DHACE_BUILD_EXAMPLES=OFF`); see `examples/c/README.md` to run them.

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
