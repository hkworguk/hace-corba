#include "../src/hal/hal.h"
#include "test_framework.h"

#include <string.h>

static int test_monotonic_time_moves_forward(void)
{
    hace_hal_time_t first;
    hace_hal_time_t second;

    HACE_ASSERT_TRUE(hace_hal_time_now(&first) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_time_now(&second) == HACE_HAL_OK);
    HACE_ASSERT_TRUE((second.seconds > first.seconds) ||
                      ((second.seconds == first.seconds) &&
                       (second.nanoseconds >= first.nanoseconds)));
    return 1;
}

static int test_udp_loopback_roundtrip(void)
{
    hace_hal_socket_handle_t receiver = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t sender = HACE_HAL_INVALID_SOCKET;
    const char *message = "hace-corba hal test";
    uint8_t buffer[64];
    size_t received_length = 0U;
    uint32_t src_addr = 0U;
    uint16_t src_port = 0U;
    uint32_t loopback = (127U << 24) | 1U; /* 127.0.0.1 */

    HACE_ASSERT_TRUE(hace_hal_socket_open(&receiver, 58241U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&sender, 58242U) == HACE_HAL_OK);

    HACE_ASSERT_TRUE(hace_hal_socket_send(sender, (const uint8_t *)message,
                                           strlen(message), loopback,
                                           58241U) == HACE_HAL_OK);

    HACE_ASSERT_TRUE(hace_hal_socket_recv(receiver, buffer, sizeof(buffer),
                                           &received_length, &src_addr,
                                           &src_port, 1000U) == HACE_HAL_OK);
    HACE_ASSERT_EQ_INT(strlen(message), received_length);
    HACE_ASSERT_TRUE(memcmp(buffer, message, received_length) == 0);
    HACE_ASSERT_EQ_INT(loopback, src_addr);

    HACE_ASSERT_TRUE(hace_hal_socket_close(receiver) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(sender) == HACE_HAL_OK);
    return 1;
}

static int test_udp_recv_would_block_when_idle(void)
{
    /* Regression test: SO_RCVTIMEO set to {0,0} means "block forever" on
     * Linux, not "don't block" -- timeout_ms == 0 must use a genuinely
     * non-blocking path (MSG_DONTWAIT), or this call hangs forever
     * instead of returning HACE_HAL_ERR_WOULD_BLOCK. */
    hace_hal_socket_handle_t idle = HACE_HAL_INVALID_SOCKET;
    uint8_t buffer[16];
    size_t received_length = 0U;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&idle, 58243U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_recv(idle, buffer, sizeof(buffer),
                                           &received_length, NULL, NULL,
                                           0U) == HACE_HAL_ERR_WOULD_BLOCK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(idle) == HACE_HAL_OK);
    return 1;
}

