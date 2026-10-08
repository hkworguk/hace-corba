#include "hace/corba_ior.h"

#include <string.h>

static hace_corba_status_t hace_corba_from_cdr_status(hace_cdr_status_t status)
{
    switch (status) {
        case HACE_CDR_OK:
            return HACE_CORBA_OK;
        case HACE_CDR_ERR_BUFFER_OVERFLOW:
            return HACE_CORBA_ERR_BUFFER_OVERFLOW;
        case HACE_CDR_ERR_BUFFER_UNDERFLOW:
            return HACE_CORBA_ERR_BUFFER_UNDERFLOW;
        case HACE_CDR_ERR_INVALID_ARGUMENT:
            return HACE_CORBA_ERR_INVALID_ARGUMENT;
        default:
            return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
}

/* ---- IOR CDR encode/decode ---- */

hace_corba_status_t hace_corba_ior_encode(hace_cdr_buffer_t *buf,
                                           const hace_corba_ior_t *ior)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (ior == NULL)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    if (ior->object_key_length > HACE_GIOP_MAX_OBJECT_KEY) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_encode_u32(buf, ior->host_ipv4);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }
    status = hace_cdr_encode_u16(buf, ior->port);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet_seq(buf, ior->object_key, ior->object_key_length);
    return hace_corba_from_cdr_status(status);
}

hace_corba_status_t hace_corba_ior_decode(hace_cdr_buffer_t *buf,
                                           hace_corba_ior_t *out_ior)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (out_ior == NULL)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_u32(buf, &out_ior->host_ipv4);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }
    status = hace_cdr_decode_u16(buf, &out_ior->port);
    if (status != HACE_CDR_OK) {
        return hace_corba_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet_seq(buf, out_ior->object_key, HACE_GIOP_MAX_OBJECT_KEY,
                                        &out_ior->object_key_length);
    return hace_corba_from_cdr_status(status);
}

/* ---- hand-rolled decimal/hex formatting (no snprintf/sscanf -- see
 * corba_ior.h) ---- */

static int format_u8_decimal(uint8_t value, char *out, size_t capacity,
                              size_t *out_len)
{
    char digits[3];
    size_t count = 0U;
    uint8_t v = value;
    size_t i;

    if (v == 0U) {
        digits[0] = '0';
        count = 1U;
    } else {
        while (v > 0U) {
            digits[count] = (char)('0' + (v % 10U));
            v = (uint8_t)(v / 10U);
            count++;
        }
    }
    if (count > capacity) {
        return 0;
    }
    for (i = 0U; i < count; i++) {
        out[i] = digits[count - 1U - i];
    }
    *out_len = count;
    return 1;
}

static int parse_u8_decimal(const char *str, size_t len, uint8_t *out_value)
{
    uint32_t value = 0U;
    size_t i;

    if ((len == 0U) || (len > 3U)) {
        return 0;
    }
    for (i = 0U; i < len; i++) {
        char c = str[i];
        if ((c < '0') || (c > '9')) {
            return 0;
        }
        value = (value * 10U) + (uint32_t)(c - '0');
    }
    if (value > 255U) {
        return 0;
    }
    *out_value = (uint8_t)value;
    return 1;
}

static int format_u16_decimal(uint16_t value, char *out, size_t capacity,
                               size_t *out_len)
{
    char digits[5];
    size_t count = 0U;
    uint16_t v = value;
    size_t i;

    if (v == 0U) {
        digits[0] = '0';
        count = 1U;
    } else {
        while (v > 0U) {
            digits[count] = (char)('0' + (v % 10U));
            v = (uint16_t)(v / 10U);
            count++;
        }
    }
    if (count > capacity) {
        return 0;
    }
    for (i = 0U; i < count; i++) {
        out[i] = digits[count - 1U - i];
    }
    *out_len = count;
    return 1;
}

static int parse_u16_decimal(const char *str, size_t len, uint16_t *out_value)
{
    uint32_t value = 0U;
    size_t i;

    if ((len == 0U) || (len > 5U)) {
        return 0;
    }
    for (i = 0U; i < len; i++) {
        char c = str[i];
        if ((c < '0') || (c > '9')) {
            return 0;
        }
        value = (value * 10U) + (uint32_t)(c - '0');
        if (value > 65535U) {
            return 0;
        }
    }
    *out_value = (uint16_t)value;
    return 1;
}

static int format_ipv4(uint32_t addr, char *out, size_t capacity, size_t *out_len)
{
    size_t pos = 0U;
    int octet_index;

    for (octet_index = 3; octet_index >= 0; octet_index--) {
        uint8_t value = (uint8_t)((addr >> (octet_index * 8)) & 0xFFU);
        size_t part_len = 0U;

        if (pos >= capacity) {
            return 0;
        }
        if (!format_u8_decimal(value, &out[pos], capacity - pos, &part_len)) {
            return 0;
        }
        pos += part_len;
        if (octet_index > 0) {
            if (pos >= capacity) {
                return 0;
            }
            out[pos] = '.';
            pos++;
        }
    }
    *out_len = pos;
    return 1;
}

