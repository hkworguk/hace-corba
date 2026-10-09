//! cdr_udp_echo (Rust)
//!
//! Encodes a small record with CDR, sends it over a raw UDP HAL socket to
//! itself, receives it back, and decodes it.
//!
//! This is **not** an RPC example: hace-corba has no ORB/POA/GIOP layer
//! yet (see docs/roadmap.md), so there is no object invocation to
//! demonstrate. What's shown here -- CDR encode/decode and the HAL's UDP
//! sockets -- is what the Rust bindings actually cover today (see
//! bindings/rust/hace-corba).

use hace_corba::{ByteOrder, CdrBuffer, UdpSocket};
use std::net::Ipv4Addr;

const LISTEN_PORT: u16 = 58300;

/// A toy "sensor reading" record, encoded by hand field-by-field since
/// the runtime only supports primitive CDR types so far (no generated
/// struct marshaling until the idlc backends land -- see
/// docs/roadmap.md, Phase 4).
struct SensorReading {
    id: u32,
    value: f64,
    valid: bool,
}

fn encode_reading(storage: &mut [u8], reading: &SensorReading) -> usize {
    let mut buf = CdrBuffer::new(storage, ByteOrder::Big).expect("init encode buffer");
    buf.encode_u32(reading.id).expect("encode id");
    buf.encode_f64(reading.value).expect("encode value");
    buf.encode_bool(reading.valid).expect("encode valid");
    buf.position()
}

fn decode_reading(storage: &mut [u8]) -> SensorReading {
    let mut buf = CdrBuffer::new(storage, ByteOrder::Big).expect("init decode buffer");
    let id = buf.decode_u32().expect("decode id");
    let value = buf.decode_f64().expect("decode value");
    let valid = buf.decode_bool().expect("decode valid");
    SensorReading { id, value, valid }
}

fn main() {
    let reading = SensorReading {
        id: 7,
        value: 98.6,
        valid: true,
    };

    let mut wire = [0u8; 32];
    let encoded_len = encode_reading(&mut wire, &reading);
    println!("encoded {encoded_len} bytes: {:?}", &wire[..encoded_len]);

    let socket = UdpSocket::open(LISTEN_PORT).expect("open UDP socket");
    socket
        .send(&wire[..encoded_len], Ipv4Addr::new(127, 0, 0, 1), LISTEN_PORT)
        .expect("send to self over loopback");

    let mut received = [0u8; 32];
    let (len, from, _port) = socket.recv(&mut received, 1000).expect("recv");
    println!("received {len} bytes from {from}");

    let decoded = decode_reading(&mut received[..len]);
    println!(
        "decoded: id={} value={} valid={}",
        decoded.id, decoded.value, decoded.valid
    );

    assert_eq!(decoded.id, reading.id);
    assert!((decoded.value - reading.value).abs() < f64::EPSILON);
    assert_eq!(decoded.valid, reading.valid);
    println!("round-trip OK");
}
