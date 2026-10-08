/*
 * Ubuntu Linux HAL backend: POSIX sockets, clock_gettime, pthreads.
 *
 * This backend is for host development, testing, and CI — it is not
 * itself a DAL C-certified target, but is written to the same interface
 * contract (hal.h) as the INTEGRITY-178 backend so the runtime above it
 * is identical on both platforms.
 */
#define _POSIX_C_SOURCE 200809L

#include "hace/hal.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define HACE_HAL_MAX_SOCKETS 16
#define HACE_HAL_MAX_TASKS 16

/* The socket/task tables below are process-global, and hace_hal_task_create
 * exists precisely so callers can run multiple concurrent tasks -- so
 * these tables must be safe to access from more than one thread at once.
 * Each table gets its own mutex, held only across the bookkeeping
 * (never across a blocking syscall like recvfrom's timeout wait or
 * pthread_join), so concurrent HAL calls from different tasks don't
 * corrupt each other's slot assignments. */
static pthread_mutex_t g_socket_table_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_sockets[HACE_HAL_MAX_SOCKETS];
static int g_sockets_in_use[HACE_HAL_MAX_SOCKETS];

static pthread_mutex_t g_task_table_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_tasks[HACE_HAL_MAX_TASKS];
static int g_tasks_in_use[HACE_HAL_MAX_TASKS];

hace_hal_status_t hace_hal_time_now(hace_hal_time_t *out_time)
{
    struct timespec ts;

    if (out_time == NULL) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return HACE_HAL_ERR;
    }

    out_time->seconds = (uint64_t)ts.tv_sec;
    out_time->nanoseconds = (uint32_t)ts.tv_nsec;
    return HACE_HAL_OK;
}

hace_hal_status_t hace_hal_socket_open(hace_hal_socket_handle_t *out_handle,
                                        uint16_t local_port)
{
    int fd;
    struct sockaddr_in addr;
    int32_t slot = -1;
    int32_t i;

    if (out_handle == NULL) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    (void)pthread_mutex_lock(&g_socket_table_mutex);

    for (i = 0; i < HACE_HAL_MAX_SOCKETS; i++) {
        if (!g_sockets_in_use[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        (void)pthread_mutex_unlock(&g_socket_table_mutex);
        return HACE_HAL_ERR_RESOURCE_EXHAUSTED;
    }
    /* Claim the slot before releasing the lock for the syscalls below,
     * so no other thread can race us for it. */
    g_sockets_in_use[slot] = 1;
    (void)pthread_mutex_unlock(&g_socket_table_mutex);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        (void)pthread_mutex_lock(&g_socket_table_mutex);
        g_sockets_in_use[slot] = 0;
        (void)pthread_mutex_unlock(&g_socket_table_mutex);
        return HACE_HAL_ERR;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(local_port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        (void)close(fd);
        (void)pthread_mutex_lock(&g_socket_table_mutex);
        g_sockets_in_use[slot] = 0;
        (void)pthread_mutex_unlock(&g_socket_table_mutex);
        return HACE_HAL_ERR;
    }

    (void)pthread_mutex_lock(&g_socket_table_mutex);
    g_sockets[slot] = fd;
    (void)pthread_mutex_unlock(&g_socket_table_mutex);

    *out_handle = (hace_hal_socket_handle_t)slot;
    return HACE_HAL_OK;
}

static int hace_hal_fd_for_handle(hace_hal_socket_handle_t handle)
{
    int fd = -1;

    (void)pthread_mutex_lock(&g_socket_table_mutex);
    if ((handle >= 0) && (handle < HACE_HAL_MAX_SOCKETS) &&
        g_sockets_in_use[handle]) {
        fd = g_sockets[handle];
    }
    (void)pthread_mutex_unlock(&g_socket_table_mutex);
    return fd;
}

hace_hal_status_t hace_hal_socket_close(hace_hal_socket_handle_t handle)
{
    int fd;

    (void)pthread_mutex_lock(&g_socket_table_mutex);
    if ((handle < 0) || (handle >= HACE_HAL_MAX_SOCKETS) ||
        !g_sockets_in_use[handle]) {
        (void)pthread_mutex_unlock(&g_socket_table_mutex);
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }
    fd = g_sockets[handle];
    g_sockets_in_use[handle] = 0;
    (void)pthread_mutex_unlock(&g_socket_table_mutex);

    (void)close(fd);
    return HACE_HAL_OK;
}

hace_hal_status_t hace_hal_socket_send(hace_hal_socket_handle_t handle,
                                        const uint8_t *data, size_t length,
                                        uint32_t dest_addr_ipv4,
                                        uint16_t dest_port)
{
    int fd = hace_hal_fd_for_handle(handle);
    struct sockaddr_in addr;
    ssize_t sent;

    if ((fd < 0) || (data == NULL)) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(dest_addr_ipv4);
    addr.sin_port = htons(dest_port);

    sent = sendto(fd, data, length, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0 || (size_t)sent != length) {
        return HACE_HAL_ERR;
    }
    return HACE_HAL_OK;
}

hace_hal_status_t hace_hal_socket_recv(hace_hal_socket_handle_t handle,
                                        uint8_t *buffer, size_t capacity,
                                        size_t *out_length,
                                        uint32_t *src_addr_ipv4,
                                        uint16_t *src_port,
                                        uint32_t timeout_ms)
{
    int fd = hace_hal_fd_for_handle(handle);
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);
    struct timeval tv;
    ssize_t received;
    int recv_flags = MSG_TRUNC;

    if ((fd < 0) || (buffer == NULL) || (out_length == NULL)) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    /* A zero struct timeval passed to SO_RCVTIMEO means "no timeout"
     * (block forever) on Linux, not "don't block" -- so timeout_ms == 0
     * (poll/non-blocking, per hal.h) is handled separately via
     * MSG_DONTWAIT instead of relying on SO_RCVTIMEO. */
    if (timeout_ms == 0U) {
        recv_flags |= MSG_DONTWAIT;
    } else {
        tv.tv_sec = (time_t)(timeout_ms / 1000U);
        tv.tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U);
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
            return HACE_HAL_ERR;
        }
    }

    /* A single call: MSG_TRUNC makes recvfrom report a UDP datagram's
     * true size even when it's larger than `capacity` (still only
     * writing `capacity` bytes into buffer), so an oversized datagram
     * can be detected and rejected without a separate peek step. This
     * matters beyond simplicity: peek-then-read is two syscalls with a
     * window between them, and multiple tasks calling this
     * concurrently on the same socket (the POA worker-task pool, see
     * docs/architecture/adr-0004-corba-core.md) could have one task's
     * peek describe a datagram a different task then consumes first,
     * leaving the first task to silently read and misreport whatever
     * arrives next instead. One atomic call has no such window. */
    received = recvfrom(fd, buffer, capacity, recv_flags, (struct sockaddr *)&src,
                         &src_len);
    if (received < 0) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return (timeout_ms == 0U) ? HACE_HAL_ERR_WOULD_BLOCK
                                       : HACE_HAL_ERR_TIMEOUT;
        }
        return HACE_HAL_ERR;
    }
    if ((size_t)received > capacity) {
        /* The datagram was still consumed (removed from the queue) by
         * the call above; reject rather than accept the truncated
         * bytes now sitting in buffer. */
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    *out_length = (size_t)received;
    if (src_addr_ipv4 != NULL) {
        *src_addr_ipv4 = ntohl(src.sin_addr.s_addr);
    }
    if (src_port != NULL) {
        *src_port = ntohs(src.sin_port);
    }
    return HACE_HAL_OK;
}

