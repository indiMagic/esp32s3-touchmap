/* S4: card size and free space, one summary per zoom pack, rescan (SD-05). */
#include "ui.h"
#include "ui_form.h"
#include "kernel.h"

#include <stdio.h>

static void card_val(char *b, size_t n)
{
    switch (k_storage_state()) {
    case K_SD_READY:    snprintf(b, n, "Ready"); break;
    case K_SD_NODATA:   snprintf(b, n, "Mounted, no map packs"); break;
    case K_SD_MISSING:  snprintf(b, n, "Missing"); break;
    default:            snprintf(b, n, "Reading..."); break;
    }
}

static void space_val(char *b, size_t n)
{
    uint64_t total, freeb;
    if (k_storage_space(&total, &freeb)) {
        snprintf(b, n, "%u.%u GB, %u.%u GB free",
                 (unsigned)(total >> 30), (unsigned)(((total >> 20) & 1023) * 10 / 1024),
                 (unsigned)(freeb >> 30), (unsigned)(((freeb >> 20) & 1023) * 10 / 1024));
    } else {
        snprintf(b, n, "-");
    }
}

static void packs_val(char *b, size_t n)
{
    int lo = k_packs_zoom_min(), hi = k_packs_zoom_max();
    if (lo < 0) {
        snprintf(b, n, "none");
    } else {
        snprintf(b, n, "%u packs, zoom %d-%d", (unsigned)k_packs_count(), lo, hi);
    }
}

static void size_val(char *b, size_t n)
{
    uint64_t sum = 0;
    for (int z = 0; z <= KG_MAX_ZOOM; z++) {
        const k_pack_info_t *p = k_packs_info((uint8_t)z);
        if (p) {
            sum += p->file_bytes;
        }
    }
    snprintf(b, n, "%u MB", (unsigned)(sum >> 20));
}

static void attr_val(char *b, size_t n)  { snprintf(b, n, "%s", k_packs_attribution()[0] ? k_packs_attribution() : "-"); }

static void rescan(void)                 { k_storage_rescan(); }

static const ui_row_t s_rows[] = {
    { "Card",         ROW_INFO,   NULL, card_val,  NULL },
    { "Capacity",     ROW_INFO,   NULL, space_val, NULL },
    { "Map packs",    ROW_INFO,   NULL, packs_val, NULL },
    { "Packs size",   ROW_INFO,   NULL, size_val,  NULL },
    { "Attribution",  ROW_INFO,   NULL, attr_val,  NULL },
    { "Rescan card",  ROW_ACTION, NULL, NULL,      rescan },
};

static const ui_form_t s_form = {
    .rows = s_rows,
    .count = sizeof(s_rows) / sizeof(s_rows[0]),
};

const ui_screen_t screen_storage = {
    .title = "Storage (SD card)",
    .draw = ui_form_draw,
    .touch = ui_form_touch,
    .data = &s_form,
};
