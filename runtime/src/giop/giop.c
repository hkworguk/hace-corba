#include "hace/giop.h"

static hace_giop_status_t hace_giop_from_cdr_status(hace_cdr_status_t status)
{
    switch (status) {
        case HACE_CDR_OK:
            return HACE_GIOP_OK;
        case HACE_CDR_ERR_BUFFER_OVERFLOW:
            return HACE_GIOP_ERR_BUFFER_OVERFLOW;
        case HACE_CDR_ERR_BUFFER_UNDERFLOW:
            return HACE_GIOP_ERR_BUFFER_UNDERFLOW;
        case HACE_CDR_ERR_INVALID_ARGUMENT:
            return HACE_GIOP_ERR_INVALID_ARGUMENT;
        default:
            return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
}

/* ---- Generic message framing ---- */

hace_giop_status_t hace_giop_begin_message(hace_cdr_buffer_t *buf,
                                            hace_cdr_byte_order_t byte_order)
{
    hace_cdr_status_t status;

    if (buf == NULL) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_buffer_set_byte_order(buf, byte_order);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    /* Reserve the 12-byte header + 4-byte pad; hace_giop_finish_message
     * writes every byte of this region later, so there's no need to
     * zero it now. */
    status = hace_cdr_buffer_set_position(buf, 16U);
    return hace_giop_from_cdr_status(status);
}

hace_giop_status_t hace_giop_finish_message(hace_cdr_buffer_t *buf,
                                             hace_giop_message_type_t message_type,
                                             int more_fragments)
{
    size_t end_position;
    size_t body_length;
    hace_cdr_byte_order_t order;
    uint8_t flags;
    hace_cdr_status_t status;

    if (buf == NULL) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }

    end_position = hace_cdr_buffer_position(buf);
    if (end_position < 16U) {
        /* hace_giop_begin_message wasn't called, or something rewound
         * past the reserved prefix. */
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    body_length = end_position - 16U;
    order = buf->byte_order;

    status = hace_cdr_buffer_set_position(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_encode_octet(buf, (uint8_t)'G');
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, (uint8_t)'I');
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, (uint8_t)'O');
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, (uint8_t)'P');
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_encode_octet(buf, 1U); /* version major */
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, 2U); /* version minor */
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    flags = (order == HACE_CDR_LITTLE_ENDIAN) ? 0x01U : 0x00U;
    if (more_fragments != 0) {
        flags |= 0x02U;
    }
    status = hace_cdr_encode_octet(buf, flags);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_encode_octet(buf, (uint8_t)message_type);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_encode_u32(buf, (uint32_t)body_length);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    /* 4 bytes of padding so the body (already encoded, starting at 16)
     * is 8-byte aligned from the start of the message. */
    status = hace_cdr_encode_octet(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_buffer_set_position(buf, end_position);
    return hace_giop_from_cdr_status(status);
}

hace_giop_status_t hace_giop_decode_message_header(hace_cdr_buffer_t *buf,
                                                    hace_giop_message_header_t *out_header)
{
    uint8_t magic0;
    uint8_t magic1;
    uint8_t magic2;
    uint8_t magic3;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t flags;
    uint8_t message_type_raw;
    uint32_t message_size;
    uint8_t pad;
    hace_cdr_status_t status;
    hace_cdr_byte_order_t order;

    if ((buf == NULL) || (out_header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_octet(buf, &magic0);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &magic1);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &magic2);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &magic3);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if ((magic0 != (uint8_t)'G') || (magic1 != (uint8_t)'I') ||
        (magic2 != (uint8_t)'O') || (magic3 != (uint8_t)'P')) {
        return HACE_GIOP_ERR_BAD_MAGIC;
    }

    status = hace_cdr_decode_octet(buf, &version_major);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &version_minor);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if ((version_major != 1U) || (version_minor != 2U)) {
        return HACE_GIOP_ERR_UNSUPPORTED_VERSION;
    }

    status = hace_cdr_decode_octet(buf, &flags);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    order = ((flags & 0x01U) != 0U) ? HACE_CDR_LITTLE_ENDIAN : HACE_CDR_BIG_ENDIAN;
    status = hace_cdr_buffer_set_byte_order(buf, order);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_octet(buf, &message_type_raw);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if (message_type_raw > (uint8_t)HACE_GIOP_MSG_FRAGMENT) {
        return HACE_GIOP_ERR_UNSUPPORTED_MESSAGE_TYPE;
    }

    status = hace_cdr_decode_u32(buf, &message_size);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_octet(buf, &pad);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &pad);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &pad);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &pad);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    out_header->version_major = version_major;
    out_header->version_minor = version_minor;
    out_header->byte_order = order;
    out_header->more_fragments = ((flags & 0x02U) != 0U) ? 1 : 0;
    out_header->message_type = (hace_giop_message_type_t)message_type_raw;
    out_header->message_size = message_size;
    return HACE_GIOP_OK;
}

