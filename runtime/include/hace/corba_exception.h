/*
 * CORBA system exceptions, per OMG CORBA Core specification
 * formal/21-02-02 Sec 4.8 (see docs/standards/spec-references.md),
 * simplified for this project's closed, non-interop profile (see
 * docs/architecture/adr-0004-corba-core.md): a small, well-chosen
 * subset of the ~34 standard exceptions, and a plain three-field wire
 * encoding instead of the full repository-ID-string convention real
 * GIOP uses for cross-ORB extensibility.
 *
 * Design constraints (same as cdr.h/giop.h/uiop.h): no dynamic
 * allocation, no language-level exceptions (every function returns a
 * hace_corba_status_t), bounded.
 */
#ifndef HACE_CORBA_EXCEPTION_H
#define HACE_CORBA_EXCEPTION_H

#include "hace/cdr.h"
#include "hace/uiop.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HACE_CORBA_OK = 0,
    HACE_CORBA_ERR_INVALID_ARGUMENT = 1,
    HACE_CORBA_ERR_BUFFER_OVERFLOW = 2,
    HACE_CORBA_ERR_BUFFER_UNDERFLOW = 3
} hace_corba_status_t;

/* A small, chosen subset of CORBA's standard system exceptions --
 * enough to describe every failure this runtime can actually produce,
 * not the full ~34-exception catalog. */
typedef enum {
    HACE_CORBA_EXC_NONE = 0, /* not an exception -- NO_EXCEPTION reply */
    HACE_CORBA_EXC_UNKNOWN = 1,
    HACE_CORBA_EXC_BAD_PARAM = 2,
    HACE_CORBA_EXC_NO_MEMORY = 3,
    HACE_CORBA_EXC_OBJECT_NOT_EXIST = 4,
    HACE_CORBA_EXC_BAD_OPERATION = 5,
    HACE_CORBA_EXC_TRANSIENT = 6,
    HACE_CORBA_EXC_TIMEOUT = 7,
    HACE_CORBA_EXC_MARSHAL = 8,
    HACE_CORBA_EXC_COMM_FAILURE = 9
} hace_corba_exception_t;

/* Per the real CORBA SystemException convention: whether the operation
 * is known to have executed, known not to have, or it's unknown --
 * genuinely useful for a caller deciding whether a retry is safe. */
typedef enum {
    HACE_CORBA_COMPLETED_YES = 0,
    HACE_CORBA_COMPLETED_NO = 1,
    HACE_CORBA_COMPLETED_MAYBE = 2
} hace_corba_completion_status_t;

typedef struct {
    hace_corba_exception_t exception;
    uint32_t minor_code; /* vendor/application-specific; 0 if unused */
    hace_corba_completion_status_t completion_status;
} hace_corba_system_exception_t;

/* Wire encoding: three plain u32s (exception, minor_code,
 * completion_status) -- not the repository-ID-string convention real
 * GIOP uses, since this project isn't an interop target (see the ADR). */
hace_corba_status_t hace_corba_encode_system_exception(
    hace_cdr_buffer_t *buf, const hace_corba_system_exception_t *exc);
hace_corba_status_t hace_corba_decode_system_exception(
    hace_cdr_buffer_t *buf, hace_corba_system_exception_t *out_exc);

/* Maps a failed hace_uiop_call/hace_uiop_send_message/hace_uiop_receive
 * status onto a system exception, so client code doesn't have to
 * invent its own mapping: HACE_UIOP_ERR_TIMEOUT -> TIMEOUT/MAYBE (every
 * retry was sent, but whether the last one was ever processed is
 * unknown), HACE_UIOP_ERR_HAL -> COMM_FAILURE/MAYBE, HACE_UIOP_ERR_TOO_LARGE
 * and HACE_UIOP_ERR_INVALID_ARGUMENT -> BAD_PARAM/NO (rejected before
 * anything was sent), HACE_UIOP_OK -> NONE. */
hace_corba_system_exception_t hace_corba_exception_from_uiop_status(
    hace_uiop_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* HACE_CORBA_EXCEPTION_H */
