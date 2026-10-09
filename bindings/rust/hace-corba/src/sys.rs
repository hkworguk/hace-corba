//! Raw FFI declarations matching `runtime/include/hace/cdr.h` and
//! `runtime/src/hal/hal.h`. Unsafe, low-level, and not meant to be used
//! directly by application code — see the safe wrappers in `cdr` and
//! `hal`.
//!
//! `hace_cdr_buffer_t` is treated as opaque: rather than mirroring its C
//! field layout (a raw pointer + two size_t + an enum, which risks
//! silently drifting out of sync with the C struct as it evolves),
//! `RawCdrBuffer` is just a fixed block of bytes sized generously larger
//! than the real struct, and `hace_cdr_buffer_sizeof()` is used at
//! runtime to assert it's actually big enough. Accessors
//! (`hace_cdr_buffer_position` etc.) expose the fields Rust code needs
//! without Rust ever interpreting the struct's memory directly.

#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int, c_void};

pub type hace_cdr_status_t = c_int;
pub const HACE_CDR_OK: hace_cdr_status_t = 0;
pub const HACE_CDR_ERR_BUFFER_OVERFLOW: hace_cdr_status_t = 1;
pub const HACE_CDR_ERR_BUFFER_UNDERFLOW: hace_cdr_status_t = 2;
pub const HACE_CDR_ERR_INVALID_ARGUMENT: hace_cdr_status_t = 3;

pub type hace_cdr_byte_order_t = c_int;
pub const HACE_CDR_BIG_ENDIAN: hace_cdr_byte_order_t = 0;
pub const HACE_CDR_LITTLE_ENDIAN: hace_cdr_byte_order_t = 1;

/// Opaque storage for `hace_cdr_buffer_t`. 64 bytes is comfortably larger
/// than the real struct (32 bytes on typical LP64 ABIs: one pointer, two
/// `size_t`, one 4-byte enum plus padding); `CdrBuffer::new` asserts this
/// against `hace_cdr_buffer_sizeof()` at construction time.
#[repr(C, align(8))]
pub struct RawCdrBuffer(pub [u8; 64]);

impl RawCdrBuffer {
    pub fn zeroed() -> Self {
        RawCdrBuffer([0u8; 64])
    }
}

pub type hace_hal_status_t = c_int;
pub const HACE_HAL_OK: hace_hal_status_t = 0;
pub const HACE_HAL_ERR: hace_hal_status_t = 1;
pub const HACE_HAL_ERR_TIMEOUT: hace_hal_status_t = 2;
pub const HACE_HAL_ERR_WOULD_BLOCK: hace_hal_status_t = 3;
pub const HACE_HAL_ERR_INVALID_ARGUMENT: hace_hal_status_t = 4;
#[allow(dead_code)]
pub const HACE_HAL_ERR_NOT_IMPLEMENTED: hace_hal_status_t = 5;
pub const HACE_HAL_ERR_RESOURCE_EXHAUSTED: hace_hal_status_t = 6;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct hace_hal_time_t {
    pub seconds: u64,
    pub nanoseconds: u32,
}

pub type hace_hal_socket_handle_t = i32;

