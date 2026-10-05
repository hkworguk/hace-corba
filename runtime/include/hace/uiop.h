/*
 * UIOP transport: sending/receiving GIOP messages (runtime/include/hace/giop.h)
 * over UDP (runtime/include/hace/hal.h), with bounded retry/timeout for twoway
 * calls and real fragmentation across multiple datagrams. See
 * docs/architecture/adr-0003-uiop-transport.md for the design
 * rationale and docs/roadmap.md (Phase 2) for scope.
 *
 * Scope: this is the transport for a single outstanding call at a time
 * -- a client sending one request and waiting for its one reply.
 * Multi-session concurrent request handling (a server dispatching many
 * clients at once) is Phase 3 (ORB/POA), which doesn't exist yet;
 * hace_uiop_receive is written to be reusable by that future dispatch
 * loop rather than assuming it will only ever wait for a Reply.
 *
 * Design constraints (same as cdr.h/giop.h): no dynamic allocation
 * (fixed-capacity buffers sized by HACE_UIOP_MAX_DATAGRAM_SIZE, no
 * VLAs), no exceptions (every function returns a hace_uiop_status_t),
 * bounded (HACE_UIOP_MAX_FRAGMENTS caps fragmentation on both send and
 * receive).
 */
#ifndef HACE_UIOP_H
#define HACE_UIOP_H

#include "hace/giop.h"
#include "hace/hal.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HACE_UIOP_OK = 0,
    HACE_UIOP_ERR_INVALID_ARGUMENT = 1,
    HACE_UIOP_ERR_HAL = 2,     /* a transport send/recv call failed */
    HACE_UIOP_ERR_TOO_LARGE = 3, /* would need more than HACE_UIOP_MAX_FRAGMENTS datagrams */
    HACE_UIOP_ERR_TIMEOUT = 4  /* hace_uiop_call: every attempt timed out */
} hace_uiop_status_t;

/* Comfortably larger than the largest fixed GIOP header this profile
 * defines (Request's, bounded by HACE_GIOP_MAX_OBJECT_KEY/
 * HACE_GIOP_MAX_OPERATION_NAME at ~134 bytes worst case) -- enforced as
 * a floor on max_datagram_size so byte-cut fragmentation never splits
 * a message's fixed header fields (see the ADR). */
#define HACE_UIOP_MIN_DATAGRAM_SIZE ((size_t)256)

/* The usual MTU-safe ceiling (1500-byte Ethernet frame minus IP/UDP
 * headers), and the compile-time size of this module's fixed internal
 * buffers -- max_datagram_size is a runtime value bounded by
 * [HACE_UIOP_MIN_DATAGRAM_SIZE, HACE_UIOP_MAX_DATAGRAM_SIZE], never a
 * VLA size. */
#define HACE_UIOP_MAX_DATAGRAM_SIZE ((size_t)1472)

/* Caps how many datagrams one logical message may be split into, both
 * sending (rejected outright rather than emitting unboundedly many
 * datagrams) and receiving (reassembly abandoned rather than
 * accumulating unboundedly). */
#define HACE_UIOP_MAX_FRAGMENTS ((size_t)16)

/* Two explicit function pointers, injected by the caller, rather than
 * this module calling the HAL's socket functions directly: lets tests
 * substitute a fault-injecting fake (drop/duplicate specific sends) to
 * exercise retry/timeout deterministically. See
 * hace_uiop_transport_for_socket for the production adapter. */
typedef hace_hal_status_t (*hace_uiop_send_fn_t)(void *context,
                                                  const uint8_t *data,
                                                  size_t length,
                                                  uint32_t dest_addr_ipv4,
                                                  uint16_t dest_port);
typedef hace_hal_status_t (*hace_uiop_recv_fn_t)(void *context,
                                                  uint8_t *buffer,
                                                  size_t capacity,
                                                  size_t *out_length,
                                                  uint32_t *src_addr_ipv4,
                                                  uint16_t *src_port,
                                                  uint32_t timeout_ms);

typedef struct {
    hace_uiop_send_fn_t send;
    hace_uiop_recv_fn_t recv;
    void *context;
} hace_uiop_transport_t;

/* Builds the production transport: send/recv forward to
 * hace_hal_socket_send/hace_hal_socket_recv on *handle. handle must
 * stay valid (and its socket open) for as long as the returned
 * transport is used. */
hace_uiop_transport_t hace_uiop_transport_for_socket(
    const hace_hal_socket_handle_t *handle);

