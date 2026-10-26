/**
 * @file hal_lcd.h
 * @brief 800x480 RGB565 panel driven by the ESP32-S3 RGB peripheral from PSRAM.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_lcd_init(void);

/** Copy a full 800x480 RGB565 frame to the panel frame buffer. */
esp_err_t hal_lcd_present(const uint16_t *frame);

/** The hidden frame buffer (800x480 RGB565, PSRAM) to draw into, or NULL before init.
 *  It changes after every hal_lcd_flip(), so fetch it again for each frame. */
uint16_t *hal_lcd_framebuffer(void);

/** Show the buffer returned by hal_lcd_framebuffer() (no copy) and hand out the other one. */
esp_err_t hal_lcd_flip(void);

/** Backlight switch (EXIO2). */
void hal_lcd_backlight(bool on);

#ifdef __cplusplus
}
#endif
