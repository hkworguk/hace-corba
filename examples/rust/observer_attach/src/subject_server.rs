//! observer_attach subject: activates a Subject object (object key
//! "subj") whose attach(obs) operation registers a caller-supplied
//! Observer object reference (CDR-encoded IOR, since there's no idlc
//! to marshal an object reference parameter automatically). Every
//! ~1s of wall-clock time, pushes one simulated event to every
//! attached observer via a oneway Observer.update() call -- the
//! server acting as a client on the very same socket it serves
//! attach() on. See examples/c/observer_attach/observer.idl for the
//! interfaces this hand-implements.

use hace_corba::corba::{COMPLETED_NO, EXC_BAD_OPERATION, EXC_MARSHAL, EXC_NO_MEMORY};
use hace_corba::giop::{self, RequestHeader};
use hace_corba::uiop::{send_message, CdrBufferView, Transport, HACE_UIOP_MAX_DATAGRAM_SIZE};
use hace_corba::{corba::Ior, monotonic_now, ByteOrder, CdrBuffer, Poa, RawCdrBuffer, SystemException};
use hace_corba_examples_common::server_start;
use std::error::Error;
use std::ffi::CStr;
use std::os::raw::{c_char, c_void};
use std::process::ExitCode;
use std::time::Duration;

const SUBJECT_PORT: u16 = 59131;
const NOTIFY_COUNT: usize = 3;
const NOTIFY_INTERVAL: Duration = Duration::from_millis(1000);
const MAX_ATTACHED: usize = 4;
const SUBJECT_OBJECT_KEY: [u8; 4] = [b's', b'u', b'b', b'j'];

struct ObserverRegistry {
    observers: [Option<Ior>; MAX_ATTACHED],
    count: usize,
}

unsafe extern "C" fn attach_servant(
    context: *mut c_void,
    operation: *const c_char,
    args: *mut RawCdrBuffer,
    _reply: *mut RawCdrBuffer,
    out_exception: *mut SystemException,
) {
    let operation = CStr::from_ptr(operation).to_str().unwrap_or("");
    if operation != "attach" {
        (*out_exception).exception = EXC_BAD_OPERATION;
        (*out_exception).completion_status = COMPLETED_NO;
        return;
    }

    let mut args_view = CdrBufferView::from_raw(args);
    let observer_ior = match Ior::decode_view(&mut args_view) {
        Ok(ior) => ior,
        Err(_) => {
            (*out_exception).exception = EXC_MARSHAL;
            (*out_exception).completion_status = COMPLETED_NO;
            return;
        }
    };

    let registry = &mut *(context as *mut ObserverRegistry);
    match registry.observers.iter().position(|o| o.is_none()) {
        Some(slot) => {
            registry.observers[slot] = Some(observer_ior);
            registry.count += 1;
            println!(
                "subject: observer attached (slot {slot}), total={}",
                registry.count
            );
        }
        None => {
            (*out_exception).exception = EXC_NO_MEMORY;
            (*out_exception).completion_status = COMPLETED_NO;
        }
    }
}

fn build_update_request<'a>(
    storage: &'a mut [u8],
    request_id: u32,
    event: &str,
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
            operation: "update".to_string(),
        },
    )?;
    buf.encode_string(event)?;
    giop::finish_message(&mut buf)?;
    Ok(buf)
}

fn notify_all(
    registry: &ObserverRegistry,
    transport: &Transport,
    next_request_id: &mut u32,
    event: &str,
) {
    for (i, observer) in registry.observers.iter().enumerate() {
        let Some(observer) = observer else { continue };
        let id = *next_request_id;
        *next_request_id += 1;

        let mut storage = [0u8; 256];
        let sent = build_update_request(&mut storage, id, event, &observer.object_key).and_then(
            |mut buf| {
                send_message(
                    transport,
                    &mut buf,
                    id,
                    observer.host_ipv4,
                    observer.port,
                    HACE_UIOP_MAX_DATAGRAM_SIZE,
                )
                .map_err(Box::<dyn Error>::from)
            },
        );
        match sent {
            Ok(()) => println!("subject: notified observer {i}: {event}"),
            Err(e) => eprintln!("subject: failed to notify observer {i}: {e}"),
        }
    }
}

fn main() -> ExitCode {
    let events = ["temperature=21C", "temperature=23C", "temperature=19C"];

    let Some((socket, _ior)) =
        server_start("observer_attach subject", SUBJECT_PORT, &SUBJECT_OBJECT_KEY)
    else {
        return ExitCode::FAILURE;
    };

    let mut registry = ObserverRegistry {
        observers: [None, None, None, None],
        count: 0,
    };

    let mut poa = match Poa::new() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("subject: poa setup failed: {e}");
            return ExitCode::FAILURE;
        }
    };
    if let Err(e) = poa.activate_object(
        &SUBJECT_OBJECT_KEY,
        attach_servant,
        &mut registry as *mut ObserverRegistry as *mut c_void,
    ) {
        eprintln!("subject: activate_object failed: {e}");
        return ExitCode::FAILURE;
    }

    let transport = Transport::for_socket(&socket);

    let start = match monotonic_now() {
        Ok(t) => t,
        Err(e) => {
            eprintln!("subject: could not read the clock: {e}");
            return ExitCode::FAILURE;
        }
    };

    let mut next_request_id: u32 = 1000; // a range distinct from clients' own ids
    let mut notifications_sent = 0usize;

    while notifications_sent < NOTIFY_COUNT {
        match poa.handle_one_request(&transport, 200, HACE_UIOP_MAX_DATAGRAM_SIZE) {
            Ok(()) => {}
            Err(hace_corba::uiop::UiopError::Timeout) => {}
            Err(e) => {
                eprintln!("subject: dispatch error: {e}");
                break;
            }
        }

        let now = match monotonic_now() {
            Ok(t) => t,
            Err(_) => break,
        };
        if now.saturating_sub(start) >= NOTIFY_INTERVAL * (notifications_sent as u32 + 1) {
            notify_all(&registry, &transport, &mut next_request_id, events[notifications_sent]);
            notifications_sent += 1;
        }
    }

    ExitCode::SUCCESS
}
