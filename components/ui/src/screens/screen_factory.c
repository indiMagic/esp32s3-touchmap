/* E2: confirmation dialog. Settings are erased; map packs on the SD card never are (SET-01..07). */
#include "ui.h"
#include "gfx.h"
#include "ui_form.h"
#include "ui_theme.h"
#include "kernel.h"

#define DLG_X 100
#define DLG_Y 120
#define DLG_W 600
#define DLG_H 240
#define BTN_W 220
#define BTN_H 64

static void btn_xy(int i, int *x, int *y)          /* 0 = Cancel, 1 = Erase */
{
    *x = DLG_X + 40 + i * (BTN_W + 80);
    *y = DLG_Y + DLG_H - BTN_H - 24;
}

static void draw(const ui_screen_t *self)
{
    gfx_clear(C_PANEL);
    ui_draw_header(self->title);
    gfx_fill_rect(DLG_X, DLG_Y, DLG_W, DLG_H, C_PANEL_ALT);
    gfx_rect(DLG_X, DLG_Y, DLG_W, DLG_H, C_WARN);
    gfx_text_center(DLG_X + DLG_W / 2, DLG_Y + 20, "Erase all settings?", C_TEXT, 2);
    gfx_text_center(DLG_X + DLG_W / 2, DLG_Y + 72, "Bluetooth name, units and map options", C_TEXT_DIM, 1);
    gfx_text_center(DLG_X + DLG_W / 2, DLG_Y + 92, "return to their defaults.", C_TEXT_DIM, 1);
    gfx_text_center(DLG_X + DLG_W / 2, DLG_Y + 120, "Map packs on the SD card are not touched.", C_TEXT_DIM, 1);

    int x, y;
    btn_xy(0, &x, &y);
    gfx_fill_rect(x, y, BTN_W, BTN_H, C_PANEL_HI);
    gfx_text_center(x + BTN_W / 2, y + 24, "Cancel", C_TEXT, 1);
    btn_xy(1, &x, &y);
    gfx_fill_rect(x, y, BTN_W, BTN_H, C_DANGER);
    gfx_text_center(x + BTN_W / 2, y + 24, "Erase", C_TEXT, 1);
}

static bool hit(int i, int px, int py)
{
    int x, y;
    btn_xy(i, &x, &y);
    return px >= x && px < x + BTN_W && py >= y && py < y + BTN_H;
}

static void touch(const ui_screen_t *self, const k_touch_evt_t *e)
{
    (void)self;
    if (ui_header_back_hit(e) || (ui_is_tap(e) && hit(0, e->sx, e->sy))) {
        ui_pop();
        return;
    }
    if (ui_is_tap(e) && hit(1, e->sx, e->sy)) {
        k_settings_factory_reset();
        k_ble_set_enabled(k_settings()->ble_on);
        k_display_backlight(k_settings()->backlight);
        ui_pop_to_root();
    }
}

const ui_screen_t screen_factory = {
    .title = "Restore factory settings",
    .draw = draw,
    .touch = touch,
};
