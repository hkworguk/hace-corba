/*
 * oneway client: sends three fire-and-forget Logger.log(message) calls
 * via hace_uiop_send_message directly -- no hace_uiop_call, no wait,
 * no reply. Demonstrates genuine oneway semantics (a real deviation
 * from IIOP's reliable-transport-backed oneway -- see
 * docs/corba-spec-gap-analysis.md Sec 7).
 */
#include "../common/example_common.h"
#include "hace/uiop.h"

#include <stdio.h>
#include <string.h>

#define ONEWAY_PORT 59111U

static const uint8_t LOG_OBJECT_KEY[] = {(uint8_t)'l', (uint8_t)'o', (uint8_t)'g'};

static int build_log_request(hace_cdr_buffer_t *buf, uint32_t request_id,
                              const char *message, const uint8_t *object_key,
                              size_t object_key_length)
{
    hace_giop_request_header_t header;

    memset(&header, 0, sizeof(header));
    header.request_id = request_id;
    header.response_expected = 0; /* oneway */
    (void)memcpy(header.object_key, object_key, object_key_length);
    header.object_key_length = object_key_length;
    (void)strncpy(header.operation, "log", sizeof(header.operation) - 1U);

    if (hace_giop_begin_message(buf, HACE_CDR_BIG_ENDIAN) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_giop_encode_request_header(buf, &header) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_cdr_encode_string(buf, message, 128U) != HACE_CDR_OK) {
        return 0;
    }
    if (hace_giop_finish_message(buf, HACE_GIOP_MSG_REQUEST, 0) != HACE_GIOP_OK) {
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    static const char *const messages[] = {"starting up", "processing batch 1",
                                             "all done"};
    hace_corba_ior_t server_ior;
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_uiop_transport_t transport;
    size_t i;

    (void)hace_example_parse_ior_arg(argc, argv, 1, ONEWAY_PORT, LOG_OBJECT_KEY,
                                      sizeof(LOG_OBJECT_KEY), &server_ior);

    if (hace_hal_socket_open(&client_socket, 0U) != HACE_HAL_OK) {
        (void)fprintf(stderr, "oneway client: could not open UDP socket\n");
        return 1;
    }
    transport = hace_uiop_transport_for_socket(&client_socket);

    for (i = 0U; i < (sizeof(messages) / sizeof(messages[0])); i++) {
        uint8_t request_storage[256];
        hace_cdr_buffer_t request_buf;
        uint32_t request_id = (uint32_t)(i + 1U);

        if ((hace_cdr_buffer_init(&request_buf, request_storage,
                                   sizeof(request_storage),
                                   HACE_CDR_BIG_ENDIAN) != HACE_CDR_OK) ||
            !build_log_request(&request_buf, request_id, messages[i],
                                server_ior.object_key, server_ior.object_key_length)) {
            (void)fprintf(stderr, "oneway client: could not encode message %zu\n", i);
            continue;
        }

        if (hace_uiop_send_message(&transport, &request_buf, HACE_GIOP_MSG_REQUEST,
                                    request_id, server_ior.host_ipv4, server_ior.port,
                                    HACE_UIOP_MAX_DATAGRAM_SIZE) == HACE_UIOP_OK) {
            (void)printf("client: sent log message %zu: \"%s\"\n", i + 1U, messages[i]);
        } else {
            (void)fprintf(stderr, "client: failed to send log message %zu\n", i);
        }
    }

    (void)hace_hal_socket_close(client_socket);
    return 0;
}
