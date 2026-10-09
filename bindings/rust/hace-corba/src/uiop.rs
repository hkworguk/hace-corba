//! Safe wrapper for the UIOP transport functions the examples call
//! directly: `hace_uiop_transport_for_socket`/`send_message`/`call`/
//! `receive`. See `runtime/include/hace/uiop.h` and
//! `docs/architecture/adr-0005-rust-ada-orb-bindings.md`.

use crate::cdr::{CdrBuffer, CdrError};
use crate::hal::UdpSocket;
use crate::sys;
use std::marker::PhantomData;
use std::net::Ipv4Addr;
use std::os::raw::{c_char, c_int};

pub use sys::HACE_UIOP_MAX_DATAGRAM_SIZE;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UiopError {
    InvalidArgument,
    Hal,
    TooLarge,
    Timeout,
    Unknown(i32),
}

impl std::fmt::Display for UiopError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            UiopError::InvalidArgument => write!(f, "invalid argument to a UIOP call"),
            UiopError::Hal => write!(f, "UIOP transport (HAL) error"),
            UiopError::TooLarge => write!(f, "message needs more fragments than allowed"),
            UiopError::Timeout => write!(f, "UIOP call timed out"),
            UiopError::Unknown(code) => write!(f, "unknown UIOP status code {code}"),
        }
    }
}

impl std::error::Error for UiopError {}

impl UiopError {
    pub(crate) fn from_status(status: sys::hace_uiop_status_t) -> Result<(), UiopError> {
        match status {
            sys::HACE_UIOP_OK => Ok(()),
            sys::HACE_UIOP_ERR_INVALID_ARGUMENT => Err(UiopError::InvalidArgument),
            sys::HACE_UIOP_ERR_HAL => Err(UiopError::Hal),
            sys::HACE_UIOP_ERR_TOO_LARGE => Err(UiopError::TooLarge),
            sys::HACE_UIOP_ERR_TIMEOUT => Err(UiopError::Timeout),
            other => Err(UiopError::Unknown(other)),
        }
    }
}

/// A transport bound to a `UdpSocket`, for `send_message`/`call`/
/// `receive_*`. Borrows the socket for its lifetime, matching
/// `hace_uiop_transport_for_socket`'s own contract in C (the handle
/// must stay valid and open for as long as the transport is used).
pub struct Transport<'a> {
    raw: sys::hace_uiop_transport_t,
    _socket: PhantomData<&'a UdpSocket>,
}

impl<'a> Transport<'a> {
    pub fn for_socket(socket: &'a UdpSocket) -> Self {
        let raw = unsafe { sys::hace_uiop_transport_for_socket(socket.raw_handle_ptr()) };
        Transport {
            raw,
            _socket: PhantomData,
        }
    }

    /// Exposes the underlying transport to `corba::Poa::handle_one_request`,
    /// a sibling module that needs to pass it straight through to
    /// `hace_corba_poa_handle_one_request`.
    pub(crate) fn as_raw_ptr(&self) -> *const sys::hace_uiop_transport_t {
        &self.raw
    }
}

/// Sends `message` (fully encoded via `giop::begin_message` + a
/// per-type encode + `giop::finish_message`) to `dest`:`dest_port`,
/// fragmenting transparently if it doesn't fit `max_datagram_size`.
/// This is also the oneway primitive: a request encoded with
/// `response_expected = false` and no following `call`/`receive_reply`
/// is a complete oneway call.
pub fn send_message(
    transport: &Transport,
    message: &mut CdrBuffer,
    request_id: u32,
    dest: Ipv4Addr,
    dest_port: u16,
    max_datagram_size: usize,
) -> Result<(), UiopError> {
    UiopError::from_status(unsafe {
        sys::hace_uiop_send_message(
            &transport.raw,
            message.as_raw_mut_ptr(),
            sys::HACE_GIOP_MSG_REQUEST,
            request_id,
            u32::from(dest),
            dest_port,
            max_datagram_size,
        )
    })
}

pub struct CallResult {
    pub is_exception: bool,
    pub body_length: usize,
}

