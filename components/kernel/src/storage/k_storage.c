#include "k_storage.h"
#include "k_tpk.h"
#include "k_events.h"
#include "hal.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "k_storage";

#define KG_MAX_ZOOM_PACKS 21      /* zoom 0..20 (SD-02) */

typedef struct {
    hal_file_t  *file;
    kt_header_t  hdr;
    k_pack_info_t info;
} pack_t;

static pack_t            s_pack[KG_MAX_ZOOM_PACKS];
static char              s_attr[KT_ATTR_BYTES + 1];
static volatile k_sd_state_t s_state = K_SD_MOUNTING;
static volatile uint32_t s_gen;
static volatile bool     s_rescan_req;
static SemaphoreHandle_t s_mtx;      /* guards packs + file handles + the SD bus */

/* ---- packs ------------------------------------------------------------- */
static void close_all_locked(void)
{
    for (int z = 0; z < KG_MAX_ZOOM_PACKS; z++) {
        if (s_pack[z].file) {
            hal_file_close(s_pack[z].file);
        }
        memset(&s_pack[z], 0, sizeof(s_pack[z]));
    }
    s_attr[0] = '\0';
}

static int scan_locked(void)
{
    close_all_locked();
    int found = 0;

    for (int z = 0; z < KG_MAX_ZOOM_PACKS; z++) {
        char name[24];
        snprintf(name, sizeof(name), "maps/z%02d.tpk", z);
        hal_file_t *f = hal_file_open(name);
        if (!f) {
            snprintf(name, sizeof(name), "maps/z%d.tpk", z);   /* also accept z5.tpk */
            f = hal_file_open(name);
        }
        if (!f) {
            continue;
        }
        uint8_t raw[KT_HEADER_BYTES];
        size_t got = 0;
        kt_header_t h;
        uint32_t fsize = hal_file_size(f);
        if (hal_file_read_at(f, 0, raw, sizeof(raw), &got) != ESP_OK || got != sizeof(raw) ||
            kt_header_parse(raw, fsize, &h) != KT_OK || h.zoom != z) {
            ESP_LOGW(TAG, "%s rejected (bad header)", name);       /* SD-04 */
            hal_file_close(f);
            continue;
        }
        pack_t *p = &s_pack[z];
        p->file = f;
        p->hdr = h;
        p->info = (k_pack_info_t){ .present = true, .zoom = (uint8_t)z,
                                   .x0 = h.x0, .y0 = h.y0, .w = h.w, .h = h.h,
                                   .file_bytes = fsize };
        if (!s_attr[0] && h.attribution[0]) {
            strncpy(s_attr, h.attribution, sizeof(s_attr) - 1);
        }
        ESP_LOGI(TAG, "pack z%d: %lux%lu tiles, %lu bytes", z,
                 (unsigned long)h.w, (unsigned long)h.h, (unsigned long)fsize);
        found++;
    }
    s_gen++;
    return found;
}

bool k_packs_has(uint8_t zoom)
{
    return zoom < KG_MAX_ZOOM_PACKS && s_pack[zoom].info.present;
}

int k_packs_zoom_min(void)
{
    for (int z = 0; z < KG_MAX_ZOOM_PACKS; z++) {
        if (s_pack[z].info.present) {
            return z;
        }
    }
    return -1;
}

int k_packs_zoom_max(void)
{
    for (int z = KG_MAX_ZOOM_PACKS - 1; z >= 0; z--) {
        if (s_pack[z].info.present) {
            return z;
        }
    }
    return -1;
}

uint32_t k_packs_count(void)
{
    uint32_t n = 0;
    for (int z = 0; z < KG_MAX_ZOOM_PACKS; z++) {
        n += s_pack[z].info.present ? 1 : 0;
    }
    return n;
}

const k_pack_info_t *k_packs_info(uint8_t zoom)
{
    return zoom < KG_MAX_ZOOM_PACKS && s_pack[zoom].info.present ? &s_pack[zoom].info : NULL;
}

const char *k_packs_attribution(void)
{
    return s_attr;
}

