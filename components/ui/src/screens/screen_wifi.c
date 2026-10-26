/* S3: WiFi is a planned service channel (SRS 7, WIFI-01..08, open issue OI-1). */
#include "ui.h"
#include "ui_form.h"

#include <stdio.h>

static void planned_val(char *b, size_t n)  { snprintf(b, n, "planned"); }
static void ble_val(char *b, size_t n)      { snprintf(b, n, "stays on Bluetooth LE"); }

static const ui_row_t s_rows[] = {
    { "Join a network",   ROW_INFO, NULL, planned_val, NULL },    /* WIFI-01..04 */
    { "Access point + QR", ROW_INFO, NULL, planned_val, NULL },   /* WIFI-05, 06 */
    { "Upload map packs", ROW_INFO, NULL, planned_val, NULL },    /* WIFI-07 */
    { "Firmware update",  ROW_INFO, NULL, planned_val, NULL },    /* WIFI-08 */
    { "Live position",    ROW_INFO, NULL, ble_val,     NULL },
};

static const ui_form_t s_form = {
    .rows = s_rows,
    .count = sizeof(s_rows) / sizeof(s_rows[0]),
    .footnote = "HTTPS pages cannot open ws:// to the board (OI-1), so WiFi is not the live link.",
};

const ui_screen_t screen_wifi = {
    .title = "WiFi",
    .draw = ui_form_draw,
    .touch = ui_form_touch,
    .data = &s_form,
};
