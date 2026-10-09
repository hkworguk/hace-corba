/*
 * async_ami server: activates a Worker object (object key "work")
 * whose compute(input) operation takes ~300ms of simulated work
 * (a bounded busy-wait against hace_hal_time_now -- hal.h has no
 * sleep) before replying with input * input. The delay exists so
 * client.c's Poller-style loop has something real to demonstrate.
 */
#include "../common/example_common.h"
#include "hace/corba_poa.h"

#include <stdio.h>
#include <string.h>

#define ASYNC_AMI_PORT 59121U
#define WORK_SIMULATED_MS 300U

static const uint8_t WORK_OBJECT_KEY[] = {(uint8_t)'w', (uint8_t)'o', (uint8_t)'r',
                                           (uint8_t)'k'};

static uint64_t time_to_ms(const hace_hal_time_t *t)
{
    return ((uint64_t)t->seconds * 1000ULL) + ((uint64_t)t->nanoseconds / 1000000ULL);
}

static void busy_wait_ms(uint32_t ms)
{
    hace_hal_time_t start;
    hace_hal_time_t now;

    if (hace_hal_time_now(&start) != HACE_HAL_OK) {
        return;
    }
    for (;;) {
        if (hace_hal_time_now(&now) != HACE_HAL_OK) {
            return;
        }
        if ((time_to_ms(&now) - time_to_ms(&start)) >= (uint64_t)ms) {
            return;
        }
    }
}

static void worker_servant(void *context, const char *operation,
                            hace_cdr_buffer_t *args, hace_cdr_buffer_t *reply,
                            hace_corba_system_exception_t *out_exception)
{
    int32_t input = 0;

    (void)context;

    if (strcmp(operation, "compute") != 0) {
        out_exception->exception = HACE_CORBA_EXC_BAD_OPERATION;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }
    if (hace_cdr_decode_i32(args, &input) != HACE_CDR_OK) {
        out_exception->exception = HACE_CORBA_EXC_MARSHAL;
        out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
        return;
    }

    (void)printf("server: computing for input=%d (simulating %ums of work)...\n", input,
                  WORK_SIMULATED_MS);
    (void)fflush(stdout);
    busy_wait_ms(WORK_SIMULATED_MS);

    (void)hace_cdr_encode_i32(reply, input * input);
    (void)printf("server: compute(%d) = %d\n", input, input * input);
    (void)fflush(stdout);
}

int main(void)
{
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_ior_t ior;
    hace_corba_poa_t poa;
    hace_uiop_transport_t transport;
    hace_uiop_status_t status;

    if (!hace_example_server_start(&server_socket, &ior, "async_ami server",
                                    ASYNC_AMI_PORT, WORK_OBJECT_KEY,
                                    sizeof(WORK_OBJECT_KEY))) {
        return 1;
    }

    if ((hace_corba_poa_init(&poa) != HACE_CORBA_OK) ||
        (hace_corba_poa_activate_object(&poa, WORK_OBJECT_KEY, sizeof(WORK_OBJECT_KEY),
                                         worker_servant, NULL) != HACE_CORBA_OK)) {
        (void)fprintf(stderr, "async_ami server: poa setup failed\n");
        return 1;
    }

    transport = hace_uiop_transport_for_socket(&server_socket);

    status = hace_corba_poa_handle_one_request(&poa, &transport, 10000U,
                                                HACE_UIOP_MAX_DATAGRAM_SIZE);
    if (status == HACE_UIOP_ERR_TIMEOUT) {
        (void)printf("async_ami server: no request received within 10s, exiting\n");
    } else if (status != HACE_UIOP_OK) {
        (void)fprintf(stderr, "async_ami server: dispatch error (%d)\n", (int)status);
        (void)hace_hal_socket_close(server_socket);
        return 1;
    }

    (void)hace_hal_socket_close(server_socket);
    return 0;
}
