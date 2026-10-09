//! oneway client: sends three fire-and-forget Logger.log(message)
//! calls via `uiop::send_message` directly -- no `uiop::call`, no
//! wait, no reply. Demonstrates genuine oneway semantics (a real
//! deviation from IIOP's reliable-transport-backed oneway -- see
//! docs/corba-spec-gap-analysis.md Sec 7). See ../../client.idl for
//! what this client calls.

use hace_corba::giop::{self, RequestHeader};
use hace_corba::uiop::{send_message, Transport, HACE_UIOP_MAX_DATAGRAM_SIZE};
use hace_corba::{ByteOrder, CdrBuffer};
use hace_corba_examples_common::{open_client_socket, parse_ior_arg};
use std::error::Error;
use std::process::ExitCode;

const ONEWAY_PORT: u16 = 59111;
const LOG_OBJECT_KEY: [u8; 3] = [b'l', b'o', b'g'];

fn build_log_request<'a>(
    storage: &'a mut [u8],
    request_id: u32,
    message: &str,
    object_key: &[u8],
) -> Result<CdrBuffer<'a>, Box<dyn Error>> {
    let mut buf = CdrBuffer::new(storage, ByteOrder::Big)?;
    giop::begin_message(&mut buf, ByteOrder::Big)?;
    giop::encode_request_header(
        &mut buf,
        &RequestHeader {
            request_id,
            response_expected: false,
            object_key: object_key.to_vec(),
            operation: "log".to_string(),
        },
    )?;
    buf.encode_string(message)?;
    giop::finish_message(&mut buf)?;
    Ok(buf)
}

fn main() -> ExitCode {
    let messages = ["starting up", "processing batch 1", "all done"];
    let server_ior = parse_ior_arg(ONEWAY_PORT, &LOG_OBJECT_KEY);
    let Some(socket) = open_client_socket("oneway client") else {
        return ExitCode::FAILURE;
    };
    let transport = Transport::for_socket(&socket);

    for (i, message) in messages.iter().enumerate() {
        let request_id = (i + 1) as u32;
        let mut storage = [0u8; 256];
        let mut request =
            match build_log_request(&mut storage, request_id, message, &server_ior.object_key) {
                Ok(buf) => buf,
                Err(e) => {
                    eprintln!("oneway client: could not encode message {i}: {e}");
                    continue;
                }
            };

        match send_message(
            &transport,
            &mut request,
            request_id,
            server_ior.host_ipv4,
            server_ior.port,
            HACE_UIOP_MAX_DATAGRAM_SIZE,
        ) {
            Ok(()) => println!("client: sent log message {}: \"{message}\"", i + 1),
            Err(e) => eprintln!("client: failed to send log message {i}: {e}"),
        }
    }

    ExitCode::SUCCESS
}
