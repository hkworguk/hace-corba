//! Shared helpers for the examples/rust/* client/server pairs: parsing
//! an optional IOR string argument (falling back to a sensible
//! localhost default), and announcing a server's own IOR on startup.
//! Mirrors `examples/c/common/example_common.h`. Not part of the
//! runtime or its DAL C boundary -- this is example/demo code, so
//! unlike the certified library it uses ordinary `println!`/`eprintln!`
//! freely.

use hace_corba::{Ior, UdpSocket};
use std::net::Ipv4Addr;

/// Every example defaults to localhost; pass a real IOR string (as
/// printed by a server) as `argv[1]` to run client/server on separate
/// hosts instead.
pub const LOOPBACK: Ipv4Addr = Ipv4Addr::new(127, 0, 0, 1);

pub fn default_ior(port: u16, object_key: &[u8]) -> Ior {
    Ior::new(LOOPBACK, port, object_key)
}

pub fn announce_server(role: &str, ior: &Ior) {
    match ior.to_uiop_string() {
        Ok(s) => println!("{role} listening as {s}"),
        Err(e) => eprintln!("{role}: could not stringify its own IOR: {e}"),
    }
}

/// Opens a UDP socket on `port`, builds the corresponding default IOR,
/// and announces it. Returns `None` on failure (with a message already
/// printed to stderr).
pub fn server_start(role: &str, port: u16, object_key: &[u8]) -> Option<(UdpSocket, Ior)> {
    match UdpSocket::open(port) {
        Ok(socket) => {
            let ior = default_ior(port, object_key);
            announce_server(role, &ior);
            Some((socket, ior))
        }
        Err(e) => {
            eprintln!("{role}: could not open UDP socket on port {port}: {e}");
            None
        }
    }
}

/// Parses `argv[1]` as an IOR string (as printed by `announce_server`)
/// if present, else falls back to the localhost default for
/// `default_port`/`default_object_key`.
pub fn parse_ior_arg(default_port: u16, default_object_key: &[u8]) -> Ior {
    let args: Vec<String> = std::env::args().collect();
    if let Some(arg) = args.get(1) {
        match Ior::from_uiop_string(arg) {
            Ok(ior) => return ior,
            Err(e) => eprintln!(
                "warning: could not parse IOR argument '{arg}' ({e}), using the \
                 localhost default instead"
            ),
        }
    }
    default_ior(default_port, default_object_key)
}

/// Convenience: open a client-side UDP socket on an ephemeral port,
/// printing a message and returning `None` on failure.
pub fn open_client_socket(role: &str) -> Option<UdpSocket> {
    match UdpSocket::open(0) {
        Ok(socket) => Some(socket),
        Err(e) => {
            eprintln!("{role}: could not open UDP socket: {e}");
            None
        }
    }
}

