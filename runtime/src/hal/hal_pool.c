/*
 * Portable fixed-block memory pool — the same implementation on every
 * backend, since it touches no OS facilities. See hal.h for the API
 * contract (no dynamic allocation; all storage is caller-provided).
 */
#include "hace/hal.h"

hace_hal_status_t hace_hal_pool_init(hace_hal_pool_t *pool, uint8_t *storage,
                                      uint8_t *free_bitmap, size_t block_size,
                                      size_t block_count)
{
    size_t i;

    if ((pool == NULL) || (storage == NULL) || (free_bitmap == NULL) ||
        (block_size == 0U) || (block_count == 0U)) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    pool->storage = storage;
    pool->free_bitmap = free_bitmap;
    pool->block_size = block_size;
    pool->block_count = block_count;

    for (i = 0U; i < ((block_count + 7U) / 8U); i++) {
        pool->free_bitmap[i] = 0xFFU; /* all blocks free */
    }

    return HACE_HAL_OK;
}

void *hace_hal_pool_alloc(hace_hal_pool_t *pool)
{
    size_t i;

    if (pool == NULL) {
        return NULL;
    }

    for (i = 0U; i < pool->block_count; i++) {
        size_t byte_index = i / 8U;
        uint8_t bit_mask = (uint8_t)(1U << (i % 8U));

        if ((pool->free_bitmap[byte_index] & bit_mask) != 0U) {
            pool->free_bitmap[byte_index] &= (uint8_t)~bit_mask;
            return &pool->storage[i * pool->block_size];
        }
    }

    return NULL; /* pool exhausted */
}

hace_hal_status_t hace_hal_pool_free(hace_hal_pool_t *pool, void *block)
{
    ptrdiff_t offset;
    size_t index;
    size_t byte_index;
    uint8_t bit_mask;

    if ((pool == NULL) || (block == NULL)) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    offset = (uint8_t *)block - pool->storage;
    if ((offset < 0) || (((size_t)offset % pool->block_size) != 0U)) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    index = (size_t)offset / pool->block_size;
    if (index >= pool->block_count) {
        return HACE_HAL_ERR_INVALID_ARGUMENT;
    }

    byte_index = index / 8U;
    bit_mask = (uint8_t)(1U << (index % 8U));
    pool->free_bitmap[byte_index] |= bit_mask;

    return HACE_HAL_OK;
}
