/**
 * @file hal.h
 * @brief Layer 1 umbrella header and bring-up entry point.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#include "board_config.h"
#include "hal_mem.h"
#include "hal_i2c.h"
#include "hal_ioexp.h"
#include "hal_lcd.h"
#include "hal_touch.h"
#include "hal_sd.h"
#include "hal_file.h"
#include "hal_jpeg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bring up I2C, the I/O expander, LCD and touch. The SD card is NOT mounted
 * here: the kernel storage service owns mount/unmount/hot-plug.
 */
esp_err_t hal_init(void);

/** Milliseconds since boot. */
uint32_t hal_millis(void);

/** Free PSRAM in bytes. */
size_t hal_psram_free(void);

#ifdef __cplusplus
}
#endif
