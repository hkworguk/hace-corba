//! Safe-ish wrappers for the CORBA core: exceptions, object references
//! (IOR), and the POA. See `runtime/include/hace/corba_exception.h`,
//! `corba_ior.h`, `corba_poa.h`, and
//! `docs/architecture/adr-0005-rust-ada-orb-bindings.md`.

use crate::cdr::CdrBuffer;
use crate::sys;
use crate::uiop::{CdrBufferView, Transport, UiopError};
use std::ffi::{CStr, CString};
use std::net::Ipv4Addr;
use std::os::raw::{c_char, c_void};

pub use sys::hace_corba_servant_fn_t as ServantFn;
pub use sys::hace_corba_system_exception_t as SystemException;
pub use sys::{
    HACE_CORBA_COMPLETED_NO as COMPLETED_NO, HACE_CORBA_EXC_BAD_OPERATION as EXC_BAD_OPERATION,
    HACE_CORBA_EXC_BAD_PARAM as EXC_BAD_PARAM, HACE_CORBA_EXC_MARSHAL as EXC_MARSHAL,
    HACE_CORBA_EXC_NONE as EXC_NONE, HACE_CORBA_EXC_NO_MEMORY as EXC_NO_MEMORY,
    HACE_CORBA_EXC_OBJECT_NOT_EXIST as EXC_OBJECT_NOT_EXIST,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CorbaError {
    InvalidArgument,
    BufferOverflow,
    BufferUnderflow,
    Unknown(i32),
}

impl std::fmt::Display for CorbaError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CorbaError::InvalidArgument => write!(f, "invalid argument to a CORBA core call"),
            CorbaError::BufferOverflow => write!(f, "CDR buffer overflow"),
            CorbaError::BufferUnderflow => write!(f, "CDR buffer underflow"),
            CorbaError::Unknown(code) => write!(f, "unknown CORBA status code {code}"),
        }
    }
}

impl std::error::Error for CorbaError {}

impl CorbaError {
    fn from_status(status: sys::hace_corba_status_t) -> Result<(), CorbaError> {
        match status {
            sys::HACE_CORBA_OK => Ok(()),
            sys::HACE_CORBA_ERR_INVALID_ARGUMENT => Err(CorbaError::InvalidArgument),
            sys::HACE_CORBA_ERR_BUFFER_OVERFLOW => Err(CorbaError::BufferOverflow),
            sys::HACE_CORBA_ERR_BUFFER_UNDERFLOW => Err(CorbaError::BufferUnderflow),
            other => Err(CorbaError::Unknown(other)),
        }
    }
}

/// Encodes/decodes a `SystemException` as a Reply's exception body
/// (plain three-`u32` wire format, no repository-ID string -- see the
/// ADR). A servant never calls these itself: it just sets the fields
/// of the `*mut SystemException` the dispatcher hands it, and
/// `corba_poa.c` encodes it on the wire. These are for the *client*
/// side, decoding an exception out of a Reply body it already has.
pub fn decode_system_exception(buf: &mut CdrBuffer) -> Result<SystemException, CorbaError> {
    let mut out = SystemException {
        exception: sys::HACE_CORBA_EXC_NONE,
        minor_code: 0,
        completion_status: sys::HACE_CORBA_COMPLETED_NO,
    };
    CorbaError::from_status(unsafe {
        sys::hace_corba_decode_system_exception(buf.as_raw_mut_ptr(), &mut out)
    })?;
    Ok(out)
}

/// An object reference: host/port/object-key, matching `hace_corba_ior_t`.
pub struct Ior {
    pub host_ipv4: Ipv4Addr,
    pub port: u16,
    pub object_key: Vec<u8>,
}

impl Ior {
    pub fn new(host_ipv4: Ipv4Addr, port: u16, object_key: &[u8]) -> Self {
        Ior {
            host_ipv4,
            port,
            object_key: object_key.to_vec(),
        }
    }

