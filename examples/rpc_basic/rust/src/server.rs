//! rpc_basic server: activates a Calculator object (object key "calc")
//! whose add(a, b) operation returns a + b, over a blocking twoway
//! call. See ../../server.idl for the interface
//! this hand-implements (there is no idlc yet -- Phase 4).

use hace_corba::corba::{COMPLETED_NO, EXC_BAD_OPERATION, EXC_MARSHAL};
use hace_corba::uiop::{CdrBufferView, Transport, UiopError, HACE_UIOP_MAX_DATAGRAM_SIZE};
use hace_corba::{Poa, RawCdrBuffer, SystemException};
use hace_corba_examples_common::server_start;
use std::ffi::CStr;
use std::os::raw::{c_char, c_void};
use std::process::ExitCode;

const RPC_BASIC_PORT: u16 = 59101;
const CALC_OBJECT_KEY: [u8; 4] = [b'c', b'a', b'l', b'c'];

unsafe extern "C" fn calculator_servant(
    _context: *mut c_void,
    operation: *const c_char,
    args: *mut RawCdrBuffer,
    reply: *mut RawCdrBuffer,
    out_exception: *mut SystemException,
) {
    let operation = CStr::from_ptr(operation).to_str().unwrap_or("");
    if operation != "add" {
        (*out_exception).exception = EXC_BAD_OPERATION;
        (*out_exception).completion_status = COMPLETED_NO;
        return;
    }

    let mut args_view = CdrBufferView::from_raw(args);
    let (a, b) = match (args_view.decode_i32(), args_view.decode_i32()) {
        (Ok(a), Ok(b)) => (a, b),
        _ => {
            (*out_exception).exception = EXC_MARSHAL;
            (*out_exception).completion_status = COMPLETED_NO;
            return;
        }
    };

    let mut reply_view = CdrBufferView::from_raw(reply);
    let _ = reply_view.encode_i32(a + b);
    println!("server: add({a}, {b}) = {}", a + b);
}

fn main() -> ExitCode {
    let Some((socket, _ior)) =
        server_start("rpc_basic server", RPC_BASIC_PORT, &CALC_OBJECT_KEY)
    else {
        return ExitCode::FAILURE;
    };

    let mut poa = match Poa::new() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("rpc_basic server: poa setup failed: {e}");
            return ExitCode::FAILURE;
        }
    };
    if let Err(e) = poa.activate_object(&CALC_OBJECT_KEY, calculator_servant, std::ptr::null_mut())
    {
        eprintln!("rpc_basic server: activate_object failed: {e}");
        return ExitCode::FAILURE;
    }

    let transport = Transport::for_socket(&socket);

    // Self-terminating (no Ctrl-C needed): handle exactly one call,
    // matching this example's "one client makes one call" scope.
    match poa.handle_one_request(&transport, 10_000, HACE_UIOP_MAX_DATAGRAM_SIZE) {
        Ok(()) => {}
        Err(UiopError::Timeout) => {
            println!("rpc_basic server: no request received within 10s, exiting")
        }
        Err(e) => {
            eprintln!("rpc_basic server: dispatch error: {e}");
            return ExitCode::FAILURE;
        }
    }

    ExitCode::SUCCESS
}
