/**
 * @file k_tiles.h
 * @brief Decoded-tile cache (32 tiles, LRU) and the background loader task.
 *
 * The UI never waits for the SD card (MAP-04):
 *
 *   UI:      k_tiles_begin_frame()  -> k_tiles_get() x N -> k_tiles_end_frame()
 *   missing: slot marked PENDING, request queued, UI draws a grey block
 *   loader:  pack read -> JPEG decode into the slot -> READY -> KEV_TILE
 *
 * Pixels are RGB565, 256x256, row-major, in PSRAM.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define K_TILE_CACHE_SLOTS  32
#define K_TILE_PIXELS       (256 * 256)
#define K_TILE_BYTES        (K_TILE_PIXELS * 2)     /* 128 KiB */

void k_tiles_start(void);

/**
 * Lock the cache for one frame and tell the loader which tiles are wanted
 * (inclusive rectangle at zoom z). Requests outside it are dropped.
 */
void k_tiles_begin_frame(uint8_t z, int32_t tx0, int32_t ty0, int32_t tx1, int32_t ty1);

/** Pixels if the tile is decoded; NULL if missing/pending/failed (a load is queued). */
const uint16_t *k_tiles_get(uint8_t z, int32_t tx, int32_t ty);

void k_tiles_end_frame(void);

/** Drop everything (pack set changed). */
void k_tiles_flush(void);

#ifdef __cplusplus
}
#endif
