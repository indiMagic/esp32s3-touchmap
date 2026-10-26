#include "hal_file.h"
#include "hal_sd.h"

#include <stdio.h>
#include <stdlib.h>
#include "ff.h"
#include "esp_log.h"

static const char *TAG = "hal_file";

struct hal_file {
    FIL fil;
};

hal_file_t *hal_file_open(const char *rel_path)
{
    int drv = hal_sd_drive();
    if (drv < 0 || !rel_path) {
        return NULL;
    }
    char path[64];
    snprintf(path, sizeof(path), "%d:/%s", drv, rel_path);

    struct hal_file *f = calloc(1, sizeof(*f));
    if (!f) {
        return NULL;
    }
    FRESULT r = f_open(&f->fil, path, FA_READ | FA_OPEN_EXISTING);
    if (r != FR_OK) {
        free(f);
        return NULL;
    }
    return f;
}

uint32_t hal_file_size(const hal_file_t *f)
{
    return f ? (uint32_t)f_size(&f->fil) : 0;
}

esp_err_t hal_file_read_at(hal_file_t *f, uint32_t offset, void *buf, size_t len, size_t *got)
{
    if (!f || !buf) {
        return ESP_ERR_INVALID_ARG;
    }
    if (got) {
        *got = 0;
    }
    if (f_lseek(&f->fil, offset) != FR_OK) {
        return ESP_FAIL;
    }
    UINT n = 0;
    FRESULT r = f_read(&f->fil, buf, (UINT)len, &n);
    if (r != FR_OK) {
        ESP_LOGW(TAG, "read error %d", (int)r);
        return ESP_FAIL;
    }
    if (got) {
        *got = n;
    }
    return ESP_OK;
}

void hal_file_close(hal_file_t *f)
{
    if (f) {
        f_close(&f->fil);
        free(f);
    }
}
