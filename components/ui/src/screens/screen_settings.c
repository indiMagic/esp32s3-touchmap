/* S1: six tiles, each with a one-line status (SRS 6, Figure 13). */
#include "ui.h"
#include "gfx.h"
#include "ui_form.h"
#include "ui_theme.h"
#include "kernel.h"

#include <stdio.h>

#define COLS   2
#define ROWS   3
#define GAP    8
#define TILE_W ((UI_W - GAP * (COLS + 1)) / COLS)
#define TILE_H ((UI_H - UI_HEADER_H - GAP * (ROWS + 1)) / ROWS)

typedef struct {
    const char *name;
    const ui_screen_t *target;
    void (*status)(char *buf, size_t n);
} tile_t;

static void st_ble(char *b, size_t n)
{
    k_nav_snapshot_t s;
    k_nav_snapshot(&s);
    snprintf(b, n, "%s", s.link == K_LINK_CONNECTED ? "Connected" : s.link == K_LINK_ADVERTISING ? "Advertising" : "Off");
}
static void st_wifi(char *b, size_t n)    { snprintf(b, n, "Planned"); }
static void st_storage(char *b, size_t n)
{
    uint64_t total, freeb;
    if (k_storage_space(&total, &freeb)) {
        snprintf(b, n, "%u.%u GB free", (unsigned)(freeb >> 30), (unsigned)(((freeb >> 20) & 1023) * 10 / 1024));
    } else {
        snprintf(b, n, "No card");
    }
}
static void st_display(char *b, size_t n)
{
    static const char *t[] = { "never", "1 min", "5 min", "15 min" };
    snprintf(b, n, "Timeout: %s", t[k_settings()->timeout_idx & 3]);
}
static void st_map(char *b, size_t n)
{
    snprintf(b, n, "%s, follow %s", k_settings()->units == K_UNITS_MI ? "mi" : "km",
             k_settings()->follow_at_start ? "on" : "off");
}
static void st_about(char *b, size_t n)   { snprintf(b, n, "%s", KERNEL_FW_VERSION); }

static const tile_t s_tiles[ROWS * COLS] = {
    { "Bluetooth", &screen_bluetooth, st_ble },
    { "WiFi",      &screen_wifi,      st_wifi },
    { "Storage",   &screen_storage,   st_storage },
    { "Display",   &screen_display,   st_display },
    { "Map",       &screen_mapopts,   st_map },
    { "About",     &screen_about,     st_about },
};

static void tile_xy(int i, int *x, int *y)
{
    *x = GAP + (i % COLS) * (TILE_W + GAP);
    *y = UI_HEADER_H + GAP + (i / COLS) * (TILE_H + GAP);
}

static void draw(const ui_screen_t *self)
{
    gfx_clear(C_PANEL);
    ui_draw_header(self->title);
    for (int i = 0; i < ROWS * COLS; i++) {
        int x, y;
        tile_xy(i, &x, &y);
        gfx_fill_rect(x, y, TILE_W, TILE_H, C_PANEL_ALT);
        gfx_fill_circle(x + 40, y + TILE_H / 2, 22, C_PANEL_HI);
        gfx_text(x + 80, y + TILE_H / 2 - 28, s_tiles[i].name, C_TEXT, 2);
        char st[48];
        s_tiles[i].status(st, sizeof(st));
        gfx_text(x + 80, y + TILE_H / 2 + 12, st, i == 0 && st[0] == 'C' ? C_OK : C_TEXT_DIM, 1);
    }
}

static void touch(const ui_screen_t *self, const k_touch_evt_t *e)
{
    (void)self;
    if (ui_header_back_hit(e)) {
        ui_pop();
        return;
    }
    if (!ui_is_tap(e)) {
        return;
    }
    for (int i = 0; i < ROWS * COLS; i++) {
        int x, y;
        tile_xy(i, &x, &y);
        if (e->sx >= x && e->sx < x + TILE_W && e->sy >= y && e->sy < y + TILE_H) {
            ui_push(s_tiles[i].target);
            return;
        }
    }
}

const ui_screen_t screen_settings = {
    .title = "Settings",
    .draw = draw,
    .touch = touch,
};
