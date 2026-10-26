/**
 * @file k_route.h
 * @brief Route storage (NAV-02: up to 12,000 points in PSRAM, 16 bytes each).
 *
 * The BLE task fills it, the UI task reads it. While an upload is in progress
 * the route reads as empty, so the UI never draws a half-uploaded line.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "k_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t  lat_e6;
    int32_t  lon_e6;
    uint32_t mx;        /* mercator fixed point, see k_geo.h */
    uint32_t my;
} k_route_pt_t;

void k_route_init(void);

/* ---- writer side (BLE task) -------------------------------------------- */
bool k_route_begin(uint16_t expected_points);
bool k_route_put(uint16_t start, const kp_pt_t *pts, size_t n);
/** Finish an upload. Returns true if all points arrived in order. */
bool k_route_end(uint16_t *received);
/** NAV-05: removes the route immediately. */
void k_route_clear(void);

/* ---- reader side (UI task) --------------------------------------------- */
/** Lock and get the points. Pair every call with k_route_unlock(). */
const k_route_pt_t *k_route_lock(uint32_t *count);
void k_route_unlock(void);

#ifdef __cplusplus
}
#endif
