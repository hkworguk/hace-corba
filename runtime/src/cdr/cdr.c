#include "hace/cdr.h"

#include <string.h>

static int hace_cdr_host_is_little_endian(void)
{
    static const uint16_t probe = 1U;
    const uint8_t *bytes = (const uint8_t *)&probe;
    return bytes[0] == 1U;
}

static hace_cdr_status_t hace_cdr_align(hace_cdr_buffer_t *buf,
                                         size_t alignment)
{
    size_t remainder = buf->position % alignment;
    size_t pad = (remainder == 0U) ? 0U : (alignment - remainder);

    if ((buf->position + pad) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_OVERFLOW;
    }
    if (pad > 0U) {
        (void)memset(&buf->data[buf->position], 0, pad);
    }
    buf->position += pad;
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_buffer_init(hace_cdr_buffer_t *buf, uint8_t *storage,
                                        size_t capacity,
                                        hace_cdr_byte_order_t byte_order)
{
    if ((buf == NULL) || (storage == NULL)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if ((byte_order != HACE_CDR_BIG_ENDIAN) &&
        (byte_order != HACE_CDR_LITTLE_ENDIAN)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    buf->data = storage;
    buf->capacity = capacity;
    buf->position = 0U;
    buf->byte_order = byte_order;
    return HACE_CDR_OK;
}

/* Encodes `width` bytes of `value` (host byte order) at buf->position,
 * aligned to `width`, swapping to buf->byte_order if that differs from
 * host order. */
static hace_cdr_status_t hace_cdr_encode_bytes(hace_cdr_buffer_t *buf,
                                                const uint8_t *value,
                                                size_t width)
{
    hace_cdr_status_t status;
    size_t i;
    int swap;

    if ((buf == NULL) || (value == NULL)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_align(buf, width);
    if (status != HACE_CDR_OK) {
        return status;
    }
    if ((buf->position + width) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_OVERFLOW;
    }

    swap = (hace_cdr_host_is_little_endian() ? HACE_CDR_LITTLE_ENDIAN
                                              : HACE_CDR_BIG_ENDIAN) !=
           buf->byte_order;

    for (i = 0U; i < width; i++) {
        size_t src_index = swap ? (width - 1U - i) : i;
        buf->data[buf->position + i] = value[src_index];
    }
    buf->position += width;
    return HACE_CDR_OK;
}

static hace_cdr_status_t hace_cdr_decode_bytes(hace_cdr_buffer_t *buf,
                                                uint8_t *out, size_t width)
{
    hace_cdr_status_t status;
    size_t i;
    int swap;

    if ((buf == NULL) || (out == NULL)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_align(buf, width);
    if (status != HACE_CDR_OK) {
        return status;
    }
    if ((buf->position + width) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_UNDERFLOW;
    }

    swap = (hace_cdr_host_is_little_endian() ? HACE_CDR_LITTLE_ENDIAN
                                              : HACE_CDR_BIG_ENDIAN) !=
           buf->byte_order;

    for (i = 0U; i < width; i++) {
        size_t dst_index = swap ? (width - 1U - i) : i;
        out[dst_index] = buf->data[buf->position + i];
    }
    buf->position += width;
    return HACE_CDR_OK;
}

size_t hace_cdr_buffer_sizeof(void)
{
    return sizeof(hace_cdr_buffer_t);
}

size_t hace_cdr_buffer_position(const hace_cdr_buffer_t *buf)
{
    return (buf == NULL) ? 0U : buf->position;
}

size_t hace_cdr_buffer_capacity(const hace_cdr_buffer_t *buf)
{
    return (buf == NULL) ? 0U : buf->capacity;
}

hace_cdr_status_t hace_cdr_buffer_set_position(hace_cdr_buffer_t *buf,
                                                size_t position)
{
    if (buf == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if (position > buf->capacity) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    buf->position = position;
    return HACE_CDR_OK;
}

/* ---- Encode ---- */

hace_cdr_status_t hace_cdr_encode_octet(hace_cdr_buffer_t *buf, uint8_t value)
{
    if (buf == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if ((buf->position + 1U) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_OVERFLOW;
    }
    buf->data[buf->position] = value;
    buf->position += 1U;
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_encode_bool(hace_cdr_buffer_t *buf, int value)
{
    return hace_cdr_encode_octet(buf, (value != 0) ? 1U : 0U);
}

hace_cdr_status_t hace_cdr_encode_u16(hace_cdr_buffer_t *buf, uint16_t value)
{
    return hace_cdr_encode_bytes(buf, (const uint8_t *)&value, sizeof(value));
}

hace_cdr_status_t hace_cdr_encode_i16(hace_cdr_buffer_t *buf, int16_t value)
{
    return hace_cdr_encode_bytes(buf, (const uint8_t *)&value, sizeof(value));
}

hace_cdr_status_t hace_cdr_encode_u32(hace_cdr_buffer_t *buf, uint32_t value)
{
    return hace_cdr_encode_bytes(buf, (const uint8_t *)&value, sizeof(value));
}

hace_cdr_status_t hace_cdr_encode_i32(hace_cdr_buffer_t *buf, int32_t value)
{
    return hace_cdr_encode_bytes(buf, (const uint8_t *)&value, sizeof(value));
}

hace_cdr_status_t hace_cdr_encode_u64(hace_cdr_buffer_t *buf, uint64_t value)
{
    return hace_cdr_encode_bytes(buf, (const uint8_t *)&value, sizeof(value));
}

hace_cdr_status_t hace_cdr_encode_i64(hace_cdr_buffer_t *buf, int64_t value)
{
    return hace_cdr_encode_bytes(buf, (const uint8_t *)&value, sizeof(value));
}

hace_cdr_status_t hace_cdr_encode_f32(hace_cdr_buffer_t *buf, float value)
{
    return hace_cdr_encode_bytes(buf, (const uint8_t *)&value, sizeof(value));
}

hace_cdr_status_t hace_cdr_encode_f64(hace_cdr_buffer_t *buf, double value)
{
    return hace_cdr_encode_bytes(buf, (const uint8_t *)&value, sizeof(value));
}

/* ---- Decode ---- */

hace_cdr_status_t hace_cdr_decode_octet(hace_cdr_buffer_t *buf, uint8_t *out)
{
    if ((buf == NULL) || (out == NULL)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if ((buf->position + 1U) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_UNDERFLOW;
    }
    *out = buf->data[buf->position];
    buf->position += 1U;
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_decode_bool(hace_cdr_buffer_t *buf, int *out)
{
    uint8_t raw;
    hace_cdr_status_t status = hace_cdr_decode_octet(buf, &raw);

    if (status != HACE_CDR_OK) {
        return status;
    }
    if (out != NULL) {
        *out = (raw != 0U) ? 1 : 0;
    }
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_decode_u16(hace_cdr_buffer_t *buf, uint16_t *out)
{
    if (out == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    return hace_cdr_decode_bytes(buf, (uint8_t *)out, sizeof(*out));
}

hace_cdr_status_t hace_cdr_decode_i16(hace_cdr_buffer_t *buf, int16_t *out)
{
    if (out == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    return hace_cdr_decode_bytes(buf, (uint8_t *)out, sizeof(*out));
}

hace_cdr_status_t hace_cdr_decode_u32(hace_cdr_buffer_t *buf, uint32_t *out)
{
    if (out == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    return hace_cdr_decode_bytes(buf, (uint8_t *)out, sizeof(*out));
}

hace_cdr_status_t hace_cdr_decode_i32(hace_cdr_buffer_t *buf, int32_t *out)
{
    if (out == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    return hace_cdr_decode_bytes(buf, (uint8_t *)out, sizeof(*out));
}

hace_cdr_status_t hace_cdr_decode_u64(hace_cdr_buffer_t *buf, uint64_t *out)
{
    if (out == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    return hace_cdr_decode_bytes(buf, (uint8_t *)out, sizeof(*out));
}

hace_cdr_status_t hace_cdr_decode_i64(hace_cdr_buffer_t *buf, int64_t *out)
{
    if (out == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    return hace_cdr_decode_bytes(buf, (uint8_t *)out, sizeof(*out));
}

hace_cdr_status_t hace_cdr_decode_f32(hace_cdr_buffer_t *buf, float *out)
{
    if (out == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    return hace_cdr_decode_bytes(buf, (uint8_t *)out, sizeof(*out));
}

hace_cdr_status_t hace_cdr_decode_f64(hace_cdr_buffer_t *buf, double *out)
{
    if (out == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    return hace_cdr_decode_bytes(buf, (uint8_t *)out, sizeof(*out));
}
