/*
 * GIOP message framing, per OMG CORBA Interoperability specification
 * formal/12-11-14 Sec 7 (see docs/standards/spec-references.md) and the
 * scope decisions in docs/architecture/adr-0002-giop-framing.md: GIOP
 * 1.2 only, object-key-only TargetAddress, a collapsed oneway/twoway
 * sync scope, and empty-only service contexts.
 *
 * This module encodes/decodes GIOP messages over a caller-supplied
 * hace_cdr_buffer_t. It does not send or receive anything -- there is
 * no transport here (see docs/roadmap.md, Phase 2 "UIOP transport").
 *
 * Design constraints (same as runtime/include/hace/cdr.h): no dynamic
 * allocation (object keys and operation names are fixed-capacity), no
 * exceptions (every function returns a hace_giop_status_t), bounded
 * (decode functions never read past a message's declared size).
 */
#ifndef HACE_GIOP_H
#define HACE_GIOP_H

#include "hace/cdr.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HACE_GIOP_OK = 0,
    HACE_GIOP_ERR_BUFFER_OVERFLOW = 1,
    HACE_GIOP_ERR_BUFFER_UNDERFLOW = 2,
    HACE_GIOP_ERR_INVALID_ARGUMENT = 3,
    HACE_GIOP_ERR_BAD_MAGIC = 4,
    HACE_GIOP_ERR_UNSUPPORTED_VERSION = 5,
    HACE_GIOP_ERR_UNSUPPORTED_MESSAGE_TYPE = 6,
    HACE_GIOP_ERR_UNSUPPORTED_ADDRESSING_DISPOSITION = 7,
    HACE_GIOP_ERR_UNSUPPORTED_SYNC_SCOPE = 8,
    HACE_GIOP_ERR_NONEMPTY_SERVICE_CONTEXT = 9
} hace_giop_status_t;

/* All 8 values per the spec, for correct numbering -- but
 * HACE_GIOP_MSG_CLOSE_CONNECTION is never produced by this module and
 * decoding one is not treated as an error by hace_giop_decode_message_header;
 * it has no meaning over a connectionless transport (see
 * docs/corba-spec-gap-analysis.md Sec 2), and acting on it is left to
 * whatever calls this module, not this module itself. */
typedef enum {
    HACE_GIOP_MSG_REQUEST = 0,
    HACE_GIOP_MSG_REPLY = 1,
    HACE_GIOP_MSG_CANCEL_REQUEST = 2,
    HACE_GIOP_MSG_LOCATE_REQUEST = 3,
    HACE_GIOP_MSG_LOCATE_REPLY = 4,
    HACE_GIOP_MSG_CLOSE_CONNECTION = 5,
    HACE_GIOP_MSG_MESSAGE_ERROR = 6,
    HACE_GIOP_MSG_FRAGMENT = 7
} hace_giop_message_type_t;

typedef enum {
    HACE_GIOP_REPLY_NO_EXCEPTION = 0,
    HACE_GIOP_REPLY_USER_EXCEPTION = 1,
    HACE_GIOP_REPLY_SYSTEM_EXCEPTION = 2,
    HACE_GIOP_REPLY_LOCATION_FORWARD = 3,
    HACE_GIOP_REPLY_LOCATION_FORWARD_PERM = 4,
    HACE_GIOP_REPLY_NEEDS_ADDRESSING_MODE = 5
} hace_giop_reply_status_t;

typedef enum {
    HACE_GIOP_LOCATE_UNKNOWN_OBJECT = 0,
    HACE_GIOP_LOCATE_OBJECT_HERE = 1,
    HACE_GIOP_LOCATE_OBJECT_FORWARD = 2,
    HACE_GIOP_LOCATE_OBJECT_FORWARD_PERM = 3,
    HACE_GIOP_LOCATE_SYSTEM_EXCEPTION = 4,
    HACE_GIOP_LOCATE_NEEDS_ADDRESSING_MODE = 5
} hace_giop_locate_status_t;

#define HACE_GIOP_MAX_OBJECT_KEY ((size_t)32)
#define HACE_GIOP_MAX_OPERATION_NAME ((size_t)64)

typedef struct {
    uint8_t version_major; /* always 1 for headers this module produces */
    uint8_t version_minor; /* always 2 */
    hace_cdr_byte_order_t byte_order;
    int more_fragments; /* GIOP 1.2 "fragment follows" flag */
    hace_giop_message_type_t message_type;
    uint32_t message_size; /* body length in bytes: NOT including the
                             * 12-byte header or the 4-byte alignment pad */
} hace_giop_message_header_t;

/* ---- Generic message framing (all message types) ---- */

/* Sets buf's byte order and advances past where the 12-byte GIOP header
 * plus the 4 bytes of padding GIOP 1.2 requires (so the body that
 * follows is 8-byte aligned from the start of the message) will go.
 * Call this before encoding any message-type-specific fields below. */
hace_giop_status_t hace_giop_begin_message(hace_cdr_buffer_t *buf,
                                            hace_cdr_byte_order_t byte_order);

/* Call once the message body has been fully encoded: computes the body
 * length from the current position, rewinds to fill in the real
 * 12-byte header + pad, then restores the position to the true end of
 * the encoded message (so hace_cdr_buffer_position(buf) afterward is
 * the total number of bytes to send). */
hace_giop_status_t hace_giop_finish_message(hace_cdr_buffer_t *buf,
                                             hace_giop_message_type_t message_type,
                                             int more_fragments);

