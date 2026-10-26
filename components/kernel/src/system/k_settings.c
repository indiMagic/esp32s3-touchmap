#include "k_settings.h"
#include "k_events.h"

#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "k_settings";
static const char *NS  = "touchmap";
static const char *KEY = "cfg";

#define CFG_MAGIC 0x54504D01u   /* bump the low byte if the struct layout changes */

typedef struct {
    uint32_t     magic;
    k_settings_t s;
} blob_t;

static k_settings_t s_cfg;

static void set_defaults(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.default_zoom    = 12;
    s_cfg.follow_at_start = true;
    s_cfg.units           = K_UNITS_KM;
    s_cfg.show_speed      = true;
    s_cfg.timeout_idx     = 0;
    s_cfg.backlight       = true;
    s_cfg.night_palette   = false;
    s_cfg.ble_on          = true;
    strncpy(s_cfg.device_name, "ESP32-Maps", K_DEVICE_NAME_MAX - 1);
}

void k_settings_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs init: %s (using defaults)", esp_err_to_name(err));
    }

    set_defaults();

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        blob_t b;
        size_t n = sizeof(b);
        if (nvs_get_blob(h, KEY, &b, &n) == ESP_OK && n == sizeof(b) && b.magic == CFG_MAGIC) {
            s_cfg = b.s;
            s_cfg.device_name[K_DEVICE_NAME_MAX - 1] = '\0';
        }
        nvs_close(h);
    }
}

k_settings_t *k_settings(void)
{
    return &s_cfg;
}

void k_settings_commit(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        blob_t b = { .magic = CFG_MAGIC, .s = s_cfg };
        nvs_set_blob(h, KEY, &b, sizeof(b));
        nvs_commit(h);
        nvs_close(h);
    }
    k_events_post(KEV_SETTINGS);
}

void k_settings_factory_reset(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    set_defaults();
    k_events_post(KEV_SETTINGS);
}

uint32_t k_settings_timeout_ms(uint8_t idx)
{
    static const uint32_t ms[] = { 0, 60u * 1000, 5u * 60 * 1000, 15u * 60 * 1000 };
    return idx < 4 ? ms[idx] : 0;
}
