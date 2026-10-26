/**
 * @file k_storage.h
 * @brief SD card service: mount, hot-plug monitor, tile-pack registry.
 *
 * State machine (SRS Figure 14):
 *   MOUNTING -> READY (packs found) | NODATA (mounted, no zNN.tpk) | MISSING
 *   READY/NODATA -> MISSING when the card is removed; back to MOUNTING on retry.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    K_SD_MOUNTING = 0,
    K_SD_READY,
    K_SD_NODATA,
    K_SD_MISSING,
} k_sd_state_t;

typedef struct {
    bool     present;
    uint8_t  zoom;
    uint32_t x0, y0, w, h;      /* index rectangle in tile units */
    uint32_t file_bytes;
} k_pack_info_t;

typedef enum {
    K_TILE_OK = 0,
    K_TILE_NONE,      /* index says "no tile here", or outside the pack */
    K_TILE_IOERR,     /* card read failed */
    K_TILE_BAD,       /* index entry inconsistent with the file */
} k_tile_rc_t;

/** Start the monitor task: mounts, scans packs, watches for removal/insertion. */
void k_storage_start(void);

k_sd_state_t k_storage_state(void);
const char  *k_storage_state_text(void);   /* user-facing error line (SD-03) */
void         k_storage_rescan(void);        /* ask the monitor to rescan now */

/** Increments whenever the set of packs changes; the tile cache watches it. */
uint32_t     k_storage_generation(void);

/* ---- packs ------------------------------------------------------------- */
bool                 k_packs_has(uint8_t zoom);
int                  k_packs_zoom_min(void);   /* -1 if none */
int                  k_packs_zoom_max(void);
uint32_t             k_packs_count(void);
const k_pack_info_t *k_packs_info(uint8_t zoom);
const char          *k_packs_attribution(void);  /* "" if none (MAP-08) */

/** Capacity for the Storage screen (SD-05). Returns false if no card. */
bool k_storage_space(uint64_t *total, uint64_t *free_bytes);

/** One index read + one tile read. @p len receives the JPEG size. */
k_tile_rc_t k_packs_read_tile(uint8_t zoom, int32_t x, int32_t y,
                              uint8_t *buf, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif
