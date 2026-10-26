/* Home screen: the map with the HUD on top (SRS 6, M1/M2/E1). */
#include "ui.h"
#include "gfx.h"
#include "hud.h"
#include "map_view.h"
#include "ui_form.h"
#include "ui_theme.h"
#include "kernel.h"

#include <stdlib.h>

#define DRAG_START_PX 6

static bool s_dragging;

static void draw(const ui_screen_t *self)
{
    (void)self;
    k_nav_snapshot_t nav;
    k_nav_snapshot(&nav);

    if (k_storage_state() == K_SD_READY) {
        map_view_draw(&nav);
    } else {
        /* E1: card missing or no packs. The map screen stays usable. */
        gfx_clear(C_MAP_BG);
        const char *msg = k_storage_state_text();
        gfx_text_center(UI_W / 2 - 32, UI_H / 2 - 8, msg, C_BTN_ICON, 1);
    }

    int32_t clat, clon;
    map_view_centre_latlon(&clat, &clon);
    hud_draw(&nav, map_view_zoom(), map_view_following(), clat, clon);

    if (k_storage_state() == K_SD_MISSING) {
        /* Right side of the second status line: free because there is no attribution. */
        gfx_text(UI_W - 2 - gfx_text_w("SD card missing", 1), UI_H - UI_STATUS_H + 16,
                 "SD card missing", C_DANGER, 1);
    }
}

static void touch(const ui_screen_t *self, const k_touch_evt_t *e)
{
    (void)self;
    switch (e->type) {
    case K_TOUCH_DOWN:
        s_dragging = false;
        break;

    case K_TOUCH_MOVE:
        if (hud_hit_test(e->sx, e->sy) != HUD_BTN_NONE) {
            break;                                  /* drag that began on a button */
        }
        if (!s_dragging && (abs(e->x - e->sx) > DRAG_START_PX || abs(e->y - e->sy) > DRAG_START_PX)) {
            s_dragging = true;                      /* MAP-02: one-finger pan */
        }
        if (s_dragging) {
            map_view_pan(e->dx, e->dy);
        }
        break;

    case K_TOUCH_UP:
        if (s_dragging || !ui_is_tap(e)) {
            s_dragging = false;
            break;
        }
        switch (hud_hit_test(e->sx, e->sy)) {
        case HUD_BTN_ZOOM_IN:   map_view_zoom_by(+1); break;
        case HUD_BTN_ZOOM_OUT:  map_view_zoom_by(-1); break;
        case HUD_BTN_RECENTER:  map_view_recenter();  break;
        case HUD_BTN_SETTINGS:  ui_push(&screen_settings); break;
        default: break;
        }
        break;
    }
}

const ui_screen_t screen_map = {
    .title = "Map",
    .draw = draw,
    .touch = touch,
};
