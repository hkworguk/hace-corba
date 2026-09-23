//! Safe wrapper over `sys`'s HAL bindings: monotonic time and UDP
//! sockets, matching `runtime/src/hal/hal.h`. There is no thread/task or
//! memory-pool binding here -- those are runtime-internal plumbing, not
//! meant for application code.

use crate::sys;
use std::fmt;
use std::net::Ipv4Addr;
use std::time::Duration;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum HalError {
    Generic,
    Timeout,
    WouldBlock,
    InvalidArgument,
    ResourceExhausted,
    NotImplemented,
    Unknown(i32),
}

impl HalError {
    fn from_status(status: sys::hace_hal_status_t) -> Result<(), HalError> {
        match status {
            sys::HACE_HAL_OK => Ok(()),
            sys::HACE_HAL_ERR => Err(HalError::Generic),
            sys::HACE_HAL_ERR_TIMEOUT => Err(HalError::Timeout),
            sys::HACE_HAL_ERR_WOULD_BLOCK => Err(HalError::WouldBlock),
            sys::HACE_HAL_ERR_INVALID_ARGUMENT => Err(HalError::InvalidArgument),
            sys::HACE_HAL_ERR_RESOURCE_EXHAUSTED => Err(HalError::ResourceExhausted),
            sys::HACE_HAL_ERR_NOT_IMPLEMENTED => Err(HalError::NotImplemented),
            other => Err(HalError::Unknown(other)),
        }
    }
}

impl fmt::Display for HalError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            HalError::Generic => write!(f, "HAL error"),
            HalError::Timeout => write!(f, "HAL operation timed out"),
            HalError::WouldBlock => write!(f, "HAL operation would block"),
            HalError::InvalidArgument => write!(f, "invalid argument to HAL call"),
            HalError::ResourceExhausted => write!(f, "HAL resource exhausted"),
            HalError::NotImplemented => write!(f, "HAL function not implemented on this backend"),
            HalError::Unknown(code) => write!(f, "unknown HAL status code {code}"),
        }
    }
}

impl std::error::Error for HalError {}

/// Monotonic clock reading -- for retry/timeout scheduling, never a
/// calendar timestamp.
pub fn monotonic_now() -> Result<Duration, HalError> {
    let mut t = sys::hace_hal_time_t {
        seconds: 0,
        nanoseconds: 0,
    };
    let status = unsafe { sys::hace_hal_time_now(&mut t) };
    HalError::from_status(status)?;
    Ok(Duration::new(t.seconds, t.nanoseconds))
}

/// A UDP datagram socket, bound for the lifetime of this value (closed on
/// drop).
pub struct UdpSocket {
    handle: sys::hace_hal_socket_handle_t,
}

impl UdpSocket {
    /// Opens a UDP socket bound to `local_port` (0 = backend-chosen
    /// ephemeral port).
    pub fn open(local_port: u16) -> Result<Self, HalError> {
        let mut handle: sys::hace_hal_socket_handle_t = -1;
        let status = unsafe { sys::hace_hal_socket_open(&mut handle, local_port) };
        HalError::from_status(status)?;
        Ok(UdpSocket { handle })
    }

    pub fn send(&self, data: &[u8], dest: Ipv4Addr, dest_port: u16) -> Result<(), HalError> {
        let status = unsafe {
            sys::hace_hal_socket_send(
                self.handle,
                data.as_ptr(),
                data.len(),
                u32::from(dest),
                dest_port,
            )
        };
        HalError::from_status(status)
    }

    /// Blocks up to `timeout_ms` for a datagram (0 = poll, returns
    /// `HalError::WouldBlock` if nothing is pending). Returns the number
    /// of bytes received and the sender's address/port. A datagram
    /// larger than `buffer` is rejected (`HalError::InvalidArgument`),
    /// never silently truncated.
    pub fn recv(
        &self,
        buffer: &mut [u8],
        timeout_ms: u32,
    ) -> Result<(usize, Ipv4Addr, u16), HalError> {
        let mut out_length: usize = 0;
        let mut src_addr: u32 = 0;
        let mut src_port: u16 = 0;
        let status = unsafe {
            sys::hace_hal_socket_recv(
                self.handle,
                buffer.as_mut_ptr(),
                buffer.len(),
                &mut out_length,
                &mut src_addr,
                &mut src_port,
                timeout_ms,
            )
        };
        HalError::from_status(status)?;
        Ok((out_length, Ipv4Addr::from(src_addr), src_port))
    }
}

impl Drop for UdpSocket {
    fn drop(&mut self) {
        // Best-effort: nothing meaningful to do with a close failure
        // during drop, and dropping must not panic.
        unsafe {
            let _ = sys::hace_hal_socket_close(self.handle);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::Ipv4Addr;

    #[test]
    fn monotonic_now_succeeds_and_moves_forward() {
        let first = monotonic_now().unwrap();
        let second = monotonic_now().unwrap();
        assert!(second >= first);
    }

    #[test]
    fn udp_socket_loopback_roundtrip() {
        // Fixed high ports rather than ephemeral (local_port = 0), since
        // the HAL doesn't expose a "what port did I get" query.
        let receiver = UdpSocket::open(58231).expect("open receiver socket");
        let sender = UdpSocket::open(58232).expect("open sender socket");

        let message = b"hace-corba rust binding";
        sender
            .send(message, Ipv4Addr::new(127, 0, 0, 1), 58231)
            .expect("send");

        let mut buf = [0u8; 64];
        let (len, from_addr, _from_port) = receiver.recv(&mut buf, 1000).expect("recv");
        assert_eq!(&buf[..len], message);
        assert_eq!(from_addr, Ipv4Addr::new(127, 0, 0, 1));
    }

    #[test]
    fn udp_socket_recv_would_block_when_idle() {
        let socket = UdpSocket::open(58233).expect("open idle socket");
        let mut buf = [0u8; 16];
        assert_eq!(socket.recv(&mut buf, 0), Err(HalError::WouldBlock));
    }
}
