#include "hal.h"

#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "hal";

esp_err_t hal_init(void)
{
    ESP_ERROR_CHECK(hal_i2c_init());
    ESP_ERROR_CHECK(hal_ioexp_init());
    ESP_ERROR_CHECK(hal_lcd_init());

    /* A missing touch controller is not fatal: the map still works. */
    if (hal_touch_init() != ESP_OK) {
        ESP_LOGW(TAG, "running without touch");
    }

    hal_lcd_backlight(true);
    ESP_LOGI(TAG, "hardware up, PSRAM free %u", (unsigned)hal_psram_free());
    return ESP_OK;
}

uint32_t hal_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

size_t hal_psram_free(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}
