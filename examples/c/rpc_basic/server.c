/*
 * rpc_basic server: activates a Calculator object (object key "calc")
 * whose add(a, b) operation returns a + b, over a blocking twoway
 * call. See calculator.idl for the interface this hand-implements
 * (there is no idlc yet -- Phase 4).
 */
#include "../common/example_common.h"
#include "hace/corba_poa.h"

#include <stdio.h>
#include <string.h>

#define RPC_BASIC_PORT 59101U

static const uint8_t CALC_OBJECT_KEY[] = {(uint8_t)'c', (uint8_t)'a', (uint8_t)'l',
                                           (uint8_t)'c'};

static void calculator_servant(void *context, const char *operation,
                                hace_cdr_buffer_t *args, hace_cdr_buffer_t *reply,
                                hace_corba_system_exception_t *out_exception)
{
    int32_t a = 0;
    int32_t b = 0;

    (void)context;

    if (strcmp(operation, "add") != 0) {
        out_exception->exception = HACE_CORBA_EXC_BAD_OPERATION;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }

    if ((hace_cdr_decode_i32(args, &a) != HACE_CDR_OK) ||
        (hace_cdr_decode_i32(args, &b) != HACE_CDR_OK)) {
        out_exception->exception = HACE_CORBA_EXC_MARSHAL;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }

    (void)hace_cdr_encode_i32(reply, a + b);
    (void)printf("server: add(%d, %d) = %d\n", a, b, a + b);
    (void)fflush(stdout);
}

int main(void)
{
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_ior_t ior;
    hace_corba_poa_t poa;
    hace_uiop_transport_t transport;
    hace_uiop_status_t status;

    if (!hace_example_server_start(&server_socket, &ior, "rpc_basic server",
                                    RPC_BASIC_PORT, CALC_OBJECT_KEY,
                                    sizeof(CALC_OBJECT_KEY))) {
        return 1;
    }

    if (hace_corba_poa_init(&poa) != HACE_CORBA_OK) {
        (void)fprintf(stderr, "rpc_basic server: poa_init failed\n");
        return 1;
    }
    if (hace_corba_poa_activate_object(&poa, CALC_OBJECT_KEY, sizeof(CALC_OBJECT_KEY),
                                        calculator_servant, NULL) != HACE_CORBA_OK) {
        (void)fprintf(stderr, "rpc_basic server: activate_object failed\n");
        return 1;
    }

    transport = hace_uiop_transport_for_socket(&server_socket);

    /* Self-terminating (no Ctrl-C needed): handle exactly one call,
     * matching this example's "one client makes one call" scope. */
    status =
        hace_corba_poa_handle_one_request(&poa, &transport, 10000U,
                                           HACE_UIOP_MAX_DATAGRAM_SIZE);
    if (status == HACE_UIOP_ERR_TIMEOUT) {
        (void)printf("rpc_basic server: no request received within 10s, exiting\n");
    } else if (status != HACE_UIOP_OK) {
        (void)fprintf(stderr, "rpc_basic server: dispatch error (%d)\n", (int)status);
        (void)hace_hal_socket_close(server_socket);
        return 1;
    }

    (void)hace_hal_socket_close(server_socket);
    return 0;
}
