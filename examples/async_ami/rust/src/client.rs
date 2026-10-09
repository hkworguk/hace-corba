//! async_ami client: sends a twoway Worker.compute(input) request via
//! `uiop::send_message` directly (not the blocking `uiop::call`), then
//! polls for the reply with `uiop::receive_reply` on a short timeout
//! in a loop, doing other visible work between polls -- the Poller-
//! style asynchronous invocation pattern, well suited to UDP's
//! non-blocking send (see docs/roadmap.md Phase 5). See
//! ../../client.idl for what this client calls.

use hace_corba::giop::{self, RequestHeader};
use hace_corba::uiop::{receive_reply, send_message, Transport, UiopError, HACE_UIOP_MAX_DATAGRAM_SIZE};
use hace_corba::{corba, ByteOrder, CdrBuffer};
use hace_corba_examples_common::{open_client_socket, parse_ior_arg};
use std::error::Error;
use std::process::ExitCode;

const ASYNC_AMI_PORT: u16 = 59121;
const POLL_INTERVAL_MS: u32 = 50;
const MAX_POLLS: u32 = 40; // 40 * 50ms = 2s, comfortably more than the server's ~300ms
const WORK_OBJECT_KEY: [u8; 4] = [b'w', b'o', b'r', b'k'];

fn build_compute_request<'a>(
    storage: &'a mut [u8],
    request_id: u32,
    input: i32,
    object_key: &'a [u8],
) -> Result<CdrBuffer<'a>, Box<dyn Error>> {
    let mut buf = CdrBuffer::new(storage, ByteOrder::Big)?;
    giop::begin_message(&mut buf, ByteOrder::Big)?;
    giop::encode_request_header(
        &mut buf,
        &RequestHeader {
            request_id,
            response_expected: true,
            object_key: object_key.to_vec(),
            operation: "compute".to_string(),
        },
    )?;
    buf.encode_i32(input)?;
    giop::finish_message(&mut buf)?;
    Ok(buf)
}

fn main() -> ExitCode {
    let server_ior = parse_ior_arg(ASYNC_AMI_PORT, &WORK_OBJECT_KEY);
    let Some(socket) = open_client_socket("async_ami client") else {
        return ExitCode::FAILURE;
    };
    let transport = Transport::for_socket(&socket);

    let request_id = 1;
    let input = 6;

    let mut storage = [0u8; 256];
    let mut request = match build_compute_request(&mut storage, request_id, input, &server_ior.object_key) {
        Ok(buf) => buf,
        Err(e) => {
            eprintln!("async_ami client: could not encode request: {e}");
            return ExitCode::FAILURE;
        }
    };

    if let Err(e) = send_message(
        &transport,
        &mut request,
        request_id,
        server_ior.host_ipv4,
        server_ior.port,
        HACE_UIOP_MAX_DATAGRAM_SIZE,
    ) {
        eprintln!("async_ami client: could not send request: {e}");
        return ExitCode::FAILURE;
    }
    println!("client: sent compute({input}), doing other work while waiting...");

    let mut reply_body = [0u8; 256];
    let mut result = None;
    for attempt in 0..MAX_POLLS {
        match receive_reply(&transport, request_id, POLL_INTERVAL_MS, &mut reply_body) {
            Ok(r) => {
                result = Some(r);
                break;
            }
            Err(UiopError::Timeout) => {
                println!("client: ...still working on other things (poll {})...", attempt + 1);
            }
            Err(e) => {
                eprintln!("async_ami client: poll error: {e}");
                return ExitCode::FAILURE;
            }
        }
    }

    let Some(result) = result else {
        eprintln!("async_ami client: never got a reply");
        return ExitCode::FAILURE;
    };

    if result.is_exception {
        match CdrBuffer::new(&mut reply_body[..result.body_length], ByteOrder::Big)
            .map_err(Box::<dyn Error>::from)
            .and_then(|mut buf| corba::decode_system_exception(&mut buf).map_err(Box::from))
        {
            Ok(exc) => eprintln!("async_ami client: server raised exception {}", exc.exception),
            Err(e) => eprintln!("async_ami client: could not decode exception: {e}"),
        }
        return ExitCode::FAILURE;
    }

    let decoded = CdrBuffer::new(&mut reply_body[..result.body_length], ByteOrder::Big)
        .map_err(Box::<dyn Error>::from)
        .and_then(|mut buf| buf.decode_i32().map_err(Box::from));
    match decoded {
        Ok(output) => println!("client: reply arrived -- compute({input}) = {output}"),
        Err(e) => eprintln!("async_ami client: could not decode result: {e}"),
    }

    ExitCode::SUCCESS
}
