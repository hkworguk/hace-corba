#include "hace/uiop.h"

#include <string.h>

/* ---- status mapping ---- */

static hace_uiop_status_t hace_uiop_from_hal_status(hace_hal_status_t status)
{
    return (status == HACE_HAL_OK) ? HACE_UIOP_OK : HACE_UIOP_ERR_HAL;
}

static hace_uiop_status_t hace_uiop_from_cdr_status(hace_cdr_status_t status)
{
    return (status == HACE_CDR_OK) ? HACE_UIOP_OK : HACE_UIOP_ERR_INVALID_ARGUMENT;
}

static hace_uiop_status_t hace_uiop_from_giop_status(hace_giop_status_t status)
{
    return (status == HACE_GIOP_OK) ? HACE_UIOP_OK : HACE_UIOP_ERR_INVALID_ARGUMENT;
}

/* ---- time helpers ---- */

static uint64_t hace_uiop_time_to_ms(const hace_hal_time_t *t)
{
    return (t->seconds * 1000ULL) + ((uint64_t)t->nanoseconds / 1000000ULL);
}

/* Milliseconds remaining until start_ms + timeout_ms, or 0 if that
 * deadline has already passed. */
static uint32_t hace_uiop_remaining_ms(uint64_t start_ms, uint32_t timeout_ms,
                                       uint64_t now_ms)
{
    uint64_t deadline_ms = start_ms + (uint64_t)timeout_ms;
    uint64_t remaining;

    if (now_ms >= deadline_ms) {
        return 0U;
    }
    remaining = deadline_ms - now_ms;
    return (remaining > (uint64_t)UINT32_MAX) ? UINT32_MAX : (uint32_t)remaining;
}

/* ---- production transport ---- */

static hace_hal_status_t hace_uiop_socket_send(void *context, const uint8_t *data,
                                                size_t length, uint32_t dest_addr_ipv4,
                                                uint16_t dest_port)
{
    const hace_hal_socket_handle_t *handle = (const hace_hal_socket_handle_t *)context;
    return hace_hal_socket_send(*handle, data, length, dest_addr_ipv4, dest_port);
}

static hace_hal_status_t hace_uiop_socket_recv(void *context, uint8_t *buffer,
                                                size_t capacity, size_t *out_length,
                                                uint32_t *src_addr_ipv4,
                                                uint16_t *src_port, uint32_t timeout_ms)
{
    const hace_hal_socket_handle_t *handle = (const hace_hal_socket_handle_t *)context;
    return hace_hal_socket_recv(*handle, buffer, capacity, out_length, src_addr_ipv4,
                                 src_port, timeout_ms);
}

hace_uiop_transport_t hace_uiop_transport_for_socket(const hace_hal_socket_handle_t *handle)
{
    hace_uiop_transport_t transport;
    transport.send = hace_uiop_socket_send;
    transport.recv = hace_uiop_socket_recv;
    transport.context = (void *)handle;
    return transport;
}

/* ---- send ---- */

