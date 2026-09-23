# hace_corba (Ada95 bindings)

Ada95 bindings for the hace-corba C runtime (`../../../runtime`): the
`Hace.Cdr` package covers CDR encode/decode, `Hace.Hal` covers the HAL's
UDP sockets and monotonic clock. **Not part of the DAL C certification
boundary** — see `../../../docs/architecture/adr-0001-runtime-language.md`.

There is no ORB/POA/GIOP layer in the runtime yet (see
`../../../docs/roadmap.md`), so these bindings cannot make CORBA-style
RPC, oneway, async, or observer/attach calls. See
`../../../examples/ada95/cdr_udp_echo` for what's demonstrable today, and
`../tests/test_hace_corba.adb` for the binding's own unit test suite.

## Design notes

- Subprograms return `Status` via an `out` parameter rather than raising
  an exception for ordinary failures (buffer overflow/underflow, invalid
  argument), matching `hace_cdr_status_t`/`hace_hal_status_t`'s
  no-exceptions design in the C runtime.
- `hace_cdr_buffer_t` is treated as opaque: `Buffer` wraps a fixed byte
  array sized larger than the real C struct, verified at elaboration
  time against `hace_cdr_buffer_sizeof()` (raises
  `Buffer_Sizeof_Mismatch` if it no longer fits) rather than mirroring
  the C field layout. The same pattern is used in the Rust bindings
  (`../../rust/hace-corba`).
- `Hace.Hal` uses `Ada.Streams.Stream_Element_Array` for datagram
  payloads (the standard Ada type for this), while `Hace.Cdr` uses its
  own `Byte_Array`/`Octet` types — converting between them at the call
  site is a plain element-copy loop; see the example for one.
- These packages call the C runtime through `Interfaces.C` using Ada's
  standard C-interfacing rules (Ada RM Annex B.3): scalar `out`
  parameters of a `pragma Import (C, ...)`-declared subprogram are
  passed by reference automatically, so no manual pointer plumbing is
  needed for `T *out`-style C signatures.

## Build

No `.gpr`/gprbuild project is set up yet — build directly with
`gnatmake`, pointing `-I` at this directory and linking the C runtime's
static library (built via CMake, see the top-level README):

```sh
gnatmake -I. your_program.adb -o your_program \
  -largs /path/to/hace-corba/build/runtime/libhace_corba_core.a -lpthread
```

See `../tests/test_hace_corba.adb` and
`../../../examples/ada95/cdr_udp_echo/main.adb` for complete examples, or
`../../../ci/build_all.sh` for the exact commands used to build and run
everything in this repository.
