/* Fake kernel for the host simulator.
 *
 * Implements the kernel API the UI layer calls, with canned data, so the real
 * UI sources (gfx, map_view, hud, screens) can be compiled and rendered on a
 * PC. The UI layer cannot tell the difference: it only sees kernel.h.
 */
#include "kernel.h"
#include "sim.h"

#include <stdio.h>
#include <string.h>

sim_state_t g_sim;

static k_settings_t s_cfg = {
    .default_zoom = 15, .follow_at_start = true, .units = K_UNITS_KM, .show_speed = true,
    .timeout_idx = 0, .backlight = true, .night_palette = false, .ble_on = true,
    .device_name = "ESP32-Maps",
};

/* ---- time / system -------------------------------------------------------- */
uint32_t kernel_millis(void)        { return g_sim.now_ms; }
uint32_t kernel_psram_free(void)    { return 2260000; }
uint32_t kernel_heap_free(void)     { return 142 * 1024; }
const char *kernel_idf_version(void){ return "v5.4.1"; }

/* ---- display -------------------------------------------------------------- */
static uint16_t s_fb[K_DISPLAY_W * K_DISPLAY_H];
uint16_t *k_display_backbuffer(void) { return s_fb; }
void k_display_present(void)         { g_sim.presented++; }
void k_display_backlight(bool on)    { (void)on; }

/* ---- events / input / power ------------------------------------------------ */
uint32_t k_events_wait(uint32_t m, uint32_t t) { (void)m; (void)t; return 0; }
void     k_events_post(uint32_t b)             { (void)b; }
bool     k_input_poll(k_touch_evt_t *e)        { (void)e; return false; }
bool     k_power_asleep(void)                  { return false; }

/* ---- settings ------------------------------------------------------------- */
k_settings_t *k_settings(void)        { return &s_cfg; }
void k_settings_commit(void)          { }
void k_settings_factory_reset(void)   { s_cfg.units = K_UNITS_KM; s_cfg.default_zoom = 12; }
void k_ble_set_enabled(bool on)       { s_cfg.ble_on = on; }

/* ---- nav ------------------------------------------------------------------ */
void k_nav_snapshot(k_nav_snapshot_t *out) { *out = g_sim.nav; }
bool k_nav_pos_valid(const k_nav_snapshot_t *s, uint32_t now)
{
    return s->has_fix && (uint32_t)(now - s->pos_ms) < K_POS_STALE_MS;
}
void k_nav_set_route(bool a, uint32_t d, uint32_t t) { (void)a; (void)d; (void)t; }
void k_nav_clear_turn(void)                          { g_sim.nav.has_nav = false; }

/* ---- route ---------------------------------------------------------------- */
const k_route_pt_t *k_route_lock(uint32_t *count) { *count = g_sim.route_n; return g_sim.route; }
void k_route_unlock(void)                         { }
void k_route_clear(void)                          { g_sim.route_n = 0; }

/* ---- storage -------------------------------------------------------------- */
k_sd_state_t k_storage_state(void) { return g_sim.sd; }
const char *k_storage_state_text(void)
{
    switch (g_sim.sd) {
    case K_SD_NODATA:  return "No map data: copy zNN.tpk files to /maps on the SD card";
    case K_SD_MISSING: return "SD card missing: insert a card with map packs";
    default:           return "";
    }
}
void k_storage_rescan(void) { }
bool k_storage_space(uint64_t *t, uint64_t *f)
{
    if (g_sim.sd == K_SD_MISSING) return false;
    *t = 29ull << 30; *f = (uint64_t)(28.4 * (1 << 30));
    return true;
}
int k_packs_zoom_min(void) { return g_sim.sd == K_SD_READY ? 10 : -1; }
int k_packs_zoom_max(void) { return g_sim.sd == K_SD_READY ? 17 : -1; }
uint32_t k_packs_count(void) { return g_sim.sd == K_SD_READY ? 8 : 0; }
const k_pack_info_t *k_packs_info(uint8_t z)
{
    static k_pack_info_t p;
    if (g_sim.sd != K_SD_READY || z < 10 || z > 17) return NULL;
    p.present = true; p.zoom = z; p.file_bytes = 8u << 20;
    return &p;
}
const char *k_packs_attribution(void) { return g_sim.sd == K_SD_READY ? "(c) Example tile provider" : ""; }

/* ---- tiles: procedural "street map" ----------------------------------------- */
#define POOL 64
static uint16_t s_pool[POOL][K_TILE_PIXELS];
static int      s_next;

void k_tiles_begin_frame(uint8_t z, int32_t a, int32_t b, int32_t c, int32_t d)
{
    (void)z; (void)a; (void)b; (void)c; (void)d; s_next = 0;
}
void k_tiles_end_frame(void) { }

static uint16_t rgb(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }

const uint16_t *k_tiles_get(uint8_t z, int32_t tx, int32_t ty)
{
    (void)z;
    if (g_sim.tiles_missing) return NULL;
    uint16_t *t = s_pool[s_next++ % POOL];
    uint32_t seed = ((uint32_t)tx * 73856093u) ^ ((uint32_t)ty * 19349663u);
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            uint16_t c = rgb(240, 236, 228);
            bool road_h = (y >= 120 && y < 136), road_v = (x >= 120 && x < 136);
            bool minor_h = (y % 64 >= 30 && y % 64 < 34), minor_v = (x % 64 >= 30 && x % 64 < 34);
            if (minor_h || minor_v) c = rgb(255, 255, 255);
            if (road_h || road_v) c = rgb(252, 214, 164);
            else if (!(minor_h || minor_v)) {
                int bx = (x / 16), by = (y / 16);
                if (((bx * 7 + by * 13 + (int)(seed % 5)) % 5) == 0 && (x % 16) > 3 && (y % 16) > 3) c = rgb(216, 208, 200);
            }
            if (seed % 7 == 0 && x > 150 && y > 150 && x < 240 && y < 240) c = rgb(200, 232, 190);
            if (seed % 11 == 0 && y > 190) c = rgb(170, 211, 223);
            t[y * 256 + x] = c;
        }
    }
    return t;
}

/* ---- frame dump -------------------------------------------------------------- */
void sim_dump_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", K_DISPLAY_W, K_DISPLAY_H);
    for (int i = 0; i < K_DISPLAY_W * K_DISPLAY_H; i++) {
        uint16_t p = s_fb[i];
        unsigned char rgb3[3] = {
            (unsigned char)(((p >> 11) & 0x1F) * 255 / 31),
            (unsigned char)(((p >> 5) & 0x3F) * 255 / 63),
            (unsigned char)((p & 0x1F) * 255 / 31),
        };
        fwrite(rgb3, 1, 3, f);
    }
    fclose(f);
}
