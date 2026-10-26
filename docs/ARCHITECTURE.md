# Firmware architecture

ESP32-S3 Touch Map, firmware for the Waveshare ESP32-S3-Touch-LCD-4.3, ESP-IDF 5.4.
Source of requirements: *SRS & System Design Guide, Draft 0.9* (requirement IDs below refer to it).

## 1. Layers

```
  main/            composition root: kernel_start(); ui_start();
    |
  components/ui        LAYER 3  what the user sees: map, HUD, menus
    |  includes only kernel.h
  components/kernel    LAYER 2  services: tiles, navigation state, BLE, storage, settings, tasks
    |  includes hw_hal headers
  components/hw_hal    LAYER 1  hardware: pins, I2C, CH422G, RGB LCD, GT911, SD, JPEG
    |
  ESP-IDF drivers
```

**Rules** (checked by `python3 tools/check_layers.py`, also a VS Code task):

| Layer | May include | May not include |
|---|---|---|
| `hw_hal` | ESP-IDF drivers | kernel, ui |
| `kernel` | hw_hal, FreeRTOS, NVS, NimBLE | ui |
| `ui` | `kernel.h` (and `esp_log`, FreeRTOS) | hw_hal, `driver/*`, `esp_lcd*`, NimBLE, FatFs |
| portable kernel files (`k_proto`, `k_geo`, `k_tpk`) | libc only | any ESP-IDF/FreeRTOS header |

Why it is worth the discipline:

* **Swap the board** by rewriting `hw_hal` only. `board_config.h` is the one file that knows GPIO numbers.
* **Unit-test on a PC.** The portable files build with plain `gcc` (`make -C test/host run`).
* **Run the UI on a PC.** `sim/` compiles the *real* UI sources against a fake kernel and renders frames to PNG.
  The UI cannot tell the difference because it only sees `kernel.h`.
* Open issue OI-3 (unverified JPEG call names) is confined to `hal_jpeg.c`; OI-4 (LCD reset pin) to `board_config.h`.

## 2. File map

```
esp32s3-touchmap/
├── CMakeLists.txt  sdkconfig.defaults  partitions.csv
├── main/
│   └── app_main.c                     kernel_start(); ui_start();
├── components/
│   ├── hw_hal/                        LAYER 1
│   │   ├── include/  board_config.h   pins, panel timing, I2C addresses
│   │   │             hal.h            umbrella + hal_init()
│   │   │             hal_i2c.h        shared I2C bus, device handles
│   │   │             hal_ioexp.h      CH422G: touch reset, backlight, LCD reset, SD CS
│   │   │             hal_lcd.h        800x480 RGB565 panel
│   │   │             hal_touch.h      GT911
│   │   │             hal_sd.h         SPI SD mount / probe / space
│   │   │             hal_file.h       4 GiB random-access reads (FatFs, 32-bit offsets)
│   │   │             hal_jpeg.h       256x256 JPEG -> RGB565
│   │   │             hal_mem.h        PSRAM / internal allocation helpers
│   │   └── src/      one .c per header (hal_sys.c holds hal_init)
│   ├── kernel/                        LAYER 2
│   │   ├── include/  kernel.h         umbrella + kernel_start()
│   │   │             k_proto.h        BLE wire format            (portable)
│   │   │             k_geo.h          Web Mercator, fixed point  (portable)
│   │   │             k_tpk.h          .tpk pack format           (portable)
│   │   │             k_events.h       change notifications for the UI task
│   │   │             k_settings.h     NVS-backed settings
│   │   │             k_nav.h          phone position + next turn + link state
│   │   │             k_route.h        12,000-point route in PSRAM
│   │   │             k_storage.h      SD state machine + pack registry
│   │   │             k_tiles.h        32-tile LRU cache + loader task
│   │   │             k_display.h      back buffer + present
│   │   │             k_input.h        touch events (DOWN/MOVE/UP)
│   │   │             k_power.h        screen timeout
│   │   │             k_ble.h          NimBLE navigator service
│   │   └── src/      proto/ geo/ tiles/ nav/ storage/ display/ ble/ system/
│   └── ui/                            LAYER 3
│       ├── include/  ui.h             screen stack, entry point
│       │             ui_theme.h       colours and sizes (64 px targets, 16 px text)
│       │             gfx.h            primitives: rect, blit, thick line, polygon, text
│       │             font_ui.h        Inter anti-aliased fonts (generated)
│       │             map_view.h       tiles + route + markers, zoom/pan/follow
│       │             hud.h            turn banner, status bar, buttons
│       │             ui_form.h        declarative settings page
│       └── src/      core/ gfx/ map/ hud/ widgets/ screens/
├── tools/   tilepack.py  gen_font.py  check_layers.py
├── test/host/   unit tests for the portable code + .tpk cross-check
├── sim/         host simulator (real UI, fake kernel)
├── docs/        this file, TRACEABILITY.md, BLE_PROTOCOL.md
└── .vscode/     tasks, launch (GDB over USB-JTAG), IntelliSense, recommended extensions
```