/* ---- Request ---- */

hace_giop_status_t hace_giop_encode_request_header(hace_cdr_buffer_t *buf,
                                                    const hace_giop_request_header_t *header)
{
    hace_cdr_status_t status;
    uint8_t response_flags;

    if ((buf == NULL) || (header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    if (header->object_key_length > HACE_GIOP_MAX_OBJECT_KEY) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_encode_u32(buf, header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    response_flags = (header->response_expected != 0) ? 3U : 0U;
    status = hace_cdr_encode_octet(buf, response_flags);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    /* TargetAddress: discriminant 0 = KeyAddr, the only addressing
     * disposition this profile supports. */
    status = hace_cdr_encode_u16(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet_seq(buf, header->object_key,
                                        header->object_key_length);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_encode_string(buf, header->operation,
                                     HACE_GIOP_MAX_OPERATION_NAME);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    /* service_context: always empty in this profile. */
    status = hace_cdr_encode_sequence_length(buf, 0U);
    return hace_giop_from_cdr_status(status);
}

hace_giop_status_t hace_giop_decode_request_header(hace_cdr_buffer_t *buf,
                                                    size_t message_size,
                                                    hace_giop_request_header_t *out_header,
                                                    hace_cdr_buffer_t *out_body)
{
    size_t body_start;
    size_t consumed;
    size_t service_context_count;
    uint8_t response_flags;
    uint8_t reserved;
    uint16_t target_discriminant;
    hace_cdr_status_t status;

    if ((buf == NULL) || (out_header == NULL) || (out_body == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    body_start = hace_cdr_buffer_position(buf);

    status = hace_cdr_decode_u32(buf, &out_header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_octet(buf, &response_flags);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if (response_flags == 0U) {
        out_header->response_expected = 0;
    } else if (response_flags == 3U) {
        out_header->response_expected = 1;
    } else {
        return HACE_GIOP_ERR_UNSUPPORTED_SYNC_SCOPE;
    }

    status = hace_cdr_decode_octet(buf, &reserved);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &reserved);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_decode_octet(buf, &reserved);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_u16(buf, &target_discriminant);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if (target_discriminant != 0U) {
        return HACE_GIOP_ERR_UNSUPPORTED_ADDRESSING_DISPOSITION;
    }
    status = hace_cdr_decode_octet_seq(buf, out_header->object_key,
                                        HACE_GIOP_MAX_OBJECT_KEY,
                                        &out_header->object_key_length);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_string(buf, out_header->operation,
                                     HACE_GIOP_MAX_OPERATION_NAME, NULL);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_sequence_length(buf, (size_t)UINT32_MAX,
                                              &service_context_count);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if (service_context_count != 0U) {
        return HACE_GIOP_ERR_NONEMPTY_SERVICE_CONTEXT;
    }

    consumed = hace_cdr_buffer_position(buf) - body_start;
    if (consumed > message_size) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    status = hace_cdr_buffer_init_view(out_body, buf, message_size - consumed);
    return hace_giop_from_cdr_status(status);
}

/* ---- Reply ---- */

hace_giop_status_t hace_giop_encode_reply_header(hace_cdr_buffer_t *buf,
                                                  const hace_giop_reply_header_t *header)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }

    /* service_context: always empty in this profile. */
    status = hace_cdr_encode_sequence_length(buf, 0U);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_encode_u32(buf, header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_encode_u32(buf, (uint32_t)header->reply_status);
    return hace_giop_from_cdr_status(status);
}

hace_giop_status_t hace_giop_decode_reply_header(hace_cdr_buffer_t *buf,
                                                  size_t message_size,
                                                  hace_giop_reply_header_t *out_header,
                                                  hace_cdr_buffer_t *out_body)
{
    size_t body_start;
    size_t consumed;
    size_t service_context_count;
    uint32_t reply_status_raw;
    hace_cdr_status_t status;

    if ((buf == NULL) || (out_header == NULL) || (out_body == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    body_start = hace_cdr_buffer_position(buf);

    status = hace_cdr_decode_sequence_length(buf, (size_t)UINT32_MAX,
                                              &service_context_count);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if (service_context_count != 0U) {
        return HACE_GIOP_ERR_NONEMPTY_SERVICE_CONTEXT;
    }

    status = hace_cdr_decode_u32(buf, &out_header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_u32(buf, &reply_status_raw);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if (reply_status_raw > (uint32_t)HACE_GIOP_REPLY_NEEDS_ADDRESSING_MODE) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    out_header->reply_status = (hace_giop_reply_status_t)reply_status_raw;

    consumed = hace_cdr_buffer_position(buf) - body_start;
    if (consumed > message_size) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    status = hace_cdr_buffer_init_view(out_body, buf, message_size - consumed);
    return hace_giop_from_cdr_status(status);
}

/* ---- CancelRequest ---- */

hace_giop_status_t hace_giop_encode_cancel_request_header(
    hace_cdr_buffer_t *buf, const hace_giop_cancel_request_header_t *header)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    status = hace_cdr_encode_u32(buf, header->request_id);
    return hace_giop_from_cdr_status(status);
}

hace_giop_status_t hace_giop_decode_cancel_request_header(
    hace_cdr_buffer_t *buf, hace_giop_cancel_request_header_t *out_header)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (out_header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    status = hace_cdr_decode_u32(buf, &out_header->request_id);
    return hace_giop_from_cdr_status(status);
}

/* ---- LocateRequest ---- */

hace_giop_status_t hace_giop_encode_locate_request_header(
    hace_cdr_buffer_t *buf, const hace_giop_locate_request_header_t *header)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    if (header->object_key_length > HACE_GIOP_MAX_OBJECT_KEY) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_encode_u32(buf, header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_encode_u16(buf, 0U); /* TargetAddress: KeyAddr */
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_octet_seq(buf, header->object_key,
                                        header->object_key_length);
    return hace_giop_from_cdr_status(status);
}

hace_giop_status_t hace_giop_decode_locate_request_header(
    hace_cdr_buffer_t *buf, hace_giop_locate_request_header_t *out_header)
{
    uint16_t target_discriminant;
    hace_cdr_status_t status;

    if ((buf == NULL) || (out_header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_u32(buf, &out_header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_u16(buf, &target_discriminant);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if (target_discriminant != 0U) {
        return HACE_GIOP_ERR_UNSUPPORTED_ADDRESSING_DISPOSITION;
    }

    status = hace_cdr_decode_octet_seq(buf, out_header->object_key,
                                        HACE_GIOP_MAX_OBJECT_KEY,
                                        &out_header->object_key_length);
    return hace_giop_from_cdr_status(status);
}

/* ---- LocateReply ---- */

hace_giop_status_t hace_giop_encode_locate_reply_header(
    hace_cdr_buffer_t *buf, const hace_giop_locate_reply_header_t *header)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    status = hace_cdr_encode_u32(buf, header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    status = hace_cdr_encode_u32(buf, (uint32_t)header->locate_status);
    return hace_giop_from_cdr_status(status);
}

hace_giop_status_t hace_giop_decode_locate_reply_header(
    hace_cdr_buffer_t *buf, hace_giop_locate_reply_header_t *out_header)
{
    uint32_t locate_status_raw;
    hace_cdr_status_t status;

    if ((buf == NULL) || (out_header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }

    status = hace_cdr_decode_u32(buf, &out_header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    status = hace_cdr_decode_u32(buf, &locate_status_raw);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }
    if (locate_status_raw > (uint32_t)HACE_GIOP_LOCATE_NEEDS_ADDRESSING_MODE) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    out_header->locate_status = (hace_giop_locate_status_t)locate_status_raw;
    return HACE_GIOP_OK;
}

/* ---- Fragment ---- */

hace_giop_status_t hace_giop_encode_fragment_header(
    hace_cdr_buffer_t *buf, const hace_giop_fragment_header_t *header)
{
    hace_cdr_status_t status;

    if ((buf == NULL) || (header == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    status = hace_cdr_encode_u32(buf, header->request_id);
    return hace_giop_from_cdr_status(status);
}

hace_giop_status_t hace_giop_decode_fragment_header(
    hace_cdr_buffer_t *buf, size_t message_size,
    hace_giop_fragment_header_t *out_header, hace_cdr_buffer_t *out_body)
{
    size_t body_start;
    size_t consumed;
    hace_cdr_status_t status;

    if ((buf == NULL) || (out_header == NULL) || (out_body == NULL)) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    body_start = hace_cdr_buffer_position(buf);

    status = hace_cdr_decode_u32(buf, &out_header->request_id);
    if (status != HACE_CDR_OK) {
        return hace_giop_from_cdr_status(status);
    }

    consumed = hace_cdr_buffer_position(buf) - body_start;
    if (consumed > message_size) {
        return HACE_GIOP_ERR_INVALID_ARGUMENT;
    }
    status = hace_cdr_buffer_init_view(out_body, buf, message_size - consumed);
    return hace_giop_from_cdr_status(status);
}
