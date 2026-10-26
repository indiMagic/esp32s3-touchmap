#include "map_view.h"
#include "gfx.h"
#include "ui_theme.h"
#include "kernel.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint8_t  s_zoom = 12;
static uint32_t s_cx = 0x8000000Fu, s_cy = 0x8000000Fu;   /* start view until a fix arrives (set in map_view_init) */

/* Start view before the phone sends a position: Mississauga City Centre, inside the GTA map packs. */
#define START_LAT_E6   43593400
#define START_LON_E6  (-79644100)
static bool     s_follow = true;
static bool     s_inited;

/* ---- zoom limits ------------------------------------------------------- */
static void zoom_limits(int *zmin, int *zmax)
{
    int lo = k_packs_zoom_min(), hi = k_packs_zoom_max();
    if (lo < 0) {                     /* no packs: allow the full range */
        lo = 0;
        hi = KG_MAX_ZOOM;
    }
    *zmin = lo;
    *zmax = hi;
}

static uint8_t clamp_zoom(int z)
{
    int lo, hi;
    zoom_limits(&lo, &hi);
    if (z < lo) z = lo;
    if (z > hi) z = hi;
    if (k_packs_zoom_min() >= 0 && !k_packs_has((uint8_t)z)) {
        /* A level is missing from the card (gaps are allowed): use the nearest one below, else above. */
        int d = z;
        while (d > lo && !k_packs_has((uint8_t)d)) d--;
        if (!k_packs_has((uint8_t)d)) {
            d = z;
            while (d < hi && !k_packs_has((uint8_t)d)) d++;
        }
        z = d;
    }
    return (uint8_t)z;
}

void map_view_init(void)
{
    s_zoom = clamp_zoom(k_settings()->default_zoom);
    s_follow = k_settings()->follow_at_start;
    s_cx = kg_merc_x(START_LON_E6);
    s_cy = kg_merc_y(START_LAT_E6);
    s_inited = true;
}

uint8_t map_view_zoom(void)              { return s_zoom; }
bool    map_view_following(void)         { return s_follow; }

void map_view_pan(int dx, int dy)
{
    s_follow = false;
    kg_pan(&s_cx, &s_cy, s_zoom, dx, dy);
}

void map_view_zoom_by(int delta)
{
    /* Step to the next zoom level that has a map file, skipping gaps. */
    int lo, hi;
    zoom_limits(&lo, &hi);
    int step = delta < 0 ? -1 : 1;
    int z = (int)s_zoom;
    for (int n = 0; n < (delta < 0 ? -delta : delta); n++) {
        int c = z + step;
        while (c >= lo && c <= hi && k_packs_zoom_min() >= 0 && !k_packs_has((uint8_t)c)) {
            c += step;
        }
        if (c < lo || c > hi) {
            break;
        }
        z = c;
    }
    s_zoom = clamp_zoom(z);
}

void map_view_recenter(void)
{
    s_follow = true;
}

void map_view_centre_latlon(int32_t *lat_e6, int32_t *lon_e6)
{
    kg_merc_to_latlon(s_cx, s_cy, lat_e6, lon_e6);
}

/* ---- drawing ----------------------------------------------------------- */
static void draw_tiles(kg_view_t v)
{
    const int z = s_zoom;
    const int32_t last = (int32_t)((1u << z) - 1u);
    int32_t tx0 = kg_tile_of(v.ox), tx1 = kg_tile_of(v.ox + UI_W - 1);
    int32_t ty0 = kg_tile_of(v.oy), ty1 = kg_tile_of(v.oy + UI_H - 1);

    gfx_clear(C_MAP_BG);
    k_tiles_begin_frame((uint8_t)z, tx0, ty0, tx1, ty1);
    for (int32_t ty = ty0; ty <= ty1; ty++) {
        for (int32_t tx = tx0; tx <= tx1; tx++) {
            if (tx < 0 || ty < 0 || tx > last || ty > last) {
                continue;                              /* off the edge of the world */
            }
            int sx = tx * KG_TILE_PX - v.ox;
            int sy = ty * KG_TILE_PX - v.oy;
            const uint16_t *px = k_tiles_get((uint8_t)z, tx, ty);
            if (px) {
                gfx_blit(sx, sy, KG_TILE_PX, KG_TILE_PX, px, KG_TILE_PX);
            } else {
                gfx_fill_rect(sx, sy, KG_TILE_PX, KG_TILE_PX, C_TILE_MISSING);   /* MAP-04 */
            }
        }
    }
    k_tiles_end_frame();
}

