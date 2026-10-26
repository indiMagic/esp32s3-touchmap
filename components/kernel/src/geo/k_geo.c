#include "k_geo.h"

#include <math.h>

#define TWO32      4294967296.0
#define MAX_LAT    85.05112878
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t clamp_u32(double v)
{
    if (v <= 0.0) {
        return 0;
    }
    if (v >= TWO32 - 1.0) {
        return 0xFFFFFFFFu;
    }
    return (uint32_t)v;
}

uint32_t kg_merc_x(int32_t lon_e6)
{
    double lon = lon_e6 / 1e6;
    return clamp_u32((lon + 180.0) / 360.0 * TWO32);
}

uint32_t kg_merc_y(int32_t lat_e6)
{
    double lat = lat_e6 / 1e6;
    if (lat > MAX_LAT) {
        lat = MAX_LAT;
    }
    if (lat < -MAX_LAT) {
        lat = -MAX_LAT;
    }
    double r = lat * M_PI / 180.0;
    double y = (1.0 - log(tan(r) + 1.0 / cos(r)) / M_PI) / 2.0;
    return clamp_u32(y * TWO32);
}

void kg_merc_to_latlon(uint32_t mx, uint32_t my, int32_t *lat_e6, int32_t *lon_e6)
{
    double lon = mx / TWO32 * 360.0 - 180.0;
    double n = M_PI * (1.0 - 2.0 * (my / TWO32));
    double lat = atan(sinh(n)) * 180.0 / M_PI;
    *lat_e6 = (int32_t)llround(lat * 1e6);
    *lon_e6 = (int32_t)llround(lon * 1e6);
}

uint32_t kg_from_px(int64_t px, uint8_t z)
{
    if (px < 0) {
        return 0;
    }
    uint64_t m = (uint64_t)px << (24 - z);
    return m > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)m;
}

kg_view_t kg_view_origin(uint32_t cx_m, uint32_t cy_m, uint8_t z, int w, int h)
{
    kg_view_t v;
    v.ox = kg_to_px(cx_m, z) - w / 2;
    v.oy = kg_to_px(cy_m, z) - h / 2;
    return v;
}

void kg_pan(uint32_t *cx_m, uint32_t *cy_m, uint8_t z, int dx_px, int dy_px)
{
    int64_t cx = (int64_t)kg_to_px(*cx_m, z) - dx_px;   /* dragging right moves the map right */
    int64_t cy = (int64_t)kg_to_px(*cy_m, z) - dy_px;
    *cx_m = kg_from_px(cx, z);
    *cy_m = kg_from_px(cy, z);
}
