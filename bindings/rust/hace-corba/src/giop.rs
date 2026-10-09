//! Safe wrapper for the three GIOP functions examples call directly to
//! build an outgoing request -- `hace_uiop_call`/`hace_uiop_receive`
//! already decode headers internally and hand back plain values, so
//! this crate has no need to bind `hace_giop_decode_*` at all. See
//! `runtime/include/hace/giop.h` and
//! `docs/architecture/adr-0005-rust-ada-orb-bindings.md`.

use crate::cdr::{ByteOrder, CdrBuffer, CdrError};
use crate::sys;
use std::os::raw::c_int;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GiopError {
    Cdr(CdrError),
    Other(i32),
}

impl std::fmt::Display for GiopError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            GiopError::Cdr(e) => write!(f, "GIOP/CDR error: {e}"),
            GiopError::Other(code) => write!(f, "GIOP error code {code}"),
        }
    }
}

impl std::error::Error for GiopError {}

fn from_status(status: sys::hace_giop_status_t) -> Result<(), GiopError> {
    match status {
        sys::HACE_GIOP_OK => Ok(()),
        sys::HACE_CDR_ERR_BUFFER_OVERFLOW => Err(GiopError::Cdr(CdrError::BufferOverflow)),
        sys::HACE_CDR_ERR_BUFFER_UNDERFLOW => Err(GiopError::Cdr(CdrError::BufferUnderflow)),
        sys::HACE_CDR_ERR_INVALID_ARGUMENT => Err(GiopError::Cdr(CdrError::InvalidArgument)),
        other => Err(GiopError::Other(other)),
    }
}

/// A request's fixed header fields, matching `hace_giop_request_header_t`.
/// `object_key`/`operation` are given as byte slices/a `&str` and copied
/// into the fixed-capacity wire representation; both are rejected
/// (`GiopError::Other`) if they don't fit `HACE_GIOP_MAX_OBJECT_KEY`/
/// `HACE_GIOP_MAX_OPERATION_NAME`.
pub struct RequestHeader {
    pub request_id: u32,
    pub response_expected: bool,
    pub object_key: Vec<u8>,
    pub operation: String,
}

/// Sets `buf`'s byte order and reserves the GIOP header region. Call
/// this first, before encoding the request header and any argument
/// bytes.
pub fn begin_message(buf: &mut CdrBuffer, byte_order: ByteOrder) -> Result<(), GiopError> {
    let raw_order = match byte_order {
        ByteOrder::Big => sys::HACE_CDR_BIG_ENDIAN,
        ByteOrder::Little => sys::HACE_CDR_LITTLE_ENDIAN,
    };
    from_status(unsafe { sys::hace_giop_begin_message(buf.as_raw_mut_ptr(), raw_order) })
}

pub fn encode_request_header(buf: &mut CdrBuffer, header: &RequestHeader) -> Result<(), GiopError> {
    if header.object_key.len() > sys::HACE_GIOP_MAX_OBJECT_KEY {
        return Err(GiopError::Other(-1));
    }
    if header.operation.len() >= sys::HACE_GIOP_MAX_OPERATION_NAME {
        return Err(GiopError::Other(-2));
    }

    let mut raw = sys::hace_giop_request_header_t::zeroed();
    raw.request_id = header.request_id;
    raw.response_expected = if header.response_expected { 1 } else { 0 } as c_int;
    raw.object_key[..header.object_key.len()].copy_from_slice(&header.object_key);
    raw.object_key_length = header.object_key.len();
    raw.operation[..header.operation.len()].copy_from_slice(header.operation.as_bytes());

    from_status(unsafe { sys::hace_giop_encode_request_header(buf.as_raw_mut_ptr(), &raw) })
}

/// Call once the request body is fully encoded: writes the real GIOP
/// header now that the body length is known.
pub fn finish_message(buf: &mut CdrBuffer) -> Result<(), GiopError> {
    from_status(unsafe {
        sys::hace_giop_finish_message(buf.as_raw_mut_ptr(), sys::HACE_GIOP_MSG_REQUEST, 0)
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cdr::ByteOrder;

    #[test]
    fn builds_a_request_with_the_16_byte_header_prefix() {
        let mut storage = [0u8; 128];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();

        begin_message(&mut buf, ByteOrder::Big).unwrap();
        assert_eq!(buf.position(), 16);

        encode_request_header(
            &mut buf,
            &RequestHeader {
                request_id: 1,
                response_expected: true,
                object_key: vec![0x01],
                operation: "add".to_string(),
            },
        )
        .unwrap();
        assert!(buf.position() > 16);

        finish_message(&mut buf).unwrap();
        // "GIOP" magic now sits at the very start of the message.
        assert_eq!(&storage[0..4], b"GIOP");
    }

    #[test]
    fn rejects_an_object_key_longer_than_the_wire_limit() {
        let mut storage = [0u8; 128];
        let mut buf = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        begin_message(&mut buf, ByteOrder::Big).unwrap();

        let header = RequestHeader {
            request_id: 1,
            response_expected: true,
            object_key: vec![0u8; sys::HACE_GIOP_MAX_OBJECT_KEY + 1],
            operation: "op".to_string(),
        };
        assert!(encode_request_header(&mut buf, &header).is_err());
    }
}
