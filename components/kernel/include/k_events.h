/**
 * @file k_events.h
 * @brief System-wide "something changed" event bits. The UI task sleeps on
 *        these and redraws only when needed.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEV_POS      (1u << 0)   /* new phone position            */
#define KEV_NAV      (1u << 1)   /* new turn instruction          */
#define KEV_ROUTE    (1u << 2)   /* route uploaded / cleared      */
#define KEV_TILE     (1u << 3)   /* a tile finished decoding      */
#define KEV_INPUT    (1u << 4)   /* touch event queued            */
#define KEV_LINK     (1u << 5)   /* BLE state changed             */
#define KEV_STORAGE  (1u << 6)   /* SD state or packs changed     */
#define KEV_SETTINGS (1u << 7)   /* a setting changed             */
#define KEV_ALL      0xFFu

void     k_events_init(void);
void     k_events_post(uint32_t bits);

/** Wait for any bit in @p mask; returns the bits that fired (cleared). */
uint32_t k_events_wait(uint32_t mask, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
