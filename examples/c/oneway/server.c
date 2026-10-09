/*
 * oneway server: activates a Logger object (object key "log") whose
 * log(message) operation is oneway -- it prints the message and sends
 * nothing back. See logger.idl for the hand-implemented interface.
 */
#include "../common/example_common.h"
#include "hace/corba_poa.h"

#include <stdio.h>
#include <string.h>

#define ONEWAY_PORT 59111U

static const uint8_t LOG_OBJECT_KEY[] = {(uint8_t)'l', (uint8_t)'o', (uint8_t)'g'};

static void logger_servant(void *context, const char *operation,
                            hace_cdr_buffer_t *args, hace_cdr_buffer_t *reply,
                            hace_corba_system_exception_t *out_exception)
{
    char message[128];
    size_t message_length = 0U;

    (void)context;
    (void)reply;

    if (strcmp(operation, "log") != 0) {
        out_exception->exception = HACE_CORBA_EXC_BAD_OPERATION;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }
    if (hace_cdr_decode_string(args, message, sizeof(message), &message_length) !=
        HACE_CDR_OK) {
        out_exception->exception = HACE_CORBA_EXC_MARSHAL;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }

    (void)printf("server: log: %s\n", message);
    (void)fflush(stdout);
}

int main(void)
{
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_ior_t ior;
    hace_corba_poa_t poa;
    hace_uiop_transport_t transport;
    int handled = 0;

    if (!hace_example_server_start(&server_socket, &ior, "oneway server", ONEWAY_PORT,
                                    LOG_OBJECT_KEY, sizeof(LOG_OBJECT_KEY))) {
        return 1;
    }

    if ((hace_corba_poa_init(&poa) != HACE_CORBA_OK) ||
        (hace_corba_poa_activate_object(&poa, LOG_OBJECT_KEY, sizeof(LOG_OBJECT_KEY),
                                         logger_servant, NULL) != HACE_CORBA_OK)) {
        (void)fprintf(stderr, "oneway server: poa setup failed\n");
        return 1;
    }

    transport = hace_uiop_transport_for_socket(&server_socket);

    /* Self-terminating: handle up to 3 fire-and-forget messages
     * (matching client.c's three log() calls), or stop once 5s passes
     * without one. */
    while (handled < 3) {
        hace_uiop_status_t status = hace_corba_poa_handle_one_request(
            &poa, &transport, 5000U, HACE_UIOP_MAX_DATAGRAM_SIZE);
        if (status == HACE_UIOP_OK) {
            handled++;
        } else if (status == HACE_UIOP_ERR_TIMEOUT) {
            (void)printf("oneway server: no more messages within 5s, exiting\n");
            break;
        } else {
            (void)fprintf(stderr, "oneway server: dispatch error (%d)\n", (int)status);
            break;
        }
    }

    (void)hace_hal_socket_close(server_socket);
    return 0;
}
