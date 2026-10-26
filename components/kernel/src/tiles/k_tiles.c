#include "k_tiles.h"
#include "k_storage.h"
#include "k_tpk.h"
#include "k_events.h"
#include "hal.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "k_tiles";

typedef enum { S_EMPTY = 0, S_PENDING, S_READY, S_FAILED } slot_state_t;

typedef struct {
    slot_state_t state;
    uint8_t      z;
    int32_t      x, y;
    uint32_t     stamp;        /* LRU clock value of the last use */
    uint32_t     frame;        /* frame in which the slot was last used */
    uint16_t    *px;
} slot_t;

static slot_t            s_slot[K_TILE_CACHE_SLOTS];
static SemaphoreHandle_t s_mtx;
static QueueHandle_t     s_req;
static uint32_t          s_clock;
static uint32_t          s_frame;
static uint32_t          s_gen_seen;
static uint32_t          s_flush_epoch;

static struct { uint8_t z; int32_t x0, y0, x1, y1; } s_view;

static uint8_t *s_jpeg;       /* scratch for compressed bytes, 192 KiB */

/* ---- UI side ----------------------------------------------------------- */
void k_tiles_begin_frame(uint8_t z, int32_t tx0, int32_t ty0, int32_t tx1, int32_t ty1)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);

    uint32_t gen = k_storage_generation();
    if (gen != s_gen_seen) {                 /* packs changed or card pulled */
        s_gen_seen = gen;
        for (int i = 0; i < K_TILE_CACHE_SLOTS; i++) {
            if (s_slot[i].state != S_PENDING) {
                s_slot[i].state = S_EMPTY;
            }
        }
        s_flush_epoch++;
    }

    s_frame++;
    s_view.z = z;
    s_view.x0 = tx0; s_view.y0 = ty0;
    s_view.x1 = tx1; s_view.y1 = ty1;
}

static int pick_victim(void)
{
    int best = -1;
    for (int i = 0; i < K_TILE_CACHE_SLOTS; i++) {
        slot_t *s = &s_slot[i];
        if (s->state == S_PENDING || s->frame == s_frame) {
            continue;                        /* in flight, or needed by this frame */
        }
        if (s->state == S_EMPTY) {
            return i;
        }
        if (best < 0 || s->stamp < s_slot[best].stamp) {
            best = i;
        }
    }
    return best;
}

const uint16_t *k_tiles_get(uint8_t z, int32_t tx, int32_t ty)
{
    for (int i = 0; i < K_TILE_CACHE_SLOTS; i++) {
        slot_t *s = &s_slot[i];
        if (s->state != S_EMPTY && s->z == z && s->x == tx && s->y == ty) {
            s->stamp = ++s_clock;
            s->frame = s_frame;
            return s->state == S_READY ? s->px : NULL;
        }
    }

    int v = pick_victim();
    if (v < 0) {
        return NULL;
    }
    slot_t *s = &s_slot[v];
    s->state = S_PENDING;
    s->z = z; s->x = tx; s->y = ty;
    s->stamp = ++s_clock;
    s->frame = s_frame;
    uint8_t idx = (uint8_t)v;
    if (xQueueSend(s_req, &idx, 0) != pdTRUE) {
        s->state = S_EMPTY;                  /* queue full: try again next frame */
    }
    return NULL;
}

void k_tiles_end_frame(void)
{
    xSemaphoreGive(s_mtx);
}

void k_tiles_flush(void)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < K_TILE_CACHE_SLOTS; i++) {
        if (s_slot[i].state != S_PENDING) {
            s_slot[i].state = S_EMPTY;
        }
    }
    s_flush_epoch++;
    xSemaphoreGive(s_mtx);
}

/* ---- loader task ------------------------------------------------------- */
static void loader_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint8_t idx;
        if (xQueueReceive(s_req, &idx, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* Skip the request if the view moved on. */
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        slot_t *s = &s_slot[idx];
        uint8_t z = s->z;
        int32_t x = s->x, y = s->y;
        uint32_t epoch = s_flush_epoch;
        bool wanted = z == s_view.z &&
                      x >= s_view.x0 - 1 && x <= s_view.x1 + 1 &&
                      y >= s_view.y0 - 1 && y <= s_view.y1 + 1;
        if (!wanted) {
            s->state = S_EMPTY;
        }
        xSemaphoreGive(s_mtx);
        if (!wanted) {
            continue;
        }

        /* Slow part, no lock held: SD read then JPEG decode into the slot. */
        size_t n = 0;
        slot_state_t result = S_FAILED;
        k_tile_rc_t rc = k_packs_read_tile(z, x, y, s_jpeg, KT_MAX_TILE_BYTES, &n);
        if (rc == K_TILE_OK) {
            if (hal_jpeg_decode_tile(s_jpeg, n, s->px, K_TILE_BYTES) == ESP_OK) {
                result = S_READY;
            }                                  /* corrupt JPEG: tile skipped (T-06) */
        }

        xSemaphoreTake(s_mtx, portMAX_DELAY);
        s->state = (epoch == s_flush_epoch) ? result : S_EMPTY;
        xSemaphoreGive(s_mtx);
        k_events_post(KEV_TILE);
    }
}

void k_tiles_start(void)
{
    s_mtx = xSemaphoreCreateMutex();
    s_req = xQueueCreate(K_TILE_CACHE_SLOTS, sizeof(uint8_t));
    s_jpeg = hal_psram_alloc(KT_MAX_TILE_BYTES);

    for (int i = 0; i < K_TILE_CACHE_SLOTS; i++) {
        s_slot[i].px = hal_psram_alloc(K_TILE_BYTES);
        if (!s_slot[i].px) {
            ESP_LOGE(TAG, "tile cache slot %d: out of PSRAM", i);
        }
    }
    s_gen_seen = k_storage_generation();

    /* Lower priority than the UI task so the screen always wins. */
    xTaskCreatePinnedToCore(loader_task, "tile_loader", 6144, NULL, 4, NULL, 0);
}
