#include "k_proto.h"

#include <string.h>

/* ---- Little-endian helpers --------------------------------------------- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* ---- POS (14 bytes) ---------------------------------------------------- */
kp_err_t kp_decode_pos(const uint8_t *b, size_t len, kp_pos_t *out)
{
    if (len < 14) {
        return KP_ERR_SHORT;
    }
    out->lat_e6      = (int32_t)rd32(b + 0);
    out->lon_e6      = (int32_t)rd32(b + 4);
    out->heading_d10 = rd16(b + 8);
    out->speed_d10   = rd16(b + 10);
    out->accuracy_m  = rd16(b + 12);
    if (out->lat_e6 < -90000000 || out->lat_e6 > 90000000 ||
        out->lon_e6 < -180000000 || out->lon_e6 > 180000000) {
        return KP_ERR_RANGE;
    }
    return KP_OK;
}

size_t kp_encode_pos(uint8_t out[14], const kp_pos_t *p)
{
    wr32(out + 0, (uint32_t)p->lat_e6);
    wr32(out + 4, (uint32_t)p->lon_e6);
    wr16(out + 8, p->heading_d10);
    wr16(out + 10, p->speed_d10);
    wr16(out + 12, p->accuracy_m);
    return 14;
}

/* ---- NAV (14 bytes + text) --------------------------------------------- */
kp_err_t kp_decode_nav(const uint8_t *b, size_t len, kp_nav_t *out)
{
    if (len < 14) {
        return KP_ERR_SHORT;
    }
    if (b[0] >= KP_MAN_COUNT) {
        return KP_ERR_RANGE;
    }
    out->maneuver  = b[0];
    out->flags     = b[1];
    out->to_turn_m = rd32(b + 2);
    out->remain_m  = rd32(b + 6);
    out->remain_s  = rd32(b + 10);

    size_t tlen = len - 14;
    if (tlen > KP_NAV_TEXT_MAX) {
        tlen = KP_NAV_TEXT_MAX;
    }
    memcpy(out->text, b + 14, tlen);
    out->text[tlen] = '\0';
    return KP_OK;
}

size_t kp_encode_nav(uint8_t *out, size_t cap, const kp_nav_t *nav)
{
    size_t tlen = strlen(nav->text);
    if (tlen > KP_NAV_TEXT_MAX) {
        tlen = KP_NAV_TEXT_MAX;
    }
    if (cap < 14 + tlen) {
        return 0;
    }
    out[0] = nav->maneuver;
    out[1] = nav->flags;
    wr32(out + 2, nav->to_turn_m);
    wr32(out + 6, nav->remain_m);
    wr32(out + 10, nav->remain_s);
    memcpy(out + 14, nav->text, tlen);
    return 14 + tlen;
}

/* ---- ROUTE ------------------------------------------------------------- */
kp_err_t kp_route_opcode(const uint8_t *b, size_t len, uint8_t *op)
{
    if (len < 1) {
        return KP_ERR_SHORT;
    }
    if (b[0] < KP_ROUTE_BEGIN || b[0] > KP_ROUTE_CLEAR) {
        return KP_ERR_OPCODE;
    }
    *op = b[0];
    return KP_OK;
}

kp_err_t kp_decode_route_begin(const uint8_t *b, size_t len, kp_route_begin_t *out)
{
    if (len < 11) {
        return KP_ERR_SHORT;
    }
    if (b[0] != KP_ROUTE_BEGIN) {
        return KP_ERR_OPCODE;
    }
    out->points     = rd16(b + 1);
    out->distance_m = rd32(b + 3);
    out->duration_s = rd32(b + 7);
    if (out->points > KP_MAX_ROUTE_POINTS) {
        return KP_ERR_RANGE;
    }
    return KP_OK;
}

size_t kp_encode_route_begin(uint8_t out[11], const kp_route_begin_t *b)
{
    out[0] = KP_ROUTE_BEGIN;
    wr16(out + 1, b->points);
    wr32(out + 3, b->distance_m);
    wr32(out + 7, b->duration_s);
    return 11;
}

kp_err_t kp_decode_route_points(const uint8_t *b, size_t len, uint16_t *start,
                                kp_pt_t *pts, size_t cap, size_t *n)
{
    if (len < 3) {
        return KP_ERR_SHORT;
    }
    if (b[0] != KP_ROUTE_POINTS) {
        return KP_ERR_OPCODE;
    }
    size_t count = (len - 3) / 8;
    if (count > cap) {
        return KP_ERR_RANGE;
    }
    *start = rd16(b + 1);
    for (size_t i = 0; i < count; i++) {
        pts[i].lat_e6 = (int32_t)rd32(b + 3 + 8 * i);
        pts[i].lon_e6 = (int32_t)rd32(b + 7 + 8 * i);
    }
    *n = count;
    return KP_OK;
}

size_t kp_encode_route_points(uint8_t *out, size_t cap, uint16_t start,
                              const kp_pt_t *pts, size_t n)
{
    if (cap < 3 + 8 * n) {
        return 0;
    }
    out[0] = KP_ROUTE_POINTS;
    wr16(out + 1, start);
    for (size_t i = 0; i < n; i++) {
        wr32(out + 3 + 8 * i, (uint32_t)pts[i].lat_e6);
        wr32(out + 7 + 8 * i, (uint32_t)pts[i].lon_e6);
    }
    return 3 + 8 * n;
}

/* ---- STATUS ------------------------------------------------------------ */
size_t kp_encode_hello(uint8_t out[3], uint8_t min_zoom, uint8_t max_zoom)
{
    out[0] = KP_STATUS_HELLO;
    out[1] = min_zoom;
    out[2] = max_zoom;
    return 3;
}

size_t kp_encode_route_ack(uint8_t out[4], uint16_t points_received, bool ok)
{
    out[0] = KP_STATUS_ROUTE_ACK;
    wr16(out + 1, points_received);
    out[3] = ok ? 1 : 0;
    return 4;
}