/* Decodes the 12-byte GIOP header + 4-byte pad, discovering buf's byte
 * order from the header's own flags octet (a single, order-independent
 * byte) and setting it via hace_cdr_buffer_set_byte_order before the
 * multi-byte message_size field is read. Leaves buf positioned right
 * after the pad, ready for a message-type-specific decode function. */
hace_giop_status_t hace_giop_decode_message_header(hace_cdr_buffer_t *buf,
                                                    hace_giop_message_header_t *out_header);

/* ---- Request ---- */

typedef struct {
    uint32_t request_id;
    int response_expected; /* 0 = oneway, 1 = wait for the target's
                             * real reply. SYNC_WITH_TRANSPORT/
                             * SYNC_WITH_SERVER have no UDP equivalent
                             * and are rejected on decode -- see
                             * docs/corba-spec-gap-analysis.md Sec 7. */
    uint8_t object_key[HACE_GIOP_MAX_OBJECT_KEY];
    size_t object_key_length;
    char operation[HACE_GIOP_MAX_OPERATION_NAME]; /* NUL-terminated */
} hace_giop_request_header_t;

hace_giop_status_t hace_giop_encode_request_header(hace_cdr_buffer_t *buf,
                                                    const hace_giop_request_header_t *header);

/* message_size is the declared GIOP body length from
 * hace_giop_message_header_t. *out_body is bound (no copy) to whatever
 * of that declared length remains after these fixed fields -- the
 * opaque operation-argument bytes, which this module has no typed way
 * to decode without idlc (Phase 4). */
hace_giop_status_t hace_giop_decode_request_header(hace_cdr_buffer_t *buf,
                                                    size_t message_size,
                                                    hace_giop_request_header_t *out_header,
                                                    hace_cdr_buffer_t *out_body);

/* ---- Reply ---- */

typedef struct {
    uint32_t request_id;
    hace_giop_reply_status_t reply_status;
} hace_giop_reply_header_t;

hace_giop_status_t hace_giop_encode_reply_header(hace_cdr_buffer_t *buf,
                                                  const hace_giop_reply_header_t *header);

/* *out_body is the opaque return-value/exception-body bytes, same
 * convention as hace_giop_decode_request_header. */
hace_giop_status_t hace_giop_decode_reply_header(hace_cdr_buffer_t *buf,
                                                  size_t message_size,
                                                  hace_giop_reply_header_t *out_header,
                                                  hace_cdr_buffer_t *out_body);

/* ---- CancelRequest ---- */

typedef struct {
    uint32_t request_id;
} hace_giop_cancel_request_header_t;

hace_giop_status_t hace_giop_encode_cancel_request_header(
    hace_cdr_buffer_t *buf, const hace_giop_cancel_request_header_t *header);
hace_giop_status_t hace_giop_decode_cancel_request_header(
    hace_cdr_buffer_t *buf, hace_giop_cancel_request_header_t *out_header);

/* ---- LocateRequest ---- */

typedef struct {
    uint32_t request_id;
    uint8_t object_key[HACE_GIOP_MAX_OBJECT_KEY];
    size_t object_key_length;
} hace_giop_locate_request_header_t;

hace_giop_status_t hace_giop_encode_locate_request_header(
    hace_cdr_buffer_t *buf, const hace_giop_locate_request_header_t *header);
hace_giop_status_t hace_giop_decode_locate_request_header(
    hace_cdr_buffer_t *buf, hace_giop_locate_request_header_t *out_header);

/* ---- LocateReply ---- */

typedef struct {
    uint32_t request_id;
    hace_giop_locate_status_t locate_status;
} hace_giop_locate_reply_header_t;

hace_giop_status_t hace_giop_encode_locate_reply_header(
    hace_cdr_buffer_t *buf, const hace_giop_locate_reply_header_t *header);

/* No *out_body: a forwarding IOR can follow for OBJECT_FORWARD* status,
 * but Phase 3 hasn't defined IOR encoding yet, so there is nothing to
 * decode it into regardless (see docs/architecture/adr-0002-giop-framing.md). */
hace_giop_status_t hace_giop_decode_locate_reply_header(
    hace_cdr_buffer_t *buf, hace_giop_locate_reply_header_t *out_header);

/* ---- MessageError ----
 *
 * No per-type functions: MessageError has no body at all. Produce one
 * with hace_giop_begin_message() immediately followed by
 * hace_giop_finish_message(buf, HACE_GIOP_MSG_MESSAGE_ERROR, 0).
 */

/* ---- Fragment ----
 *
 * Only the Fragment message's own header (request_id) is covered here.
 * Splitting an oversized message across multiple Fragment datagrams and
 * reassembling them is a transport concern, not a wire-format one --
 * see docs/roadmap.md, Phase 2.
 */

typedef struct {
    uint32_t request_id;
} hace_giop_fragment_header_t;

hace_giop_status_t hace_giop_encode_fragment_header(
    hace_cdr_buffer_t *buf, const hace_giop_fragment_header_t *header);

/* *out_body is bound to whatever of the declared message_size remains
 * after request_id -- the continuation bytes Phase 2's reassembly logic
 * will consume. */
hace_giop_status_t hace_giop_decode_fragment_header(
    hace_cdr_buffer_t *buf, size_t message_size,
    hace_giop_fragment_header_t *out_header, hace_cdr_buffer_t *out_body);

#ifdef __cplusplus
}
#endif

#endif /* HACE_GIOP_H */
