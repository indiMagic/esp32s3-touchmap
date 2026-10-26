/**
 * @file kernel.h
 * @brief Layer 2 umbrella header. The UI layer includes this, never hw_hal.
 */
#pragma once

#include "esp_err.h"

#include "k_events.h"
#include "k_settings.h"
#include "k_geo.h"
#include "k_proto.h"
#include "k_tpk.h"
#include "k_nav.h"
#include "k_route.h"
#include "k_storage.h"
#include "k_tiles.h"
#include "k_display.h"
#include "k_input.h"
#include "k_power.h"
#include "k_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Milliseconds since boot (wraps the hardware clock for upper layers). */
uint32_t kernel_millis(void);

/** Free PSRAM in bytes (About screen, memory budget check). */
uint32_t kernel_psram_free(void);

/** Free internal heap in bytes (About screen). */
uint32_t kernel_heap_free(void);

/** ESP-IDF version string, e.g. "v5.4.1" (About screen). */
const char *kernel_idf_version(void);

/** Firmware version string (About screen). */
#define KERNEL_FW_VERSION "v0.1.0"

/**
 * Bring the whole system up in dependency order:
 *   events -> settings -> nav/route -> hardware -> display -> storage ->
 *   tile cache -> input -> power -> BLE
 * Starts the kernel tasks (sd_mon, tile_loader, input, NimBLE host).
 */
esp_err_t kernel_start(void);

#ifdef __cplusplus
}
#endif
