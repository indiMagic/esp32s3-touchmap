/**
 * @file hal_file.h
 * @brief Random-access read of big files on the SD card (up to 4 GiB).
 *
 * Uses FatFs directly with 32-bit offsets. The POSIX layer on this target has
 * a 32-bit off_t, which would cap files at 2 GiB; map packs may reach
 * 4 GiB (SRS section 3), so tile reads bypass it.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_file hal_file_t;

/** Open @p rel_path (relative to the card root, e.g. "maps/z12.tpk") read-only. */
hal_file_t *hal_file_open(const char *rel_path);

uint32_t hal_file_size(const hal_file_t *f);

/** Read up to @p len bytes at @p offset. @p got receives the byte count. */
esp_err_t hal_file_read_at(hal_file_t *f, uint32_t offset, void *buf, size_t len, size_t *got);

void hal_file_close(hal_file_t *f);

#ifdef __cplusplus
}
#endif
