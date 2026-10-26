#include "hal_ioexp.h"
#include "hal_i2c.h"
#include "board_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "hal_ioexp";

static hal_i2c_dev_t s_setup;
static hal_i2c_dev_t s_out;
static uint8_t s_shadow;
static SemaphoreHandle_t s_lock;

static esp_err_t write_out(void)
{
    return hal_i2c_write(s_out, &s_shadow, 1);
}

esp_err_t hal_ioexp_init(void)
{
    esp_err_t err = hal_i2c_add(BOARD_CH422G_ADDR_SETUP, &s_setup);
    if (err == ESP_OK) {
        err = hal_i2c_add(BOARD_CH422G_ADDR_OUT, &s_out);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err));
        return err;
    }
    s_lock = xSemaphoreCreateMutex();

    /* 0x01 = IO_OE: EXIO0..7 become push-pull outputs. */
    const uint8_t mode_out = 0x01;
    err = hal_i2c_write(s_setup, &mode_out, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CH422G not answering at 0x%02x", BOARD_CH422G_ADDR_SETUP);
        return err;
    }

    /* Safe start: everything low (backlight off, resets asserted, SD CS held low). */
    s_shadow = 0x00;
    return write_out();
}

esp_err_t hal_ioexp_set(int exio, bool level)
{
    if (exio < 0 || exio > 7 || !s_lock) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (level) {
        s_shadow |= (uint8_t)(1u << exio);
    } else {
        s_shadow &= (uint8_t)~(1u << exio);
    }
    esp_err_t err = write_out();
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t hal_ioexp_backlight(bool on)
{
    return hal_ioexp_set(BOARD_EXIO_BACKLIGHT, on);
}
