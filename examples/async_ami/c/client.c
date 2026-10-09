/*
 * async_ami client: sends a twoway Worker.compute(input) request via
 * hace_uiop_send_message directly (not the blocking hace_uiop_call),
 * then polls for the reply with hace_uiop_receive on a short timeout
 * in a loop, doing other visible work between polls -- the Poller-
 * style asynchronous invocation pattern, well suited to UDP's
 * non-blocking send (see docs/roadmap.md Phase 5). See ../client.idl
 * for what this client calls.
 */
#include "../../common/c/example_common.h"
#include "hace/corba_exception.h"
#include "hace/uiop.h"

#include <stdio.h>
#include <string.h>

#define ASYNC_AMI_PORT 59121U
#define POLL_INTERVAL_MS 50U
#define MAX_POLLS 40U /* 40 * 50ms = 2s, comfortably more than the server's ~300ms */

static const uint8_t WORK_OBJECT_KEY[] = {(uint8_t)'w', (uint8_t)'o', (uint8_t)'r',
                                           (uint8_t)'k'};

static int build_compute_request(hace_cdr_buffer_t *buf, uint32_t request_id,
                                   int32_t input, const uint8_t *object_key,
                                   size_t object_key_length)
{
    hace_giop_request_header_t header;

    memset(&header, 0, sizeof(header));
    header.request_id = request_id;
    header.response_expected = 1;
    (void)memcpy(header.object_key, object_key, object_key_length);
    header.object_key_length = object_key_length;
    (void)strncpy(header.operation, "compute", sizeof(header.operation) - 1U);

    if (hace_giop_begin_message(buf, HACE_CDR_BIG_ENDIAN) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_giop_encode_request_header(buf, &header) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_cdr_encode_i32(buf, input) != HACE_CDR_OK) {
        return 0;
    }
    if (hace_giop_finish_message(buf, HACE_GIOP_MSG_REQUEST, 0) != HACE_GIOP_OK) {
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    hace_corba_ior_t server_ior;
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_uiop_transport_t transport;
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t reply_body[256];
    hace_giop_message_header_t reply_header;
    uint32_t reply_request_id = 0U;
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status = HACE_UIOP_ERR_TIMEOUT;
    const uint32_t request_id = 1U;
    const int32_t input = 6;
    uint32_t attempt;

    (void)hace_example_parse_ior_arg(argc, argv, 1, ASYNC_AMI_PORT, WORK_OBJECT_KEY,
                                      sizeof(WORK_OBJECT_KEY), &server_ior);

    if (hace_hal_socket_open(&client_socket, 0U) != HACE_HAL_OK) {
        (void)fprintf(stderr, "async_ami client: could not open UDP socket\n");
        return 1;
    }
    transport = hace_uiop_transport_for_socket(&client_socket);

    if ((hace_cdr_buffer_init(&request_buf, request_storage, sizeof(request_storage),
                               HACE_CDR_BIG_ENDIAN) != HACE_CDR_OK) ||
        !build_compute_request(&request_buf, request_id, input, server_ior.object_key,
                                server_ior.object_key_length)) {
        (void)fprintf(stderr, "async_ami client: could not encode request\n");
        (void)hace_hal_socket_close(client_socket);
        return 1;
    }

    if (hace_uiop_send_message(&transport, &request_buf, HACE_GIOP_MSG_REQUEST,
                                request_id, server_ior.host_ipv4, server_ior.port,
                                HACE_UIOP_MAX_DATAGRAM_SIZE) != HACE_UIOP_OK) {
        (void)fprintf(stderr, "async_ami client: could not send request\n");
        (void)hace_hal_socket_close(client_socket);
        return 1;
    }
    (void)printf("client: sent compute(%d), doing other work while waiting...\n", input);

    for (attempt = 0U; attempt < MAX_POLLS; attempt++) {
        status = hace_uiop_receive(&transport, HACE_GIOP_MSG_REPLY, &request_id,
                                    POLL_INTERVAL_MS, reply_body, sizeof(reply_body),
                                    &reply_header, &reply_request_id, &reply_status,
                                    &reply_body_length, NULL, NULL, NULL);
        if (status == HACE_UIOP_OK) {
            break;
        }
        if (status != HACE_UIOP_ERR_TIMEOUT) {
            (void)fprintf(stderr, "async_ami client: poll error (%d)\n", (int)status);
            (void)hace_hal_socket_close(client_socket);
            return 1;
        }
        (void)printf("client: ...still working on other things (poll %u)...\n",
                      attempt + 1U);
    }

    if (status != HACE_UIOP_OK) {
        (void)fprintf(stderr, "async_ami client: never got a reply\n");
        (void)hace_hal_socket_close(client_socket);
        return 1;
    }

    if (reply_status != HACE_GIOP_REPLY_NO_EXCEPTION) {
        hace_cdr_buffer_t exc_buf;
        hace_corba_system_exception_t exc;

        (void)hace_cdr_buffer_init(&exc_buf, reply_body, reply_body_length,
                                    HACE_CDR_BIG_ENDIAN);
        (void)hace_corba_decode_system_exception(&exc_buf, &exc);
        (void)fprintf(stderr, "async_ami client: server raised exception %d\n",
                       (int)exc.exception);
        (void)hace_hal_socket_close(client_socket);
        return 1;
    }

    {
        hace_cdr_buffer_t result_buf;
        int32_t result = 0;

        (void)hace_cdr_buffer_init(&result_buf, reply_body, reply_body_length,
                                    HACE_CDR_BIG_ENDIAN);
        (void)hace_cdr_decode_i32(&result_buf, &result);
        (void)printf("client: reply arrived -- compute(%d) = %d\n", input, result);
    }

    (void)hace_hal_socket_close(client_socket);
    return 0;
}
