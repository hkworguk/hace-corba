#include "hace/uiop.h"
#include "test_framework.h"

#include <string.h>

#define LOOPBACK_ADDR (((uint32_t)127U << 24) | 1U)

static int build_request(hace_cdr_buffer_t *buf, uint32_t request_id,
                          const char *operation, const uint8_t *arg_data,
                          size_t arg_len)
{
    hace_giop_request_header_t header;

    memset(&header, 0, sizeof(header));
    header.request_id = request_id;
    header.response_expected = 1;
    header.object_key[0] = 0x01U;
    header.object_key_length = 1U;
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

/* ---- a small echo server, run on a HAL task: receives one Request,
 * replies with a fixed payload ---- */

typedef struct {
    hace_hal_socket_handle_t server_socket;
    int server_saw_request;
    const char *reply_payload;
} echo_server_ctx_t;

static void echo_server_task(void *arg)
{
    echo_server_ctx_t *ctx = (echo_server_ctx_t *)arg;
    hace_uiop_transport_t transport = hace_uiop_transport_for_socket(&ctx->server_socket);
    uint8_t body[256];
    hace_giop_message_header_t header;
    uint32_t request_id;
    size_t body_length = 0U;
    uint32_t src_addr = 0U;
    uint16_t src_port = 0U;
    hace_uiop_status_t status;

    status = hace_uiop_receive(&transport, HACE_GIOP_MSG_REQUEST, NULL, 2000U, body,
                                sizeof(body), &header, &request_id, NULL, &body_length,
                                &src_addr, &src_port, NULL);
    if (status != HACE_UIOP_OK) {
        ctx->server_saw_request = 0;
        return;
    }
    ctx->server_saw_request = 1;

    {
        uint8_t reply_storage[256];
        hace_cdr_buffer_t reply_buf;
        hace_giop_reply_header_t reply_header;
        size_t payload_len = strlen(ctx->reply_payload);

        (void)hace_cdr_buffer_init(&reply_buf, reply_storage, sizeof(reply_storage),
                                    HACE_CDR_BIG_ENDIAN);
        (void)hace_giop_begin_message(&reply_buf, HACE_CDR_BIG_ENDIAN);
        reply_header.request_id = request_id;
        reply_header.reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
        (void)hace_giop_encode_reply_header(&reply_buf, &reply_header);
        (void)hace_cdr_encode_raw_bytes(&reply_buf, (const uint8_t *)ctx->reply_payload,
                                         payload_len);
        (void)hace_giop_finish_message(&reply_buf, HACE_GIOP_MSG_REPLY, 0);

        (void)hace_uiop_send_message(&transport, &reply_buf, HACE_GIOP_MSG_REPLY,
                                      request_id, src_addr, src_port,
                                      HACE_UIOP_MAX_DATAGRAM_SIZE);
    }
}

/* ---- fault-injecting transport: wraps a real one, can drop/count sends ---- */

typedef struct {
    hace_uiop_transport_t inner;
    int drop_next_n_sends;
    int send_count;
} fault_injection_ctx_t;

static hace_hal_status_t fault_send(void *context, const uint8_t *data, size_t length,
                                     uint32_t dest_addr_ipv4, uint16_t dest_port)
{
    fault_injection_ctx_t *ctx = (fault_injection_ctx_t *)context;
    ctx->send_count++;
    if (ctx->drop_next_n_sends > 0) {
        ctx->drop_next_n_sends--;
        return HACE_HAL_OK; /* pretend it was sent, but never deliver it */
    }
    return ctx->inner.send(ctx->inner.context, data, length, dest_addr_ipv4, dest_port);
}

static hace_hal_status_t fault_recv(void *context, uint8_t *buffer, size_t capacity,
                                     size_t *out_length, uint32_t *src_addr_ipv4,
                                     uint16_t *src_port, uint32_t timeout_ms)
{
    fault_injection_ctx_t *ctx = (fault_injection_ctx_t *)context;
    return ctx->inner.recv(ctx->inner.context, buffer, capacity, out_length,
                            src_addr_ipv4, src_port, timeout_ms);
}

static hace_uiop_transport_t make_fault_transport(fault_injection_ctx_t *ctx)
{
    hace_uiop_transport_t t;
    t.send = fault_send;
    t.recv = fault_recv;
    t.context = ctx;
    return t;
}

/* ---- tests ---- */

static int test_request_reply_roundtrip(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_task_handle_t server_task;
    echo_server_ctx_t server_ctx;
    hace_uiop_transport_t client_transport;
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t reply_body[256];
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58501U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58502U) == HACE_HAL_OK);

    server_ctx.server_socket = server_socket;
    server_ctx.server_saw_request = 0;
    server_ctx.reply_payload = "pong";
    HACE_ASSERT_TRUE(hace_hal_task_create(&server_task, echo_server_task, &server_ctx,
                                           0) == HACE_HAL_OK);

    client_transport = hace_uiop_transport_for_socket(&client_socket);
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 1U, "ping", NULL, 0U));

    status = hace_uiop_call(&client_transport, &request_buf, 1U, LOOPBACK_ADDR, 58502U,
                             HACE_UIOP_MAX_DATAGRAM_SIZE, 1000U, 3U, reply_body,
                             sizeof(reply_body), &reply_status, &reply_body_length);

    HACE_ASSERT_TRUE(hace_hal_task_join(server_task) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(server_ctx.server_saw_request);

    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_TRUE(reply_status == HACE_GIOP_REPLY_NO_EXCEPTION);
    HACE_ASSERT_EQ_INT(4, reply_body_length);
    HACE_ASSERT_TRUE(memcmp(reply_body, "pong", 4) == 0);

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

static int test_retry_succeeds_after_dropped_send(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_task_handle_t server_task;
    echo_server_ctx_t server_ctx;
    fault_injection_ctx_t fault_ctx;
    hace_uiop_transport_t client_transport;
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t reply_body[256];
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58511U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58512U) == HACE_HAL_OK);

    server_ctx.server_socket = server_socket;
    server_ctx.server_saw_request = 0;
    server_ctx.reply_payload = "pong";
    HACE_ASSERT_TRUE(hace_hal_task_create(&server_task, echo_server_task, &server_ctx,
                                           0) == HACE_HAL_OK);

    fault_ctx.inner = hace_uiop_transport_for_socket(&client_socket);
    fault_ctx.drop_next_n_sends = 1; /* the first attempt's request never arrives */
    fault_ctx.send_count = 0;
    client_transport = make_fault_transport(&fault_ctx);

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 2U, "ping", NULL, 0U));

    status = hace_uiop_call(&client_transport, &request_buf, 2U, LOOPBACK_ADDR, 58512U,
                             HACE_UIOP_MAX_DATAGRAM_SIZE, 300U, 3U, reply_body,
                             sizeof(reply_body), &reply_status, &reply_body_length);

    HACE_ASSERT_TRUE(hace_hal_task_join(server_task) == HACE_HAL_OK);

    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_TRUE(fault_ctx.send_count >= 2); /* at least two attempts were made */
    HACE_ASSERT_TRUE(server_ctx.server_saw_request);
    HACE_ASSERT_EQ_INT(4, reply_body_length);
    HACE_ASSERT_TRUE(memcmp(reply_body, "pong", 4) == 0);

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