hace_uiop_status_t hace_uiop_send_message(const hace_uiop_transport_t *transport,
                                           hace_cdr_buffer_t *message,
                                           hace_giop_message_type_t message_type,
                                           uint32_t request_id,
                                           uint32_t dest_addr_ipv4, uint16_t dest_port,
                                           size_t max_datagram_size)
{
    size_t total_length;
    size_t body_length;
    hace_hal_status_t hal_status;

    if ((transport == NULL) || (transport->send == NULL) || (message == NULL)) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }
    if ((max_datagram_size < HACE_UIOP_MIN_DATAGRAM_SIZE) ||
        (max_datagram_size > HACE_UIOP_MAX_DATAGRAM_SIZE)) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }

    total_length = hace_cdr_buffer_position(message);
    if (total_length < 16U) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }
    body_length = total_length - 16U;

    if (total_length <= max_datagram_size) {
        hal_status = transport->send(transport->context, message->data, total_length,
                                      dest_addr_ipv4, dest_port);
        return hace_uiop_from_hal_status(hal_status);
    }

    /* Needs fragmenting: the first chunk keeps message_type and all the
     * fixed header fields already encoded in `message` (GIOP never
     * splits those -- see the ADR); only the trailing opaque body may
     * be split across HACE_GIOP_MSG_FRAGMENT continuations. */
    {
        size_t first_chunk_body_len = max_datagram_size - 16U;
        size_t fragment_payload_capacity = max_datagram_size - 16U - 4U;
        size_t remaining_after_first = body_length - first_chunk_body_len;
        size_t fragments_after_first =
            (remaining_after_first + fragment_payload_capacity - 1U) /
            fragment_payload_capacity;
        size_t sent_body;
        size_t first_chunk_total_length;
        hace_cdr_status_t cdr_status;
        hace_giop_status_t giop_status;
        uint8_t fragment_storage[HACE_UIOP_MAX_DATAGRAM_SIZE];

        if ((1U + fragments_after_first) > HACE_UIOP_MAX_FRAGMENTS) {
            return HACE_UIOP_ERR_TOO_LARGE;
        }

        /* Re-stamp the header for the truncated first chunk. */
        cdr_status = hace_cdr_buffer_set_position(message, 16U + first_chunk_body_len);
        if (cdr_status != HACE_CDR_OK) {
            return hace_uiop_from_cdr_status(cdr_status);
        }
        giop_status = hace_giop_finish_message(message, message_type, 1);
        if (giop_status != HACE_GIOP_OK) {
            return hace_uiop_from_giop_status(giop_status);
        }
        first_chunk_total_length = hace_cdr_buffer_position(message);

        hal_status = transport->send(transport->context, message->data,
                                      first_chunk_total_length, dest_addr_ipv4,
                                      dest_port);
        if (hal_status != HACE_HAL_OK) {
            /* Restore message before returning -- see below. */
            (void)hace_cdr_buffer_set_position(message, total_length);
            return hace_uiop_from_hal_status(hal_status);
        }

        sent_body = first_chunk_body_len;
        while (sent_body < body_length) {
            size_t chunk_len = body_length - sent_body;
            int is_last;
            hace_cdr_buffer_t fragment_buf;
            hace_giop_fragment_header_t frag_header;

            if (chunk_len > fragment_payload_capacity) {
                chunk_len = fragment_payload_capacity;
            }
            is_last = ((sent_body + chunk_len) >= body_length) ? 1 : 0;

            cdr_status = hace_cdr_buffer_init(&fragment_buf, fragment_storage,
                                               sizeof(fragment_storage),
                                               message->byte_order);
            if (cdr_status != HACE_CDR_OK) {
                (void)hace_cdr_buffer_set_position(message, total_length);
                return hace_uiop_from_cdr_status(cdr_status);
            }
            giop_status = hace_giop_begin_message(&fragment_buf, message->byte_order);
            if (giop_status != HACE_GIOP_OK) {
                (void)hace_cdr_buffer_set_position(message, total_length);
                return hace_uiop_from_giop_status(giop_status);
            }
            frag_header.request_id = request_id;
            giop_status = hace_giop_encode_fragment_header(&fragment_buf, &frag_header);
            if (giop_status != HACE_GIOP_OK) {
                (void)hace_cdr_buffer_set_position(message, total_length);
                return hace_uiop_from_giop_status(giop_status);
            }
            cdr_status = hace_cdr_encode_raw_bytes(
                &fragment_buf, &message->data[16U + sent_body], chunk_len);
            if (cdr_status != HACE_CDR_OK) {
                (void)hace_cdr_buffer_set_position(message, total_length);
                return hace_uiop_from_cdr_status(cdr_status);
            }
            giop_status = hace_giop_finish_message(&fragment_buf, HACE_GIOP_MSG_FRAGMENT,
                                                     is_last ? 0 : 1);
            if (giop_status != HACE_GIOP_OK) {
                (void)hace_cdr_buffer_set_position(message, total_length);
                return hace_uiop_from_giop_status(giop_status);
            }

            hal_status = transport->send(transport->context, fragment_buf.data,
                                          hace_cdr_buffer_position(&fragment_buf),
                                          dest_addr_ipv4, dest_port);
            if (hal_status != HACE_HAL_OK) {
                (void)hace_cdr_buffer_set_position(message, total_length);
                return hace_uiop_from_hal_status(hal_status);
            }

            sent_body += chunk_len;
        }

        /* Restore message to its original, fully-encoded state so a
         * caller retrying the same call (e.g. hace_uiop_call) can send
         * it again unchanged. */
        cdr_status = hace_cdr_buffer_set_position(message, total_length);
        if (cdr_status != HACE_CDR_OK) {
            return hace_uiop_from_cdr_status(cdr_status);
        }
    }

    return HACE_UIOP_OK;
}

/* ---- receive (with reassembly) ---- */

