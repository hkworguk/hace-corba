//! Safe wrapper over `sys`'s CDR bindings, matching the encode/decode
//! contract of `runtime/include/hace/cdr.h`.

use crate::sys;
use std::fmt;
use std::marker::PhantomData;
use std::os::raw::{c_char, c_int};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ByteOrder {
    Big,
    Little,
}

impl ByteOrder {
    fn to_raw(self) -> sys::hace_cdr_byte_order_t {
        match self {
            ByteOrder::Big => sys::HACE_CDR_BIG_ENDIAN,
            ByteOrder::Little => sys::HACE_CDR_LITTLE_ENDIAN,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CdrError {
    BufferOverflow,
    BufferUnderflow,
    InvalidArgument,
    Unknown(i32),
}

impl CdrError {
    pub(crate) fn from_status(status: sys::hace_cdr_status_t) -> Result<(), CdrError> {
        match status {
            sys::HACE_CDR_OK => Ok(()),
            sys::HACE_CDR_ERR_BUFFER_OVERFLOW => Err(CdrError::BufferOverflow),
            sys::HACE_CDR_ERR_BUFFER_UNDERFLOW => Err(CdrError::BufferUnderflow),
            sys::HACE_CDR_ERR_INVALID_ARGUMENT => Err(CdrError::InvalidArgument),
            other => Err(CdrError::Unknown(other)),
        }
    }
}

impl fmt::Display for CdrError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            CdrError::BufferOverflow => write!(f, "CDR buffer overflow"),
            CdrError::BufferUnderflow => write!(f, "CDR buffer underflow"),
            CdrError::InvalidArgument => write!(f, "invalid argument to CDR call"),
            CdrError::Unknown(code) => write!(f, "unknown CDR status code {code}"),
        }
    }
}

impl std::error::Error for CdrError {}

/// A CDR encode/decode cursor over caller-owned storage. Borrows
/// `storage` for its lifetime; never allocates. Mirrors
/// `hace_cdr_buffer_t`, treated as opaque (see `sys::RawCdrBuffer`).
pub struct CdrBuffer<'a> {
    raw: sys::RawCdrBuffer,
    _storage: PhantomData<&'a mut [u8]>,
}

macro_rules! impl_encode_decode {
    ($ty:ty, $encode_fn:ident, $decode_fn:ident, $sys_encode:path, $sys_decode:path) => {
        pub fn $encode_fn(&mut self, value: $ty) -> Result<(), CdrError> {
            CdrError::from_status(unsafe { $sys_encode(&mut self.raw, value) })
        }

        pub fn $decode_fn(&mut self) -> Result<$ty, CdrError> {
            let mut out: $ty = Default::default();
            let status = unsafe { $sys_decode(&mut self.raw, &mut out) };
            CdrError::from_status(status)?;
            Ok(out)
        }
    };
}

impl<'a> CdrBuffer<'a> {
    pub fn new(storage: &'a mut [u8], byte_order: ByteOrder) -> Result<Self, CdrError> {
        let native_size = unsafe { sys::hace_cdr_buffer_sizeof() };
        assert!(
            native_size <= std::mem::size_of::<sys::RawCdrBuffer>(),
            "hace_cdr_buffer_t ({native_size} bytes) no longer fits the Rust binding's opaque \
             storage ({} bytes) -- update sys::RawCdrBuffer in bindings/rust/hace-corba/src/sys.rs",
            std::mem::size_of::<sys::RawCdrBuffer>()
        );

        let mut buf = CdrBuffer {
            raw: sys::RawCdrBuffer::zeroed(),
            _storage: PhantomData,
        };
        let status = unsafe {
            sys::hace_cdr_buffer_init(
                &mut buf.raw,
                storage.as_mut_ptr(),
                storage.len(),
                byte_order.to_raw(),
            )
        };
        CdrError::from_status(status)?;
        Ok(buf)
    }

    /// Bytes produced so far (encode) or the read cursor (decode).
    pub fn position(&self) -> usize {
        unsafe { sys::hace_cdr_buffer_position(&self.raw) }
    }

