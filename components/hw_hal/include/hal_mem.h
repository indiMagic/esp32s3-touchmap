/**
 * @file hal_mem.h
 * @brief Memory placement helpers (PSRAM vs internal RAM).
 */
#pragma once

#include <stddef.h>
#include "esp_heap_caps.h"

/** Large, bandwidth-tolerant buffers (frame buffers, tile cache, route). */
static inline void *hal_psram_alloc(size_t bytes)
{
    return heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

/** Small, latency-sensitive buffers (queues, scratch). */
static inline void *hal_iram_alloc(size_t bytes)
{
    return heap_caps_calloc(1, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
