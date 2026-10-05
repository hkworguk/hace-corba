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

hace_cdr_status_t hace_cdr_buffer_set_byte_order(hace_cdr_buffer_t *buf,
                                                  hace_cdr_byte_order_t order)
{
    if (buf == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if ((order != HACE_CDR_BIG_ENDIAN) && (order != HACE_CDR_LITTLE_ENDIAN)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    buf->byte_order = order;
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_buffer_init_view(hace_cdr_buffer_t *view,
                                             hace_cdr_buffer_t *parent,
                                             size_t length)
{
    if ((view == NULL) || (parent == NULL)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if (length > (parent->capacity - parent->position)) {
        return HACE_CDR_ERR_BUFFER_OVERFLOW;
    }

    view->data = &parent->data[parent->position];
    view->capacity = length;
    view->position = 0U;
    view->byte_order = parent->byte_order;

    parent->position += length;
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

/* ---- Strings, sequences ---- */

hace_cdr_status_t hace_cdr_encode_string(hace_cdr_buffer_t *buf,
                                          const char *str, size_t max_length)
{
    size_t length;
    hace_cdr_status_t status;

    if ((buf == NULL) || (str == NULL)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    length = strlen(str);
    if (length >= max_length) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    /* length + 1 for the NUL terminator, which CDR strings always
     * include on the wire. */
    status = hace_cdr_encode_u32(buf, (uint32_t)(length + 1U));
    if (status != HACE_CDR_OK) {
        return status;
    }

    if ((buf->position + length + 1U) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_OVERFLOW;
    }
    (void)memcpy(&buf->data[buf->position], str, length);
    buf->data[buf->position + length] = 0U;
    buf->position += length + 1U;
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_decode_string(hace_cdr_buffer_t *buf, char *out,
                                          size_t out_capacity,
                                          size_t *out_length)
{
    uint32_t wire_length;
    hace_cdr_status_t status;

    if ((buf == NULL) || (out == NULL) || (out_capacity == 0U)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_u32(buf, &wire_length);
    if (status != HACE_CDR_OK) {
        return status;
    }
    if ((wire_length == 0U) || ((size_t)wire_length > out_capacity)) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if ((buf->position + wire_length) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_UNDERFLOW;
    }

    (void)memcpy(out, &buf->data[buf->position], wire_length);
    buf->position += wire_length;

    /* wire_length includes the NUL; report the length excluding it. */
    if (out_length != NULL) {
        *out_length = (size_t)wire_length - 1U;
    }
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_encode_sequence_length(hace_cdr_buffer_t *buf,
                                                   size_t count)
{
    return hace_cdr_encode_u32(buf, (uint32_t)count);
}

hace_cdr_status_t hace_cdr_decode_sequence_length(hace_cdr_buffer_t *buf,
                                                   size_t max_count,
                                                   size_t *out_count)
{
    uint32_t wire_count;
    hace_cdr_status_t status;

    if (out_count == NULL) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_u32(buf, &wire_count);
    if (status != HACE_CDR_OK) {
        return status;
    }
    if ((size_t)wire_count > max_count) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    *out_count = (size_t)wire_count;
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_encode_octet_seq(hace_cdr_buffer_t *buf,
                                             const uint8_t *data,
                                             size_t count)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || ((data == NULL) && (count > 0U))) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_encode_sequence_length(buf, count);
    if (status != HACE_CDR_OK) {
        return status;
    }
    if ((buf->position + count) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_OVERFLOW;
    }
    if (count > 0U) {
        (void)memcpy(&buf->data[buf->position], data, count);
    }
    buf->position += count;
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_decode_octet_seq(hace_cdr_buffer_t *buf,
                                             uint8_t *out, size_t max_count,
                                             size_t *out_count)
{
    size_t count;
    hace_cdr_status_t status;

    if ((buf == NULL) || ((out == NULL) && (max_count > 0U))) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_sequence_length(buf, max_count, &count);
    if (status != HACE_CDR_OK) {
        return status;
    }
    if ((buf->position + count) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_UNDERFLOW;
    }
    if (count > 0U) {
        (void)memcpy(out, &buf->data[buf->position], count);
    }
    buf->position += count;

    if (out_count != NULL) {
        *out_count = count;
    }
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_encode_raw_bytes(hace_cdr_buffer_t *buf,
                                             const uint8_t *data, size_t length)
{
    if ((buf == NULL) || ((data == NULL) && (length > 0U))) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if ((buf->position + length) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_OVERFLOW;
    }
    if (length > 0U) {
        (void)memcpy(&buf->data[buf->position], data, length);
    }
    buf->position += length;
    return HACE_CDR_OK;
}

hace_cdr_status_t hace_cdr_decode_raw_bytes(hace_cdr_buffer_t *buf,
                                             uint8_t *out, size_t length)
{
    if ((buf == NULL) || ((out == NULL) && (length > 0U))) {
        return HACE_CDR_ERR_INVALID_ARGUMENT;
    }
    if ((buf->position + length) > buf->capacity) {
        return HACE_CDR_ERR_BUFFER_UNDERFLOW;
    }
    if (length > 0U) {
        (void)memcpy(out, &buf->data[buf->position], length);
    }
    buf->position += length;
    return HACE_CDR_OK;
}