    pub fn capacity(&self) -> usize {
        unsafe { sys::hace_cdr_buffer_capacity(&self.raw) }
    }

    /// Exposes the underlying opaque buffer pointer to other modules in
    /// this crate (`giop`, `uiop`) that need to pass it straight through
    /// to a `hace_giop_*`/`hace_uiop_*` C call taking a `hace_cdr_buffer_t *`
    /// -- those functions operate on the same buffer `CdrBuffer` already
    /// owns, not a separate one, so no new wrapper type is needed for
    /// that case (contrast `CdrBufferView` in `uiop.rs`, which exists
    /// because a servant callback's `args`/`reply` are buffers *C*
    /// allocated, not ones this crate constructed).
    pub(crate) fn as_raw_mut_ptr(&mut self) -> *mut sys::RawCdrBuffer {
        &mut self.raw
    }

    /// Rewinds/seeks the cursor -- e.g. `set_position(0)` before decoding
    /// what was just encoded into the same buffer.
    pub fn set_position(&mut self, position: usize) -> Result<(), CdrError> {
        CdrError::from_status(unsafe { sys::hace_cdr_buffer_set_position(&mut self.raw, position) })
    }

    pub fn encode_octet(&mut self, value: u8) -> Result<(), CdrError> {
        CdrError::from_status(unsafe { sys::hace_cdr_encode_octet(&mut self.raw, value) })
    }

    pub fn decode_octet(&mut self) -> Result<u8, CdrError> {
        let mut out: u8 = 0;
        let status = unsafe { sys::hace_cdr_decode_octet(&mut self.raw, &mut out) };
        CdrError::from_status(status)?;
        Ok(out)
    }

    pub fn encode_bool(&mut self, value: bool) -> Result<(), CdrError> {
        let raw: c_int = if value { 1 } else { 0 };
        CdrError::from_status(unsafe { sys::hace_cdr_encode_bool(&mut self.raw, raw) })
    }

    pub fn decode_bool(&mut self) -> Result<bool, CdrError> {
        let mut out: c_int = 0;
        let status = unsafe { sys::hace_cdr_decode_bool(&mut self.raw, &mut out) };
        CdrError::from_status(status)?;
        Ok(out != 0)
    }

    impl_encode_decode!(
        u16,
        encode_u16,
        decode_u16,
        sys::hace_cdr_encode_u16,
        sys::hace_cdr_decode_u16
    );
    impl_encode_decode!(
        i16,
        encode_i16,
        decode_i16,
        sys::hace_cdr_encode_i16,
        sys::hace_cdr_decode_i16
    );
    impl_encode_decode!(
        u32,
        encode_u32,
        decode_u32,
        sys::hace_cdr_encode_u32,
        sys::hace_cdr_decode_u32
    );
    impl_encode_decode!(
        i32,
        encode_i32,
        decode_i32,
        sys::hace_cdr_encode_i32,
        sys::hace_cdr_decode_i32
    );
    impl_encode_decode!(
        u64,
        encode_u64,
        decode_u64,
        sys::hace_cdr_encode_u64,
        sys::hace_cdr_decode_u64
    );
    impl_encode_decode!(
        i64,
        encode_i64,
        decode_i64,
        sys::hace_cdr_encode_i64,
        sys::hace_cdr_decode_i64
    );
    impl_encode_decode!(
        f32,
        encode_f32,
        decode_f32,
        sys::hace_cdr_encode_f32,
        sys::hace_cdr_decode_f32
    );
    impl_encode_decode!(
        f64,
        encode_f64,
        decode_f64,
        sys::hace_cdr_encode_f64,
        sys::hace_cdr_decode_f64
    );

    /// Decodes a CDR string up to `max_length` bytes (including the
    /// NUL the wire format carries).
    pub fn decode_string(&mut self, max_length: usize) -> Result<String, CdrError> {
        let mut storage = vec![0u8; max_length];
        let mut out_length: usize = 0;
        let status = unsafe {
            sys::hace_cdr_decode_string(
                &mut self.raw,
                storage.as_mut_ptr() as *mut c_char,
                storage.len(),
                &mut out_length,
            )
        };
        CdrError::from_status(status)?;
        storage.truncate(out_length);
        Ok(String::from_utf8_lossy(&storage).into_owned())
    }