## 3. Runtime: tasks and data flow

| Task | Core | Prio | Owns |
|---|---|---|---|
| `ui` | 1 | 7 | drawing at <= 30 fps, screen stack, touch dispatch |
| `input` | 1 | 6 | GT911 poll every 20 ms, debounce, event queue |
| `tile_loader` | 1 | 4 | SD read + JPEG decode into cache slots (below the UI so the screen always wins) |
| `sd_mon` | 1 | 3 | mount, 1 s presence probe, rescan, hot-plug recovery |
| NimBLE host | 0 | (stack) | GATT writes -> `k_nav` / `k_route`, STATUS notifications |

```
phone --BLE--> k_ble --> k_nav (position, turn)  --KEV_POS/NAV-->  ui task --> map_view + hud
                    \--> k_route (points)        --KEV_ROUTE----/
SD card --> k_storage (packs) --> k_tiles loader --KEV_TILE-----/
touch --> k_input (queue) --KEV_INPUT------------------------------/
```

The UI task sleeps on an event group and redraws only when something changed (plus once a second so the
10-second stale-position rule, NAV-04, still fires). **The UI never waits on storage (MAP-04):** a missing
tile is queued, a grey block is drawn, and the loader posts `KEV_TILE` when it is ready.

Shared-state discipline: each shared object has one lock, held only while copying
(`k_nav_snapshot`, `k_route_lock`). The tile cache lock is held for one frame composition, never during SD I/O.

## 4. Key design decisions

* **Mercator in 32-bit fixed point** (`k_geo.h`). A point is `u32 mx, my` over the whole world; world pixel at zoom z is
  `m >> (24 - z)`. Exact at zoom 20 (28 bits), no float drift while panning, and a route point is 16 bytes
  (`lat, lon, mx, my`), which is exactly the 0.18 MiB the SRS budgets for 12,000 points.
* **Big files via FatFs, not POSIX** (`hal_file.c`). `off_t` is 32-bit signed on this target, which caps `fseek` at 2 GiB;
  packs may be up to 4 GiB, so tile reads use `f_lseek` with unsigned 32-bit offsets.
* **Index validation** (`k_tpk.c`). Every index entry is checked against the file size and the 192 KB tile limit with
  overflow-safe arithmetic, so a corrupt pack cannot cause an out-of-bounds read (SD-04).
* **Failed tiles are cached as failed** until evicted, so a corrupt JPEG is skipped once and never retried every
  frame (T-06).
* **SD hot-plug** is owned by one task and one mutex. A read error asks the monitor to probe; on removal it closes every
  file, unmounts, bumps a generation counter (which flushes the tile cache) and retries every second (T-05).
* **Route upload is atomic to the reader.** While a route is uploading it reads as empty; `ROUTE_END` publishes it only
  if every point arrived in order. `ROUTE_CLEAR` empties it immediately (NAV-05).
* **Settings pages are data** (`ui_form.h`). A page is a table of rows; adding a setting is one row.

## 5. Memory budget (PSRAM 8 MiB)

| Item | Size |
|---|---|
| Tile cache, 32 x 256x256x2 B | 4.00 MiB |
| Panel frame buffer | 0.73 MiB |
| Back buffer | 0.73 MiB |
| Route, 12,000 x 16 B | 0.18 MiB |
| JPEG scratch (192 KiB) | 0.19 MiB |
| **Used** | **5.84 MiB** |
| **Free** (requirement: >= 1.5 MiB) | **2.16 MiB** |

`kernel_start()` logs a warning if free PSRAM drops below 1.5 MiB.

## 6. Where the SRS leaves choices, and what was picked

| Topic | Choice |
|---|---|
| `.tpk` header fields | defined in `k_tpk.h`; `tools/tilepack.py` writes the same layout |
| Pack file names | writer emits `z05.tpk`; reader accepts `z05.tpk` and `z5.tpk` |
| Maneuver codes 0..13 | enum in `k_proto.h`, documented in `BLE_PROTOCOL.md` |
| Storage screen "Tiles" column | shows pack count, zoom range and total size; counting filled index cells would add seconds to every scan |
| Backlight | on/off only (EXIO2), so there is no brightness slider |
