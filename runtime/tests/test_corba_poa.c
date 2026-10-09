#include "hace/corba_poa.h"
#include "test_framework.h"

#include <string.h>

#define LOOPBACK_ADDR (((uint32_t)127U << 24) | 1U)

static int build_request(hace_cdr_buffer_t *buf, uint32_t request_id,
                          const uint8_t *object_key, size_t object_key_length,
                          const char *operation, const uint8_t *arg_data,
                          size_t arg_len, int response_expected)
{
    hace_giop_request_header_t header;

    memset(&header, 0, sizeof(header));
    header.request_id = request_id;
    header.response_expected = response_expected;
    (void)memcpy(header.object_key, object_key, object_key_length);
    header.object_key_length = object_key_length;
    (void)strncpy(header.operation, operation, sizeof(header.operation) - 1U);

    if (hace_giop_begin_message(buf, HACE_CDR_BIG_ENDIAN) != HACE_GIOP_OK) {
        return 0;
    }
    if (hace_giop_encode_request_header(buf, &header) != HACE_GIOP_OK) {
        return 0;
    }
    if ((arg_len > 0U) &&
        (hace_cdr_encode_raw_bytes(buf, arg_data, arg_len) != HACE_CDR_OK)) {
        return 0;
    }
    if (hace_giop_finish_message(buf, HACE_GIOP_MSG_REQUEST, 0) != HACE_GIOP_OK) {
        return 0;
    }
    return 1;
}

/* ---- servants ---- */

static void echo_servant(void *context, const char *operation, hace_cdr_buffer_t *args,
                          hace_cdr_buffer_t *reply,
                          hace_corba_system_exception_t *out_exception)
{
    uint8_t buf[256];
    size_t len = hace_cdr_buffer_capacity(args);

    (void)context;
    (void)operation;
    (void)out_exception;
    if (len > sizeof(buf)) {
        len = sizeof(buf);
    }
    (void)hace_cdr_decode_raw_bytes(args, buf, len);
    (void)hace_cdr_encode_raw_bytes(reply, buf, len);
}

static void uppercase_servant(void *context, const char *operation,
                               hace_cdr_buffer_t *args, hace_cdr_buffer_t *reply,
                               hace_corba_system_exception_t *out_exception)
{
    uint8_t buf[256];
    size_t len = hace_cdr_buffer_capacity(args);
    size_t i;

    (void)context;
    (void)operation;
    (void)out_exception;
    if (len > sizeof(buf)) {
        len = sizeof(buf);
    }
    (void)hace_cdr_decode_raw_bytes(args, buf, len);
    for (i = 0U; i < len; i++) {
        if ((buf[i] >= (uint8_t)'a') && (buf[i] <= (uint8_t)'z')) {
            buf[i] = (uint8_t)((buf[i] - (uint8_t)'a') + (uint8_t)'A');
        }
    }
    (void)hace_cdr_encode_raw_bytes(reply, buf, len);
}

static void exploding_servant(void *context, const char *operation,
                               hace_cdr_buffer_t *args, hace_cdr_buffer_t *reply,
                               hace_corba_system_exception_t *out_exception)
{
    (void)context;
    (void)operation;
    (void)args;
    (void)reply;
    out_exception->exception = HACE_CORBA_EXC_BAD_OPERATION;
    out_exception->minor_code = 777U;
    out_exception->completion_status = HACE_CORBA_COMPLETED_NO;
}

/* ---- single-dispatch server task, for the sequential tests ---- */

typedef struct {
    const hace_corba_poa_t *poa;
    hace_hal_socket_handle_t server_socket;
    hace_uiop_status_t result;
} single_dispatch_ctx_t;

static void single_dispatch_task(void *arg)
{
    single_dispatch_ctx_t *ctx = (single_dispatch_ctx_t *)arg;
    hace_uiop_transport_t transport = hace_uiop_transport_for_socket(&ctx->server_socket);
    ctx->result =
        hace_corba_poa_handle_one_request(ctx->poa, &transport, 2000U,
                                           HACE_UIOP_MAX_DATAGRAM_SIZE);
}

/* ---- tests ---- */