/// The twoway client primitive: sends `request` and waits
/// `per_attempt_timeout_ms` for a matching Reply, resending up to
/// `max_attempts` times before giving up with `UiopError::Timeout`.
#[allow(clippy::too_many_arguments)]
pub fn call(
    transport: &Transport,
    request: &mut CdrBuffer,
    request_id: u32,
    dest: Ipv4Addr,
    dest_port: u16,
    max_datagram_size: usize,
    per_attempt_timeout_ms: u32,
    max_attempts: u32,
    reply_body: &mut [u8],
) -> Result<CallResult, UiopError> {
    let mut reply_status: c_int = 0;
    let mut reply_body_length: usize = 0;

    let status = unsafe {
        sys::hace_uiop_call(
            &transport.raw,
            request.as_raw_mut_ptr(),
            request_id,
            u32::from(dest),
            dest_port,
            max_datagram_size,
            per_attempt_timeout_ms,
            max_attempts,
            reply_body.as_mut_ptr(),
            reply_body.len(),
            &mut reply_status,
            &mut reply_body_length,
        )
    };
    UiopError::from_status(status)?;
    Ok(CallResult {
        is_exception: reply_status != 0, /* 0 == NO_EXCEPTION */
        body_length: reply_body_length,
    })
}

/// The object key/operation/response_expected a dispatcher needs to
/// route a received Request -- `request_id` alone isn't enough for
/// that. Matches `hace_uiop_request_info_t`.
pub struct RequestInfo {
    pub object_key: Vec<u8>,
    pub operation: String,
    pub response_expected: bool,
}

pub struct ReceivedRequest {
    pub request_id: u32,
    pub body_length: usize,
    pub src_addr: Ipv4Addr,
    pub src_port: u16,
    pub info: RequestInfo,
}

fn nul_terminated_to_string(bytes: &[u8]) -> String {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..end]).into_owned()
}

/// Waits up to `timeout_ms` for a complete Request (reassembling
/// fragments transparently), for a server dispatching to a servant.
pub fn receive_request(
    transport: &Transport,
    timeout_ms: u32,
    body: &mut [u8],
) -> Result<ReceivedRequest, UiopError> {
    let mut header = sys::hace_giop_message_header_t::zeroed();
    let mut request_id: u32 = 0;
    let mut body_length: usize = 0;
    let mut src_addr: u32 = 0;
    let mut src_port: u16 = 0;
    let mut info_raw = sys::hace_uiop_request_info_t::zeroed();

    let status = unsafe {
        sys::hace_uiop_receive(
            &transport.raw,
            sys::HACE_GIOP_MSG_REQUEST,
            std::ptr::null(),
            timeout_ms,
            body.as_mut_ptr(),
            body.len(),
            &mut header,
            &mut request_id,
            std::ptr::null_mut(),
            &mut body_length,
            &mut src_addr,
            &mut src_port,
            &mut info_raw,
        )
    };
    UiopError::from_status(status)?;

    Ok(ReceivedRequest {
        request_id,
        body_length,
        src_addr: Ipv4Addr::from(src_addr),
        src_port,
        info: RequestInfo {
            object_key: info_raw.object_key[..info_raw.object_key_length].to_vec(),
            operation: nul_terminated_to_string(&info_raw.operation),
            response_expected: info_raw.response_expected != 0,
        },
    })
}

pub struct ReceivedReply {
    pub is_exception: bool,
    pub body_length: usize,
}

/// Waits up to `timeout_ms` for a Reply matching `match_request_id` --
/// the Poller-style half of an asynchronous invocation (send via
/// `send_message`, do other work, poll this on a short timeout until
/// the reply lands).
pub fn receive_reply(
    transport: &Transport,
    match_request_id: u32,
    timeout_ms: u32,
    body: &mut [u8],
) -> Result<ReceivedReply, UiopError> {
    let mut header = sys::hace_giop_message_header_t::zeroed();
    let mut request_id: u32 = 0;
    let mut reply_status: c_int = 0;
    let mut body_length: usize = 0;

    let status = unsafe {
        sys::hace_uiop_receive(
            &transport.raw,
            sys::HACE_GIOP_MSG_REPLY,
            &match_request_id,
            timeout_ms,
            body.as_mut_ptr(),
            body.len(),
            &mut header,
            &mut request_id,
            &mut reply_status,
            &mut body_length,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
        )
    };
    UiopError::from_status(status)?;
    Ok(ReceivedReply {
        is_exception: reply_status != 0,
        body_length,
    })
}

/// A `CdrBuffer` that borrows an existing, C-owned `hace_cdr_buffer_t`
/// rather than constructing its own -- used for a servant callback's
/// `args`/`reply` parameters, where `corba_poa.c` already built those
/// buffers and position/capacity changes must be visible back to it
/// (contrast the owning `CdrBuffer`, used for client-built requests).
pub struct CdrBufferView<'a> {
    ptr: *mut sys::RawCdrBuffer,
    _marker: PhantomData<&'a mut ()>,
}

