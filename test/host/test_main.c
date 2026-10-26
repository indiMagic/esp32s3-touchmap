/* Host unit tests for the portable kernel code: k_proto, k_geo, k_tpk.
 * Build and run:  make -C test/host run
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "k_geo.h"
#include "k_proto.h"
#include "k_tpk.h"

static int g_checks, g_fails;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fails++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- k_proto ------------------------------------------------------------- */
static void test_pos(void)
{
    kp_pos_t in = { 43650000, -79380000, 1234, 135, 5 }, out;
    uint8_t b[14];
    CHECK(kp_encode_pos(b, &in) == 14);
    CHECK(kp_decode_pos(b, 14, &out) == KP_OK);
    CHECK(out.lat_e6 == in.lat_e6 && out.lon_e6 == in.lon_e6);
    CHECK(out.heading_d10 == 1234 && out.speed_d10 == 135 && out.accuracy_m == 5);
    CHECK(kp_decode_pos(b, 13, &out) == KP_ERR_SHORT);
    /* little-endian check: lat 43650000 = 0x029A_0F50?  verify first byte */
    CHECK(b[0] == (uint8_t)(43650000 & 0xFF));
    /* out-of-range latitude is rejected */
    in.lat_e6 = 91000000;
    kp_encode_pos(b, &in);
    CHECK(kp_decode_pos(b, 14, &out) == KP_ERR_RANGE);
}

static void test_route(void)
{
    uint8_t b[11];
    kp_route_begin_t bg = { 3000, 12400, 1080 }, bo;
    CHECK(kp_encode_route_begin(b, &bg) == 11);
    CHECK(kp_decode_route_begin(b, 11, &bo) == KP_OK);
    CHECK(bo.points == 3000 && bo.distance_m == 12400 && bo.duration_s == 1080);
    bg.points = KP_MAX_ROUTE_POINTS + 1;
    kp_encode_route_begin(b, &bg);
    CHECK(kp_decode_route_begin(b, 11, &bo) == KP_ERR_RANGE);

    kp_pt_t pts[KP_POINTS_PER_WRITE], back[KP_POINTS_PER_WRITE];
    for (int i = 0; i < KP_POINTS_PER_WRITE; i++) {
        pts[i].lat_e6 = 43000000 + i * 1000;
        pts[i].lon_e6 = -79000000 - i * 1000;
    }
    uint8_t chunk[200];
    size_t n = kp_encode_route_points(chunk, sizeof(chunk), 36, pts, KP_POINTS_PER_WRITE);
    CHECK(n == 3 + 8 * KP_POINTS_PER_WRITE);
    CHECK(n <= 185 - 3);                       /* fits one write at the preferred MTU */
    uint16_t start = 0;
    size_t got = 0;
    CHECK(kp_decode_route_points(chunk, n, &start, back, KP_POINTS_PER_WRITE, &got) == KP_OK);
    CHECK(start == 36 && got == KP_POINTS_PER_WRITE);
    CHECK(memcmp(pts, back, sizeof(pts)) == 0);
    CHECK(kp_decode_route_points(chunk, n, &start, back, 4, &got) == KP_ERR_RANGE);

    uint8_t op = 0;
    uint8_t end = KP_ROUTE_END, clear = KP_ROUTE_CLEAR, bad = 0x7F;
    CHECK(kp_route_opcode(&end, 1, &op) == KP_OK && op == KP_ROUTE_END);
    CHECK(kp_route_opcode(&clear, 1, &op) == KP_OK && op == KP_ROUTE_CLEAR);
    CHECK(kp_route_opcode(&bad, 1, &op) == KP_ERR_OPCODE);
    CHECK(kp_route_opcode(&end, 0, &op) == KP_ERR_SHORT);
}

