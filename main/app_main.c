/**
 * @file app_main.c
 * @brief Composition root: bring the kernel up, then start the UI.
 *
 * main -> ui -> kernel -> hw_hal. Nothing else lives here.
 */
#include "kernel.h"
#include "ui.h"

#include "esp_log.h"

static const char *TAG = "app";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 Touch Map %s", KERNEL_FW_VERSION);
    ESP_ERROR_CHECK(kernel_start());
    ui_start();
}
