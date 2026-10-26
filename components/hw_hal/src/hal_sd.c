#include "hal_sd.h"
#include "hal_ioexp.h"
#include "board_config.h"

#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "diskio_sdmmc.h"
#include "esp_log.h"

static const char *TAG = "hal_sd";

static sdmmc_card_t *s_card;
static bool s_bus_ready;

esp_err_t hal_sd_mount(void)
{
    if (s_card) {
        return ESP_OK;
    }

    /* Hold the card selected for the life of the program. */
    hal_ioexp_set(BOARD_EXIO_SD_CS, false);

    if (!s_bus_ready) {
        spi_bus_config_t bus = {
            .mosi_io_num = BOARD_SD_MOSI,
            .miso_io_num = BOARD_SD_MISO,
            .sclk_io_num = BOARD_SD_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4000,
        };
        esp_err_t e = spi_bus_initialize(BOARD_SD_HOST, &bus, SDSPI_DEFAULT_DMA);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(e));
            return e;
        }
        s_bus_ready = true;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = BOARD_SD_HOST;
    host.max_freq_khz = BOARD_SD_FREQ_KHZ;

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = GPIO_NUM_NC;
    slot.host_id = BOARD_SD_HOST;

    esp_vfs_fat_mount_config_t mnt = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };

    esp_err_t err = esp_vfs_fat_sdspi_mount(BOARD_SD_MOUNT_POINT, &host, &slot, &mnt, &s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mount failed: %s", esp_err_to_name(err));
        s_card = NULL;
        return err;
    }
    ESP_LOGI(TAG, "card mounted");
    return ESP_OK;
}

void hal_sd_unmount(void)
{
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(BOARD_SD_MOUNT_POINT, s_card);
        s_card = NULL;
    }
}

bool hal_sd_is_mounted(void)
{
    return s_card != NULL;
}

bool hal_sd_probe(void)
{
    return s_card && sdmmc_get_status(s_card) == ESP_OK;
}

int hal_sd_drive(void)
{
    return s_card ? (int)ff_diskio_get_pdrv_card(s_card) : -1;
}

esp_err_t hal_sd_space(uint64_t *total, uint64_t *free_bytes)
{
    if (!s_card) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_vfs_fat_info(BOARD_SD_MOUNT_POINT, total, free_bytes);
}
