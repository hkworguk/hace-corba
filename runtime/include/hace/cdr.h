/*
 * Common Data Representation (CDR) encode/decode, per OMG CORBA
 * Interoperability specification formal/12-11-14 (see
 * docs/standards/spec-references.md). Primitive types only in this
 * first slice — structs/sequences/unions/strings follow in Phase 1 (see
 * docs/roadmap.md).
 *
 * Design constraints (DAL C-friendly, see docs/architecture/):
 *  - No dynamic allocation: caller supplies the backing storage.
 *  - No exceptions: every function returns a hace_cdr_status_t.
 *  - Bounded: encode/decode never write or read past the supplied
 *    capacity; overflow/underflow are reported, not silently truncated.
 */
#ifndef HACE_CDR_H
#define HACE_CDR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HACE_CDR_OK = 0,
    HACE_CDR_ERR_BUFFER_OVERFLOW = 1,
    HACE_CDR_ERR_BUFFER_UNDERFLOW = 2,
    HACE_CDR_ERR_INVALID_ARGUMENT = 3
} hace_cdr_status_t;

/* CDR encodes a byte-order flag per encapsulation; this project always
 * picks one explicitly rather than relying on host-native default. */
typedef enum {
    HACE_CDR_BIG_ENDIAN = 0,
    HACE_CDR_LITTLE_ENDIAN = 1
} hace_cdr_byte_order_t;

typedef struct {
    uint8_t *data;        /* caller-owned storage, size == capacity */
    size_t capacity;      /* usable size in bytes: encode/decode bound.
                            * For decode, callers pass the exact number
                            * of valid received bytes as capacity. */
    size_t position;      /* current read/write cursor; after encoding,
                            * this is the number of bytes produced. */
    hace_cdr_byte_order_t byte_order;
} hace_cdr_buffer_t;

/* Binds buf to storage[0..capacity). position is reset to 0. byte_order
 * applies to all subsequent encode/decode calls on buf. */
hace_cdr_status_t hace_cdr_buffer_init(hace_cdr_buffer_t *buf, uint8_t *storage,
                                        size_t capacity,
                                        hace_cdr_byte_order_t byte_order);

/* Returns sizeof(hace_cdr_buffer_t). Non-C bindings (Ada95, Rust — see
 * bindings/) that treat hace_cdr_buffer_t as opaque storage rather than
 * mirroring its field layout use this to size that storage and to
 * runtime-verify it's large enough, instead of hard-coding an assumed
 * size. */
size_t hace_cdr_buffer_sizeof(void);

/* Accessors for bindings that treat hace_cdr_buffer_t as opaque (see
 * hace_cdr_buffer_sizeof above) and therefore cannot read/write its
 * fields directly. */
size_t hace_cdr_buffer_position(const hace_cdr_buffer_t *buf);
size_t hace_cdr_buffer_capacity(const hace_cdr_buffer_t *buf);

/* Rewinds/seeks the cursor, e.g. to 0 before decoding what was just
 * encoded. Rejects position > capacity. */
hace_cdr_status_t hace_cdr_buffer_set_position(hace_cdr_buffer_t *buf,
                                                size_t position);

/* Changes buf's active byte order without touching position/capacity/
 * data. Needed to bootstrap formats (like a GIOP message header) that
 * carry their own byte-order flag as the first thing decoded: read that
 * flag as a single order-independent octet, then call this before
 * decoding any subsequent multi-byte field. */
hace_cdr_status_t hace_cdr_buffer_set_byte_order(hace_cdr_buffer_t *buf,
                                                  hace_cdr_byte_order_t order);

/* Binds view to the next `length` bytes of parent (same storage, no
 * copy), starting at parent's current position, and advances parent's
 * position past them. view inherits parent's byte order. Lets a caller
 * hand out a hard-bounded slice of "the rest of this message" instead
 * of a raw pointer+length pair the recipient has to trust itself.
 * Rejects length > parent's remaining bytes (capacity - position). */
hace_cdr_status_t hace_cdr_buffer_init_view(hace_cdr_buffer_t *view,
                                             hace_cdr_buffer_t *parent,
                                             size_t length);

/* ---- Encode ---- */

hace_cdr_status_t hace_cdr_encode_octet(hace_cdr_buffer_t *buf, uint8_t value);
hace_cdr_status_t hace_cdr_encode_bool(hace_cdr_buffer_t *buf, int value);
hace_cdr_status_t hace_cdr_encode_u16(hace_cdr_buffer_t *buf, uint16_t value);
hace_cdr_status_t hace_cdr_encode_i16(hace_cdr_buffer_t *buf, int16_t value);
hace_cdr_status_t hace_cdr_encode_u32(hace_cdr_buffer_t *buf, uint32_t value);
hace_cdr_status_t hace_cdr_encode_i32(hace_cdr_buffer_t *buf, int32_t value);
hace_cdr_status_t hace_cdr_encode_u64(hace_cdr_buffer_t *buf, uint64_t value);
hace_cdr_status_t hace_cdr_encode_i64(hace_cdr_buffer_t *buf, int64_t value);
hace_cdr_status_t hace_cdr_encode_f32(hace_cdr_buffer_t *buf, float value);
hace_cdr_status_t hace_cdr_encode_f64(hace_cdr_buffer_t *buf, double value);

