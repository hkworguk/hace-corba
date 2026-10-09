/*
 * rpc_basic client: calls Calculator.add(a, b) via a blocking twoway
 * hace_uiop_call and prints the result. Takes the server's IOR string
 * as an optional argv[1] (as printed by server.c); defaults to the
 * localhost/RPC_BASIC_PORT pair if not given.
 */
#include "../common/example_common.h"
#include "hace/corba_exception.h"
#include "hace/uiop.h"

#include <stdio.h>
#include <string.h>

#define RPC_BASIC_PORT 59101U

static const uint8_t CALC_OBJECT_KEY[] = {(uint8_t)'c', (uint8_t)'a', (uint8_t)'l',
                                           (uint8_t)'c'};

static int build_add_request(hace_cdr_buffer_t *buf, uint32_t request_id, int32_t a,
                              int32_t b, const uint8_t *object_key,
                              size_t object_key_length)
{
    hace_giop_request_header_t header;

    memset(&header, 0, sizeof(header));
    header.request_id = request_id;
    header.response_expected = 1;
    (void)memcpy(header.object_key, object_key, object_key_length);
    header.object_key_length = object_key_length;
    (void)strncpy(header.operation, "add", sizeof(header.operation) - 1U);

    if (hace_giop_begin_message(buf, HACE_CDR_BIG_ENDIAN) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_giop_encode_request_header(buf, &header) != HACE_GIOP_OK) {
        return 0;
    }
    if ((hace_cdr_encode_i32(buf, a) != HACE_CDR_OK) ||
        (hace_cdr_encode_i32(buf, b) != HACE_CDR_OK)) {
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
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;
    const int32_t a = 7;
    const int32_t b = 35;

    (void)hace_example_parse_ior_arg(argc, argv, 1, RPC_BASIC_PORT, CALC_OBJECT_KEY,
                                      sizeof(CALC_OBJECT_KEY), &server_ior);

    if (hace_hal_socket_open(&client_socket, 0U) != HACE_HAL_OK) {
        (void)fprintf(stderr, "rpc_basic client: could not open UDP socket\n");
        return 1;
    }
    transport = hace_uiop_transport_for_socket(&client_socket);

    if ((hace_cdr_buffer_init(&request_buf, request_storage, sizeof(request_storage),
                               HACE_CDR_BIG_ENDIAN) != HACE_CDR_OK) ||
        !build_add_request(&request_buf, 1U, a, b, server_ior.object_key,
                            server_ior.object_key_length)) {
        (void)fprintf(stderr, "rpc_basic client: could not encode request\n");
        (void)hace_hal_socket_close(client_socket);
        return 1;
    }

    status = hace_uiop_call(&transport, &request_buf, 1U, server_ior.host_ipv4,
                             server_ior.port, HACE_UIOP_MAX_DATAGRAM_SIZE, 1000U, 3U,
                             reply_body, sizeof(reply_body), &reply_status,
                             &reply_body_length);
    if (status != HACE_UIOP_OK) {
        (void)fprintf(stderr, "rpc_basic client: call failed (status %d)\n",
                       (int)status);
        (void)hace_hal_socket_close(client_socket);
        return 1;
    }

    if (reply_status != HACE_GIOP_REPLY_NO_EXCEPTION) {
        hace_cdr_buffer_t exc_buf;
        hace_corba_system_exception_t exc;

        (void)hace_cdr_buffer_init(&exc_buf, reply_body, reply_body_length,
                                    HACE_CDR_BIG_ENDIAN);
        (void)hace_corba_decode_system_exception(&exc_buf, &exc);
        (void)fprintf(stderr, "rpc_basic client: server raised exception %d\n",
                       (int)exc.exception);
        (void)hace_hal_socket_close(client_socket);
        return 1;
    }

    {
        hace_cdr_buffer_t result_buf;
        int32_t sum = 0;

        (void)hace_cdr_buffer_init(&result_buf, reply_body, reply_body_length,
                                    HACE_CDR_BIG_ENDIAN);
        (void)hace_cdr_decode_i32(&result_buf, &sum);
        (void)printf("client: %d + %d = %d\n", a, b, sum);
    }

    (void)hace_hal_socket_close(client_socket);
    return 0;
}