static int test_udp_recv_times_out_when_idle(void)
{
    hace_hal_socket_handle_t idle = HACE_HAL_INVALID_SOCKET;
    uint8_t buffer[16];
    size_t received_length = 0U;

    HACE_ASSERT_TRUE(hace_hal_socket_open(&idle, 58244U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_recv(idle, buffer, sizeof(buffer),
                                           &received_length, NULL, NULL,
                                           50U) == HACE_HAL_ERR_TIMEOUT);
    HACE_ASSERT_TRUE(hace_hal_socket_close(idle) == HACE_HAL_OK);
    return 1;
}

static int test_oversized_datagram_is_rejected_not_truncated(void)
{
    hace_hal_socket_handle_t receiver = HACE_HAL_INVALID_SOCKET;
    hace_hal_socket_handle_t sender = HACE_HAL_INVALID_SOCKET;
    uint8_t big_message[32];
    uint8_t small_buffer[8];
    size_t received_length = 0U;
    uint32_t loopback = (127U << 24) | 1U;
    size_t i;

    for (i = 0U; i < sizeof(big_message); i++) {
        big_message[i] = (uint8_t)i;
    }

    HACE_ASSERT_TRUE(hace_hal_socket_open(&receiver, 58245U) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_open(&sender, 58246U) == HACE_HAL_OK);

    HACE_ASSERT_TRUE(hace_hal_socket_send(sender, big_message,
                                           sizeof(big_message), loopback,
                                           58245U) == HACE_HAL_OK);

    HACE_ASSERT_TRUE(hace_hal_socket_recv(receiver, small_buffer,
                                           sizeof(small_buffer),
                                           &received_length, NULL, NULL,
                                           1000U) ==
                      HACE_HAL_ERR_INVALID_ARGUMENT);

    HACE_ASSERT_TRUE(hace_hal_socket_close(receiver) == HACE_HAL_OK);
    HACE_ASSERT_TRUE(hace_hal_socket_close(sender) == HACE_HAL_OK);
    return 1;
}

#define HACE_CONCURRENCY_TASK_COUNT 8

typedef struct {
    uint16_t port;
    int ok;
} hace_concurrency_task_ctx_t;

static void hace_concurrency_task(void *arg)
{
    hace_concurrency_task_ctx_t *ctx = (hace_concurrency_task_ctx_t *)arg;
    hace_hal_socket_handle_t sock = HACE_HAL_INVALID_SOCKET;
    uint8_t message[4];
    uint8_t buffer[4];
    size_t received_length = 0U;
    uint32_t loopback = (127U << 24) | 1U;

    ctx->ok = 0;

    message[0] = (uint8_t)(ctx->port & 0xFFU);
    message[1] = (uint8_t)((ctx->port >> 8) & 0xFFU);
    message[2] = 0xABU;
    message[3] = 0xCDU;

    if (hace_hal_socket_open(&sock, ctx->port) != HACE_HAL_OK) {
        return;
    }
    if (hace_hal_socket_send(sock, message, sizeof(message), loopback,
                              ctx->port) != HACE_HAL_OK) {
        (void)hace_hal_socket_close(sock);
        return;
    }
    if (hace_hal_socket_recv(sock, buffer, sizeof(buffer), &received_length,
                              NULL, NULL, 1000U) != HACE_HAL_OK) {
        (void)hace_hal_socket_close(sock);
        return;
    }
    if ((received_length != sizeof(message)) ||
        (memcmp(buffer, message, sizeof(message)) != 0)) {
        (void)hace_hal_socket_close(sock);
        return;
    }
    if (hace_hal_socket_close(sock) != HACE_HAL_OK) {
        return;
    }
    ctx->ok = 1;
}

static int test_concurrent_socket_open_is_race_free(void)
{
    /* Regression test: hace_hal_socket_open/close used to race on a
     * global, unlocked slot table -- concurrent opens from different
     * tasks could alias the same handle to different file descriptors.
     * Every task here opens its own port, round-trips a unique message
     * to itself over loopback, and must see exactly that message back:
     * any cross-talk between tasks means the slot table let two tasks
     * collide on the same handle. */
    hace_hal_task_handle_t tasks[HACE_CONCURRENCY_TASK_COUNT];
    hace_concurrency_task_ctx_t contexts[HACE_CONCURRENCY_TASK_COUNT];
    int i;
    int all_ok = 1;

    for (i = 0; i < HACE_CONCURRENCY_TASK_COUNT; i++) {
        contexts[i].port = (uint16_t)(58250 + i);
        contexts[i].ok = 0;
        HACE_ASSERT_TRUE(hace_hal_task_create(&tasks[i], hace_concurrency_task,
                                               &contexts[i], 0) == HACE_HAL_OK);
    }

    for (i = 0; i < HACE_CONCURRENCY_TASK_COUNT; i++) {
        HACE_ASSERT_TRUE(hace_hal_task_join(tasks[i]) == HACE_HAL_OK);
        if (!contexts[i].ok) {
            all_ok = 0;
        }
    }

    HACE_ASSERT_TRUE(all_ok);
    return 1;
}

int main(void)
{
    HACE_RUN_TEST(test_monotonic_time_moves_forward);
    HACE_RUN_TEST(test_udp_loopback_roundtrip);
    HACE_RUN_TEST(test_udp_recv_would_block_when_idle);
    HACE_RUN_TEST(test_udp_recv_times_out_when_idle);
    HACE_RUN_TEST(test_oversized_datagram_is_rejected_not_truncated);
    HACE_RUN_TEST(test_concurrent_socket_open_is_race_free);
    HACE_TEST_REPORT();
}
