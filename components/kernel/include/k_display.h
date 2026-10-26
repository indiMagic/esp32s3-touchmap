/**
 * @file k_display.h
 * @brief Back buffer ownership and frame presentation.
 *
 * The UI draws into the back buffer (RGB565, 800x480) and calls
 * k_display_present() to copy it to the panel.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define K_DISPLAY_W 800
#define K_DISPLAY_H 480

uint16_t *k_display_backbuffer(void);
void      k_display_present(void);
void      k_display_backlight(bool on);

#ifdef __cplusplus
}
#endif