static void test_nav(void)
{
    kp_nav_t in = { KP_MAN_RIGHT, KP_NAVF_REROUTING, 300, 12400, 1080, "Turn right onto King St" }, out;
    uint8_t b[80];
    size_t n = kp_encode_nav(b, sizeof(b), &in);
    CHECK(n == 14 + strlen(in.text));
    CHECK(kp_decode_nav(b, n, &out) == KP_OK);
    CHECK(out.maneuver == KP_MAN_RIGHT && out.flags == KP_NAVF_REROUTING);
    CHECK(out.to_turn_m == 300 && out.remain_m == 12400 && out.remain_s == 1080);
    CHECK(strcmp(out.text, in.text) == 0);

    /* over-long text is truncated, never overflows */
    uint8_t big[14 + 120];
    memset(big, 'x', sizeof(big));
    big[0] = KP_MAN_STRAIGHT; big[1] = 0;
    CHECK(kp_decode_nav(big, sizeof(big), &out) == KP_OK);
    CHECK(strlen(out.text) == KP_NAV_TEXT_MAX);

    big[0] = 14;                                /* maneuver codes are 0..13 */
    CHECK(kp_decode_nav(big, sizeof(big), &out) == KP_ERR_RANGE);
}

static void test_status(void)
{
    uint8_t h[3], a[4];
    CHECK(kp_encode_hello(h, 10, 14) == 3 && h[0] == 1 && h[1] == 10 && h[2] == 14);
    CHECK(kp_encode_route_ack(a, 3000, true) == 4);
    CHECK(a[0] == 2 && a[1] == (3000 & 0xFF) && a[2] == (3000 >> 8) && a[3] == 1);
    kp_encode_route_ack(a, 5, false);
    CHECK(a[3] == 0);
}

/* ---- k_geo --------------------------------------------------------------- */
static void test_geo(void)
{
    /* The equator / prime meridian is the middle of the world. */
    CHECK(llabs((long long)kg_merc_x(0) - 0x80000000LL) < 4);
    CHECK(llabs((long long)kg_merc_y(0) - 0x80000000LL) < 4);

    /* Toronto (43.65, -79.38) at zoom 12 is tile (1144, 1494). Reference values come from the
     * standard slippy-map formula, evaluated independently in Python:
     *   x = (lon+180)/360 * 2^z ;  y = (1 - asinh(tan(lat))/pi)/2 * 2^z                      */
    uint32_t mx = kg_merc_x(-79380000), my = kg_merc_y(43650000);
    CHECK(kg_tile_of(kg_to_px(mx, 12)) == 1144);
    CHECK(kg_tile_of(kg_to_px(my, 12)) == 1494);

    /* Round trip lat/lon -> mercator -> lat/lon within about 1 m (1e-5 deg). */
    int32_t lat, lon;
    kg_merc_to_latlon(mx, my, &lat, &lon);
    CHECK(abs(lat - 43650000) < 20);
    CHECK(abs(lon - (-79380000)) < 20);

    /* Same point one zoom deeper has twice the world pixel. */
    CHECK(llabs((long long)kg_to_px(mx, 13) - 2LL * kg_to_px(mx, 12)) <= 1);

    /* Poles clamp instead of exploding. */
    CHECK(kg_merc_y(90000000) < 0x00100000u);
    CHECK(kg_merc_y(-90000000) > 0xFFF00000u);

    /* View origin: centre minus half the screen, and pan is exact. */
    kg_view_t v = kg_view_origin(mx, my, 15, 800, 480);
    CHECK(v.ox == kg_to_px(mx, 15) - 400 && v.oy == kg_to_px(my, 15) - 240);
    uint32_t cx = mx, cy = my;
    kg_pan(&cx, &cy, 15, -100, 50);            /* drag left 100, down 50 */
    CHECK(kg_to_px(cx, 15) == kg_to_px(mx, 15) + 100);
    CHECK(kg_to_px(cy, 15) == kg_to_px(my, 15) - 50);

    /* Panning off the west/north edge saturates at 0. */
    cx = 10; cy = 10;
    kg_pan(&cx, &cy, 5, 100000, 100000);
    CHECK(cx == 0 && cy == 0);

    /* Zoom 20 still fits in 32-bit pixel math. */
    CHECK(kg_to_px(0xFFFFFFFFu, 20) == (1 << 28) - 1);
}

/* ---- k_tpk --------------------------------------------------------------- */
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void make_header(uint8_t h[KT_HEADER_BYTES], uint8_t zoom, uint32_t x0, uint32_t y0,
                        uint32_t w, uint32_t hh, uint32_t data_off)
{
    memset(h, 0, KT_HEADER_BYTES);
    memcpy(h, "TPK1", 4);
    h[4] = 1;
    h[6] = zoom;
    put32(h + 8, x0); put32(h + 12, y0); put32(h + 16, w); put32(h + 20, hh);
    h[24] = 0; h[25] = 1;                       /* tile_size 256 = 0x0100 */
    put32(h + 28, KT_HEADER_BYTES);
    put32(h + 32, data_off);
    strcpy((char *)h + 36, "(c) Test tiles");
}

