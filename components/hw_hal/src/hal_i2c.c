#include "hal_i2c.h"
#include "board_config.h"

#include <stdlib.h>
#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_TIMEOUT_MS 50

static const char *TAG = "hal_i2c";

struct hal_i2c_dev {
    i2c_master_dev_handle_t h;
};

static i2c_master_bus_handle_t s_bus;

esp_err_t hal_i2c_init(void)
{
    if (s_bus) {
        return ESP_OK;
    }
    i2c_master_bus_config_t cfg = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA,
        .scl_io_num = BOARD_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus init failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t hal_i2c_add(uint8_t addr7, hal_i2c_dev_t *out)
{
    if (!s_bus || !out) {
        return ESP_ERR_INVALID_STATE;
    }
    struct hal_i2c_dev *d = calloc(1, sizeof(*d));
    if (!d) {
        return ESP_ERR_NO_MEM;
    }
    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr7,
        .scl_speed_hz = BOARD_I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(s_bus, &dc, &d->h);
    if (err != ESP_OK) {
        free(d);
        return err;
    }
    *out = d;
    return ESP_OK;
}

esp_err_t hal_i2c_write(hal_i2c_dev_t dev, const uint8_t *data, size_t len)
{
    return i2c_master_transmit(dev->h, data, len, I2C_TIMEOUT_MS);
}

esp_err_t hal_i2c_write_read(hal_i2c_dev_t dev, const uint8_t *w, size_t wlen,
                             uint8_t *r, size_t rlen)
{
    return i2c_master_transmit_receive(dev->h, w, wlen, r, rlen, I2C_TIMEOUT_MS);
}
