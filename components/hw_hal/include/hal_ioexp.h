/**
 * @file hal_ioexp.h
 * @brief CH422G I/O expander: touch reset, backlight, LCD reset, SD chip-select.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_ioexp_init(void);

/** Set one expander output (EXIOn). The shadow byte is kept in RAM. */
esp_err_t hal_ioexp_set(int exio, bool level);

/** Backlight is a plain on/off switch on this board (no PWM). */
esp_err_t hal_ioexp_backlight(bool on);

#ifdef __cplusplus
}
#endif
