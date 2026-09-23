/*
 * hace-corba Hardware/OS Abstraction Layer.
 *
 * Everything the runtime needs from the underlying platform: monotonic
 * time, UDP datagram sockets, bounded task creation, and fixed-block
 * memory pools. No function here may allocate memory dynamically (no
 * malloc/free) — pool and buffer storage is always supplied by the
 * caller. Handles are opaque integers, not pointers, so callers never
 * need to know backend-specific struct layouts.
 *
 * Backends: runtime/src/hal/linux/ (Ubuntu, POSIX) and
 * runtime/src/hal/integrity178/ (Green Hills INTEGRITY-178 tuMP, stub
 * pending access to the licensed Green Hills SDK — see that directory's
 * README).
 */
#ifndef HACE_HAL_H
#define HACE_HAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HACE_HAL_OK = 0,
    HACE_HAL_ERR = 1,
    HACE_HAL_ERR_TIMEOUT = 2,
    HACE_HAL_ERR_WOULD_BLOCK = 3,
    HACE_HAL_ERR_INVALID_ARGUMENT = 4,
    HACE_HAL_ERR_NOT_IMPLEMENTED = 5,
    HACE_HAL_ERR_RESOURCE_EXHAUSTED = 6
} hace_hal_status_t;

/* ---- Time ---- */

typedef struct {
    uint64_t seconds;
    uint32_t nanoseconds;
} hace_hal_time_t;

/* Monotonic clock reading; not wall-clock time. Used for retry/timeout
 * scheduling, never for calendar timestamps. */
hace_hal_status_t hace_hal_time_now(hace_hal_time_t *out_time);

/* ---- UDP sockets ---- */

typedef int32_t hace_hal_socket_handle_t;
#define HACE_HAL_INVALID_SOCKET ((hace_hal_socket_handle_t)-1)

/* Opens a UDP socket bound to local_port (0 = ephemeral port, backend
 * chooses). On success *out_handle is a valid handle to later pass to
 * send/recv/close. */
hace_hal_status_t hace_hal_socket_open(hace_hal_socket_handle_t *out_handle,
                                        uint16_t local_port);

hace_hal_status_t hace_hal_socket_close(hace_hal_socket_handle_t handle);

hace_hal_status_t hace_hal_socket_send(hace_hal_socket_handle_t handle,
                                        const uint8_t *data, size_t length,
                                        uint32_t dest_addr_ipv4,
                                        uint16_t dest_port);

/* Blocks up to timeout_ms for a datagram (0 = poll/non-blocking,
 * returns HACE_HAL_ERR_WOULD_BLOCK if nothing is pending). On success,
 * out_length holds the datagram size and out_src_addr_ipv4/out_src_port
 * the sender's address. Truncating silently is never done: if the datagram
 * is larger than capacity, HACE_HAL_ERR_INVALID_ARGUMENT is returned and
 * the datagram is discarded, matching UIOP's bounded-datagram design
 * (see docs/standards/spec-references.md). */
hace_hal_status_t hace_hal_socket_recv(hace_hal_socket_handle_t handle,
                                        uint8_t *buffer, size_t capacity,
                                        size_t *out_length,
                                        uint32_t *src_addr_ipv4,
                                        uint16_t *src_port,
                                        uint32_t timeout_ms);

/* ---- Bounded tasks/threads ---- */

typedef int32_t hace_hal_task_handle_t;
#define HACE_HAL_INVALID_TASK ((hace_hal_task_handle_t)-1)

typedef void (*hace_hal_task_fn_t)(void *arg);

/* Creates a single task/thread running fn(arg) at the given priority
 * (backend-defined range/meaning; on Linux this maps to a pthread
 * scheduling hint, on INTEGRITY-178 to a partition-scoped task
 * priority). There is no dynamic task pool: callers are expected to
 * create a small, statically-known number of tasks at startup, per the
 * DAL C-friendly bounded-resource design. */
hace_hal_status_t hace_hal_task_create(hace_hal_task_handle_t *out_handle,
                                        hace_hal_task_fn_t fn, void *arg,
                                        int32_t priority);

hace_hal_status_t hace_hal_task_join(hace_hal_task_handle_t handle);

/* ---- Fixed-block memory pool (portable, no dynamic allocation) ---- */

typedef struct {
    uint8_t *storage;     /* block_count * block_size bytes, caller-owned */
    uint8_t *free_bitmap; /* ceil(block_count / 8) bytes, caller-owned */
    size_t block_size;
    size_t block_count;
} hace_hal_pool_t;

hace_hal_status_t hace_hal_pool_init(hace_hal_pool_t *pool, uint8_t *storage,
                                      uint8_t *free_bitmap, size_t block_size,
                                      size_t block_count);

/* Returns NULL if the pool is exhausted. Never allocates from the heap. */
void *hace_hal_pool_alloc(hace_hal_pool_t *pool);

hace_hal_status_t hace_hal_pool_free(hace_hal_pool_t *pool, void *block);

#ifdef __cplusplus
}
#endif

#endif /* HACE_HAL_H */