    fn to_raw(&self) -> Result<sys::hace_corba_ior_t, CorbaError> {
        if self.object_key.len() > sys::HACE_GIOP_MAX_OBJECT_KEY {
            return Err(CorbaError::InvalidArgument);
        }
        let mut raw = sys::hace_corba_ior_t::zeroed();
        raw.host_ipv4 = u32::from(self.host_ipv4);
        raw.port = self.port;
        raw.object_key[..self.object_key.len()].copy_from_slice(&self.object_key);
        raw.object_key_length = self.object_key.len();
        Ok(raw)
    }

    fn from_raw(raw: &sys::hace_corba_ior_t) -> Self {
        Ior {
            host_ipv4: Ipv4Addr::from(raw.host_ipv4),
            port: raw.port,
            object_key: raw.object_key[..raw.object_key_length].to_vec(),
        }
    }

    /// CDR-encodes this IOR as a request/reply argument (e.g.
    /// `observer_attach`'s `attach(in Observer obs)`).
    pub fn encode(&self, buf: &mut CdrBuffer) -> Result<(), CorbaError> {
        let raw = self.to_raw()?;
        CorbaError::from_status(unsafe { sys::hace_corba_ior_encode(buf.as_raw_mut_ptr(), &raw) })
    }

    pub fn decode(buf: &mut CdrBuffer) -> Result<Self, CorbaError> {
        let mut raw = sys::hace_corba_ior_t::zeroed();
        CorbaError::from_status(unsafe {
            sys::hace_corba_ior_decode(buf.as_raw_mut_ptr(), &mut raw)
        })?;
        Ok(Self::from_raw(&raw))
    }

    /// Same as `decode`, but for a servant's C-owned `args` buffer
    /// (`observer_attach`'s `attach(in Observer obs)`) rather than a
    /// client-built, crate-owned one.
    pub fn decode_view(view: &mut CdrBufferView) -> Result<Self, CorbaError> {
        let mut raw = sys::hace_corba_ior_t::zeroed();
        CorbaError::from_status(unsafe {
            sys::hace_corba_ior_decode(view.as_raw_mut_ptr(), &mut raw)
        })?;
        Ok(Self::from_raw(&raw))
    }

    /// `"uiop:a.b.c.d:port/hexkey"`, as a server announces its own
    /// address on startup.
    pub fn to_uiop_string(&self) -> Result<String, CorbaError> {
        let raw = self.to_raw()?;
        let mut storage = vec![0u8; sys::HACE_CORBA_MAX_IOR_STRING];
        CorbaError::from_status(unsafe {
            sys::hace_corba_object_to_string(
                &raw,
                storage.as_mut_ptr() as *mut c_char,
                storage.len(),
            )
        })?;
        let cstr = unsafe { CStr::from_ptr(storage.as_ptr() as *const c_char) };
        Ok(cstr.to_string_lossy().into_owned())
    }

    /// Parses a string in exactly the format `to_uiop_string` produces.
    pub fn from_uiop_string(s: &str) -> Result<Self, CorbaError> {
        let c_string = CString::new(s).map_err(|_| CorbaError::InvalidArgument)?;
        let mut raw = sys::hace_corba_ior_t::zeroed();
        CorbaError::from_status(unsafe {
            sys::hace_corba_string_to_object(c_string.as_ptr(), &mut raw)
        })?;
        Ok(Self::from_raw(&raw))
    }
}

/// A minimal POA: bounded servant table, activated before any dispatch
/// begins (see the ADR for why that means no locking is needed here
/// even though `handle_one_request` may be called from multiple
/// threads). Treated as opaque storage (`RawPoa`) rather than mirrored
/// field-by-field -- see `sys::RawPoa`'s doc comment.
pub struct Poa {
    raw: sys::RawPoa,
}

