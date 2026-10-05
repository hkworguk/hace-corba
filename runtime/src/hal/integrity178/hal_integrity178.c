/*
 * Green Hills INTEGRITY-178 tuMP HAL backend — STUB.
 *
 * Every function here returns HACE_HAL_ERR_NOT_IMPLEMENTED. This backend
 * cannot be completed without access to the licensed Green Hills SDK and
 * its (typically NDA'd) API documentation, which was not available when
 * this project was scaffolded. See README.md in this directory for what
 * a real implementation needs to do.
 *
 * This file exists so the runtime and build system have a real
 * translation unit to link against for the integrity178 target, and so
 * the HAL contract (hal.h) has two implementations to keep it honest —
 * even though this one does nothing yet.
 */
#include "hace/hal.h"

hace_hal_status_t hace_hal_time_now(hace_hal_time_t *out_time)
{
    (void)out_time;
    return HACE_HAL_ERR_NOT_IMPLEMENTED;
}

hace_hal_status_t hace_hal_socket_open(hace_hal_socket_handle_t *out_handle,
                                        uint16_t local_port)
{
    (void)out_handle;
    (void)local_port;
    return HACE_HAL_ERR_NOT_IMPLEMENTED;
}

hace_hal_status_t hace_hal_socket_close(hace_hal_socket_handle_t handle)
{
    (void)handle;
    return HACE_HAL_ERR_NOT_IMPLEMENTED;
}

hace_hal_status_t hace_hal_socket_send(hace_hal_socket_handle_t handle,
                                        const uint8_t *data, size_t length,
                                        uint32_t dest_addr_ipv4,
                                        uint16_t dest_port)
{
    (void)handle;
    (void)data;
    (void)length;
    (void)dest_addr_ipv4;
    (void)dest_port;
    return HACE_HAL_ERR_NOT_IMPLEMENTED;
}

hace_hal_status_t hace_hal_socket_recv(hace_hal_socket_handle_t handle,
                                        uint8_t *buffer, size_t capacity,
                                        size_t *out_length,
                                        uint32_t *src_addr_ipv4,
                                        uint16_t *src_port,
                                        uint32_t timeout_ms)
{
    (void)handle;
    (void)buffer;
    (void)capacity;
    (void)out_length;
    (void)src_addr_ipv4;
    (void)src_port;
    (void)timeout_ms;
    return HACE_HAL_ERR_NOT_IMPLEMENTED;
}

hace_hal_status_t hace_hal_task_create(hace_hal_task_handle_t *out_handle,
                                        hace_hal_task_fn_t fn, void *arg,
                                        int32_t priority)
{
    (void)out_handle;
    (void)fn;
    (void)arg;
    (void)priority;
    return HACE_HAL_ERR_NOT_IMPLEMENTED;
}

hace_hal_status_t hace_hal_task_join(hace_hal_task_handle_t handle)
{
    (void)handle;
    return HACE_HAL_ERR_NOT_IMPLEMENTED;
}