    pub fn encode_string(&mut self, value: &str) -> Result<(), CdrError> {
        let c_string =
            std::ffi::CString::new(value).map_err(|_| CdrError::InvalidArgument)?;
        CdrError::from_status(unsafe {
            sys::hace_cdr_encode_string(&mut self.raw, c_string.as_ptr(), value.len() + 1)
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn octet_roundtrip() {
        let mut storage = [0u8; 8];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        buf.encode_octet(0x42).unwrap();
        assert_eq!(buf.position(), 1);

        buf.set_position(0).unwrap();
        assert_eq!(buf.decode_octet().unwrap(), 0x42);
    }

    #[test]
    fn bool_roundtrip() {
        let mut storage = [0u8; 8];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Little).unwrap();
        buf.encode_bool(true).unwrap();
        buf.encode_bool(false).unwrap();

        buf.set_position(0).unwrap();
        assert!(buf.decode_bool().unwrap());
        assert!(!buf.decode_bool().unwrap());
    }

    #[test]
    fn u16_alignment_matches_c_runtime() {
        // Exact byte-level padding/ordering is covered by
        // runtime/tests/test_cdr.c; this checks the binding passes
        // alignment through correctly via position() and a roundtrip.
        let mut storage = [0u8; 8];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        buf.encode_octet(0xAA).unwrap();
        buf.encode_u16(0x1234).unwrap();
        // one octet + one pad byte + a 2-byte-aligned u16 == position 4
        assert_eq!(buf.position(), 4);

        buf.set_position(0).unwrap();
        assert_eq!(buf.decode_octet().unwrap(), 0xAA);
        assert_eq!(buf.decode_u16().unwrap(), 0x1234);
    }

    #[test]
    fn u32_byte_order() {
        // Exact byte-level ordering is covered by runtime/tests/test_cdr.c;
        // this checks both byte orders round-trip correctly through the
        // binding.
        let mut storage_be = [0u8; 8];
        let mut storage_le = [0u8; 8];
        let mut buf_be = CdrBuffer::new(&mut storage_be, ByteOrder::Big).unwrap();
        let mut buf_le = CdrBuffer::new(&mut storage_le, ByteOrder::Little).unwrap();

        buf_be.encode_u32(0x0102_0304).unwrap();
        buf_le.encode_u32(0x0102_0304).unwrap();

        buf_be.set_position(0).unwrap();
        buf_le.set_position(0).unwrap();
        assert_eq!(buf_be.decode_u32().unwrap(), 0x0102_0304);
        assert_eq!(buf_le.decode_u32().unwrap(), 0x0102_0304);
    }

    #[test]
    fn i64_roundtrip() {
        let mut storage = [0u8; 16];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        buf.encode_i64(-123_456_789_012_345).unwrap();
        assert_eq!(buf.position(), 8);

        buf.set_position(0).unwrap();
        assert_eq!(buf.decode_i64().unwrap(), -123_456_789_012_345);
    }

    #[test]
    fn f64_roundtrip() {
        let mut storage = [0u8; 16];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Little).unwrap();
        buf.encode_f64(std::f64::consts::PI).unwrap();

        buf.set_position(0).unwrap();
        let out = buf.decode_f64().unwrap();
        assert!((out - std::f64::consts::PI).abs() < 1e-10);
    }

    #[test]
    fn encode_overflow_is_rejected() {
        let mut storage = [0u8; 2];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        assert_eq!(buf.encode_u32(1), Err(CdrError::BufferOverflow));
        assert_eq!(buf.position(), 0);
    }

    #[test]
    fn decode_underflow_is_rejected() {
        let mut storage = [0u8; 2];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        assert_eq!(buf.decode_u32(), Err(CdrError::BufferUnderflow));
    }

    #[test]
    fn set_position_past_capacity_is_rejected() {
        let mut storage = [0u8; 4];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        assert_eq!(
            buf.set_position(5),
            Err(CdrError::InvalidArgument)
        );
    }
}