/* Sends message (fully encoded via hace_giop_begin_message + a
 * per-type encode + hace_giop_finish_message) to dest_addr:dest_port,
 * fragmenting across multiple HACE_GIOP_MSG_FRAGMENT datagrams if it
 * doesn't fit in max_datagram_size. message_type and request_id must
 * match what was encoded into message (needed to re-stamp the header
 * if fragmenting; not re-derived by decoding message, to avoid a
 * redundant decode pass).
 *
 * This is also the oneway primitive: a Request encoded with
 * response_expected = 0, sent once via this function, with no call to
 * hace_uiop_receive/hace_uiop_call afterward, is a complete oneway
 * call -- no separate API needed.
 *
 * Rejects max_datagram_size outside [HACE_UIOP_MIN_DATAGRAM_SIZE,
 * HACE_UIOP_MAX_DATAGRAM_SIZE], and a message needing more than
 * HACE_UIOP_MAX_FRAGMENTS datagrams (HACE_UIOP_ERR_TOO_LARGE). */
hace_uiop_status_t hace_uiop_send_message(const hace_uiop_transport_t *transport,
                                           hace_cdr_buffer_t *message,
                                           hace_giop_message_type_t message_type,
                                           uint32_t request_id,
                                           uint32_t dest_addr_ipv4,
                                           uint16_t dest_port,
                                           size_t max_datagram_size);

/* Waits up to timeout_ms for a complete message of expected_type
 * (HACE_GIOP_MSG_REQUEST or HACE_GIOP_MSG_REPLY), transparently
 * reassembling it if the sender fragmented it. match_request_id, if
 * non-NULL, filters to only a message whose request_id equals
 * *match_request_id; pass NULL to accept any. Anything that doesn't
 * match -- wrong type, wrong id, a stray Fragment with no matching
 * first chunk seen, unparseable noise -- is silently discarded (never
 * surfaced as an error: unrelated UDP traffic on a shared socket is
 * expected, not exceptional) and waiting continues until timeout_ms
 * elapses, at which point this returns HACE_UIOP_ERR_TIMEOUT.
 *
 * On success, *out_header is the GIOP message header of the message's
 * first datagram, *out_request_id its request_id, body_storage[0 ..
 * *out_body_length) the reassembled opaque body (operation arguments
 * for a Request, return value/exception body for a Reply).
 * *out_reply_status is set only when expected_type is
 * HACE_GIOP_MSG_REPLY (pass NULL if not needed; untouched otherwise).
 * out_src_addr_ipv4/out_src_port (pass NULL for either if not
 * needed) report the first chunk's sender address -- how a server finds
 * out where to send its reply, since UDP carries no persistent
 * connection to answer back on. Reassembly exceeding
 * HACE_UIOP_MAX_FRAGMENTS datagrams or body_capacity returns
 * HACE_UIOP_ERR_TOO_LARGE. */
hace_uiop_status_t hace_uiop_receive(const hace_uiop_transport_t *transport,
                                      hace_giop_message_type_t expected_type,
                                      const uint32_t *match_request_id,
                                      uint32_t timeout_ms,
                                      uint8_t *body_storage,
                                      size_t body_capacity,
                                      hace_giop_message_header_t *out_header,
                                      uint32_t *out_request_id,
                                      hace_giop_reply_status_t *out_reply_status,
                                      size_t *out_body_length,
                                      uint32_t *out_src_addr_ipv4,
                                      uint16_t *out_src_port);

/* The twoway client primitive: sends request (a fully encoded
 * HACE_GIOP_MSG_REQUEST message, built the same way as for
 * hace_uiop_send_message) and waits per_attempt_timeout_ms for a
 * matching Reply, resending up to max_attempts times total before
 * giving up with HACE_UIOP_ERR_TIMEOUT. A non-timeout transport error
 * from send or receive propagates immediately rather than being
 * retried. On success, semantics of the out parameters match
 * hace_uiop_receive's (for expected_type = HACE_GIOP_MSG_REPLY). */
hace_uiop_status_t hace_uiop_call(const hace_uiop_transport_t *transport,
                                   hace_cdr_buffer_t *request,
                                   uint32_t request_id,
                                   uint32_t dest_addr_ipv4,
                                   uint16_t dest_port,
                                   size_t max_datagram_size,
                                   uint32_t per_attempt_timeout_ms,
                                   uint32_t max_attempts,
                                   uint8_t *reply_body_storage,
                                   size_t reply_body_capacity,
                                   hace_giop_reply_status_t *out_reply_status,
                                   size_t *out_reply_body_length);

#ifdef __cplusplus
}
#endif

#endif /* HACE_UIOP_H */