static void test_tpk(void)
{
    uint8_t h[KT_HEADER_BYTES];
    kt_header_t hd;
    uint32_t data_off = KT_HEADER_BYTES + 12 * 4 * 8;        /* the 12 x 4 example from the SRS */
    make_header(h, 12, 1100, 1480, 12, 4, data_off);

    CHECK(kt_header_parse(h, data_off + 1000, &hd) == KT_OK);
    CHECK(hd.zoom == 12 && hd.w == 12 && hd.h == 4 && hd.index_off == 96);
    CHECK(strcmp(hd.attribution, "(c) Test tiles") == 0);

    /* SD-04: wrong magic, bad version, inconsistent layout, file too short */
    uint8_t bad[KT_HEADER_BYTES];
    memcpy(bad, h, sizeof(bad)); bad[0] = 'X';
    CHECK(kt_header_parse(bad, 100000, &hd) == KT_ERR_MAGIC);
    memcpy(bad, h, sizeof(bad)); bad[4] = 9;
    CHECK(kt_header_parse(bad, 100000, &hd) == KT_ERR_VERSION);
    CHECK(kt_header_parse(h, data_off - 1, &hd) == KT_ERR_LAYOUT);          /* data_off past EOF */
    make_header(bad, 12, 0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, data_off);         /* w*h*8 overflows 32 bits */
    CHECK(kt_header_parse(bad, 0xFFFFFFFFu, &hd) == KT_ERR_LAYOUT);

    make_header(h, 12, 1100, 1480, 12, 4, data_off);
    kt_header_parse(h, data_off + 1000, &hd);

    /* cell = (y - y0) * w + (x - x0); outside the rectangle is rejected */
    uint32_t cell;
    CHECK(kt_cell(&hd, 1100, 1480, &cell) && cell == 0);
    CHECK(kt_cell(&hd, 1106, 1482, &cell) && cell == 2 * 12 + 6);   /* the highlighted "x,y" cell in Figure 5 */
    CHECK(kt_cell(&hd, 1111, 1483, &cell) && cell == 47);
    CHECK(!kt_cell(&hd, 1112, 1480, &cell));
    CHECK(!kt_cell(&hd, 1099, 1480, &cell));
    CHECK(!kt_cell(&hd, 1100, 1484, &cell));
    CHECK(!kt_cell(&hd, -1, 1480, &cell));
    CHECK(kt_index_pos(&hd, 0) == 96 && kt_index_pos(&hd, 14) == 96 + 8 * 14);

    /* index entries: valid, empty (length 0 = no tile), out-of-file, too large */
    uint8_t e[8];
    uint32_t off, len, fsize = data_off + 5000;
    put32(e, data_off + 100); put32(e + 4, 2000);
    CHECK(kt_entry_decode(e, fsize, &off, &len) && off == data_off + 100 && len == 2000);
    put32(e, 0); put32(e + 4, 0);
    CHECK(kt_entry_decode(e, fsize, &off, &len) && len == 0);
    put32(e, fsize - 10); put32(e + 4, 100);
    CHECK(!kt_entry_decode(e, fsize, &off, &len));
    put32(e, data_off); put32(e + 4, KT_MAX_TILE_BYTES + 1);
    CHECK(!kt_entry_decode(e, 0xFFFFFFFFu, &off, &len));
    put32(e, 0xFFFFFFF0u); put32(e + 4, 0x20);                              /* offset + length wraps */
    CHECK(!kt_entry_decode(e, 0xFFFFFFFFu, &off, &len));
}

/* ---- runner -------------------------------------------------------------- */
int main(void)
{
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "proto: POS",    test_pos },
        { "proto: ROUTE",  test_route },
        { "proto: NAV",    test_nav },
        { "proto: STATUS", test_status },
        { "geo",           test_geo },
        { "tpk",           test_tpk },
    };
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        int before = g_fails;
        tests[i].fn();
        printf("%-14s %s\n", tests[i].name, g_fails == before ? "ok" : "FAILED");
    }
    printf("\n%d checks, %d failed\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
