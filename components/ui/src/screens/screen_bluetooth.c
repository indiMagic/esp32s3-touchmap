/* S2: on/off, device name, status, passkey and bonded phones (the last two are planned: BLE-06). */
#include "ui.h"
#include "ui_form.h"
#include "kernel.h"

#include <stdio.h>

static bool ble_get(void)  { return k_settings()->ble_on; }
static void ble_flip(void)
{
    k_settings()->ble_on = !k_settings()->ble_on;
    k_ble_set_enabled(k_settings()->ble_on);      /* BLE-07 */
    k_settings_commit();
}

static void name_val(char *b, size_t n) { snprintf(b, n, "%s", k_settings()->device_name); }

static void status_val(char *b, size_t n)
{
    k_nav_snapshot_t s;
    k_nav_snapshot(&s);
    snprintf(b, n, "%s", s.link == K_LINK_CONNECTED ? "Connected: phone page (Bluefy)" :
                         s.link == K_LINK_ADVERTISING ? "Advertising" : "Off");
}

static void planned_val(char *b, size_t n) { snprintf(b, n, "planned"); }

static const ui_row_t s_rows[] = {
    { "Bluetooth",       ROW_TOGGLE, ble_get, NULL,        ble_flip },
    { "Device name",     ROW_INFO,   NULL,    name_val,    NULL },       /* BLE-08: editor is planned */
    { "Status",          ROW_INFO,   NULL,    status_val,  NULL },
    { "Passkey pairing", ROW_INFO,   NULL,    planned_val, NULL },       /* BLE-06 */
    { "Bonded phones",   ROW_INFO,   NULL,    planned_val, NULL },       /* BLE-06 */
};

static const ui_form_t s_form = {
    .rows = s_rows,
    .count = sizeof(s_rows) / sizeof(s_rows[0]),
    .footnote = "The phone page lists this device by the name above.",
};

const ui_screen_t screen_bluetooth = {
    .title = "Bluetooth",
    .draw = ui_form_draw,
    .touch = ui_form_touch,
    .data = &s_form,
};
