//! async_ami server: activates a Worker object (object key "work")
//! whose compute(input) operation takes ~300ms of simulated work
//! before replying with input * input. The delay exists so client.rs's
//! Poller-style loop has something real to demonstrate. See
//! ../../server.idl for the interface this hand-implements.

use hace_corba::corba::{COMPLETED_NO, EXC_BAD_OPERATION, EXC_MARSHAL};
use hace_corba::uiop::{CdrBufferView, Transport, UiopError, HACE_UIOP_MAX_DATAGRAM_SIZE};
use hace_corba::{Poa, RawCdrBuffer, SystemException};
use hace_corba_examples_common::server_start;
use std::ffi::CStr;
use std::os::raw::{c_char, c_void};
use std::process::ExitCode;
use std::time::Duration;

const ASYNC_AMI_PORT: u16 = 59121;
const WORK_SIMULATED: Duration = Duration::from_millis(300);
const WORK_OBJECT_KEY: [u8; 4] = [b'w', b'o', b'r', b'k'];

unsafe extern "C" fn worker_servant(
    _context: *mut c_void,
    operation: *const c_char,
    args: *mut RawCdrBuffer,
    reply: *mut RawCdrBuffer,
    out_exception: *mut SystemException,
) {
    let operation = CStr::from_ptr(operation).to_str().unwrap_or("");
    if operation != "compute" {
        (*out_exception).exception = EXC_BAD_OPERATION;
        (*out_exception).completion_status = COMPLETED_NO;
        return;
    }

    let mut args_view = CdrBufferView::from_raw(args);
    let input = match args_view.decode_i32() {
        Ok(v) => v,
        Err(_) => {
            (*out_exception).exception = EXC_MARSHAL;
            (*out_exception).completion_status = COMPLETED_NO;
            return;
        }
    };

    println!("server: computing for input={input} (simulating {}ms of work)...", WORK_SIMULATED.as_millis());
    std::thread::sleep(WORK_SIMULATED);

    let mut reply_view = CdrBufferView::from_raw(reply);
    let _ = reply_view.encode_i32(input * input);
    println!("server: compute({input}) = {}", input * input);
}

fn main() -> ExitCode {
    let Some((socket, _ior)) = server_start("async_ami server", ASYNC_AMI_PORT, &WORK_OBJECT_KEY)
    else {
        return ExitCode::FAILURE;
    };

    let mut poa = match Poa::new() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("async_ami server: poa setup failed: {e}");
            return ExitCode::FAILURE;
        }
    };
    if let Err(e) = poa.activate_object(&WORK_OBJECT_KEY, worker_servant, std::ptr::null_mut()) {
        eprintln!("async_ami server: activate_object failed: {e}");
        return ExitCode::FAILURE;
    }

    let transport = Transport::for_socket(&socket);

    match poa.handle_one_request(&transport, 10_000, HACE_UIOP_MAX_DATAGRAM_SIZE) {
        Ok(()) => {}
        Err(UiopError::Timeout) => {
            println!("async_ami server: no request received within 10s, exiting")
        }
        Err(e) => {
            eprintln!("async_ami server: dispatch error: {e}");
            return ExitCode::FAILURE;
        }
    }

    ExitCode::SUCCESS
}