hace_uiop_status_t hace_uiop_receive(const hace_uiop_transport_t *transport,
                                      hace_giop_message_type_t expected_type,
                                      const uint32_t *match_request_id,
                                      uint32_t timeout_ms, uint8_t *body_storage,
                                      size_t body_capacity,
                                      hace_giop_message_header_t *out_header,
                                      uint32_t *out_request_id,
                                      hace_giop_reply_status_t *out_reply_status,
                                      size_t *out_body_length,
                                      uint32_t *out_src_addr_ipv4,
                                      uint16_t *out_src_port)
{
    hace_hal_time_t start_time;
    uint64_t start_ms;
    uint8_t datagram[HACE_UIOP_MAX_DATAGRAM_SIZE];
    size_t accumulated = 0U;
    size_t fragments_received = 0U;
    int have_first_chunk = 0;
    uint32_t session_request_id = 0U;
    hace_giop_message_header_t first_header;
    hace_giop_reply_status_t first_reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    uint32_t first_src_addr = 0U;
    uint16_t first_src_port = 0U;

    if ((transport == NULL) || (transport->recv == NULL) || (body_storage == NULL) ||
        (out_header == NULL) || (out_request_id == NULL) || (out_body_length == NULL)) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }
    if ((expected_type != HACE_GIOP_MSG_REQUEST) && (expected_type != HACE_GIOP_MSG_REPLY)) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }

    if (hace_hal_time_now(&start_time) != HACE_HAL_OK) {
        return HACE_UIOP_ERR_HAL;
    }
    start_ms = hace_uiop_time_to_ms(&start_time);

    for (;;) {
        hace_hal_time_t now_time;
        uint64_t now_ms;
        uint32_t remaining_ms;
        size_t datagram_length = 0U;
        uint32_t datagram_src_addr = 0U;
        uint16_t datagram_src_port = 0U;
        hace_hal_status_t hal_status;
        hace_cdr_buffer_t datagram_buf;
        hace_giop_message_header_t header;
        hace_giop_status_t giop_status;

        if (hace_hal_time_now(&now_time) != HACE_HAL_OK) {
            return HACE_UIOP_ERR_HAL;
        }
        now_ms = hace_uiop_time_to_ms(&now_time);
        remaining_ms = hace_uiop_remaining_ms(start_ms, timeout_ms, now_ms);
        if (remaining_ms == 0U) {
            return HACE_UIOP_ERR_TIMEOUT;
        }

        hal_status = transport->recv(transport->context, datagram, sizeof(datagram),
                                      &datagram_length, &datagram_src_addr,
                                      &datagram_src_port, remaining_ms);
        if ((hal_status == HACE_HAL_ERR_TIMEOUT) ||
            (hal_status == HACE_HAL_ERR_WOULD_BLOCK)) {
            continue; /* nothing arrived this round; re-check the deadline */
        }
        if (hal_status != HACE_HAL_OK) {
            return HACE_UIOP_ERR_HAL;
        }

        if (hace_cdr_buffer_init(&datagram_buf, datagram, datagram_length,
                                  HACE_CDR_BIG_ENDIAN) != HACE_CDR_OK) {
            continue; /* can't happen with datagram_length <= sizeof(datagram), but
                        * fail safe rather than crash if it somehow did */
        }
        giop_status = hace_giop_decode_message_header(&datagram_buf, &header);
        if (giop_status != HACE_GIOP_OK) {
            continue; /* not a valid GIOP message -- discard as noise */
        }

        if (header.message_type == expected_type) {
            uint32_t this_request_id;
            hace_giop_reply_status_t this_reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
            hace_cdr_buffer_t chunk_view;
            hace_giop_status_t decode_status;
            size_t chunk_len;

            if (expected_type == HACE_GIOP_MSG_REQUEST) {
                hace_giop_request_header_t req_header;
                decode_status = hace_giop_decode_request_header(
                    &datagram_buf, header.message_size, &req_header, &chunk_view);
                this_request_id = req_header.request_id;
            } else {
                hace_giop_reply_header_t reply_header;
                decode_status = hace_giop_decode_reply_header(
                    &datagram_buf, header.message_size, &reply_header, &chunk_view);
                this_request_id = reply_header.request_id;
                this_reply_status = reply_header.reply_status;
            }
            if (decode_status != HACE_GIOP_OK) {
                continue; /* malformed fixed header -- discard as noise */
            }
            if ((match_request_id != NULL) && (this_request_id != *match_request_id)) {
                continue; /* not ours */
            }
            if (have_first_chunk) {
                continue; /* duplicate/retransmitted first chunk; keep waiting
                            * on the reassembly already in progress */
            }

            chunk_len = hace_cdr_buffer_capacity(&chunk_view);
            if (chunk_len > body_capacity) {
                return HACE_UIOP_ERR_TOO_LARGE;
            }
            if (chunk_len > 0U) {
                (void)memcpy(body_storage, chunk_view.data, chunk_len);
            }
            accumulated = chunk_len;
            fragments_received = 1U;
            session_request_id = this_request_id;
            first_header = header;
            first_reply_status = this_reply_status;
            first_src_addr = datagram_src_addr;
            first_src_port = datagram_src_port;

            if (header.more_fragments == 0) {
                *out_header = first_header;
                *out_request_id = session_request_id;
                if (out_reply_status != NULL) {
                    *out_reply_status = first_reply_status;
                }
                *out_body_length = accumulated;
                if (out_src_addr_ipv4 != NULL) {
                    *out_src_addr_ipv4 = first_src_addr;
                }
                if (out_src_port != NULL) {
                    *out_src_port = first_src_port;
                }
                return HACE_UIOP_OK;
            }
            have_first_chunk = 1;
        } else if ((header.message_type == HACE_GIOP_MSG_FRAGMENT) && have_first_chunk) {
            hace_giop_fragment_header_t frag_header;
            hace_cdr_buffer_t chunk_view;
            hace_giop_status_t decode_status;
            size_t chunk_len;

            decode_status = hace_giop_decode_fragment_header(
                &datagram_buf, header.message_size, &frag_header, &chunk_view);
            if (decode_status != HACE_GIOP_OK) {
                continue; /* malformed -- discard as noise */
            }
            if (frag_header.request_id != session_request_id) {
                continue; /* unrelated fragment stream */
            }
            if ((fragments_received + 1U) > HACE_UIOP_MAX_FRAGMENTS) {
                return HACE_UIOP_ERR_TOO_LARGE;
            }

            chunk_len = hace_cdr_buffer_capacity(&chunk_view);
            if ((accumulated + chunk_len) > body_capacity) {
                return HACE_UIOP_ERR_TOO_LARGE;
            }
            if (chunk_len > 0U) {
                (void)memcpy(&body_storage[accumulated], chunk_view.data, chunk_len);
            }
            accumulated += chunk_len;
            fragments_received += 1U;

            if (header.more_fragments == 0) {
                *out_header = first_header;
                *out_request_id = session_request_id;
                if (out_reply_status != NULL) {
                    *out_reply_status = first_reply_status;
                }
                *out_body_length = accumulated;
                if (out_src_addr_ipv4 != NULL) {
                    *out_src_addr_ipv4 = first_src_addr;
                }
                if (out_src_port != NULL) {
                    *out_src_port = first_src_port;
                }
                return HACE_UIOP_OK;
            }
        } else {
            /* Unrelated message type, or a Fragment with no first chunk
             * seen yet -- discard as noise and keep waiting. */
            continue;
        }
    }
}

