//! Rust bindings for the hace-corba runtime.
//!
//! This crate binds the certified C runtime in `runtime/` for host
//! tooling, examples, and non-safety-critical application code. It is
//! **not** part of the DAL C certification boundary -- see
//! `docs/architecture/adr-0001-runtime-language.md`.
//!
//! CDR, HAL, GIOP (just enough to build a request), UIOP, and the
//! CORBA core (exceptions, object references, a minimal POA) are
//! bound -- enough for the four `examples/*/rust` ORB-based pairs
//! (`rpc_basic`, `oneway`, `async_ami`, `observer_attach`), matching
//! `examples/*/c`. Not the full C API surface of each module -- see
//! `docs/architecture/adr-0005-rust-ada-orb-bindings.md` for exactly
//! what's bound and why, and what's deliberately left for later.

mod sys;

/// Re-exported so examples can write an `unsafe extern "C" fn` matching
/// `ServantFn`'s exact signature -- its `args`/`reply` parameters are
/// `*mut RawCdrBuffer`, and that type must be nameable from outside
/// this crate for such a function to type-check, even though the
/// `sys` module itself stays private.
pub use sys::RawCdrBuffer;

pub mod cdr;
pub mod corba;
pub mod giop;
pub mod hal;
pub mod uiop;

pub use cdr::{ByteOrder, CdrBuffer, CdrError};
pub use corba::{CorbaError, Ior, Poa, ServantFn, SystemException};
pub use giop::{GiopError, RequestHeader};
pub use hal::{monotonic_now, HalError, UdpSocket};
pub use uiop::{CallResult, CdrBufferView, ReceivedReply, ReceivedRequest, RequestInfo, Transport, UiopError};
