/*
 * CORBA object references (IOR), per OMG CORBA Core specification
 * formal/21-02-02 Sec 9 (see docs/standards/spec-references.md),
 * carrying a UIOP profile (host, UDP port, object key) instead of an
 * IIOP one. See docs/architecture/adr-0004-corba-core.md.
 *
 * object_to_string/string_to_object use a corbaloc-style "uiop:"
 * scheme (`uiop:host:port/hex-object-key`) rather than the classic
 * hex-dump-of-the-whole-IOR "IOR:" convention real ORBs use for their
 * object_to_string -- simpler to read and test, and this project has
 * no interop requirement with ORBs expecting the "IOR:" form.
 *
 * object_to_string/string_to_object are hand-rolled (small IPv4
 * dotted-decimal and hex encode/decode helpers) rather than built on
 * snprintf/sscanf: many avionics coding standards restrict or prohibit
 * the C stdio/variadic printf family in flight code, and this project
 * has avoided libc dependencies beyond memcpy/strlen/strcmp throughout.
 *
 * Design constraints (same as cdr.h/giop.h/uiop.h): no dynamic
 * allocation, no exceptions (every function returns a
 * hace_corba_status_t), bounded.
 */
#ifndef HACE_CORBA_IOR_H
#define HACE_CORBA_IOR_H

#include "hace/cdr.h"
#include "hace/giop.h"
#include "hace/corba_exception.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t host_ipv4;
    uint16_t port;
    uint8_t object_key[HACE_GIOP_MAX_OBJECT_KEY];
    size_t object_key_length;
} hace_corba_ior_t;

/* "uiop:" (5) + max IPv4 "255.255.255.255" (15) + ":" (1) + max port
 * "65535" (5) + "/" (1) + hex object key (2 * HACE_GIOP_MAX_OBJECT_KEY)
 * + NUL (1), rounded up with a little margin. */
#define HACE_CORBA_MAX_IOR_STRING ((size_t)96)

/* Plain CDR: host_ipv4 (u32), port (u16), object_key (sequence<octet>,
 * via hace_cdr_encode_octet_seq/decode_octet_seq -- no new CDR
 * primitive needed). */
hace_corba_status_t hace_corba_ior_encode(hace_cdr_buffer_t *buf,
                                           const hace_corba_ior_t *ior);
hace_corba_status_t hace_corba_ior_decode(hace_cdr_buffer_t *buf,
                                           hace_corba_ior_t *out_ior);

/* Writes "uiop:a.b.c.d:port/hexkey\0" into out_str[0..out_capacity).
 * Rejects (HACE_CORBA_ERR_INVALID_ARGUMENT) rather than truncating if
 * it doesn't fit. */
hace_corba_status_t hace_corba_object_to_string(const hace_corba_ior_t *ior,
                                                 char *out_str,
                                                 size_t out_capacity);

/* Parses a string in exactly the format hace_corba_object_to_string
 * produces. Rejects anything malformed (wrong scheme, bad IPv4/port,
 * odd-length or non-hex key, key longer than HACE_GIOP_MAX_OBJECT_KEY)
 * with HACE_CORBA_ERR_INVALID_ARGUMENT rather than guessing. */
hace_corba_status_t hace_corba_string_to_object(const char *str,
                                                 hace_corba_ior_t *out_ior);

#ifdef __cplusplus
}
#endif

#endif /* HACE_CORBA_IOR_H */
