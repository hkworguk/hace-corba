//! Rust bindings for the hace-corba CDR/HAL runtime.
//!
//! This crate binds the certified C runtime in `runtime/` for host
//! tooling, examples, and non-safety-critical application code. It is
//! **not** part of the DAL C certification boundary -- see
//! `docs/architecture/adr-0001-runtime-language.md`.
//!
//! Only CDR encode/decode and the HAL (UDP sockets, monotonic time) are
//! bound so far, matching what the runtime itself implements today.
//! There is no ORB/POA yet (see `docs/roadmap.md`), so this crate cannot
//! make CORBA-style RPC/oneway/async/observer calls -- see
//! `examples/rust/cdr_udp_echo` for what's demonstrable at this stage.

mod sys;

pub mod cdr;
pub mod hal;

pub use cdr::{ByteOrder, CdrBuffer, CdrError};
pub use hal::{monotonic_now, HalError, UdpSocket};