static inline int to_sx(uint32_t m, kg_view_t v) { return (int)((int64_t)kg_to_px(m, s_zoom) - v.ox); }
static inline int to_sy(uint32_t m, kg_view_t v) { return (int)((int64_t)kg_to_px(m, s_zoom) - v.oy); }

static void draw_route(kg_view_t v)
{
    uint32_t n = 0;
    const k_route_pt_t *pts = k_route_lock(&n);
    if (n >= 2) {
        /* Two passes so the white casing never paints over the blue line. */
        for (int pass = 0; pass < 2; pass++) {
            int thick = pass == 0 ? 9 : 5;
            uint16_t col = pass == 0 ? C_ROUTE_CASING : C_ROUTE;
            int px = to_sx(pts[0].mx, v), py = to_sy(pts[0].my, v);
            for (uint32_t i = 1; i < n; i++) {
                int x = to_sx(pts[i].mx, v), y = to_sy(pts[i].my, v);
                if (abs(x - px) + abs(y - py) < 2 && i != n - 1) {
                    continue;                          /* thin dense points */
                }
                gfx_line(px, py, x, y, thick, col);    /* MAP-05, clipped to the view */
                px = x; py = y;
            }
        }
        /* MAP-06: destination marker on the last route point */
        int dx = to_sx(pts[n - 1].mx, v), dy = to_sy(pts[n - 1].my, v);
        gfx_fill_circle(dx, dy, 10, C_ROUTE_CASING);
        gfx_fill_circle(dx, dy, 7, C_DEST);
    }
    k_route_unlock();
}

static void draw_position(int sx, int sy, uint16_t heading_d10, bool has_heading)
{
    gfx_fill_circle(sx, sy, 14, C_ROUTE_CASING);
    if (!has_heading) {
        gfx_fill_circle(sx, sy, 10, C_POS);
        return;
    }
    double a = heading_d10 / 10.0 * M_PI / 180.0;       /* 0 = north, clockwise */
    double s = sin(a), c = cos(a);
    /* Arrow pointing "up", rotated by the heading. */
    static const int8_t shape[4][2] = { { 0, -16 }, { 11, 12 }, { 0, 6 }, { -11, 12 } };
    gfx_pt_t p[4];
    for (int i = 0; i < 4; i++) {
        p[i].x = sx + (int)lround(shape[i][0] * c - shape[i][1] * s);
        p[i].y = sy + (int)lround(shape[i][0] * s + shape[i][1] * c);
    }
    gfx_fill_poly(p, 4, C_POS);
}

void map_view_draw(const k_nav_snapshot_t *nav)
{
    if (!s_inited) {
        map_view_init();
    }
    s_zoom = clamp_zoom(s_zoom);                       /* packs may have changed */

    bool pos_ok = k_nav_pos_valid(nav, kernel_millis());     /* NAV-04 */
    if (s_follow && pos_ok) {                                /* MAP-07 */
        s_cx = kg_merc_x(nav->lon_e6);
        s_cy = kg_merc_y(nav->lat_e6);
    }

    kg_view_t v = kg_view_origin(s_cx, s_cy, s_zoom, UI_W, UI_H);
    draw_tiles(v);
    draw_route(v);

    if (pos_ok) {
        int sx = to_sx(kg_merc_x(nav->lon_e6), v);
        int sy = to_sy(kg_merc_y(nav->lat_e6), v);
        draw_position(sx, sy, nav->heading_d10, nav->speed_d10 > 5);   /* > 0.5 m/s */
    }
}
