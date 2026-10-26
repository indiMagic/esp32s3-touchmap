/* S6: default zoom, follow position, units, show speed, clear route. */
#include "ui.h"
#include "ui_form.h"
#include "kernel.h"

#include <stdio.h>

#define ZOOM_MIN_CHOICE 5
#define ZOOM_MAX_CHOICE 18

static void zoom_val(char *b, size_t n) { snprintf(b, n, "%u  (tap to change)", k_settings()->default_zoom); }
static void zoom_next(void)
{
    uint8_t z = k_settings()->default_zoom + 1;
    k_settings()->default_zoom = (uint8_t)(z > ZOOM_MAX_CHOICE ? ZOOM_MIN_CHOICE : z);
    k_settings_commit();
}

static bool follow_get(void) { return k_settings()->follow_at_start; }
static void follow_flip(void)
{
    k_settings()->follow_at_start = !k_settings()->follow_at_start;
    k_settings_commit();
}

static void units_val(char *b, size_t n) { snprintf(b, n, "%s  (tap to change)", k_settings()->units == K_UNITS_MI ? "miles" : "kilometres"); }
static void units_flip(void)
{
    k_settings()->units = k_settings()->units == K_UNITS_MI ? K_UNITS_KM : K_UNITS_MI;   /* NAV-06 */
    k_settings_commit();
}

static bool speed_get(void) { return k_settings()->show_speed; }
static void speed_flip(void)
{
    k_settings()->show_speed = !k_settings()->show_speed;
    k_settings_commit();
}

static void clear_route(void)
{
    k_route_clear();
    k_nav_set_route(false, 0, 0);
    k_nav_clear_turn();
}

static const ui_row_t s_rows[] = {
    { "Default zoom",       ROW_CHOICE, NULL,       zoom_val,  zoom_next },
    { "Follow position",    ROW_TOGGLE, follow_get, NULL,      follow_flip },
    { "Units",              ROW_CHOICE, NULL,       units_val, units_flip },
    { "Show speed",         ROW_TOGGLE, speed_get,  NULL,      speed_flip },
    { "Clear route",        ROW_DANGER, NULL,       NULL,      clear_route },
};

static const ui_form_t s_form = {
    .rows = s_rows,
    .count = sizeof(s_rows) / sizeof(s_rows[0]),
    .footnote = "Clear route removes the route and the next-turn banner.",
};

const ui_screen_t screen_mapopts = {
    .title = "Map",
    .draw = ui_form_draw,
    .touch = ui_form_touch,
    .data = &s_form,
};
