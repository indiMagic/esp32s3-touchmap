/**
 * @file k_input.h
 * @brief Touch events: the input task polls the GT911 every 20 ms and turns it
 *        into DOWN / MOVE / UP events with deltas, so the UI never sees I2C.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    K_TOUCH_DOWN = 0,
    K_TOUCH_MOVE,
    K_TOUCH_UP,
} k_touch_type_t;

typedef struct {
    k_touch_type_t type;
    int16_t x, y;          /* current position */
    int16_t dx, dy;        /* movement since the previous event */
    int16_t sx, sy;        /* where this touch started (for tap vs drag) */
} k_touch_evt_t;

void k_input_start(void);

/** Pop the oldest event; returns false if none is queued. Non-blocking. */
bool k_input_poll(k_touch_evt_t *out);

#ifdef __cplusplus
}
#endif
