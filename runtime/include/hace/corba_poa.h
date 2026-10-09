/*
 * A minimal Portable Object Adapter, per OMG CORBA Core specification
 * formal/21-02-02 Sec 11 (see docs/standards/spec-references.md),
 * restricted to the CORBA/e Compact Profile subset already committed
 * to in docs/standards/spec-references.md: static object activation
 * only, no servant managers, no DII/DSI/IR. See
 * docs/architecture/adr-0004-corba-core.md.
 *
 * Built on Phase 2's UIOP transport (hace/uiop.h): dispatch is "wait
 * for one Request via hace_uiop_receive, look up its object key, call
 * the matching servant, send the Reply via hace_uiop_send_message."
 * "Multi-session" concurrent dispatch needs no new bookkeeping here --
 * run several worker tasks (hace_hal_task_create, each at its own
 * priority) calling hace_corba_poa_run against the same socket; the OS
 * delivers each datagram to exactly one waiting recvfrom(), and the
 * HAL's socket table is already safe for concurrent access.
 *
 * Design constraints (same as cdr.h/giop.h/uiop.h): no dynamic
 * allocation, bounded (HACE_CORBA_POA_MAX_OBJECTS servants,
 * HACE_CORBA_POA_MAX_BODY_SIZE request/reply bodies).
 */
#ifndef HACE_CORBA_POA_H
#define HACE_CORBA_POA_H

#include "hace/cdr.h"
#include "hace/giop.h"
#include "hace/uiop.h"
#include "hace/corba_exception.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HACE_CORBA_POA_MAX_OBJECTS ((size_t)8)

/* Scratch buffer size for a dispatched request/reply body. A request
 * (or reply) whose opaque body doesn't fit this is rejected by the
 * underlying hace_uiop_receive (HACE_UIOP_ERR_TOO_LARGE) rather than
 * accommodated -- consistent with Phase 2's "too-large is rejected,
 * not grown" bound, chosen here to keep worker task stack usage
 * reasonable rather than sized to Phase 2's full fragmentation ceiling. */
#define HACE_CORBA_POA_MAX_BODY_SIZE ((size_t)4096)

/* A servant: handles one operation on one activated object. `args` is
 * a bounded, read-only view of the decoded request's opaque argument
 * bytes (hace_cdr_decode_* calls); `reply` is caller-provided storage,
 * positioned at 0, for the servant to encode its result into with
 * plain hace_cdr_encode_* calls. `out_exception` is zero-initialized
 * (HACE_CORBA_EXC_NONE) by the dispatcher before the call; a servant
 * only touches it to raise a system exception instead of returning a
 * normal result, in which case whatever was written to `reply` is
 * ignored. */
typedef void (*hace_corba_servant_fn_t)(void *context, const char *operation,
                                         hace_cdr_buffer_t *args,
                                         hace_cdr_buffer_t *reply,
                                         hace_corba_system_exception_t *out_exception);

typedef struct {
    uint8_t object_key[HACE_GIOP_MAX_OBJECT_KEY];
    size_t object_key_length;
    hace_corba_servant_fn_t servant;
    void *context;
    int in_use;
} hace_corba_poa_entry_t;

typedef struct {
    hace_corba_poa_entry_t entries[HACE_CORBA_POA_MAX_OBJECTS];
    size_t count;
} hace_corba_poa_t;

/* Returns sizeof(hace_corba_poa_t). Non-C bindings (Ada95, Rust --
 * see bindings/) that treat hace_corba_poa_t as opaque storage (it
 * holds a function-pointer-bearing array that's fragile to mirror by
 * hand) rather than mirroring its field layout use this to size that
 * storage and runtime-verify it's large enough, the same way
 * hace_cdr_buffer_sizeof() already supports hace_cdr_buffer_t. */
size_t hace_corba_poa_sizeof(void);

hace_corba_status_t hace_corba_poa_init(hace_corba_poa_t *poa);

/* Registers servant/context under object_key. Must be called before
 * any worker task starts calling hace_corba_poa_handle_one_request/
 * hace_corba_poa_run on this poa -- the servant table is read-only
 * once dispatch begins, so no locking is needed for concurrent
 * dispatch from multiple worker tasks (see the ADR). Rejects
 * (HACE_CORBA_ERR_INVALID_ARGUMENT) a key longer than
 * HACE_GIOP_MAX_OBJECT_KEY or once HACE_CORBA_POA_MAX_OBJECTS entries
 * are already activated. */
hace_corba_status_t hace_corba_poa_activate_object(
    hace_corba_poa_t *poa, const uint8_t *object_key, size_t object_key_length,
    hace_corba_servant_fn_t servant, void *context);

/* Waits up to timeout_ms for one Request, dispatches it to the
 * matching activated servant, and sends a Reply -- unless the request
 * was oneway (response_expected == 0), in which case nothing is sent
 * back at all, per UIOP's existing oneway semantics. An unrecognized
 * object key never reaches a servant: a SYSTEM_EXCEPTION Reply
 * (OBJECT_NOT_EXIST) is sent instead. A servant that doesn't recognize
 * `operation` is expected to set *out_exception to BAD_OPERATION
 * itself -- this module has no notion of which operations a servant
 * supports.
 *
 * The return value is about dispatch *mechanics*, not the RPC
 * outcome (which travels to the real client as the Reply's status):
 * HACE_UIOP_ERR_TIMEOUT means nothing arrived within timeout_ms --
 * routine, not a failure ("no work this round"); a caller looping
 * this (see hace_corba_poa_run) is expected to see it regularly. Any
 * other non-OK status is a real dispatch/transport problem. */
hace_uiop_status_t hace_corba_poa_handle_one_request(
    const hace_corba_poa_t *poa, const hace_uiop_transport_t *transport,
    uint32_t timeout_ms, size_t max_datagram_size);

/* Convenience worker loop: calls hace_corba_poa_handle_one_request
 * repeatedly (swallowing the routine HACE_UIOP_ERR_TIMEOUT between
 * iterations) until *stop_flag becomes nonzero, then returns
 * HACE_UIOP_OK. Returns early with any other non-OK status from
 * handle_one_request. This is what a worker task's hace_hal_task_fn_t
 * body calls; every worker needs the exact same loop, hence the
 * wrapper. Running N such tasks (hace_hal_task_create, each at its own
 * priority) against the same transport/socket is this project's
 * bounded, RT-CORBA-inspired worker-task pool -- see the ADR. */
hace_uiop_status_t hace_corba_poa_run(const hace_corba_poa_t *poa,
                                       const hace_uiop_transport_t *transport,
                                       size_t max_datagram_size,
                                       uint32_t iteration_timeout_ms,
                                       const volatile int *stop_flag);

#ifdef __cplusplus
}
#endif

#endif /* HACE_CORBA_POA_H */
