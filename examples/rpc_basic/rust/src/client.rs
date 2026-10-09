//! rpc_basic client: calls Calculator.add(a, b) via a blocking twoway
//! `uiop::call` and prints the result. Takes the server's IOR string
//! as an optional `argv[1]` (as printed by server.rs); defaults to the
//! localhost/RPC_BASIC_PORT pair if not given. See ../../client.idl
//! for what this client calls.

use hace_corba::giop::{self, RequestHeader};
use hace_corba::uiop::{call, Transport, HACE_UIOP_MAX_DATAGRAM_SIZE};
use hace_corba::{corba, ByteOrder, CdrBuffer};
use hace_corba_examples_common::{open_client_socket, parse_ior_arg};
use std::error::Error;
use std::process::ExitCode;

const RPC_BASIC_PORT: u16 = 59101;
const CALC_OBJECT_KEY: [u8; 4] = [b'c', b'a', b'l', b'c'];

fn build_add_request(
    storage: &mut [u8],
    request_id: u32,
    a: i32,
    b: i32,
) -> Result<CdrBuffer<'_>, Box<dyn Error>> {
    let mut buf = CdrBuffer::new(storage, ByteOrder::Big)?;
    giop::begin_message(&mut buf, ByteOrder::Big)?;
    giop::encode_request_header(
        &mut buf,
        &RequestHeader {
            request_id,
            response_expected: true,
            object_key: CALC_OBJECT_KEY.to_vec(),
            operation: "add".to_string(),
        },
    )?;
    buf.encode_i32(a)?;
    buf.encode_i32(b)?;
    giop::finish_message(&mut buf)?;
    Ok(buf)
}

fn main() -> ExitCode {
    let server_ior = parse_ior_arg(RPC_BASIC_PORT, &CALC_OBJECT_KEY);
    let Some(socket) = open_client_socket("rpc_basic client") else {
        return ExitCode::FAILURE;
    };
    let transport = Transport::for_socket(&socket);

    let a = 7;
    let b = 35;

    let mut storage = [0u8; 256];
    let mut request = match build_add_request(&mut storage, 1, a, b) {
        Ok(buf) => buf,
        Err(e) => {
            eprintln!("rpc_basic client: could not encode request: {e}");
            return ExitCode::FAILURE;
        }
    };

    let mut reply_body = [0u8; 256];
    let result = call(
        &transport,
        &mut request,
        1,
        server_ior.host_ipv4,
        server_ior.port,
        HACE_UIOP_MAX_DATAGRAM_SIZE,
        1000,
        3,
        &mut reply_body,
    );
    let result = match result {
        Ok(r) => r,
        Err(e) => {
            eprintln!("rpc_basic client: call failed: {e}");
            return ExitCode::FAILURE;
        }
    };

    if result.is_exception {
        match CdrBuffer::new(&mut reply_body[..result.body_length], ByteOrder::Big)
            .map_err(Box::<dyn Error>::from)
            .and_then(|mut buf| corba::decode_system_exception(&mut buf).map_err(Box::from))
        {
            Ok(exc) => eprintln!("rpc_basic client: server raised exception {}", exc.exception),
            Err(e) => eprintln!("rpc_basic client: could not decode exception: {e}"),
        }
        return ExitCode::FAILURE;
    }

    let decoded = CdrBuffer::new(&mut reply_body[..result.body_length], ByteOrder::Big)
        .map_err(Box::<dyn Error>::from)
        .and_then(|mut buf| buf.decode_i32().map_err(Box::from));
    match decoded {
        Ok(sum) => println!("client: {a} + {b} = {sum}"),
        Err(e) => eprintln!("rpc_basic client: could not decode result: {e}"),
    }

    ExitCode::SUCCESS
}
