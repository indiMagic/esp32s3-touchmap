/**
 * @file hal_sd.h
 * @brief microSD over SPI2 (FAT32/exFAT) mounted at BOARD_SD_MOUNT_POINT.
 *
 * Chip-select is held low through the CH422G expander, so the driver is told
 * the CS pin is "not connected".
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_sd_mount(void);
void      hal_sd_unmount(void);
bool      hal_sd_is_mounted(void);

/** Cheap presence check (card answers a status command). */
bool      hal_sd_probe(void);

/** FatFs drive number of the mounted card, or -1 (used by hal_file). */
int       hal_sd_drive(void);

/** Capacity in bytes; returns ESP_ERR_INVALID_STATE when not mounted. */
esp_err_t hal_sd_space(uint64_t *total, uint64_t *free_bytes);

#ifdef __cplusplus
}
#endif
