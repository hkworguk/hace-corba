# hace-corba (Rust bindings)

Rust bindings for the hace-corba C runtime (`../../../runtime`). Covers
CDR encode/decode (`hace_corba::cdr`) and the HAL's UDP sockets and
monotonic clock (`hace_corba::hal`). **Not part of the DAL C
certification boundary** — see
`../../../docs/architecture/adr-0001-runtime-language.md`.

There is no ORB/POA/GIOP layer in the runtime yet (see
`../../../docs/roadmap.md`), so this crate cannot make CORBA-style RPC,
oneway, async, or observer/attach calls. See
`../../../examples/rust/cdr_udp_echo` for what's demonstrable today.

## How it's wired to the C runtime

`build.rs` compiles `runtime/src/cdr/cdr.c`, `runtime/src/hal/hal_pool.c`,
and `runtime/src/hal/linux/hal_linux.c` directly via `cc`/`ar` (no `cc`
crate dependency, to avoid assuming registry access) and links the
result statically. `src/sys.rs` holds the raw `extern "C"` declarations;
`src/cdr.rs` and `src/hal.rs` are the safe wrappers applications should
use.

`hace_cdr_buffer_t` is treated as opaque (a fixed-size byte array sized
larger than the real struct, checked at runtime against
`hace_cdr_buffer_sizeof()`) rather than mirrored field-by-field, so this
binding can't silently drift out of sync if the C struct's layout
changes. The same pattern is used in the Ada95 bindings
(`../../ada95/hace_corba`).

## Build and test

```sh
cargo build
cargo test
```
