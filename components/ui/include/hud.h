/**
 * @file hud.h
 * @brief Heads-up display drawn over the map: turn banner, status bar,
 *        attribution and the four round-button column.
 */
#pragma once

#include <stdbool.h>
#include "k_nav.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HUD_BTN_NONE = -1,
    HUD_BTN_ZOOM_IN = 0,
    HUD_BTN_ZOOM_OUT,
    HUD_BTN_RECENTER,
    HUD_BTN_SETTINGS,
    HUD_BTN_COUNT
} hud_button_t;

/** Turn banner (NAV-01), "waiting" bar when no phone, status bar, buttons. */
void hud_draw(const k_nav_snapshot_t *nav, uint8_t zoom, bool following,
              int32_t centre_lat_e6, int32_t centre_lon_e6);

/** Which button contains the point, or HUD_BTN_NONE. */
hud_button_t hud_hit_test(int x, int y);

/** Format a distance for the user's unit setting ("300 m", "0.4 mi"). */
void hud_fmt_distance(char *out, unsigned n, uint32_t metres);

#ifdef __cplusplus
}
#endif
