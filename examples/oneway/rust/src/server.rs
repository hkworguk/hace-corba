//! oneway server: activates a Logger object (object key "log") whose
//! log(message) operation is oneway -- it prints the message and sends
//! nothing back. See ../../server.idl for the interface
//! this hand-implements.

use hace_corba::corba::{COMPLETED_NO, EXC_BAD_OPERATION, EXC_MARSHAL};
use hace_corba::uiop::{CdrBufferView, Transport, UiopError, HACE_UIOP_MAX_DATAGRAM_SIZE};
use hace_corba::{Poa, RawCdrBuffer, SystemException};
use hace_corba_examples_common::server_start;
use std::ffi::CStr;
use std::os::raw::{c_char, c_void};
use std::process::ExitCode;

const ONEWAY_PORT: u16 = 59111;
const LOG_OBJECT_KEY: [u8; 3] = [b'l', b'o', b'g'];

unsafe extern "C" fn logger_servant(
    _context: *mut c_void,
    operation: *const c_char,
    args: *mut RawCdrBuffer,
    _reply: *mut RawCdrBuffer,
    out_exception: *mut SystemException,
) {
    let operation = CStr::from_ptr(operation).to_str().unwrap_or("");
    if operation != "log" {
        (*out_exception).exception = EXC_BAD_OPERATION;
        (*out_exception).completion_status = COMPLETED_NO;
        return;
    }

    let mut args_view = CdrBufferView::from_raw(args);
    let message = match args_view.decode_string(128) {
        Ok(m) => m,
        Err(_) => {
            (*out_exception).exception = EXC_MARSHAL;
            (*out_exception).completion_status = COMPLETED_NO;
            return;
        }
    };

    println!("server: log: {message}");
}

fn main() -> ExitCode {
    let Some((socket, _ior)) = server_start("oneway server", ONEWAY_PORT, &LOG_OBJECT_KEY) else {
        return ExitCode::FAILURE;
    };

    let mut poa = match Poa::new() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("oneway server: poa setup failed: {e}");
            return ExitCode::FAILURE;
        }
    };
    if let Err(e) = poa.activate_object(&LOG_OBJECT_KEY, logger_servant, std::ptr::null_mut()) {
        eprintln!("oneway server: activate_object failed: {e}");
        return ExitCode::FAILURE;
    }

    let transport = Transport::for_socket(&socket);

    // Self-terminating: handle up to 3 fire-and-forget messages
    // (matching client.rs's three log() calls), or stop once 5s
    // passes without one.
    let mut handled = 0;
    while handled < 3 {
        match poa.handle_one_request(&transport, 5000, HACE_UIOP_MAX_DATAGRAM_SIZE) {
            Ok(()) => handled += 1,
            Err(UiopError::Timeout) => {
                println!("oneway server: no more messages within 5s, exiting");
                break;
            }
            Err(e) => {
                eprintln!("oneway server: dispatch error: {e}");
                return ExitCode::FAILURE;
            }
        }
    }

    ExitCode::SUCCESS
}