extern "C" {
    // ---- CDR (runtime/include/hace/cdr.h) ----

    pub fn hace_cdr_buffer_sizeof() -> usize;

    pub fn hace_cdr_buffer_init(
        buf: *mut RawCdrBuffer,
        storage: *mut u8,
        capacity: usize,
        byte_order: hace_cdr_byte_order_t,
    ) -> hace_cdr_status_t;

    pub fn hace_cdr_buffer_position(buf: *const RawCdrBuffer) -> usize;
    pub fn hace_cdr_buffer_capacity(buf: *const RawCdrBuffer) -> usize;
    pub fn hace_cdr_buffer_set_position(
        buf: *mut RawCdrBuffer,
        position: usize,
    ) -> hace_cdr_status_t;

    pub fn hace_cdr_encode_octet(buf: *mut RawCdrBuffer, value: u8) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_bool(buf: *mut RawCdrBuffer, value: c_int) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_u16(buf: *mut RawCdrBuffer, value: u16) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_i16(buf: *mut RawCdrBuffer, value: i16) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_u32(buf: *mut RawCdrBuffer, value: u32) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_i32(buf: *mut RawCdrBuffer, value: i32) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_u64(buf: *mut RawCdrBuffer, value: u64) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_i64(buf: *mut RawCdrBuffer, value: i64) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_f32(buf: *mut RawCdrBuffer, value: f32) -> hace_cdr_status_t;
    pub fn hace_cdr_encode_f64(buf: *mut RawCdrBuffer, value: f64) -> hace_cdr_status_t;

    pub fn hace_cdr_decode_octet(buf: *mut RawCdrBuffer, out: *mut u8) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_bool(buf: *mut RawCdrBuffer, out: *mut c_int) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_u16(buf: *mut RawCdrBuffer, out: *mut u16) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_i16(buf: *mut RawCdrBuffer, out: *mut i16) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_u32(buf: *mut RawCdrBuffer, out: *mut u32) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_i32(buf: *mut RawCdrBuffer, out: *mut i32) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_u64(buf: *mut RawCdrBuffer, out: *mut u64) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_i64(buf: *mut RawCdrBuffer, out: *mut i64) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_f32(buf: *mut RawCdrBuffer, out: *mut f32) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_f64(buf: *mut RawCdrBuffer, out: *mut f64) -> hace_cdr_status_t;

    pub fn hace_cdr_encode_string(
        buf: *mut RawCdrBuffer,
        value: *const c_char,
        max_length: usize,
    ) -> hace_cdr_status_t;
    pub fn hace_cdr_decode_string(
        buf: *mut RawCdrBuffer,
        out: *mut c_char,
        out_capacity: usize,
        out_length: *mut usize,
    ) -> hace_cdr_status_t;

    // ---- HAL (runtime/src/hal/hal.h) ----

    pub fn hace_hal_time_now(out_time: *mut hace_hal_time_t) -> hace_hal_status_t;

    pub fn hace_hal_socket_open(
        out_handle: *mut hace_hal_socket_handle_t,
        local_port: u16,
    ) -> hace_hal_status_t;
    pub fn hace_hal_socket_close(handle: hace_hal_socket_handle_t) -> hace_hal_status_t;
    pub fn hace_hal_socket_send(
        handle: hace_hal_socket_handle_t,
        data: *const u8,
        length: usize,
        dest_addr_ipv4: u32,
        dest_port: u16,
    ) -> hace_hal_status_t;
    pub fn hace_hal_socket_recv(
        handle: hace_hal_socket_handle_t,
        buffer: *mut u8,
        capacity: usize,
        out_length: *mut usize,
        src_addr_ipv4: *mut u32,
        src_port: *mut u16,
        timeout_ms: u32,
    ) -> hace_hal_status_t;
}

// ---- GIOP (runtime/include/hace/giop.h) ----
//
// Only what the examples/{rust,c} call *directly* is bound here --
// decoding a Reply/Request header happens inside hace_uiop_call/
// hace_uiop_receive already, so this crate only needs the three
// functions that build an outgoing request, plus the plain-data
// structs those and hace_uiop_receive hand values through. Every
// struct below is a direct #[repr(C)] mirror, not an opaque blob like
// RawCdrBuffer/RawPoa: each was checked by hand for C struct-layout
// risk (field order, natural alignment, platform size-rounding) and
// has none -- no pointers, no function pointers, no hidden padding
// surprises. See docs/architecture/adr-0005-rust-ada-orb-bindings.md.

pub const HACE_GIOP_MAX_OBJECT_KEY: usize = 32;
pub const HACE_GIOP_MAX_OPERATION_NAME: usize = 64;

pub type hace_giop_status_t = c_int;
pub const HACE_GIOP_OK: hace_giop_status_t = 0;

pub type hace_giop_message_type_t = c_int;
pub const HACE_GIOP_MSG_REQUEST: hace_giop_message_type_t = 0;
pub const HACE_GIOP_MSG_REPLY: hace_giop_message_type_t = 1;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct hace_giop_message_header_t {
    pub version_major: u8,
    pub version_minor: u8,
    pub byte_order: hace_cdr_byte_order_t,
    pub more_fragments: c_int,
    pub message_type: hace_giop_message_type_t,
    pub message_size: u32,
}

