# Requirement traceability

Status key: **done** = implemented and, where noted, verified on the host; **partial** = some of it;
**planned** = design only. *Nothing here has run on the board yet* (see README, "What is verified").

## Map and SD card

| ID | Where | Status |
|---|---|---|
| MAP-01 draw view from current zoom | `ui/src/map/map_view.c` `draw_tiles` | done (rendered in sim) |
| MAP-02 one-finger pan, +/- zoom, clamped to packs | `ui/src/screens/screen_map.c`, `map_view.c` `clamp_zoom` | done (sim: zoom clamp 10..17 exercised) |
| MAP-03 32-tile LRU cache | `kernel/src/tiles/k_tiles.c` | done |
| MAP-04 UI never blocks on SD, grey placeholder | `k_tiles.c` queue + loader; `map_view.c` | done (grey frame rendered in sim) |
| MAP-05 route, white casing under blue line, clipped | `map_view.c` `draw_route`, `ui/src/gfx/gfx.c` `gfx_line` | done (rendered in sim) |
| MAP-06 destination marker | `map_view.c` `draw_route` | done |
| MAP-07 heading arrow, follow mode | `map_view.c` `draw_position`, `map_view_draw` | done |
| MAP-08 attribution from pack header | `kernel/src/storage/k_storage.c`, `ui/src/hud/hud.c` | done (always in the status bar) |
| SD-01 FAT32 SPI mount, CS on expander | `hw_hal/src/hal_sd.c` | done, unbuilt |
| SD-02 find zNN.tpk for zoom 0..20 | `k_storage.c` `scan_locked` | done, unbuilt |
| SD-03 clear error when card missing / no packs | `k_storage_state_text`, `screen_map.c` | done (both frames rendered in sim) |
| SD-04 reject wrong magic / tile outside index | `kernel/src/tiles/k_tpk.c` | done, **host-tested** incl. overflow cases |
| SD-05 card size and free space on Storage screen | `ui/src/screens/screen_storage.c` | done |

## Navigation and Bluetooth

| ID | Where | Status |
|---|---|---|
| NAV-01 next maneuver arrow, distance, street | `ui/src/hud/hud.c` `draw_banner`, `draw_arrow` | done (all 14 arrows rendered) |
| NAV-02 12,000 route points in PSRAM | `kernel/src/nav/k_route.c` | done |
| NAV-03 distance and time remaining | `hud.c` `draw_status_bar` | done |
| NAV-04 unknown, not stale, after 10 s | `k_nav.c` `k_nav_pos_valid`, `map_view.c`, `hud.c` | done (stale frame rendered in sim) |
| NAV-05 CLEAR removes route within one frame | `k_ble.c` -> `k_route_clear` | done, unbuilt |
| NAV-06 km / miles | `hud.c` `hud_fmt_distance`, `screen_mapopts.c` | done (miles frame rendered) |
| NAV-07 night palette | setting stored (`screen_display.c`); colours not switched | partial |
| BLE-01 advertise UUID, name in scan response | `kernel/src/ble/k_ble.c` `advertise` | done, unbuilt |
| BLE-02 chunked route upload with ROUTE_ACK | `k_ble.c` `handle_route`, `k_route.c` | done, unbuilt; wire format host-tested |
| BLE-03 HELLO with zoom range on subscribe | `k_ble.c` `BLE_GAP_EVENT_SUBSCRIBE` | done, unbuilt |
| BLE-04 prefer 185-byte MTU | `k_ble.c` `ble_att_set_preferred_mtu`, `sdkconfig.defaults` | done, unbuilt |
| BLE-05 resume advertising after disconnect | `k_ble.c` `BLE_GAP_EVENT_DISCONNECT` | done, unbuilt |
| BLE-06 passkey pairing, bonded list | hook and TODO in `k_ble_start`; UI rows show "planned" | planned |
| BLE-07 Bluetooth off from Settings | `k_ble_set_enabled`, `screen_bluetooth.c` | done |
| BLE-08 change device name | name stored and advertised; no on-screen editor | partial |

## WiFi and settings

| ID | Where | Status |
|---|---|---|
| WIFI-01..08 | `ui/src/screens/screen_wifi.c` placeholder only | planned |
| SET-01..07 persistence in flash | `kernel/src/system/k_settings.c` (NVS blob with magic) | done, unbuilt |
| SET factory reset leaves the SD card alone | `k_settings_factory_reset`, `screen_factory.c` | done |

## Quality

| Area | Where | Status |
|---|---|---|
| Pan < 100 ms with cached tiles | measure on the board (T-04) | open |
| No watchdog reset when card removed; recovers | `k_storage.c` monitor, `CONFIG_ESP_TASK_WDT_TIMEOUT_S=10` | designed, test T-05 |
| >= 1.5 MiB PSRAM free | budget in ARCHITECTURE.md; warning in `k_sys.c` | 2.16 MiB by calculation |
| Screen timeout switches off backlight | `kernel/src/system/k_power.c` | done, unbuilt |
| Touch targets >= 64 px, text >= 16 px | `ui_theme.h`; 8x16 font | done |
| Tile attribution always visible | `hud.c` | done |
| No bulk download from forbidding servers | `tools/tilepack.py fetch` refuses openstreetmap.org | done |
| Writes only from bonded phone once pairing is on | depends on BLE-06 | planned |
