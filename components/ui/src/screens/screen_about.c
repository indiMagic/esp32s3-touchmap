/* S7: firmware, IDF, memory, uptime, card state, factory reset entry. */
#include "ui.h"
#include "ui_form.h"
#include "kernel.h"

#include <stdio.h>

static void fw_val(char *b, size_t n)
{
    snprintf(b, n, "%s  /  ESP-IDF %s", KERNEL_FW_VERSION, kernel_idf_version());
}

static void mem_val(char *b, size_t n)
{
    snprintf(b, n, "heap %u KB, PSRAM %u.%u MB free",
             (unsigned)(kernel_heap_free() / 1024),
             (unsigned)(kernel_psram_free() >> 20),
             (unsigned)(((kernel_psram_free() >> 10) & 1023) * 10 / 1024));
}

static void up_val(char *b, size_t n)
{
    uint32_t s = kernel_millis() / 1000;
    snprintf(b, n, "%02u:%02u:%02u", (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
}

static void sd_val(char *b, size_t n)
{
    switch (k_storage_state()) {
    case K_SD_READY:   snprintf(b, n, "Ready, %u packs", (unsigned)k_packs_count()); break;
    case K_SD_NODATA:  snprintf(b, n, "No packs"); break;
    case K_SD_MISSING: snprintf(b, n, "Missing"); break;
    default:           snprintf(b, n, "Reading..."); break;
    }
}

static void open_factory(void) { ui_push(&screen_factory); }

static const ui_row_t s_rows[] = {
    { "Firmware",   ROW_INFO,   NULL, fw_val,  NULL },
    { "Memory",     ROW_INFO,   NULL, mem_val, NULL },
    { "Uptime",     ROW_INFO,   NULL, up_val,  NULL },
    { "SD card",    ROW_INFO,   NULL, sd_val,  NULL },
    { "Restore factory settings", ROW_DANGER, NULL, NULL, open_factory },
};

static const ui_form_t s_form = {
    .rows = s_rows,
    .count = sizeof(s_rows) / sizeof(s_rows[0]),
};

const ui_screen_t screen_about = {
    .title = "About",
    .draw = ui_form_draw,
    .touch = ui_form_touch,
    .data = &s_form,
};