impl hace_giop_message_header_t {
    pub fn zeroed() -> Self {
        hace_giop_message_header_t {
            version_major: 0,
            version_minor: 0,
            byte_order: HACE_CDR_BIG_ENDIAN,
            more_fragments: 0,
            message_type: HACE_GIOP_MSG_REQUEST,
            message_size: 0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct hace_giop_request_header_t {
    pub request_id: u32,
    pub response_expected: c_int,
    pub object_key: [u8; HACE_GIOP_MAX_OBJECT_KEY],
    pub object_key_length: usize,
    pub operation: [u8; HACE_GIOP_MAX_OPERATION_NAME],
}

impl hace_giop_request_header_t {
    pub fn zeroed() -> Self {
        hace_giop_request_header_t {
            request_id: 0,
            response_expected: 0,
            object_key: [0u8; HACE_GIOP_MAX_OBJECT_KEY],
            object_key_length: 0,
            operation: [0u8; HACE_GIOP_MAX_OPERATION_NAME],
        }
    }
}

extern "C" {
    pub fn hace_giop_begin_message(
        buf: *mut RawCdrBuffer,
        byte_order: hace_cdr_byte_order_t,
    ) -> hace_giop_status_t;
    pub fn hace_giop_finish_message(
        buf: *mut RawCdrBuffer,
        message_type: hace_giop_message_type_t,
        more_fragments: c_int,
    ) -> hace_giop_status_t;
    pub fn hace_giop_encode_request_header(
        buf: *mut RawCdrBuffer,
        header: *const hace_giop_request_header_t,
    ) -> hace_giop_status_t;
}

// ---- UIOP (runtime/include/hace/uiop.h) ----

pub const HACE_UIOP_MAX_DATAGRAM_SIZE: usize = 1472;

pub type hace_uiop_status_t = c_int;
pub const HACE_UIOP_OK: hace_uiop_status_t = 0;
pub const HACE_UIOP_ERR_INVALID_ARGUMENT: hace_uiop_status_t = 1;
pub const HACE_UIOP_ERR_HAL: hace_uiop_status_t = 2;
pub const HACE_UIOP_ERR_TOO_LARGE: hace_uiop_status_t = 3;
pub const HACE_UIOP_ERR_TIMEOUT: hace_uiop_status_t = 4;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct hace_uiop_request_info_t {
    pub object_key: [u8; HACE_GIOP_MAX_OBJECT_KEY],
    pub object_key_length: usize,
    pub operation: [u8; HACE_GIOP_MAX_OPERATION_NAME],
    pub response_expected: c_int,
}

impl hace_uiop_request_info_t {
    pub fn zeroed() -> Self {
        hace_uiop_request_info_t {
            object_key: [0u8; HACE_GIOP_MAX_OBJECT_KEY],
            object_key_length: 0,
            operation: [0u8; HACE_GIOP_MAX_OPERATION_NAME],
            response_expected: 0,
        }
    }
}

/// hace_uiop_transport_t: three pointer-sized fields (two function
/// pointers, one `void *`), all naturally 8-aligned -- unlike
/// hace_corba_poa_t's array-of-mixed-size-entries, this has no
/// internal padding ambiguity, so it's mirrored directly rather than
/// treated as opaque.
pub type hace_uiop_send_fn_t = unsafe extern "C" fn(
    context: *mut c_void,
    data: *const u8,
    length: usize,
    dest_addr_ipv4: u32,
    dest_port: u16,
) -> hace_hal_status_t;
pub type hace_uiop_recv_fn_t = unsafe extern "C" fn(
    context: *mut c_void,
    buffer: *mut u8,
    capacity: usize,
    out_length: *mut usize,
    src_addr_ipv4: *mut u32,
    src_port: *mut u16,
    timeout_ms: u32,
) -> hace_hal_status_t;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct hace_uiop_transport_t {
    pub send: hace_uiop_send_fn_t,
    pub recv: hace_uiop_recv_fn_t,
    pub context: *mut c_void,
}

extern "C" {
    pub fn hace_uiop_transport_for_socket(
        handle: *const hace_hal_socket_handle_t,
    ) -> hace_uiop_transport_t;

    pub fn hace_uiop_send_message(
        transport: *const hace_uiop_transport_t,
        message: *mut RawCdrBuffer,
        message_type: hace_giop_message_type_t,
        request_id: u32,
        dest_addr_ipv4: u32,
        dest_port: u16,
        max_datagram_size: usize,
    ) -> hace_uiop_status_t;

    #[allow(clippy::too_many_arguments)]
    pub fn hace_uiop_receive(
        transport: *const hace_uiop_transport_t,
        expected_type: hace_giop_message_type_t,
        match_request_id: *const u32,
        timeout_ms: u32,
        body_storage: *mut u8,
        body_capacity: usize,
        out_header: *mut hace_giop_message_header_t,
        out_request_id: *mut u32,
        out_reply_status: *mut c_int,
        out_body_length: *mut usize,
        out_src_addr_ipv4: *mut u32,
        out_src_port: *mut u16,
        out_request_info: *mut hace_uiop_request_info_t,
    ) -> hace_uiop_status_t;

    #[allow(clippy::too_many_arguments)]
    pub fn hace_uiop_call(
        transport: *const hace_uiop_transport_t,
        request: *mut RawCdrBuffer,
        request_id: u32,
        dest_addr_ipv4: u32,
        dest_port: u16,
        max_datagram_size: usize,
        per_attempt_timeout_ms: u32,
        max_attempts: u32,
        reply_body_storage: *mut u8,
        reply_body_capacity: usize,
        out_reply_status: *mut c_int,
        out_reply_body_length: *mut usize,
    ) -> hace_uiop_status_t;
}

// ---- CORBA core (runtime/include/hace/corba_exception.h, corba_ior.h, corba_poa.h) ----

pub type hace_corba_status_t = c_int;
pub const HACE_CORBA_OK: hace_corba_status_t = 0;
pub const HACE_CORBA_ERR_INVALID_ARGUMENT: hace_corba_status_t = 1;
pub const HACE_CORBA_ERR_BUFFER_OVERFLOW: hace_corba_status_t = 2;
pub const HACE_CORBA_ERR_BUFFER_UNDERFLOW: hace_corba_status_t = 3;

pub type hace_corba_exception_t = c_int;
pub const HACE_CORBA_EXC_NONE: hace_corba_exception_t = 0;
#[allow(dead_code)]
pub const HACE_CORBA_EXC_UNKNOWN: hace_corba_exception_t = 1;
pub const HACE_CORBA_EXC_BAD_PARAM: hace_corba_exception_t = 2;
pub const HACE_CORBA_EXC_NO_MEMORY: hace_corba_exception_t = 3;
pub const HACE_CORBA_EXC_OBJECT_NOT_EXIST: hace_corba_exception_t = 4;
pub const HACE_CORBA_EXC_BAD_OPERATION: hace_corba_exception_t = 5;
#[allow(dead_code)]
pub const HACE_CORBA_EXC_TRANSIENT: hace_corba_exception_t = 6;
#[allow(dead_code)]
pub const HACE_CORBA_EXC_TIMEOUT: hace_corba_exception_t = 7;
pub const HACE_CORBA_EXC_MARSHAL: hace_corba_exception_t = 8;
#[allow(dead_code)]
pub const HACE_CORBA_EXC_COMM_FAILURE: hace_corba_exception_t = 9;

pub type hace_corba_completion_status_t = c_int;
#[allow(dead_code)]
pub const HACE_CORBA_COMPLETED_YES: hace_corba_completion_status_t = 0;
pub const HACE_CORBA_COMPLETED_NO: hace_corba_completion_status_t = 1;
#[allow(dead_code)]
pub const HACE_CORBA_COMPLETED_MAYBE: hace_corba_completion_status_t = 2;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct hace_corba_system_exception_t {
    pub exception: hace_corba_exception_t,
    pub minor_code: u32,
    pub completion_status: hace_corba_completion_status_t,
}

impl hace_corba_system_exception_t {
    pub fn none() -> Self {
        hace_corba_system_exception_t {
            exception: HACE_CORBA_EXC_NONE,
            minor_code: 0,
            completion_status: HACE_CORBA_COMPLETED_YES,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct hace_corba_ior_t {
    pub host_ipv4: u32,
    pub port: u16,
    pub object_key: [u8; HACE_GIOP_MAX_OBJECT_KEY],
    pub object_key_length: usize,
}

impl hace_corba_ior_t {
    pub fn zeroed() -> Self {
        hace_corba_ior_t {
            host_ipv4: 0,
            port: 0,
            object_key: [0u8; HACE_GIOP_MAX_OBJECT_KEY],
            object_key_length: 0,
        }
    }
}

pub const HACE_CORBA_MAX_IOR_STRING: usize = 96;

/// Opaque storage for `hace_corba_poa_t`: it holds an array of entries
/// each containing a function pointer, a `void *`, and mixed-size
/// fields -- reproducible in principle, but fragile to mirror by hand
/// (exact padding between a `uint8_t[32]` and a following `size_t`,
/// etc.) and not worth the risk for a handle type nothing but
/// init/activate_object/handle_one_request ever touches. 768 bytes is
/// comfortably larger than the real struct (520 bytes on typical LP64
/// ABIs); `Poa::new` asserts this against `hace_corba_poa_sizeof()` at
/// construction time, the same pattern as `RawCdrBuffer`.
#[repr(C, align(8))]
pub struct RawPoa(pub [u8; 768]);

impl RawPoa {
    pub fn zeroed() -> Self {
        RawPoa([0u8; 768])
    }
}

pub type hace_corba_servant_fn_t = unsafe extern "C" fn(
    context: *mut c_void,
    operation: *const c_char,
    args: *mut RawCdrBuffer,
    reply: *mut RawCdrBuffer,
    out_exception: *mut hace_corba_system_exception_t,
);

extern "C" {
    /// Not called by this crate (no example encodes an exception itself
    /// -- a servant just sets the fields of the `out_exception` struct
    /// the dispatcher hands it, and `corba_poa.c` encodes it on the
    /// wire), declared for symmetry with `decode_system_exception`.
    #[allow(dead_code)]
    pub fn hace_corba_encode_system_exception(
        buf: *mut RawCdrBuffer,
        exc: *const hace_corba_system_exception_t,
    ) -> hace_corba_status_t;
    pub fn hace_corba_decode_system_exception(
        buf: *mut RawCdrBuffer,
        out_exc: *mut hace_corba_system_exception_t,
    ) -> hace_corba_status_t;

    pub fn hace_corba_ior_encode(
        buf: *mut RawCdrBuffer,
        ior: *const hace_corba_ior_t,
    ) -> hace_corba_status_t;
    pub fn hace_corba_ior_decode(
        buf: *mut RawCdrBuffer,
        out_ior: *mut hace_corba_ior_t,
    ) -> hace_corba_status_t;
    pub fn hace_corba_object_to_string(
        ior: *const hace_corba_ior_t,
        out_str: *mut c_char,
        out_capacity: usize,
    ) -> hace_corba_status_t;
    pub fn hace_corba_string_to_object(
        str_: *const c_char,
        out_ior: *mut hace_corba_ior_t,
    ) -> hace_corba_status_t;

    pub fn hace_corba_poa_sizeof() -> usize;
    pub fn hace_corba_poa_init(poa: *mut RawPoa) -> hace_corba_status_t;
    pub fn hace_corba_poa_activate_object(
        poa: *mut RawPoa,
        object_key: *const u8,
        object_key_length: usize,
        servant: hace_corba_servant_fn_t,
        context: *mut c_void,
    ) -> hace_corba_status_t;
    pub fn hace_corba_poa_handle_one_request(
        poa: *const RawPoa,
        transport: *const hace_uiop_transport_t,
        timeout_ms: u32,
        max_datagram_size: usize,
    ) -> hace_uiop_status_t;
}
