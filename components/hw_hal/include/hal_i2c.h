/**
 * @file hal_i2c.h
 * @brief Shared I2C master bus. Devices are addressed through opaque handles.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_i2c_dev *hal_i2c_dev_t;

esp_err_t hal_i2c_init(void);

/** Register a 7-bit device on the bus. */
esp_err_t hal_i2c_add(uint8_t addr7, hal_i2c_dev_t *out);

esp_err_t hal_i2c_write(hal_i2c_dev_t dev, const uint8_t *data, size_t len);

/** Write @p wlen bytes then read @p rlen bytes in one transaction. */
esp_err_t hal_i2c_write_read(hal_i2c_dev_t dev, const uint8_t *w, size_t wlen,
                             uint8_t *r, size_t rlen);

#ifdef __cplusplus
}
#endif
