#include "hace/corba_poa.h"

#include <string.h>

hace_corba_status_t hace_corba_poa_init(hace_corba_poa_t *poa)
{
    size_t i;

    if (poa == NULL) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    poa->count = 0U;
    for (i = 0U; i < HACE_CORBA_POA_MAX_OBJECTS; i++) {
        poa->entries[i].in_use = 0;
        poa->entries[i].object_key_length = 0U;
        poa->entries[i].servant = NULL;
        poa->entries[i].context = NULL;
    }
    return HACE_CORBA_OK;
}

hace_corba_status_t hace_corba_poa_activate_object(
    hace_corba_poa_t *poa, const uint8_t *object_key, size_t object_key_length,
    hace_corba_servant_fn_t servant, void *context)
{
    size_t slot;
    size_t i;

    if ((poa == NULL) || (object_key == NULL) || (servant == NULL)) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }
    if (object_key_length > HACE_GIOP_MAX_OBJECT_KEY) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT;
    }

    slot = HACE_CORBA_POA_MAX_OBJECTS;
    for (i = 0U; i < HACE_CORBA_POA_MAX_OBJECTS; i++) {
        if (!poa->entries[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot == HACE_CORBA_POA_MAX_OBJECTS) {
        return HACE_CORBA_ERR_INVALID_ARGUMENT; /* table full */
    }

    (void)memcpy(poa->entries[slot].object_key, object_key, object_key_length);
    poa->entries[slot].object_key_length = object_key_length;
    poa->entries[slot].servant = servant;
    poa->entries[slot].context = context;
    poa->entries[slot].in_use = 1;
    poa->count++;
    return HACE_CORBA_OK;
}

static const hace_corba_poa_entry_t *hace_corba_poa_find(
    const hace_corba_poa_t *poa, const uint8_t *object_key, size_t object_key_length)
{
    size_t i;

    for (i = 0U; i < HACE_CORBA_POA_MAX_OBJECTS; i++) {
        const hace_corba_poa_entry_t *entry = &poa->entries[i];
        if (entry->in_use && (entry->object_key_length == object_key_length) &&
            (memcmp(entry->object_key, object_key, object_key_length) == 0)) {
            return entry;
        }
    }
    return NULL;
}

hace_uiop_status_t hace_corba_poa_handle_one_request(
    const hace_corba_poa_t *poa, const hace_uiop_transport_t *transport,
    uint32_t timeout_ms, size_t max_datagram_size)
{
    uint8_t request_body[HACE_CORBA_POA_MAX_BODY_SIZE];
    hace_giop_message_header_t header;
    uint32_t request_id = 0U;
    size_t request_body_length = 0U;
    uint32_t src_addr = 0U;
    uint16_t src_port = 0U;
    hace_uiop_request_info_t request_info;
    hace_uiop_status_t uiop_status;
    const hace_corba_poa_entry_t *entry;
    hace_cdr_buffer_t args_view;
    uint8_t reply_storage[HACE_CORBA_POA_MAX_BODY_SIZE];
    hace_cdr_buffer_t reply_buf;
    hace_corba_system_exception_t exception;

    if ((poa == NULL) || (transport == NULL)) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }

    (void)memset(&request_info, 0, sizeof(request_info));
    uiop_status = hace_uiop_receive(transport, HACE_GIOP_MSG_REQUEST, NULL, timeout_ms,
                                     request_body, sizeof(request_body), &header,
                                     &request_id, NULL, &request_body_length, &src_addr,
                                     &src_port, &request_info);
    if (uiop_status != HACE_UIOP_OK) {
        return uiop_status; /* includes the routine HACE_UIOP_ERR_TIMEOUT */
    }

    if (hace_cdr_buffer_init(&args_view, request_body, request_body_length,
                              header.byte_order) != HACE_CDR_OK) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }
    if (hace_cdr_buffer_init(&reply_buf, reply_storage, sizeof(reply_storage),
                              header.byte_order) != HACE_CDR_OK) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }

    (void)memset(&exception, 0, sizeof(exception));
    exception.exception = HACE_CORBA_EXC_NONE;
    exception.completion_status = HACE_CORBA_COMPLETED_YES;

    entry = hace_corba_poa_find(poa, request_info.object_key,
                                 request_info.object_key_length);
    if (entry == NULL) {
        exception.exception = HACE_CORBA_EXC_OBJECT_NOT_EXIST;
        exception.completion_status = HACE_CORBA_COMPLETED_NO;
    } else {
        entry->servant(entry->context, request_info.operation, &args_view, &reply_buf,
                        &exception);
    }

    if (request_info.response_expected == 0) {
        /* Oneway: no reply, even if the servant raised an exception --
         * matches UIOP's existing oneway semantics (see docs/roadmap.md). */
        return HACE_UIOP_OK;
    }

    {
        hace_cdr_buffer_t message_buf;
        uint8_t message_storage[HACE_CORBA_POA_MAX_BODY_SIZE + 64U];
        hace_giop_reply_header_t reply_header;

        if (hace_cdr_buffer_init(&message_buf, message_storage,
                                  sizeof(message_storage),
                                  header.byte_order) != HACE_CDR_OK) {
            return HACE_UIOP_ERR_INVALID_ARGUMENT;
        }
        if (hace_giop_begin_message(&message_buf, header.byte_order) != HACE_GIOP_OK) {
            return HACE_UIOP_ERR_INVALID_ARGUMENT;
        }

        reply_header.request_id = request_id;
        reply_header.reply_status = (exception.exception == HACE_CORBA_EXC_NONE)
                                         ? HACE_GIOP_REPLY_NO_EXCEPTION
                                         : HACE_GIOP_REPLY_SYSTEM_EXCEPTION;
        if (hace_giop_encode_reply_header(&message_buf, &reply_header) != HACE_GIOP_OK) {
            return HACE_UIOP_ERR_INVALID_ARGUMENT;
        }

        if (exception.exception == HACE_CORBA_EXC_NONE) {
            size_t reply_body_len = hace_cdr_buffer_position(&reply_buf);
            if (hace_cdr_encode_raw_bytes(&message_buf, reply_storage, reply_body_len) !=
                HACE_CDR_OK) {
                return HACE_UIOP_ERR_INVALID_ARGUMENT;
            }
        } else {
            if (hace_corba_encode_system_exception(&message_buf, &exception) !=
                HACE_CORBA_OK) {
                return HACE_UIOP_ERR_INVALID_ARGUMENT;
            }
        }

        if (hace_giop_finish_message(&message_buf, HACE_GIOP_MSG_REPLY, 0) !=
            HACE_GIOP_OK) {
            return HACE_UIOP_ERR_INVALID_ARGUMENT;
        }

        return hace_uiop_send_message(transport, &message_buf, HACE_GIOP_MSG_REPLY,
                                       request_id, src_addr, src_port,
                                       max_datagram_size);
    }
}

hace_uiop_status_t hace_corba_poa_run(const hace_corba_poa_t *poa,
                                       const hace_uiop_transport_t *transport,
                                       size_t max_datagram_size,
                                       uint32_t iteration_timeout_ms,
                                       const volatile int *stop_flag)
{
    if ((poa == NULL) || (transport == NULL) || (stop_flag == NULL)) {
        return HACE_UIOP_ERR_INVALID_ARGUMENT;
    }

    while (*stop_flag == 0) {
        hace_uiop_status_t status = hace_corba_poa_handle_one_request(
            poa, transport, iteration_timeout_ms, max_datagram_size);
        if ((status != HACE_UIOP_OK) && (status != HACE_UIOP_ERR_TIMEOUT)) {
            return status;
        }
    }
    return HACE_UIOP_OK;
}