/* ---- Decode ---- */

hace_cdr_status_t hace_cdr_decode_octet(hace_cdr_buffer_t *buf, uint8_t *out);
hace_cdr_status_t hace_cdr_decode_bool(hace_cdr_buffer_t *buf, int *out);
hace_cdr_status_t hace_cdr_decode_u16(hace_cdr_buffer_t *buf, uint16_t *out);
hace_cdr_status_t hace_cdr_decode_i16(hace_cdr_buffer_t *buf, int16_t *out);
hace_cdr_status_t hace_cdr_decode_u32(hace_cdr_buffer_t *buf, uint32_t *out);
hace_cdr_status_t hace_cdr_decode_i32(hace_cdr_buffer_t *buf, int32_t *out);
hace_cdr_status_t hace_cdr_decode_u64(hace_cdr_buffer_t *buf, uint64_t *out);
hace_cdr_status_t hace_cdr_decode_i64(hace_cdr_buffer_t *buf, int64_t *out);
hace_cdr_status_t hace_cdr_decode_f32(hace_cdr_buffer_t *buf, float *out);
hace_cdr_status_t hace_cdr_decode_f64(hace_cdr_buffer_t *buf, double *out);

/* ---- Strings, sequences ----
 *
 * CDR has no generic struct/union/array/enum marshaling primitive: a
 * struct is its members encoded in declared order, a union is its
 * discriminant then whichever member is active, an array is its
 * elements in order, an enum is a u32. See runtime/tests/test_cdr.c for
 * worked examples of each pattern built from the primitives above —
 * there is deliberately no separate API for them.
 *
 * Strings and sequences DO need dedicated functions because of the
 * length-prefix framing per formal/12-11-14 Sec 9.3, and because this
 * runtime never allocates: every bounded operation below takes an
 * explicit max-length/max-count supplied by the caller's fixed storage,
 * and rejects anything that doesn't fit rather than truncating it.
 */

/* Encodes a NUL-terminated C string as a CDR string: a u32 length
 * (character count including the NUL) followed by that many bytes.
 * Rejects strlen(str) >= max_length (i.e. str plus its NUL must fit in
 * max_length bytes) without writing anything. */
hace_cdr_status_t hace_cdr_encode_string(hace_cdr_buffer_t *buf,
                                          const char *str, size_t max_length);

/* Decodes a CDR string into out[0..out_capacity), NUL-terminated.
 * Rejects a decoded length (including NUL) greater than out_capacity.
 * *out_length is set to the string length excluding the NUL. */
hace_cdr_status_t hace_cdr_decode_string(hace_cdr_buffer_t *buf, char *out,
                                          size_t out_capacity,
                                          size_t *out_length);

/* Encodes/decodes just a sequence<T>'s u32 length prefix; the caller
 * loops over the matching hace_cdr_encode_ or hace_cdr_decode_ function
 * for T, count times.
 * Decode rejects a claimed count greater than max_count outright
 * (a malformed or hostile peer's claimed length is never honored or
 * silently truncated). */
hace_cdr_status_t hace_cdr_encode_sequence_length(hace_cdr_buffer_t *buf,
                                                   size_t count);
hace_cdr_status_t hace_cdr_decode_sequence_length(hace_cdr_buffer_t *buf,
                                                   size_t max_count,
                                                   size_t *out_count);

/* Bulk sequence<octet>: length prefix plus the raw bytes in one call
 * (octets need no alignment or byte-swap, so this is just a memcpy
 * after the length prefix). The common case in practice -- GIOP message
 * bodies are sequence<octet>. */
hace_cdr_status_t hace_cdr_encode_octet_seq(hace_cdr_buffer_t *buf,
                                             const uint8_t *data,
                                             size_t count);
hace_cdr_status_t hace_cdr_decode_octet_seq(hace_cdr_buffer_t *buf,
                                             uint8_t *out, size_t max_count,
                                             size_t *out_count);

/* Copies length raw bytes verbatim: no length prefix, no alignment, no
 * byte-swapping (same convention as sequence<octet>'s payload, just
 * without the length prefix). For copying an already-framed blob of
 * known length, such as a GIOP Fragment's continuation bytes, which
 * are appended raw rather than re-wrapped as a sequence<octet>. */
hace_cdr_status_t hace_cdr_encode_raw_bytes(hace_cdr_buffer_t *buf,
                                             const uint8_t *data,
                                             size_t length);
hace_cdr_status_t hace_cdr_decode_raw_bytes(hace_cdr_buffer_t *buf,
                                             uint8_t *out, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* HACE_CDR_H */