/* ---- call (twoway, with retry) ---- */

hace_uiop_status_t hace_uiop_call(const hace_uiop_transport_t *transport,
                                   hace_cdr_buffer_t *request, uint32_t request_id,
                                   uint32_t dest_addr_ipv4, uint16_t dest_port,
                                   size_t max_datagram_size,
                                   uint32_t per_attempt_timeout_ms,
                                   uint32_t max_attempts, uint8_t *reply_body_storage,
                                   size_t reply_body_capacity,
                                   hace_giop_reply_status_t *out_reply_status,
                                   size_t *out_reply_body_length)
{
    uint32_t attempt;

    if ((transport == NULL) || (request == NULL) || (max_attempts == 0U)) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }

    for (attempt = 0U; attempt < max_attempts; attempt++) {
        hace_uiop_status_t send_status;
        hace_uiop_status_t recv_status;
        hace_giop_message_header_t reply_header;
        uint32_t reply_request_id;

        send_status = hace_uiop_send_message(transport, request, HACE_GIOP_MSG_REQUEST,
                                              request_id, dest_addr_ipv4, dest_port,
                                              max_datagram_size);
        if (send_status != HACE_UIOP_OK) {
            return send_status;
        }

        recv_status = hace_uiop_receive(transport, HACE_GIOP_MSG_REPLY, &request_id,
                                         per_attempt_timeout_ms, reply_body_storage,
                                         reply_body_capacity, &reply_header,
                                         &reply_request_id, out_reply_status,
                                         out_reply_body_length, NULL, NULL);
        if (recv_status == HACE_UIOP_OK) {
            return HACE_UIOP_OK;
        }
        if (recv_status != HACE_UIOP_ERR_TIMEOUT) {
            return recv_status; /* a non-timeout error propagates immediately */
        }
        /* this attempt timed out -- loop around and resend */
    }

    return HACE_UIOP_ERR_TIMEOUT;
}
