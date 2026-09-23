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

use std::os::raw::c_int;

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