static int test_poa_echo_roundtrip(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_poa_t poa;
    hace_uiop_transport_t client_transport;
    single_dispatch_ctx_t dispatch_ctx;
    hace_hal_task_handle_t task;
    uint8_t object_key[1] = {0x01U};
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t reply_body[256];
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58601U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58602U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_init(&poa) == HACE_CORBA_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_activate_object(&poa, object_key, 1U, echo_servant,
                                                      NULL) == HACE_CORBA_OK);

    dispatch_ctx.poa = &poa;
    dispatch_ctx.server_socket = server_socket;
    dispatch_ctx.result = HACE_UIOP_ERR_TIMEOUT;
    HACE_ASSERT_TRUE(hace_hal_task_create(&task, single_dispatch_task, &dispatch_ctx,
                                           0) == HACE_HAL_OK);

    client_transport = hace_uiop_transport_for_socket(&client_socket);
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 1U, object_key, 1U, "echo",
                                    (const uint8_t *)"hello", 5U, 1));

    status = hace_uiop_call(&client_transport, &request_buf, 1U, LOOPBACK_ADDR, 58602U,
                             HACE_UIOP_MAX_DATAGRAM_SIZE, 1000U, 3U, reply_body,
                             sizeof(reply_body), &reply_status, &reply_body_length);

    HACE_ASSERT_TRUE(hace_hal_task_join(task) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(dispatch_ctx.result == HACE_UIOP_OK);

    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_TRUE(reply_status == HACE_GIOP_REPLY_NO_EXCEPTION);
    HACE_ASSERT_EQ_INT(5, reply_body_length);
    HACE_ASSERT_TRUE(memcmp(reply_body, "hello", 5) == 0);

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

static int test_poa_oneway_sends_no_reply(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_poa_t poa;
    hace_uiop_transport_t client_transport;
    single_dispatch_ctx_t dispatch_ctx;
    hace_hal_task_handle_t task;
    uint8_t object_key[1] = {0x02U};
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58611U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58612U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_init(&poa) == HACE_CORBA_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_activate_object(&poa, object_key, 1U, echo_servant,
                                                      NULL) == HACE_CORBA_OK);

    dispatch_ctx.poa = &poa;
    dispatch_ctx.server_socket = server_socket;
    dispatch_ctx.result = HACE_UIOP_ERR_TIMEOUT;
    HACE_ASSERT_TRUE(hace_hal_task_create(&task, single_dispatch_task, &dispatch_ctx,
                                           0) == HACE_HAL_OK);

    client_transport = hace_uiop_transport_for_socket(&client_socket);
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 2U, object_key, 1U, "echo",
                                    (const uint8_t *)"hi", 2U, 0));
    HACE_ASSERT_TRUE(hace_uiop_send_message(&client_transport, &request_buf,
                                             HACE_GIOP_MSG_REQUEST, 2U, LOOPBACK_ADDR,
                                             58612U,
                                             HACE_UIOP_MAX_DATAGRAM_SIZE) == HACE_UIOP_OK);

    HACE_ASSERT_TRUE(hace_hal_task_join(task) == HACE_HAL_OK);
    /* Dispatch itself succeeded (the request was received and handed to
     * the servant) -- it just correctly sent nothing back. */
    HACE_ASSERT_TRUE(dispatch_ctx.result == HACE_UIOP_OK);

    {
        uint8_t body[64];
        hace_giop_message_header_t out_header;
        uint32_t out_request_id = 0U;
        size_t out_body_length = 0U;
        hace_uiop_status_t recv_status = hace_uiop_receive(
            &client_transport, HACE_GIOP_MSG_REPLY, NULL, 200U, body, sizeof(body),
            &out_header, &out_request_id, NULL, &out_body_length, NULL, NULL, NULL);
        HACE_ASSERT_TRUE(recv_status == HACE_UIOP_ERR_TIMEOUT);
    }

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

