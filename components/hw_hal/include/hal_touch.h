/**
 * @file hal_touch.h
 * @brief GT911 capacitive touch controller (single touch point is used).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_touch_init(void);

/**
 * Poll the controller. Returns true and fills x/y when a finger is down.
 * Returns false when no touch. Never blocks longer than one I2C transaction.
 */
bool hal_touch_read(uint16_t *x, uint16_t *y);

#ifdef __cplusplus
}
#endif
