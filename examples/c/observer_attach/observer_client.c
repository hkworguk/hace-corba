/*
 * observer_attach observer: runs its own POA on its own socket (object
 * key "obsv") so it can receive callbacks, calls Subject.attach(self)
 * on the subject via a real twoway hace_uiop_call (passing its own IOR,
 * CDR-encoded, as the request argument), then dispatches incoming
 * oneway Observer.update() calls on that same local POA -- the same
 * mechanism the subject uses to serve attach(), just acting as client
 * for that one call and server for the rest. See observer.idl.
 */
#include "../common/example_common.h"
#include "hace/corba_poa.h"

#include <stdio.h>
#include <string.h>

#define OBSERVER_PORT 59132U
#define SUBJECT_PORT 59131U
#define EXPECTED_UPDATES 3

static const uint8_t SUBJECT_OBJECT_KEY[] = {(uint8_t)'s', (uint8_t)'u', (uint8_t)'b',
                                              (uint8_t)'j'};
static const uint8_t OBSERVER_OBJECT_KEY[] = {(uint8_t)'o', (uint8_t)'b', (uint8_t)'s',
                                               (uint8_t)'v'};

static void update_servant(void *context, const char *operation,
                            hace_cdr_buffer_t *args, hace_cdr_buffer_t *reply,
                            hace_corba_system_exception_t *out_exception)
{
    int *received_count = (int *)context;
    char event[128];
    size_t event_length = 0U;

    (void)reply;

    if (strcmp(operation, "update") != 0) {
        out_exception->exception = HACE_CORBA_EXC_BAD_OPERATION;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }
    if (hace_cdr_decode_string(args, event, sizeof(event), &event_length) !=
        HACE_CDR_OK) {
        out_exception->exception = HACE_CORBA_EXC_MARSHAL;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }

    (void)printf("observer: received update: %s\n", event);
    (void)fflush(stdout);
    (*received_count)++;
}

static int build_attach_request(hace_cdr_buffer_t *buf, uint32_t request_id,
                                   const hace_corba_ior_t *self_ior,
                                   const uint8_t *subject_key,
                                   size_t subject_key_length)
{
    hace_giop_request_header_t header;

    memset(&header, 0, sizeof(header));
    header.request_id = request_id;
    header.response_expected = 1;
    (void)memcpy(header.object_key, subject_key, subject_key_length);
    header.object_key_length = subject_key_length;
    (void)strncpy(header.operation, "attach", sizeof(header.operation) - 1U);

    if (hace_giop_begin_message(buf, HACE_CDR_BIG_ENDIAN) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_giop_encode_request_header(buf, &header) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_corba_ior_encode(buf, self_ior) != HACE_CORBA_OK) {
        return 0;
    }
    if (hace_giop_finish_message(buf, HACE_GIOP_MSG_REQUEST, 0) != HACE_GIOP_OK) {
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    hace_corba_ior_t subject_ior;
    hace_hal_socket_handle_t observer_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_ior_t self_ior;
    hace_corba_poa_t poa;
    hace_uiop_transport_t transport;
    int received_count = 0;

    (void)hace_example_parse_ior_arg(argc, argv, 1, SUBJECT_PORT, SUBJECT_OBJECT_KEY,
                                      sizeof(SUBJECT_OBJECT_KEY), &subject_ior);

    /* The observer is itself a tiny server: it needs a known, stable
     * address for the subject to call update() back on. */
    if (!hace_example_server_start(&observer_socket, &self_ior,
                                    "observer_attach observer", OBSERVER_PORT,
                                    OBSERVER_OBJECT_KEY, sizeof(OBSERVER_OBJECT_KEY))) {
        return 1;
    }

    if ((hace_corba_poa_init(&poa) != HACE_CORBA_OK) ||
        (hace_corba_poa_activate_object(&poa, OBSERVER_OBJECT_KEY,
                                         sizeof(OBSERVER_OBJECT_KEY), update_servant,
                                         &received_count) != HACE_CORBA_OK)) {
        (void)fprintf(stderr, "observer: poa setup failed\n");
        (void)hace_hal_socket_close(observer_socket);
        return 1;
    }

    transport = hace_uiop_transport_for_socket(&observer_socket);

    {
        uint8_t request_storage[256];
        hace_cdr_buffer_t request_buf;
        uint8_t reply_body[256];
        hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
        size_t reply_body_length = 0U;
        hace_uiop_status_t status;

        if ((hace_cdr_buffer_init(&request_buf, request_storage,
                                   sizeof(request_storage),
                                   HACE_CDR_BIG_ENDIAN) != HACE_CDR_OK) ||
            !build_attach_request(&request_buf, 1U, &self_ior,
                                   subject_ior.object_key,
                                   subject_ior.object_key_length)) {
            (void)fprintf(stderr, "observer: could not encode attach request\n");
            (void)hace_hal_socket_close(observer_socket);
            return 1;
        }

        status = hace_uiop_call(&transport, &request_buf, 1U, subject_ior.host_ipv4,
                                 subject_ior.port, HACE_UIOP_MAX_DATAGRAM_SIZE, 1000U,
                                 3U, reply_body, sizeof(reply_body), &reply_status,
                                 &reply_body_length);
        if (status != HACE_UIOP_OK) {
            (void)fprintf(stderr, "observer: attach call failed (%d)\n", (int)status);
            (void)hace_hal_socket_close(observer_socket);
            return 1;
        }
        if (reply_status != HACE_GIOP_REPLY_NO_EXCEPTION) {
            (void)fprintf(stderr, "observer: attach was rejected by the subject\n");
            (void)hace_hal_socket_close(observer_socket);
            return 1;
        }
        (void)printf("observer: attached to subject\n");
        (void)fflush(stdout);
    }

    while (received_count < EXPECTED_UPDATES) {
        hace_uiop_status_t status = hace_corba_poa_handle_one_request(
            &poa, &transport, 5000U, HACE_UIOP_MAX_DATAGRAM_SIZE);
        if (status == HACE_UIOP_ERR_TIMEOUT) {
            (void)printf("observer: no update received within 5s, exiting\n");
            break;
        }
        if (status != HACE_UIOP_OK) {
            (void)fprintf(stderr, "observer: dispatch error (%d)\n", (int)status);
            break;
        }
    }

    (void)hace_hal_socket_close(observer_socket);
    return 0;
}
