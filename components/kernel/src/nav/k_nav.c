#include "k_nav.h"
#include "k_events.h"
#include "hal.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static k_nav_snapshot_t  s_nav;
static SemaphoreHandle_t s_lock;

void k_nav_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    memset(&s_nav, 0, sizeof(s_nav));
}

#define LOCK()   xSemaphoreTake(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_lock)

void k_nav_set_pos(const kp_pos_t *p)
{
    LOCK();
    s_nav.has_fix     = true;
    s_nav.lat_e6      = p->lat_e6;
    s_nav.lon_e6      = p->lon_e6;
    s_nav.heading_d10 = p->heading_d10;
    s_nav.speed_d10   = p->speed_d10;
    s_nav.accuracy_m  = p->accuracy_m;
    s_nav.pos_ms      = hal_millis();
    UNLOCK();
    k_events_post(KEV_POS);
}

void k_nav_set_turn(const kp_nav_t *n)
{
    LOCK();
    s_nav.has_nav   = true;
    s_nav.maneuver  = n->maneuver;
    s_nav.flags     = n->flags;
    s_nav.to_turn_m = n->to_turn_m;
    s_nav.remain_m  = n->remain_m;
    s_nav.remain_s  = n->remain_s;
    memcpy(s_nav.text, n->text, sizeof(s_nav.text));
    UNLOCK();
    k_events_post(KEV_NAV);
}

void k_nav_clear_turn(void)
{
    LOCK();
    s_nav.has_nav = false;
    s_nav.text[0] = '\0';
    UNLOCK();
    k_events_post(KEV_NAV);
}

void k_nav_set_route(bool active, uint32_t dist_m, uint32_t dur_s)
{
    LOCK();
    s_nav.route_active = active;
    s_nav.route_dist_m = dist_m;
    s_nav.route_dur_s  = dur_s;
    UNLOCK();
    k_events_post(KEV_ROUTE);
}

void k_nav_set_link(k_link_t link)
{
    LOCK();
    s_nav.link = link;
    UNLOCK();
    k_events_post(KEV_LINK);
}

void k_nav_snapshot(k_nav_snapshot_t *out)
{
    LOCK();
    *out = s_nav;
    UNLOCK();
}

bool k_nav_pos_valid(const k_nav_snapshot_t *s, uint32_t now_ms)
{
    return s->has_fix && (uint32_t)(now_ms - s->pos_ms) < K_POS_STALE_MS;
}
