#include "hal_touch.h"
#include "hal_i2c.h"
#include "hal_ioexp.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_touch";

#define GT911_REG_ID      0x8140
#define GT911_REG_STATUS  0x814E
#define GT911_REG_POINT1  0x8150

static hal_i2c_dev_t s_dev;

static esp_err_t reg_read(uint16_t reg, uint8_t *buf, size_t len)
{
    uint8_t a[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xff) };
    return hal_i2c_write_read(s_dev, a, 2, buf, len);
}

static esp_err_t reg_write8(uint16_t reg, uint8_t v)
{
    uint8_t b[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xff), v };
    return hal_i2c_write(s_dev, b, 3);
}

static bool probe(uint8_t addr)
{
    hal_i2c_dev_t d;
    if (hal_i2c_add(addr, &d) != ESP_OK) {
        return false;
    }
    s_dev = d;
    uint8_t id[4] = { 0 };
    return reg_read(GT911_REG_ID, id, 4) == ESP_OK && id[0] == '9';
}

esp_err_t hal_touch_init(void)
{
    /* Reset sequence: INT level during reset picks the I2C address.
     * INT low -> 0x5D. We probe both anyway (SRS: "0x5D or 0x14, probed"). */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOARD_TOUCH_INT,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(BOARD_TOUCH_INT, 0);
    hal_ioexp_set(BOARD_EXIO_TOUCH_RST, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    hal_ioexp_set(BOARD_EXIO_TOUCH_RST, true);
    vTaskDelay(pdMS_TO_TICKS(8));
    io.mode = GPIO_MODE_INPUT;
    gpio_config(&io);          /* INT is only used at reset; we poll afterwards */
    vTaskDelay(pdMS_TO_TICKS(60));

    if (probe(BOARD_GT911_ADDR_A)) {
        ESP_LOGI(TAG, "GT911 at 0x%02x", BOARD_GT911_ADDR_A);
        return ESP_OK;
    }
    if (probe(BOARD_GT911_ADDR_B)) {
        ESP_LOGI(TAG, "GT911 at 0x%02x", BOARD_GT911_ADDR_B);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "GT911 not found");
    s_dev = NULL;
    return ESP_ERR_NOT_FOUND;
}

bool hal_touch_read(uint16_t *x, uint16_t *y)
{
    if (!s_dev) {
        return false;
    }
    uint8_t st = 0;
    if (reg_read(GT911_REG_STATUS, &st, 1) != ESP_OK) {
        return false;
    }
    bool down = false;
    if ((st & 0x80) && (st & 0x0f) > 0) {
        uint8_t p[4];
        if (reg_read(GT911_REG_POINT1, p, 4) == ESP_OK) {
            *x = (uint16_t)(p[0] | (p[1] << 8));
            *y = (uint16_t)(p[2] | (p[3] << 8));
            down = true;
        }
    }
    if (st & 0x80) {
        reg_write8(GT911_REG_STATUS, 0);   /* acknowledge, ready for next frame */
    }
    return down;
}
