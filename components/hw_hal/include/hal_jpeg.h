/**
 * @file hal_jpeg.h
 * @brief Baseline JPEG -> RGB565 decoder (software, esp_jpeg component).
 *
 * All calls into the esp_jpeg component are confined to hal_jpeg.c so that
 * open issue OI-3 (unverified call names) has exactly one place to fix.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decode a 256x256 baseline JPEG into @p out (256*256 RGB565 pixels,
 * native byte order, row-major).
 * @return ESP_OK, ESP_ERR_INVALID_SIZE if the image is not 256x256,
 *         or the decoder error.
 */
esp_err_t hal_jpeg_decode_tile(const uint8_t *jpg, size_t jpg_len,
                               uint16_t *out, size_t out_bytes);

#ifdef __cplusplus
}
#endif
