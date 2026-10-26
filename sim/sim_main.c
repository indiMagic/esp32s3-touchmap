/* Host simulator: renders the real UI screens to PPM files.
 *   make -C sim run      (then: python3 sim/to_png.py)
 */
#include "sim.h"
#include "ui.h"
#include "gfx.h"
#include "map_view.h"

#include <stdio.h>
#include <string.h>

/* The simulator owns the screen stack (the firmware's lives in ui_task.c). */
static const ui_screen_t *s_stack[6];
static int s_depth;

void ui_invalidate(void) { }
void ui_push(const ui_screen_t *s) { s_stack[s_depth++] = s; if (s->enter) s->enter(s); }
void ui_pop(void)                  { if (s_depth > 1) s_depth--; }
void ui_pop_to_root(void)          { s_depth = 1; }

static void frame(const char *name)
{
    const ui_screen_t *top = s_stack[s_depth - 1];
    top->draw(top);
    k_display_present();
    char path[96];
    snprintf(path, sizeof(path), "sim/out/%s.ppm", name);
    sim_dump_ppm(path);
    printf("rendered %s\n", path);
}

static void tap(int x, int y)
{
    const ui_screen_t *top = s_stack[s_depth - 1];
    k_touch_evt_t d = { K_TOUCH_DOWN, (int16_t)x, (int16_t)y, 0, 0, (int16_t)x, (int16_t)y };
    k_touch_evt_t u = { K_TOUCH_UP,   (int16_t)x, (int16_t)y, 0, 0, (int16_t)x, (int16_t)y };
    if (top->touch) { top->touch(top, &d); top->touch(top, &u); }
}

static void set_route(void)
{
    /* A short L-shaped route in the middle of Toronto. */
    static const int32_t pts[][2] = {
        { 43649000, -79385000 }, { 43649800, -79385000 }, { 43650600, -79385000 },
        { 43651400, -79385000 }, { 43651400, -79383600 }, { 43651400, -79382200 },
        { 43651400, -79380800 }, { 43652600, -79380800 }, { 43653800, -79380800 },
    };
    g_sim.route_n = sizeof(pts) / sizeof(pts[0]);
    for (uint32_t i = 0; i < g_sim.route_n; i++) {
        g_sim.route[i].lat_e6 = pts[i][0];
        g_sim.route[i].lon_e6 = pts[i][1];
        g_sim.route[i].mx = kg_merc_x(pts[i][1]);
        g_sim.route[i].my = kg_merc_y(pts[i][0]);
    }
}

int main(void)
{
    g_sim.now_ms = 10000;
    g_sim.sd = K_SD_READY;
    gfx_init();
    map_view_init();
    ui_push(&screen_map);

    /* M1: idle, waiting for a phone */
    g_sim.nav.link = K_LINK_ADVERTISING;
    frame("m1_idle");

    /* M2: navigating */
    g_sim.nav.link = K_LINK_CONNECTED;
    g_sim.nav.has_fix = true;
    g_sim.nav.lat_e6 = 43649400; g_sim.nav.lon_e6 = -79385000;
    g_sim.nav.heading_d10 = 0; g_sim.nav.speed_d10 = 133; g_sim.nav.accuracy_m = 5;
    g_sim.nav.pos_ms = g_sim.now_ms - 500;
    g_sim.nav.route_active = true;
    g_sim.nav.has_nav = true;
    g_sim.nav.maneuver = KP_MAN_RIGHT;
    g_sim.nav.to_turn_m = 300; g_sim.nav.remain_m = 12400; g_sim.nav.remain_s = 1080;
    strcpy(g_sim.nav.text, "Turn right onto King St");
    set_route();
    frame("m2_navigating");

    /* every maneuver arrow on the banner */
    for (int m = 0; m < KP_MAN_COUNT; m++) {
        g_sim.nav.maneuver = (uint8_t)m;
        char n[32]; snprintf(n, sizeof(n), "arrow_%02d", m);
        frame(n);
    }
    g_sim.nav.maneuver = KP_MAN_LEFT;
    g_sim.nav.flags = KP_NAVF_OFFROUTE;
    frame("m2_offroute");
    g_sim.nav.flags = 0;
    g_sim.nav.maneuver = KP_MAN_RIGHT;

    /* stale position (NAV-04): no POS for 10 s -> "no position yet" */
    uint32_t keep = g_sim.nav.pos_ms;
    g_sim.nav.pos_ms = g_sim.now_ms - 11000;
    frame("nav04_stale");
    g_sim.nav.pos_ms = keep;

    /* miles */
    k_settings()->units = K_UNITS_MI;
    frame("m2_miles");
    k_settings()->units = K_UNITS_KM;

    /* missing tiles show grey blocks (MAP-04) */
    g_sim.tiles_missing = true;
    frame("map04_grey");
    g_sim.tiles_missing = false;

    /* drive the UI through taps, as a finger would */
    map_view_pan(-120, 40);                      /* drag: leaves follow mode */
    frame("pan_free");
    tap(728 + 32, 100 + 2 * 72 + 32);            /* recenter button */
    frame("recentered");
    tap(728 + 32, 100 + 32);                     /* zoom + */
    printf("zoom after + : %u\n", map_view_zoom());
    tap(728 + 32, 100 + 72 + 32);                /* zoom - */
    tap(728 + 32, 100 + 72 + 32);
    printf("zoom after - - : %u\n", map_view_zoom());
    tap(728 + 32, 100 + 3 * 72 + 32);            /* gear -> Settings */
    frame("s1_settings");

    tap(200, 56 + 8 + 60);                       /* Bluetooth tile */
    frame("s2_bluetooth");
    tap(30, 28);                                 /* back */
    tap(600, 56 + 8 + 2 * 132 + 60);             /* Map tile (row 3, col 2)... */
    frame("s6_map_options");
    tap(30, 28);
    tap(200, 56 + 8 + 132 + 60);                 /* Storage tile (row 2, col 1) */
    frame("s4_storage");
    tap(30, 28);
    tap(600, 56 + 8 + 2 * 132 + 60);             /* About? row3 col2 is About */
    frame("s7_about");
    tap(400, 56 + 4 * 64 + 32);                  /* Restore factory settings */
    frame("e2_factory");
    tap(30, 28);                                 /* back out of the dialog */
    tap(30, 28);                                 /* back to Settings */
    tap(30, 28);                                 /* back to map */
    printf("stack depth back at map: %d\n", s_depth);

    /* E1: card missing / no packs */
    g_sim.sd = K_SD_MISSING;
    frame("e1_sd_missing");
    g_sim.sd = K_SD_NODATA;
    frame("e1_sd_nodata");
    return 0;
}
