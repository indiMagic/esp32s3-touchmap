#include "k_route.h"
#include "k_geo.h"
#include "k_events.h"
#include "hal_mem.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "k_route";

static k_route_pt_t     *s_pts;          /* PSRAM, KP_MAX_ROUTE_POINTS entries */
static uint32_t          s_count;        /* visible to readers (0 while uploading) */
static uint32_t          s_expected;
static uint32_t          s_next;         /* next index we expect in sequence */
static bool              s_in_order;
static SemaphoreHandle_t s_lock;

void k_route_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_pts = hal_psram_alloc(sizeof(k_route_pt_t) * KP_MAX_ROUTE_POINTS);
    if (!s_pts) {
        ESP_LOGE(TAG, "no PSRAM for route");
    }
}

bool k_route_begin(uint16_t expected_points)
{
    if (!s_pts || expected_points > KP_MAX_ROUTE_POINTS) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_count = 0;
    s_expected = expected_points;
    s_next = 0;
    s_in_order = true;
    xSemaphoreGive(s_lock);
    k_events_post(KEV_ROUTE);
    return true;
}

bool k_route_put(uint16_t start, const kp_pt_t *pts, size_t n)
{
    if (!s_pts) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = s_in_order && start == s_next && (size_t)start + n <= s_expected;
    if (ok) {
        for (size_t i = 0; i < n; i++) {
            k_route_pt_t *d = &s_pts[start + i];
            d->lat_e6 = pts[i].lat_e6;
            d->lon_e6 = pts[i].lon_e6;
            d->mx = kg_merc_x(pts[i].lon_e6);
            d->my = kg_merc_y(pts[i].lat_e6);
        }
        s_next = start + (uint32_t)n;
    } else {
        s_in_order = false;
    }
    xSemaphoreGive(s_lock);
    return ok;
}

bool k_route_end(uint16_t *received)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = s_in_order && s_next == s_expected && s_expected > 0;
    if (received) {
        *received = (uint16_t)s_next;
    }
    s_count = ok ? s_next : 0;
    xSemaphoreGive(s_lock);
    k_events_post(KEV_ROUTE);
    return ok;
}

void k_route_clear(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_count = 0;
    s_expected = 0;
    s_next = 0;
    xSemaphoreGive(s_lock);
    k_events_post(KEV_ROUTE);
}

const k_route_pt_t *k_route_lock(uint32_t *count)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *count = s_count;
    return s_pts;
}

void k_route_unlock(void)
{
    xSemaphoreGive(s_lock);
}
