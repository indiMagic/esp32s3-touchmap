/**
 * @file map_view.h
 * @brief The 800x480 window over the tile grid: tiles, route, destination,
 *        position arrow. Owns zoom, centre and follow mode.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "k_nav.h"

#ifdef __cplusplus
extern "C" {
#endif

void map_view_init(void);

/** Draw tiles, route, markers into the back buffer (no HUD). */
void map_view_draw(const k_nav_snapshot_t *nav);

void map_view_pan(int dx_px, int dy_px);     /* MAP-02: one-finger pan; leaves follow mode */
void map_view_zoom_by(int delta);            /* clamped to the zoom levels on the card */
void map_view_recenter(void);                /* back to follow mode */

uint8_t map_view_zoom(void);
bool    map_view_following(void);

/** Centre as latitude/longitude (e6), for the status bar. */
void    map_view_centre_latlon(int32_t *lat_e6, int32_t *lon_e6);

#ifdef __cplusplus
}
#endif
