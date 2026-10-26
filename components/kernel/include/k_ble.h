/**
 * @file k_ble.h
 * @brief Bluetooth LE navigator service (NimBLE, peripheral role).
 *
 * One GATT service. The phone writes POS / ROUTE / NAV; the board notifies
 * STATUS (HELLO on subscribe, ROUTE_ACK after an upload). SRS section 5.
 *
 *   Service 5a1b0001-7c0d-4f2e-9b7a-3c6d8e1f2a40
 *   POS 0002  ROUTE 0003  NAV 0004  STATUS 0005 (same base UUID)
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void k_ble_start(void);

/** BLE-07: switch Bluetooth on/off. Off stops advertising and drops the link. */
void k_ble_set_enabled(bool on);
bool k_ble_enabled(void);

#ifdef __cplusplus
}
#endif