static int test_call_times_out_when_nothing_listening(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_uiop_transport_t client_transport;
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t reply_body[256];
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58521U) == HACE_HAL_OK);
    client_transport = hace_uiop_transport_for_socket(&client_socket);

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 3U, "ping", NULL, 0U));

    /* Nobody is listening on 58522. */
    status = hace_uiop_call(&client_transport, &request_buf, 3U, LOOPBACK_ADDR, 58522U,
                             HACE_UIOP_MAX_DATAGRAM_SIZE, 50U, 3U, reply_body,
                             sizeof(reply_body), &reply_status, &reply_body_length);

    HACE_ASSERT_TRUE(status == HACE_UIOP_ERR_TIMEOUT);

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    return 1;
}

static int test_fragmentation_send_emits_multiple_datagrams(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    fault_injection_ctx_t fault_ctx;
    hace_uiop_transport_t client_transport;
    uint8_t request_storage[2048];
    hace_cdr_buffer_t request_buf;
    uint8_t big_argument[1024];
    hace_uiop_status_t status;
    size_t i;

    for (i = 0U; i < sizeof(big_argument); i++) {
        big_argument[i] = (uint8_t)(i & 0xFFU);
    }

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58531U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58532U) == HACE_HAL_OK);

    fault_ctx.inner = hace_uiop_transport_for_socket(&client_socket);
    fault_ctx.drop_next_n_sends = 0;
    fault_ctx.send_count = 0;
    client_transport = make_fault_transport(&fault_ctx);

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 4U, "upload", big_argument,
                                    sizeof(big_argument)));

    status = hace_uiop_send_message(&client_transport, &request_buf, HACE_GIOP_MSG_REQUEST,
                                     4U, LOOPBACK_ADDR, 58532U,
                                     HACE_UIOP_MIN_DATAGRAM_SIZE);
    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_TRUE(fault_ctx.send_count > 1);

    /* Drain whatever arrived so it doesn't confuse a later test. */
    {
        uint8_t drain_buf[HACE_UIOP_MAX_DATAGRAM_SIZE];
        size_t drain_len = 0U;
        while (hace_hal_socket_recv(server_socket, drain_buf, sizeof(drain_buf),
                                     &drain_len, NULL, NULL, 50U) == HACE_HAL_OK) {
            /* keep draining */
        }
    }

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