typedef struct {
    hace_hal_task_fn_t fn;
    void *arg;
} hace_hal_task_trampoline_args_t;

static hace_hal_task_trampoline_args_t g_task_args[HACE_HAL_MAX_TASKS];

static void *hace_hal_task_trampoline(void *arg)
{
    hace_hal_task_trampoline_args_t *targs =
        (hace_hal_task_trampoline_args_t *)arg;
    targs->fn(targs->arg);
    return NULL;
}

hace_hal_status_t hace_hal_task_create(hace_hal_task_handle_t *out_handle,
                                        hace_hal_task_fn_t fn, void *arg,
                                        int32_t priority)
{
    int32_t slot = -1;
    int32_t i;

    (void)priority; /* Linux backend: no scheduling hint applied yet. */

    if ((out_handle == NULL) || (fn == NULL)) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    (void)pthread_mutex_lock(&g_task_table_mutex);
    for (i = 0; i < HACE_HAL_MAX_TASKS; i++) {
        if (!g_tasks_in_use[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        (void)pthread_mutex_unlock(&g_task_table_mutex);
        return HACE_HAL_ERR_RESOURCE_EXHAUSTED;
    }
    /* Claim the slot and stash the trampoline args before releasing the
     * lock and starting the thread, so no other thread can race us for
     * this slot. */
    g_tasks_in_use[slot] = 1;
    g_task_args[slot].fn = fn;
    g_task_args[slot].arg = arg;
    (void)pthread_mutex_unlock(&g_task_table_mutex);

    if (pthread_create(&g_tasks[slot], NULL, hace_hal_task_trampoline,
                        &g_task_args[slot]) != 0) {
        (void)pthread_mutex_lock(&g_task_table_mutex);
        g_tasks_in_use[slot] = 0;
        (void)pthread_mutex_unlock(&g_task_table_mutex);
        return HACE_HAL_ERR;
    }

    *out_handle = (hace_hal_task_handle_t)slot;
    return HACE_HAL_OK;
}

hace_hal_status_t hace_hal_task_join(hace_hal_task_handle_t handle)
{
    pthread_t thread;

    (void)pthread_mutex_lock(&g_task_table_mutex);
    if ((handle < 0) || (handle >= HACE_HAL_MAX_TASKS) ||
        !g_tasks_in_use[handle]) {
        (void)pthread_mutex_unlock(&g_task_table_mutex);
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }
    thread = g_tasks[handle];
    (void)pthread_mutex_unlock(&g_task_table_mutex);

    /* pthread_join blocks until the task finishes -- must not hold the
     * table lock across that wait, or every other HAL task call would
     * stall behind it. */
    if (pthread_join(thread, NULL) != 0) {
        return HACE_HAL_ERR;
    }

    (void)pthread_mutex_lock(&g_task_table_mutex);
    g_tasks_in_use[handle] = 0;
    (void)pthread_mutex_unlock(&g_task_table_mutex);
    return HACE_HAL_OK;
}