impl<'a> CdrBufferView<'a> {
    /// # Safety
    /// `ptr` must point to a valid, already-initialized
    /// `hace_cdr_buffer_t` for the lifetime `'a` -- exactly the
    /// contract a servant callback's `args`/`reply` raw pointers
    /// already satisfy.
    pub unsafe fn from_raw(ptr: *mut sys::RawCdrBuffer) -> Self {
        CdrBufferView {
            ptr,
            _marker: PhantomData,
        }
    }

    /// Exposes the underlying raw pointer to `corba.rs` (a sibling
    /// module), which needs it to encode/decode an `Ior` straight into
    /// a servant's C-owned `args`/`reply` buffer (`observer_attach`'s
    /// `attach(in Observer obs)`).
    pub(crate) fn as_raw_mut_ptr(&mut self) -> *mut sys::RawCdrBuffer {
        self.ptr
    }

    pub fn position(&self) -> usize {
        unsafe { sys::hace_cdr_buffer_position(self.ptr) }
    }

    pub fn capacity(&self) -> usize {
        unsafe { sys::hace_cdr_buffer_capacity(self.ptr) }
    }

    pub fn decode_i32(&mut self) -> Result<i32, CdrError> {
        let mut out: i32 = 0;
        let status = unsafe { sys::hace_cdr_decode_i32(self.ptr, &mut out) };
        CdrError::from_status(status)?;
        Ok(out)
    }

    pub fn encode_i32(&mut self, value: i32) -> Result<(), CdrError> {
        CdrError::from_status(unsafe { sys::hace_cdr_encode_i32(self.ptr, value) })
    }

    /// Decodes a CDR string up to `max_length` bytes (including the
    /// NUL the wire format carries).
    pub fn decode_string(&mut self, max_length: usize) -> Result<String, CdrError> {
        let mut storage = vec![0u8; max_length];
        let mut out_length: usize = 0;
        let status = unsafe {
            sys::hace_cdr_decode_string(
                self.ptr,
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
            sys::hace_cdr_encode_string(self.ptr, c_string.as_ptr(), value.len() + 1)
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cdr::ByteOrder;
    use crate::giop;

    #[test]
    fn send_message_and_receive_request_roundtrip() {
        let receiver = UdpSocket::open(58701).expect("open receiver");
        let sender = UdpSocket::open(58702).expect("open sender");
        let receiver_transport = Transport::for_socket(&receiver);
        let sender_transport = Transport::for_socket(&sender);

        let mut storage = [0u8; 256];
        let mut request = CdrBuffer::new(&mut storage, ByteOrder::Big).unwrap();
        giop::begin_message(&mut request, ByteOrder::Big).unwrap();
        giop::encode_request_header(
            &mut request,
            &giop::RequestHeader {
                request_id: 42,
                response_expected: true,
                object_key: vec![0x01],
                operation: "ping".to_string(),
            },
        )
        .unwrap();
        request.encode_i32(99).unwrap();
        giop::finish_message(&mut request).unwrap();

        send_message(
            &sender_transport,
            &mut request,
            42,
            Ipv4Addr::new(127, 0, 0, 1),
            58701,
            HACE_UIOP_MAX_DATAGRAM_SIZE,
        )
        .unwrap();

        let mut body = [0u8; 256];
        let received = receive_request(&receiver_transport, 1000, &mut body).unwrap();
        assert_eq!(received.request_id, 42);
        assert_eq!(received.info.object_key, vec![0x01]);
        assert_eq!(received.info.operation, "ping");
        assert!(received.info.response_expected);

        // The reassembled argument bytes are a plain opaque body this
        // test owns outright, so a fresh owning CdrBuffer (not
        // CdrBufferView, which exists specifically for C-owned servant
        // callback buffers) is the right tool to decode it with.
        let mut arg_buf =
            CdrBuffer::new(&mut body[..received.body_length], ByteOrder::Big).unwrap();
        assert_eq!(arg_buf.decode_i32().unwrap(), 99);
    }

    #[test]
    fn receive_request_times_out_when_idle() {
        let socket = UdpSocket::open(58703).expect("open idle socket");
        let transport = Transport::for_socket(&socket);
        let mut body = [0u8; 64];
        let result = receive_request(&transport, 50, &mut body);
        assert_eq!(result.err(), Some(UiopError::Timeout));
    }
}