static int parse_ipv4(const char *str, size_t len, uint32_t *out_addr)
{
    size_t part_start = 0U;
    size_t part_index = 0U;
    uint8_t octets[4];
    size_t i;

    for (i = 0U; i <= len; i++) {
        if ((i == len) || (str[i] == '.')) {
            size_t part_len = i - part_start;
            if (part_index >= 4U) {
                return 0;
            }
            if (!parse_u8_decimal(&str[part_start], part_len, &octets[part_index])) {
                return 0;
            }
            part_index++;
            part_start = i + 1U;
        }
    }
    if (part_index != 4U) {
        return 0;
    }
    *out_addr = ((uint32_t)octets[0] << 24) | ((uint32_t)octets[1] << 16) |
                ((uint32_t)octets[2] << 8) | (uint32_t)octets[3];
    return 1;
}

static const char HACE_HEX_DIGITS[] = "0123456789abcdef";

static int hex_encode(const uint8_t *data, size_t length, char *out,
                       size_t capacity, size_t *out_len)
{
    size_t i;

    if ((length * 2U) > capacity) {
        return 0;
    }
    for (i = 0U; i < length; i++) {
        out[i * 2U] = HACE_HEX_DIGITS[(data[i] >> 4) & 0x0FU];
        out[(i * 2U) + 1U] = HACE_HEX_DIGITS[data[i] & 0x0FU];
    }
    *out_len = length * 2U;
    return 1;
}

static int hex_nibble(char c, uint8_t *out_nibble)
{
    if ((c >= '0') && (c <= '9')) {
        *out_nibble = (uint8_t)(c - '0');
        return 1;
    }
    if ((c >= 'a') && (c <= 'f')) {
        *out_nibble = (uint8_t)(10 + (c - 'a'));
        return 1;
    }
    if ((c >= 'A') && (c <= 'F')) {
        *out_nibble = (uint8_t)(10 + (c - 'A'));
        return 1;
    }
    return 0;
}

static int hex_decode(const char *str, size_t len, uint8_t *out,
                       size_t max_count, size_t *out_count)
{
    size_t count;
    size_t i;

    if ((len % 2U) != 0U) {
        return 0;
    }
    count = len / 2U;
    if (count > max_count) {
        return 0;
    }
    for (i = 0U; i < count; i++) {
        uint8_t hi;
        uint8_t lo;
        if (!hex_nibble(str[i * 2U], &hi) || !hex_nibble(str[(i * 2U) + 1U], &lo)) {
            return 0;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    *out_count = count;
    return 1;
}

/* ---- object_to_string / string_to_object ---- */

hace_corba_status_t hace_corba_object_to_string(const hace_corba_ior_t *ior,
                                                 char *out_str, size_t out_capacity)
{
    static const char prefix[] = "uiop:";
    static const size_t prefix_len = sizeof(prefix) - 1U;
    size_t pos = 0U;
    size_t len = 0U;

    if ((ior == NULL) || (out_str == NULL) || (out_capacity == 0U)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    if (ior->object_key_length > HACE_GIOP_MAX_OBJECT_KEY) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    if (prefix_len >= out_capacity) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    (void)memcpy(&out_str[pos], prefix, prefix_len);
    pos += prefix_len;

    if (!format_ipv4(ior->host_ipv4, &out_str[pos], out_capacity - pos, &len)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    pos += len;

    if (pos >= out_capacity) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    out_str[pos] = ':';
    pos++;

    if (!format_u16_decimal(ior->port, &out_str[pos], out_capacity - pos, &len)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    pos += len;

    if (pos >= out_capacity) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    out_str[pos] = '/';
    pos++;

    if (!hex_encode(ior->object_key, ior->object_key_length, &out_str[pos],
                     out_capacity - pos, &len)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    pos += len;

    if (pos >= out_capacity) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    out_str[pos] = '\0';

    return HACE_CORBA_OK;
}

hace_corba_status_t hace_corba_string_to_object(const char *str,
                                                 hace_corba_ior_t *out_ior)
{
    static const char prefix[] = "uiop:";
    static const size_t prefix_len = sizeof(prefix) - 1U;
    size_t total_len;
    size_t pos;
    size_t colon_pos;
    size_t slash_pos;
    uint32_t host = 0U;
    uint16_t port = 0U;
    size_t key_len = 0U;

    if ((str == NULL) || (out_ior == NULL)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    total_len = strlen(str);
    if ((total_len < prefix_len) || (memcmp(str, prefix, prefix_len) != 0)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    pos = prefix_len;

    colon_pos = pos;
    while ((colon_pos < total_len) && (str[colon_pos] != ':')) {
        colon_pos++;
    }
    if (colon_pos >= total_len) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    if (!parse_ipv4(&str[pos], colon_pos - pos, &host)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    pos = colon_pos + 1U;
    slash_pos = pos;
    while ((slash_pos < total_len) && (str[slash_pos] != '/')) {
        slash_pos++;
    }
    if (slash_pos >= total_len) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    if (!parse_u16_decimal(&str[pos], slash_pos - pos, &port)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    pos = slash_pos + 1U;
    if (!hex_decode(&str[pos], total_len - pos, out_ior->object_key,
                     HACE_GIOP_MAX_OBJECT_KEY, &key_len)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    out_ior->host_ipv4 = host;
    out_ior->port = port;
    out_ior->object_key_length = key_len;
    return HACE_CORBA_OK;
}
