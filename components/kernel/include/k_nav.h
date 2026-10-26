/**
 * @file k_nav.h
 * @brief Shared navigation state: phone position, next turn, link state.
 *
 * Writers: BLE task. Reader: UI task via k_nav_snapshot() (a locked copy).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "k_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define K_POS_STALE_MS  10000    /* NAV-04 */

typedef enum {
    K_LINK_OFF = 0,
    K_LINK_ADVERTISING,
    K_LINK_CONNECTED,
} k_link_t;

typedef struct {
    /* position */
    bool     has_fix;
    int32_t  lat_e6, lon_e6;
    uint16_t heading_d10, speed_d10, accuracy_m;
    uint32_t pos_ms;            /* hal_millis() of the last POS */

    /* next turn */
    bool     has_nav;
    uint8_t  maneuver, flags;
    uint32_t to_turn_m, remain_m, remain_s;
    char     text[KP_NAV_TEXT_MAX + 1];

    /* route totals from ROUTE_BEGIN */
    bool     route_active;
    uint32_t route_dist_m, route_dur_s;

    k_link_t link;
} k_nav_snapshot_t;

void k_nav_init(void);
void k_nav_set_pos(const kp_pos_t *p);
void k_nav_set_turn(const kp_nav_t *n);
void k_nav_set_route(bool active, uint32_t dist_m, uint32_t dur_s);
void k_nav_set_link(k_link_t link);
void k_nav_clear_turn(void);

void k_nav_snapshot(k_nav_snapshot_t *out);

/** NAV-04: position is "unknown, not stale" after K_POS_STALE_MS without POS. */
bool k_nav_pos_valid(const k_nav_snapshot_t *s, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
