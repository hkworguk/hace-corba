//! observer_attach observer: runs its own POA on its own socket
//! (object key "obsv") so it can receive callbacks, calls
//! Subject.attach(self) on the subject via a real twoway `uiop::call`
//! (passing its own IOR, CDR-encoded, as the request argument), then
//! dispatches incoming oneway Observer.update() calls on that same
//! local POA -- the same mechanism the subject uses to serve attach(),
//! just acting as client for that one call and server for the rest.
//! See examples/c/observer_attach/observer.idl.

use hace_corba::corba::Ior;
use hace_corba::giop::{self, RequestHeader};
use hace_corba::uiop::{call, CdrBufferView, Transport, UiopError, HACE_UIOP_MAX_DATAGRAM_SIZE};
use hace_corba::{ByteOrder, CdrBuffer, Poa, RawCdrBuffer, SystemException};
use hace_corba::corba::{COMPLETED_NO, EXC_BAD_OPERATION, EXC_MARSHAL};
use hace_corba_examples_common::{parse_ior_arg, server_start};
use std::error::Error;
use std::ffi::CStr;
use std::os::raw::{c_char, c_void};
use std::process::ExitCode;

const OBSERVER_PORT: u16 = 59132;
const SUBJECT_PORT: u16 = 59131;
const EXPECTED_UPDATES: i32 = 3;
const SUBJECT_OBJECT_KEY: [u8; 4] = [b's', b'u', b'b', b'j'];
const OBSERVER_OBJECT_KEY: [u8; 4] = [b'o', b'b', b's', b'v'];

unsafe extern "C" fn update_servant(
    context: *mut c_void,
    operation: *const c_char,
    args: *mut RawCdrBuffer,
    _reply: *mut RawCdrBuffer,
    out_exception: *mut SystemException,
) {
    let operation = CStr::from_ptr(operation).to_str().unwrap_or("");
    if operation != "update" {
        (*out_exception).exception = EXC_BAD_OPERATION;
        (*out_exception).completion_status = COMPLETED_NO;
        return;
    }

    let mut args_view = CdrBufferView::from_raw(args);
    let event = match args_view.decode_string(128) {
        Ok(e) => e,
        Err(_) => {
            (*out_exception).exception = EXC_MARSHAL;
            (*out_exception).completion_status = COMPLETED_NO;
            return;
        }
    };

    println!("observer: received update: {event}");
    let received_count = &mut *(context as *mut i32);
    *received_count += 1;
}

fn build_attach_request<'a>(
    storage: &'a mut [u8],
    request_id: u32,
    self_ior: &Ior,
    subject_key: &[u8],
) -> Result<CdrBuffer<'a>, Box<dyn Error>> {
    let mut buf = CdrBuffer::new(storage, ByteOrder::Big)?;
    giop::begin_message(&mut buf, ByteOrder::Big)?;
    giop::encode_request_header(
        &mut buf,
        &RequestHeader {
            request_id,
            response_expected: true,
            object_key: subject_key.to_vec(),
            operation: "attach".to_string(),
        },
    )?;
    self_ior.encode(&mut buf)?;
    giop::finish_message(&mut buf)?;
    Ok(buf)
}

fn main() -> ExitCode {
    let subject_ior = parse_ior_arg(SUBJECT_PORT, &SUBJECT_OBJECT_KEY);

    // The observer is itself a tiny server: it needs a known, stable
    // address for the subject to call update() back on.
    let Some((socket, self_ior)) = server_start(
        "observer_attach observer",
        OBSERVER_PORT,
        &OBSERVER_OBJECT_KEY,
    ) else {
        return ExitCode::FAILURE;
    };

    let mut received_count: i32 = 0;
    let mut poa = match Poa::new() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("observer: poa setup failed: {e}");
            return ExitCode::FAILURE;
        }
    };
    if let Err(e) = poa.activate_object(
        &OBSERVER_OBJECT_KEY,
        update_servant,
        &mut received_count as *mut i32 as *mut c_void,
    ) {
        eprintln!("observer: activate_object failed: {e}");
        return ExitCode::FAILURE;
    }

    let transport = Transport::for_socket(&socket);

    {
        let mut storage = [0u8; 256];
        let mut request =
            match build_attach_request(&mut storage, 1, &self_ior, &subject_ior.object_key) {
                Ok(buf) => buf,
                Err(e) => {
                    eprintln!("observer: could not encode attach request: {e}");
                    return ExitCode::FAILURE;
                }
            };

        let mut reply_body = [0u8; 256];
        let result = call(
            &transport,
            &mut request,
            1,
            subject_ior.host_ipv4,
            subject_ior.port,
            HACE_UIOP_MAX_DATAGRAM_SIZE,
            1000,
            3,
            &mut reply_body,
        );
        match result {
            Ok(r) if !r.is_exception => {
                println!("observer: attached to subject");
            }
            Ok(_) => {
                eprintln!("observer: attach was rejected by the subject");
                return ExitCode::FAILURE;
            }
            Err(e) => {
                eprintln!("observer: attach call failed: {e}");
                return ExitCode::FAILURE;
            }
        }
    }

    while received_count < EXPECTED_UPDATES {
        match poa.handle_one_request(&transport, 5000, HACE_UIOP_MAX_DATAGRAM_SIZE) {
            Ok(()) => {}
            Err(UiopError::Timeout) => {
                println!("observer: no update received within 5s, exiting");
                break;
            }
            Err(e) => {
                eprintln!("observer: dispatch error: {e}");
                break;
            }
        }
    }

    ExitCode::SUCCESS
}
