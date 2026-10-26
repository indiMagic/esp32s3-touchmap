#include "ui_form.h"
#include "gfx.h"
#include "ui_theme.h"

#include <stdlib.h>

#define TAP_SLOP 14

bool ui_is_tap(const k_touch_evt_t *e)
{
    return e->type == K_TOUCH_UP && abs(e->x - e->sx) <= TAP_SLOP && abs(e->y - e->sy) <= TAP_SLOP;
}

void ui_draw_header(const char *title)
{
    gfx_fill_rect(0, 0, UI_W, UI_HEADER_H, C_PANEL_ALT);
    gfx_fill_rect(0, 0, UI_BTN, UI_HEADER_H, C_PANEL_HI);
    /* back chevron */
    gfx_line(36, 14, 22, UI_HEADER_H / 2, 5, C_TEXT);
    gfx_line(22, UI_HEADER_H / 2, 36, UI_HEADER_H - 14, 5, C_TEXT);
    gfx_text(UI_BTN + 12, (UI_HEADER_H - 32) / 2, title, C_TEXT, 2);
}

bool ui_header_back_hit(const k_touch_evt_t *e)
{
    return ui_is_tap(e) && e->sx < UI_BTN && e->sy < UI_HEADER_H;
}

static void draw_switch(int x, int y, bool on)
{
    gfx_fill_rect(x, y, 64, 32, on ? C_OK : C_PANEL_HI);
    gfx_fill_rect(on ? x + 36 : x + 4, y + 4, 24, 24, C_TEXT);
}

void ui_form_draw(const ui_screen_t *self)
{
    const ui_form_t *f = self->data;
    gfx_clear(C_PANEL);
    ui_draw_header(self->title);

    for (int i = 0; i < f->count; i++) {
        const ui_row_t *r = &f->rows[i];
        int y = UI_HEADER_H + i * UI_ROW_H;
        gfx_fill_rect(0, y, UI_W, UI_ROW_H - 2, i % 2 ? C_PANEL : C_PANEL_ALT);

        char val[64] = "";
        if (r->value) {
            r->value(val, sizeof(val));
        }

        if (r->kind == ROW_ACTION || r->kind == ROW_DANGER) {
            uint16_t col = r->kind == ROW_DANGER ? C_DANGER : C_ACCENT;
            gfx_fill_rect(16, y + 8, UI_W - 32, UI_ROW_H - 18, col);
            gfx_text_center(UI_W / 2, y + (UI_ROW_H - 2 - 16) / 2, r->label, C_TEXT, 1);
            continue;
        }

        gfx_text(16, y + (UI_ROW_H - 2 - 32) / 2, r->label, C_TEXT, 2);
        if (r->kind == ROW_TOGGLE) {
            draw_switch(UI_W - 16 - 64, y + (UI_ROW_H - 2 - 32) / 2, r->get ? r->get() : false);
        } else if (val[0]) {
            uint16_t col = r->kind == ROW_CHOICE ? C_ACCENT : C_TEXT_DIM;
            gfx_text(UI_W - 16 - gfx_text_w(val, 1), y + (UI_ROW_H - 2 - 16) / 2, val, col, 1);
        }
    }

    if (f->footnote) {
        gfx_text(16, UI_H - 24, f->footnote, C_TEXT_DIM, 1);
    }
}

void ui_form_touch(const ui_screen_t *self, const k_touch_evt_t *e)
{
    if (ui_header_back_hit(e)) {
        ui_pop();
        return;
    }
    if (!ui_is_tap(e)) {
        return;
    }
    const ui_form_t *f = self->data;
    int row = (e->sy - UI_HEADER_H) / UI_ROW_H;
    if (e->sy >= UI_HEADER_H && row >= 0 && row < f->count) {
        const ui_row_t *r = &f->rows[row];
        if (r->kind != ROW_INFO && r->act) {
            r->act();
            ui_invalidate();
        }
    }
}
