/**
 * @file k_settings.h
 * @brief Persistent settings in NVS (SET-01..07). Factory reset never touches
 *        the SD card.
 *
 * Threading: the UI task is the only writer. Other tasks read single fields.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define K_DEVICE_NAME_MAX 24

typedef enum { K_UNITS_KM = 0, K_UNITS_MI = 1 } k_units_t;

typedef struct {
    uint8_t  default_zoom;
    bool     follow_at_start;
    uint8_t  units;                 /* k_units_t */
    bool     show_speed;
    uint8_t  timeout_idx;           /* 0 never, 1 = 1 min, 2 = 5 min, 3 = 15 min */
    bool     backlight;
    bool     night_palette;         /* planned (NAV-07) */
    bool     ble_on;                /* BLE-07 */
    char     device_name[K_DEVICE_NAME_MAX];   /* BLE-08 */
} k_settings_t;

void           k_settings_init(void);       /* nvs_flash_init + load */
k_settings_t  *k_settings(void);            /* live struct */
void           k_settings_commit(void);     /* persist and post KEV_SETTINGS */
void           k_settings_factory_reset(void);

/** Screen timeout in ms for a timeout_idx; 0 = never. */
uint32_t       k_settings_timeout_ms(uint8_t idx);

#ifdef __cplusplus
}
#endif
