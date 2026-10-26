/**
 * @file k_power.h
 * @brief Screen timeout: switches the backlight off after inactivity.
 *        (Backlight is an on/off switch on this board, so there is no dimming.)
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void k_power_init(void);

/** Record user activity (touch). */
void k_power_poke(void);

/** Call periodically; turns the backlight off when the timeout expires. */
void k_power_tick(void);

/** If the screen is off, switch it on and return true (caller swallows the touch). */
bool k_power_wake_if_asleep(void);

bool k_power_asleep(void);

#ifdef __cplusplus
}
#endif