k_tile_rc_t k_packs_read_tile(uint8_t zoom, int32_t x, int32_t y,
                              uint8_t *buf, size_t cap, size_t *len)
{
    *len = 0;
    if (zoom >= KG_MAX_ZOOM_PACKS) {
        return K_TILE_NONE;
    }
    k_tile_rc_t rc = K_TILE_NONE;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    pack_t *p = &s_pack[zoom];
    uint32_t cell;
    if (p->file && kt_cell(&p->hdr, x, y, &cell)) {
        uint8_t ent[KT_INDEX_ENTRY];
        size_t got = 0;
        uint32_t off, n;
        if (hal_file_read_at(p->file, kt_index_pos(&p->hdr, cell), ent, sizeof(ent), &got) != ESP_OK ||
            got != sizeof(ent)) {
            rc = K_TILE_IOERR;
        } else if (!kt_entry_decode(ent, p->info.file_bytes, &off, &n) || n > cap) {
            rc = K_TILE_BAD;
        } else if (n == 0) {
            rc = K_TILE_NONE;
        } else if (hal_file_read_at(p->file, off, buf, n, &got) != ESP_OK || got != n) {
            rc = K_TILE_IOERR;
        } else {
            *len = n;
            rc = K_TILE_OK;
        }
    }
    xSemaphoreGive(s_mtx);
    if (rc == K_TILE_IOERR) {
        s_rescan_req = true;          /* let the monitor decide if the card vanished */
    }
    return rc;
}

/* ---- state ------------------------------------------------------------- */
k_sd_state_t k_storage_state(void)
{
    return s_state;
}

const char *k_storage_state_text(void)
{
    switch (s_state) {
    case K_SD_MOUNTING: return "Reading SD card...";
    case K_SD_READY:    return "";
    case K_SD_NODATA:   return "No map data: copy zNN.tpk files to /maps on the SD card";
    case K_SD_MISSING:  return "SD card missing: insert a card with map packs";
    }
    return "";
}

void k_storage_rescan(void)
{
    s_rescan_req = true;
}

uint32_t k_storage_generation(void)
{
    return s_gen;
}

bool k_storage_space(uint64_t *total, uint64_t *free_bytes)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    bool ok = hal_sd_space(total, free_bytes) == ESP_OK;
    xSemaphoreGive(s_mtx);
    return ok;
}

static void set_state(k_sd_state_t st)
{
    if (s_state != st) {
        s_state = st;
        k_events_post(KEV_STORAGE);
    }
}

/* ---- monitor task ------------------------------------------------------ */
static void try_mount_and_scan(void)
{
    set_state(K_SD_MOUNTING);
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    esp_err_t err = hal_sd_mount();
    int found = 0;
    if (err == ESP_OK) {
        found = scan_locked();
    }
    xSemaphoreGive(s_mtx);

    if (err != ESP_OK) {
        set_state(K_SD_MISSING);
    } else {
        set_state(found > 0 ? K_SD_READY : K_SD_NODATA);
    }
    k_events_post(KEV_STORAGE);
}

static void monitor_task(void *arg)
{
    (void)arg;
    try_mount_and_scan();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (s_state == K_SD_MISSING) {
            /* Retry: covers "card reinserted" (T-05). Mount failure is cheap. */
            try_mount_and_scan();
            continue;
        }

        xSemaphoreTake(s_mtx, portMAX_DELAY);
        bool alive = hal_sd_probe();
        if (!alive) {
            ESP_LOGW(TAG, "card removed");
            close_all_locked();
            hal_sd_unmount();
            s_gen++;
        }
        xSemaphoreGive(s_mtx);

        if (!alive) {
            set_state(K_SD_MISSING);
            k_events_post(KEV_STORAGE);
        } else if (s_rescan_req) {
            s_rescan_req = false;
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            int found = scan_locked();
            xSemaphoreGive(s_mtx);
            set_state(found > 0 ? K_SD_READY : K_SD_NODATA);
            k_events_post(KEV_STORAGE);
        }
    }
}

void k_storage_start(void)
{
    s_mtx = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(monitor_task, "sd_mon", 4096, NULL, 3, NULL, 0);
}