typedef struct {
    hace_hal_socket_handle_t server_socket;
    int server_saw_request;
    size_t received_body_length;
    uint8_t received_body[2048];
    const uint8_t *reply_payload;
    size_t reply_payload_length;
} big_echo_server_ctx_t;

static void big_echo_server_task(void *arg)
{
    big_echo_server_ctx_t *ctx = (big_echo_server_ctx_t *)arg;
    hace_uiop_transport_t transport = hace_uiop_transport_for_socket(&ctx->server_socket);
    hace_giop_message_header_t header;
    uint32_t request_id;
    uint32_t src_addr = 0U;
    uint16_t src_port = 0U;
    hace_uiop_status_t status;

    status = hace_uiop_receive(&transport, HACE_GIOP_MSG_REQUEST, NULL, 2000U,
                                ctx->received_body, sizeof(ctx->received_body), &header,
                                &request_id, NULL, &ctx->received_body_length, &src_addr,
                                &src_port, NULL);
    if (status != HACE_UIOP_OK) {
        ctx->server_saw_request = 0;
        return;
    }
    ctx->server_saw_request = 1;

    {
        uint8_t reply_storage[2048];
        hace_cdr_buffer_t reply_buf;
        hace_giop_reply_header_t reply_header;

        (void)hace_cdr_buffer_init(&reply_buf, reply_storage, sizeof(reply_storage),
                                    HACE_CDR_BIG_ENDIAN);
        (void)hace_giop_begin_message(&reply_buf, HACE_CDR_BIG_ENDIAN);
        reply_header.request_id = request_id;
        reply_header.reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
        (void)hace_giop_encode_reply_header(&reply_buf, &reply_header);
        (void)hace_cdr_encode_raw_bytes(&reply_buf, ctx->reply_payload,
                                         ctx->reply_payload_length);
        (void)hace_giop_finish_message(&reply_buf, HACE_GIOP_MSG_REPLY, 0);

        (void)hace_uiop_send_message(&transport, &reply_buf, HACE_GIOP_MSG_REPLY,
                                      request_id, src_addr, src_port,
                                      HACE_UIOP_MIN_DATAGRAM_SIZE);
    }
}

static int test_fragmentation_roundtrip(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t server_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_task_handle_t server_task;
    big_echo_server_ctx_t server_ctx;
    hace_uiop_transport_t client_transport;
    uint8_t request_storage[2048];
    hace_cdr_buffer_t request_buf;
    uint8_t request_argument[800];
    uint8_t reply_payload[900];
    uint8_t reply_body[2048];
    hace_giop_reply_status_t reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t reply_body_length = 0U;
    hace_uiop_status_t status;
    size_t i;

    for (i = 0U; i < sizeof(request_argument); i++) {
        request_argument[i] = (uint8_t)((i * 3U) & 0xFFU);
    }
    for (i = 0U; i < sizeof(reply_payload); i++) {
        reply_payload[i] = (uint8_t)((i * 7U) & 0xFFU);
    }

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58541U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&server_socket, 58542U) == HACE_HAL_OK);

    server_ctx.server_socket = server_socket;
    server_ctx.server_saw_request = 0;
    server_ctx.received_body_length = 0U;
    server_ctx.reply_payload = reply_payload;
    server_ctx.reply_payload_length = sizeof(reply_payload);
    HACE_ASSERT_TRUE(hace_hal_task_create(&server_task, big_echo_server_task,
                                           &server_ctx, 0) == HACE_HAL_OK);

    client_transport = hace_uiop_transport_for_socket(&client_socket);
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 5U, "upload", request_argument,
                                    sizeof(request_argument)));

    status = hace_uiop_call(&client_transport, &request_buf, 5U, LOOPBACK_ADDR, 58542U,
                             HACE_UIOP_MIN_DATAGRAM_SIZE, 1000U, 3U, reply_body,
                             sizeof(reply_body), &reply_status, &reply_body_length);

    HACE_ASSERT_TRUE(hace_hal_task_join(server_task) == HACE_HAL_OK);

    HACE_ASSERT_TRUE(server_ctx.server_saw_request);
    HACE_ASSERT_EQ_INT(sizeof(request_argument), server_ctx.received_body_length);
    HACE_ASSERT_TRUE(memcmp(server_ctx.received_body, request_argument,
                             sizeof(request_argument)) == 0);

    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_TRUE(reply_status == HACE_GIOP_REPLY_NO_EXCEPTION);
    HACE_ASSERT_EQ_INT(sizeof(reply_payload), reply_body_length);
    HACE_ASSERT_TRUE(memcmp(reply_body, reply_payload, sizeof(reply_payload)) == 0);

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(server_socket) == HACE_HAL_OK);
    return 1;
}