static int test_poa_unknown_object_key_raises_object_not_exist(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_poa_t poa;
    hace_uiop_transport_t client_transport;
    single_dispatch_ctx_t dispatch_ctx;
    hace_hal_task_handle_t task;
    uint8_t activated_key[1] = {0x03U};
    uint8_t unknown_key[1] = {0x99U};
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t reply_body[256];
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58621U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58622U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_init(&poa) == HACE_CORBA_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_activate_object(&poa, activated_key, 1U, echo_servant,
                                                      NULL) == HACE_CORBA_OK);

    dispatch_ctx.poa = &poa;
    dispatch_ctx.server_socket = server_socket;
    dispatch_ctx.result = HACE_UIOP_ERR_TIMEOUT;
    HACE_ASSERT_TRUE(hace_hal_task_create(&task, single_dispatch_task, &dispatch_ctx,
                                           0) == HACE_HAL_OK);

    client_transport = hace_uiop_transport_for_socket(&client_socket);
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 3U, unknown_key, 1U, "echo",
                                    (const uint8_t *)"hi", 2U, 1));

    status = hace_uiop_call(&client_transport, &request_buf, 3U, LOOPBACK_ADDR, 58622U,
                             HACE_UIOP_MAX_DATAGRAM_SIZE, 1000U, 3U, reply_body,
                             sizeof(reply_body), &reply_status, &reply_body_length);

    HACE_ASSERT_TRUE(hace_hal_task_join(task) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_TRUE(reply_status == HACE_GIOP_REPLY_SYSTEM_EXCEPTION);

    {
        hace_cdr_buffer_t exc_buf;
        hace_corba_system_exception_t exc;
        HACE_ASSERT_TRUE(hace_cdr_buffer_init(&exc_buf, reply_body, reply_body_length,
                                               HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
        HACE_ASSERT_TRUE(hace_corba_decode_system_exception(&exc_buf, &exc) ==
                          HACE_CORBA_OK);
        HACE_ASSERT_TRUE(exc.exception == HACE_CORBA_EXC_OBJECT_NOT_EXIST);
    }

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

static int test_poa_servant_raised_exception(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_poa_t poa;
    hace_uiop_transport_t client_transport;
    single_dispatch_ctx_t dispatch_ctx;
    hace_hal_task_handle_t task;
    uint8_t object_key[1] = {0x04U};
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t reply_body[256];
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58631U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58632U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_init(&poa) == HACE_CORBA_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_activate_object(&poa, object_key, 1U,
                                                      exploding_servant,
                                                      NULL) == HACE_CORBA_OK);

    dispatch_ctx.poa = &poa;
    dispatch_ctx.server_socket = server_socket;
    dispatch_ctx.result = HACE_UIOP_ERR_TIMEOUT;
    HACE_ASSERT_TRUE(hace_hal_task_create(&task, single_dispatch_task, &dispatch_ctx,
                                           0) == HACE_HAL_OK);

    client_transport = hace_uiop_transport_for_socket(&client_socket);
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 4U, object_key, 1U, "boom", NULL, 0U,
                                    1));

    status = hace_uiop_call(&client_transport, &request_buf, 4U, LOOPBACK_ADDR, 58632U,
                             HACE_UIOP_MAX_DATAGRAM_SIZE, 1000U, 3U, reply_body,
                             sizeof(reply_body), &reply_status, &reply_body_length);

    HACE_ASSERT_TRUE(hace_hal_task_join(task) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_TRUE(reply_status == HACE_GIOP_REPLY_SYSTEM_EXCEPTION);

    {
        hace_cdr_buffer_t exc_buf;
        hace_corba_system_exception_t exc;
        HACE_ASSERT_TRUE(hace_cdr_buffer_init(&exc_buf, reply_body, reply_body_length,
                                               HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
        HACE_ASSERT_TRUE(hace_corba_decode_system_exception(&exc_buf, &exc) ==
                          HACE_CORBA_OK);
        HACE_ASSERT_TRUE(exc.exception == HACE_CORBA_EXC_BAD_OPERATION);
        HACE_ASSERT_EQ_INT(777, exc.minor_code);
        HACE_ASSERT_TRUE(exc.completion_status == HACE_CORBA_COMPLETED_NO);
    }

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

/* ---- concurrent dispatch: a bounded worker-task pool ---- */

typedef struct {
    const hace_corba_poa_t *poa;
    hace_uiop_transport_t transport;
    size_t max_datagram_size;
    uint32_t iteration_timeout_ms;
    const volatile int *stop_flag;
    hace_uiop_status_t result;
} poa_worker_ctx_t;

static void poa_worker_task(void *arg)
{
    poa_worker_ctx_t *ctx = (poa_worker_ctx_t *)arg;
    ctx->result = hace_corba_poa_run(ctx->poa, &ctx->transport, ctx->max_datagram_size,
                                      ctx->iteration_timeout_ms, ctx->stop_flag);
}

typedef struct {
    uint16_t client_port;
    uint16_t server_port;
    uint32_t request_id;
    const uint8_t *object_key;
    size_t object_key_length;
    const uint8_t *arg;
    size_t arg_len;
    const uint8_t *expected_reply;
    int ok;
} concurrent_client_ctx_t;

static void concurrent_client_task(void *arg)
{
    concurrent_client_ctx_t *ctx = (concurrent_client_ctx_t *)arg;
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_uiop_transport_t client_transport;
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t reply_body[256];
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;

    ctx->ok = 0;
    if (hace_hal_socket_open(&client_socket, ctx->client_port) != HACE_HAL_OK) {
        return;
    }
    client_transport = hace_uiop_transport_for_socket(&client_socket);

    if ((hace_cdr_buffer_init(&request_buf, request_storage, sizeof(request_storage),
                               HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK) &&
        build_request(&request_buf, ctx->request_id, ctx->object_key,
                       ctx->object_key_length, "op", ctx->arg, ctx->arg_len, 1)) {
        status = hace_uiop_call(&client_transport, &request_buf, ctx->request_id,
                                 LOOPBACK_ADDR, ctx->server_port,
                                 HACE_UIOP_MAX_DATAGRAM_SIZE, 1000U, 5U, reply_body,
                                 sizeof(reply_body), &reply_status, &reply_body_length);

        if ((status == HACE_UIOP_OK) && (reply_status == HACE_GIOP_REPLY_NO_EXCEPTION) &&
            (reply_body_length == ctx->arg_len) &&
            (memcmp(reply_body, ctx->expected_reply, ctx->arg_len) == 0)) {
            ctx->ok = 1;
        }
    }

    (void)hace_hal_socket_close(client_socket);
}

static int test_poa_concurrent_dispatch(void)
{
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_corba_poa_t poa;
    uint8_t key_a[1] = {0xAAU};
    uint8_t key_b[1] = {0xBBU};
    volatile int stop_flag = 0;
    poa_worker_ctx_t worker_ctxs[3];
    hace_hal_task_handle_t workers[3];
    concurrent_client_ctx_t client_ctxs[4];
    hace_hal_task_handle_t clients[4];
    static const uint8_t arg_ping[] = {(uint8_t)'p', (uint8_t)'i', (uint8_t)'n',
                                        (uint8_t)'g'};
    static const uint8_t expected_echo[] = {(uint8_t)'p', (uint8_t)'i', (uint8_t)'n',
                                              (uint8_t)'g'};
    static const uint8_t expected_upper[] = {(uint8_t)'P', (uint8_t)'I', (uint8_t)'N',
                                               (uint8_t)'G'};
    size_t i;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58641U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_init(&poa) == HACE_CORBA_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_activate_object(&poa, key_a, 1U, echo_servant,
                                                      NULL) == HACE_CORBA_OK);
    HACE_ASSERT_TRUE(hace_corba_poa_activate_object(&poa, key_b, 1U, uppercase_servant,
                                                      NULL) == HACE_CORBA_OK);

    for (i = 0U; i < 3U; i++) {
        worker_ctxs[i].poa = &poa;
        worker_ctxs[i].transport = hace_uiop_transport_for_socket(&server_socket);
        worker_ctxs[i].max_datagram_size = HACE_UIOP_MAX_DATAGRAM_SIZE;
        worker_ctxs[i].iteration_timeout_ms = 100U;
        worker_ctxs[i].stop_flag = &stop_flag;
        worker_ctxs[i].result = HACE_UIOP_ERR_TIMEOUT;
        HACE_ASSERT_TRUE(hace_hal_task_create(&workers[i], poa_worker_task,
                                               &worker_ctxs[i], 0) == HACE_HAL_OK);
    }

    client_ctxs[0].client_port = 58651U;
    client_ctxs[1].client_port = 58652U;
    client_ctxs[2].client_port = 58653U;
    client_ctxs[3].client_port = 58654U;
    for (i = 0U; i < 4U; i++) {
        client_ctxs[i].server_port = 58641U;
        client_ctxs[i].request_id = (uint32_t)(200U + i);
        client_ctxs[i].arg = arg_ping;
        client_ctxs[i].arg_len = sizeof(arg_ping);
        if ((i % 2U) == 0U) {
            client_ctxs[i].object_key = key_a;
            client_ctxs[i].expected_reply = expected_echo;
        } else {
            client_ctxs[i].object_key = key_b;
            client_ctxs[i].expected_reply = expected_upper;
        }
        client_ctxs[i].object_key_length = 1U;
        HACE_ASSERT_TRUE(hace_hal_task_create(&clients[i], concurrent_client_task,
                                               &client_ctxs[i], 0) == HACE_HAL_OK);
    }

    for (i = 0U; i < 4U; i++) {
        HACE_ASSERT_TRUE(hace_hal_task_join(clients[i]) == HACE_HAL_OK);
        HACE_ASSERT_TRUE(client_ctxs[i].ok);
    }

    stop_flag = 1;
    for (i = 0U; i < 3U; i++) {
        HACE_ASSERT_TRUE(hace_hal_task_join(workers[i]) == HACE_HAL_OK);
        HACE_ASSERT_TRUE(worker_ctxs[i].result == HACE_UIOP_OK);
    }

    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

static int test_poa_sizeof_matches_struct(void)
{
    /* Guards the Ada95/Rust bindings' opaque-blob sizing: their
     * compile-time storage must stay >= this at runtime. */
    HACE_ASSERT_EQ_INT(sizeof(hace_corba_poa_t), hace_corba_poa_sizeof());
    return 1;
}

int main(void)
{
    HACE_RUN_TEST(test_poa_sizeof_matches_struct);
    HACE_RUN_TEST(test_poa_echo_roundtrip);
    HACE_RUN_TEST(test_poa_oneway_sends_no_reply);
    HACE_RUN_TEST(test_poa_unknown_object_key_raises_object_not_exist);
    HACE_RUN_TEST(test_poa_servant_raised_exception);
    HACE_RUN_TEST(test_poa_concurrent_dispatch);
    HACE_TEST_REPORT();
}
