/**
 * @file k_geo.h
 * @brief Web Mercator math in 32-bit fixed point (PORTABLE, host-testable).
 *
 * A "mercator coordinate" (mx, my) is the position on the whole world map
 * scaled to 0..2^32-1. At zoom z the world is 256 * 2^z pixels wide, so
 *     world_px = m >> (24 - z)         (valid for z <= 24; firmware uses z <= 20)
 * Storing fixed point lets the route keep one u32 pair per point and makes
 * panning exact (no float drift) at every zoom level.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KG_TILE_PX    256
#define KG_MAX_ZOOM   20

uint32_t kg_merc_x(int32_t lon_e6);
uint32_t kg_merc_y(int32_t lat_e6);
void     kg_merc_to_latlon(uint32_t mx, uint32_t my, int32_t *lat_e6, int32_t *lon_e6);

/** Mercator fixed point -> world pixel at zoom z. */
static inline int32_t kg_to_px(uint32_t m, uint8_t z)
{
    return (int32_t)(m >> (24 - z));
}

/** World pixel -> mercator fixed point (saturating). */
uint32_t kg_from_px(int64_t px, uint8_t z);

/** Tile column/row for a world pixel (floor division by 256). */
static inline int32_t kg_tile_of(int32_t world_px)
{
    return world_px >> 8;
}

typedef struct {
    int32_t ox;   /* world pixel at the left edge of the screen */
    int32_t oy;   /* world pixel at the top edge of the screen  */
} kg_view_t;

/** View origin = floor(centre - half the screen). */
kg_view_t kg_view_origin(uint32_t cx_m, uint32_t cy_m, uint8_t z, int w, int h);

/** Move a mercator centre by a screen-space drag (dx, dy in pixels). */
void kg_pan(uint32_t *cx_m, uint32_t *cy_m, uint8_t z, int dx_px, int dy_px);

#ifdef __cplusplus
}
#endif
