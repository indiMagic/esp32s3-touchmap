#include "kernel.h"
#include "hal.h"

#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "kernel";

#define PSRAM_FREE_MIN (1536u * 1024u)    /* SRS quality: at least 1.5 MiB stays free */

uint32_t kernel_millis(void)
{
    return hal_millis();
}

uint32_t kernel_psram_free(void)
{
    return (uint32_t)hal_psram_free();
}

uint32_t kernel_heap_free(void)
{
    return (uint32_t)esp_get_free_internal_heap_size();
}

const char *kernel_idf_version(void)
{
    return esp_get_idf_version();
}

esp_err_t kernel_start(void)
{
    k_events_init();
    k_settings_init();
    k_nav_init();
    k_route_init();

    ESP_ERROR_CHECK(hal_init());

    (void)k_display_backbuffer();          /* allocate the back buffer early */
    k_storage_start();                     /* mount + monitor */
    k_tiles_start();                       /* cache + loader task */
    k_input_start();
    k_power_init();
    k_ble_start();

    uint32_t free_psram = kernel_psram_free();
    ESP_LOGI(TAG, "up; PSRAM free %lu KiB", (unsigned long)(free_psram / 1024));
    if (free_psram < PSRAM_FREE_MIN) {
        ESP_LOGW(TAG, "PSRAM free below the 1.5 MiB budget");
    }
    return ESP_OK;
}
