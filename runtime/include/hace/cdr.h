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

#ifdef __cplusplus
}
#endif

#endif /* HACE_CDR_H */