impl Poa {
    pub fn new() -> Result<Self, CorbaError> {
        let native_size = unsafe { sys::hace_corba_poa_sizeof() };
        assert!(
            native_size <= std::mem::size_of::<sys::RawPoa>(),
            "hace_corba_poa_t ({native_size} bytes) no longer fits the Rust binding's opaque \
             storage ({} bytes) -- update sys::RawPoa in bindings/rust/hace-corba/src/sys.rs",
            std::mem::size_of::<sys::RawPoa>()
        );

        let mut poa = Poa {
            raw: sys::RawPoa::zeroed(),
        };
        CorbaError::from_status(unsafe { sys::hace_corba_poa_init(&mut poa.raw) })?;
        Ok(poa)
    }

    /// Registers `servant`/`context` under `object_key`. Must be called
    /// before any worker calls `handle_one_request` on this POA -- see
    /// the module doc comment.
    pub fn activate_object(
        &mut self,
        object_key: &[u8],
        servant: ServantFn,
        context: *mut c_void,
    ) -> Result<(), CorbaError> {
        CorbaError::from_status(unsafe {
            sys::hace_corba_poa_activate_object(
                &mut self.raw,
                object_key.as_ptr(),
                object_key.len(),
                servant,
                context,
            )
        })
    }

    /// Waits up to `timeout_ms` for one Request, dispatches it, and
    /// sends the Reply (or nothing, for oneway). `Err(UiopError::Timeout)`
    /// is routine ("no work this round"), not a failure -- matching
    /// `hace_corba_poa_handle_one_request`'s own contract in C.
    pub fn handle_one_request(
        &self,
        transport: &Transport,
        timeout_ms: u32,
        max_datagram_size: usize,
    ) -> Result<(), UiopError> {
        UiopError::from_status(unsafe {
            sys::hace_corba_poa_handle_one_request(
                &self.raw,
                transport.as_raw_ptr(),
                timeout_ms,
                max_datagram_size,
            )
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cdr::ByteOrder;

    #[test]
    fn ior_cdr_roundtrip() {
        let ior = Ior::new(Ipv4Addr::new(127, 0, 0, 1), 59101, &[0xDE, 0xAD]);
        let mut storage = [0u8; 64];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        ior.encode(&mut buf).unwrap();

        buf.set_position(0).unwrap();
        let decoded = Ior::decode(&mut buf).unwrap();
        assert_eq!(decoded.host_ipv4, ior.host_ipv4);
        assert_eq!(decoded.port, ior.port);
        assert_eq!(decoded.object_key, ior.object_key);
    }

    #[test]
    fn ior_string_roundtrip() {
        let ior = Ior::new(Ipv4Addr::new(192, 168, 0, 1), 12345, &[0x10, 0x11]);
        let s = ior.to_uiop_string().unwrap();
        assert_eq!(s, "uiop:192.168.0.1:12345/1011");

        let decoded = Ior::from_uiop_string(&s).unwrap();
        assert_eq!(decoded.host_ipv4, ior.host_ipv4);
        assert_eq!(decoded.port, ior.port);
        assert_eq!(decoded.object_key, ior.object_key);
    }

    #[test]
    fn system_exception_decode_roundtrip() {
        let mut storage = [0u8; 32];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        // Hand-encode the same three-u32 wire format corba_poa.c produces.
        buf.encode_u32(EXC_OBJECT_NOT_EXIST as u32).unwrap();
        buf.encode_u32(0).unwrap();
        buf.encode_u32(COMPLETED_NO as u32).unwrap();

        buf.set_position(0).unwrap();
        let exc = decode_system_exception(&mut buf).unwrap();
        assert_eq!(exc.exception, EXC_OBJECT_NOT_EXIST);
        assert_eq!(exc.completion_status, COMPLETED_NO);
    }

    #[test]
    fn poa_new_and_activate_object_succeed() {
        unsafe extern "C" fn noop_servant(
            _context: *mut c_void,
            _operation: *const c_char,
            _args: *mut sys::RawCdrBuffer,
            _reply: *mut sys::RawCdrBuffer,
            _out_exception: *mut SystemException,
        ) {
        }

        let mut poa = Poa::new().unwrap();
        poa.activate_object(&[0x01], noop_servant, std::ptr::null_mut())
            .unwrap();
    }
}
