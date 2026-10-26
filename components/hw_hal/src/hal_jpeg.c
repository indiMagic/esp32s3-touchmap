#include "hal_jpeg.h"

#include "jpeg_decoder.h"      /* espressif/esp_jpeg */
#include "esp_log.h"

static const char *TAG = "hal_jpeg";

#define TILE_PX 256

esp_err_t hal_jpeg_decode_tile(const uint8_t *jpg, size_t jpg_len,
                               uint16_t *out, size_t out_bytes)
{
    if (!jpg || !out || out_bytes < (size_t)TILE_PX * TILE_PX * 2) {
        return ESP_ERR_INVALID_ARG;
    }

    /* TODO(OI-3): verify these names against the installed esp_jpeg version. */
    esp_jpeg_image_cfg_t cfg = {
        .indata = (uint8_t *)jpg,
        .indata_size = jpg_len,
        .outbuf = (uint8_t *)out,
        .outbuf_size = out_bytes,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = { .swap_color_bytes = 0 },
    };
    esp_jpeg_image_output_t img;
    esp_err_t err = esp_jpeg_decode(&cfg, &img);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "decode failed: %s", esp_err_to_name(err));
        return err;
    }
    if (img.width != TILE_PX || img.height != TILE_PX) {
        ESP_LOGW(TAG, "unexpected tile size %ux%u", img.width, img.height);
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}
