/* S5: screen timeout, backlight switch, night palette (palette is planned: NAV-07). */
#include "ui.h"
#include "ui_form.h"
#include "kernel.h"

#include <stdio.h>

static const char *const TIMEOUTS[] = { "Never", "1 min", "5 min", "15 min" };

static void timeout_val(char *b, size_t n) { snprintf(b, n, "%s  (tap to change)", TIMEOUTS[k_settings()->timeout_idx & 3]); }
static void timeout_next(void)
{
    k_settings()->timeout_idx = (uint8_t)((k_settings()->timeout_idx + 1) & 3);
    k_settings_commit();
}

static bool bl_get(void)  { return k_settings()->backlight; }
static void bl_flip(void)
{
    k_settings()->backlight = !k_settings()->backlight;
    k_display_backlight(k_settings()->backlight);
    k_settings_commit();
}

static bool night_get(void) { return k_settings()->night_palette; }
static void night_flip(void)
{
    k_settings()->night_palette = !k_settings()->night_palette;     /* stored; map colours are planned */
    k_settings_commit();
}

static const ui_row_t s_rows[] = {
    { "Screen timeout", ROW_CHOICE, NULL,      timeout_val, timeout_next },
    { "Backlight",      ROW_TOGGLE, bl_get,    NULL,        bl_flip },
    { "Night palette",  ROW_TOGGLE, night_get, NULL,        night_flip },
};

static const ui_form_t s_form = {
    .rows = s_rows,
    .count = sizeof(s_rows) / sizeof(s_rows[0]),
    .footnote = "Brightness is not adjustable: the backlight is a switch, not a dimmer.",
};

const ui_screen_t screen_display = {
    .title = "Display",
    .draw = ui_form_draw,
    .touch = ui_form_touch,
    .data = &s_form,
};