static int test_send_rejects_message_needing_too_many_fragments(void)
{
    hace_hal_socket_handle_t client_socket = HACE_HAL_INVALID_SOCKET;
    hace_uiop_transport_t client_transport;
    uint8_t request_storage[8192];
    hace_cdr_buffer_t request_buf;
    uint8_t huge_argument[6000]; /* > 16 * (256 - 16 - 4) = 3776 max payload */
    hace_uiop_status_t status;

    memset(huge_argument, 0, sizeof(huge_argument));

    HACE_ASSERT_TRUE(hace_hal_socket_open(&client_socket, 58551U) == HACE_HAL_OK);
    client_transport = hace_uiop_transport_for_socket(&client_socket);

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 6U, "upload", huge_argument,
                                    sizeof(huge_argument)));

    status = hace_uiop_send_message(&client_transport, &request_buf, HACE_GIOP_MSG_REQUEST,
                                     6U, LOOPBACK_ADDR, 58552U,
                                     HACE_UIOP_MIN_DATAGRAM_SIZE);
    HACE_ASSERT_TRUE(status == HACE_UIOP_ERR_TOO_LARGE);

    HACE_ASSERT_TRUE(hace_hal_socket_close(client_socket) == HACE_HAL_OK);
    return 1;
}

static int test_receive_discards_unrelated_traffic(void)
{
    hace_hal_socket_handle_t receiver_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t sender_socket = HACE_HAL_INVALID_SOCKET;
    hace_uiop_transport_t receiver_transport;
    hace_uiop_transport_t sender_transport;
    uint32_t match_id = 42U;
    uint8_t noise_storage[256];
    hace_cdr_buffer_t noise_buf;
    uint8_t real_storage[256];
    hace_cdr_buffer_t real_buf;
    hace_giop_reply_header_t reply_header;
    uint8_t body[256];
    hace_giop_message_header_t out_header;
    uint32_t out_request_id = 0U;
    hace_giop_reply_status_t out_reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    size_t out_body_length = 0U;
    hace_uiop_status_t status;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&receiver_socket, 58561U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&sender_socket, 58562U) == HACE_HAL_OK);
    receiver_transport = hace_uiop_transport_for_socket(&receiver_socket);
    sender_transport = hace_uiop_transport_for_socket(&sender_socket);

    /* Noise: a Reply with an unrelated request_id. */
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&noise_buf, noise_storage,
                                           sizeof(noise_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&noise_buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    reply_header.request_id = 999U;
    reply_header.reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    HACE_ASSERT_TRUE(hace_giop_encode_reply_header(&noise_buf, &reply_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(&noise_buf, HACE_GIOP_MSG_REPLY, 0) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_uiop_send_message(&sender_transport, &noise_buf,
                                             HACE_GIOP_MSG_REPLY, 999U, LOOPBACK_ADDR,
                                             58561U,
                                             HACE_UIOP_MAX_DATAGRAM_SIZE) == HACE_UIOP_OK);

    /* The real reply, matching request_id. */
    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&real_buf, real_storage, sizeof(real_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_begin_message(&real_buf, HACE_CDR_BIG_ENDIAN) ==
                      HACE_GIOP_OK);
    reply_header.request_id = match_id;
    reply_header.reply_status = HACE_GIOP_REPLY_NO_EXCEPTION;
    HACE_ASSERT_TRUE(hace_giop_encode_reply_header(&real_buf, &reply_header) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_cdr_encode_raw_bytes(&real_buf, (const uint8_t *)"real", 4U) ==
                      HACE_CDR_OK);
    HACE_ASSERT_TRUE(hace_giop_finish_message(&real_buf, HACE_GIOP_MSG_REPLY, 0) ==
                      HACE_GIOP_OK);
    HACE_ASSERT_TRUE(hace_uiop_send_message(&sender_transport, &real_buf,
                                             HACE_GIOP_MSG_REPLY, match_id, LOOPBACK_ADDR,
                                             58561U,
                                             HACE_UIOP_MAX_DATAGRAM_SIZE) == HACE_UIOP_OK);

    status = hace_uiop_receive(&receiver_transport, HACE_GIOP_MSG_REPLY, &match_id, 1000U,
                                body, sizeof(body), &out_header, &out_request_id,
                                &out_reply_status, &out_body_length, NULL, NULL, NULL);

    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_EQ_INT(match_id, out_request_id);
    HACE_ASSERT_EQ_INT(4, out_body_length);
    HACE_ASSERT_TRUE(memcmp(body, "real", 4) == 0);

    HACE_ASSERT_TRUE(hace_hal_socket_close(receiver_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(sender_socket) == HACE_HAL_OK);
    return 1;
}

static int test_receive_reports_request_info(void)
{
    hace_hal_socket_handle_t receiver_socket = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t sender_socket = HACE_HAL_INVALID_SOCKET;
    hace_uiop_transport_t receiver_transport;
    hace_uiop_transport_t sender_transport;
    uint8_t request_storage[256];
    hace_cdr_buffer_t request_buf;
    uint8_t body[256];
    hace_giop_message_header_t out_header;
    uint32_t out_request_id = 0U;
    size_t out_body_length = 0U;
    hace_uiop_request_info_t request_info;
    hace_uiop_status_t status;

    memset(&request_info, 0, sizeof(request_info));

    HACE_ASSERT_TRUE(hace_hal_socket_open(&receiver_socket, 58571U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&sender_socket, 58572U) == HACE_HAL_OK);
    receiver_transport = hace_uiop_transport_for_socket(&receiver_socket);
    sender_transport = hace_uiop_transport_for_socket(&sender_socket);

    HACE_ASSERT_TRUE(hace_cdr_buffer_init(&request_buf, request_storage,
                                           sizeof(request_storage),
                                           HACE_CDR_BIG_ENDIAN) == HACE_CDR_OK);
    HACE_ASSERT_TRUE(build_request(&request_buf, 7U, "do_thing", NULL, 0U));
    HACE_ASSERT_TRUE(hace_uiop_send_message(&sender_transport, &request_buf,
                                             HACE_GIOP_MSG_REQUEST, 7U, LOOPBACK_ADDR,
                                             58571U,
                                             HACE_UIOP_MAX_DATAGRAM_SIZE) == HACE_UIOP_OK);

    status = hace_uiop_receive(&receiver_transport, HACE_GIOP_MSG_REQUEST, NULL, 1000U,
                                body, sizeof(body), &out_header, &out_request_id, NULL,
                                &out_body_length, NULL, NULL, &request_info);

    HACE_ASSERT_TRUE(status == HACE_UIOP_OK);
    HACE_ASSERT_EQ_INT(7, out_request_id);
    /* build_request always sets a 1-byte object key {0x01}. */
    HACE_ASSERT_EQ_INT(1, request_info.object_key_length);
    HACE_ASSERT_EQ_INT(0x01, request_info.object_key[0]);
    HACE_ASSERT_TRUE(strcmp(request_info.operation, "do_thing") == 0);
    HACE_ASSERT_EQ_INT(1, request_info.response_expected);

    HACE_ASSERT_TRUE(hace_hal_socket_close(receiver_socket) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(sender_socket) == HACE_HAL_OK);
    return 1;
}

int main(void)
{
    HACE_RUN_TEST(test_request_reply_roundtrip);
    HACE_RUN_TEST(test_retry_succeeds_after_dropped_send);
    HACE_RUN_TEST(test_call_times_out_when_nothing_listening);
    HACE_RUN_TEST(test_fragmentation_send_emits_multiple_datagrams);
    HACE_RUN_TEST(test_fragmentation_roundtrip);
    HACE_RUN_TEST(test_send_rejects_message_needing_too_many_fragments);
    HACE_RUN_TEST(test_receive_discards_unrelated_traffic);
    HACE_RUN_TEST(test_receive_reports_request_info);
    HACE_TEST_REPORT();
}
