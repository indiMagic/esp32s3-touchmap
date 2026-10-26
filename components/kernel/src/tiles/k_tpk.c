#include "k_tpk.h"

#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

kt_err_t kt_header_parse(const uint8_t raw[KT_HEADER_BYTES], uint32_t file_size, kt_header_t *out)
{
    if (memcmp(raw, "TPK1", 4) != 0) {
        return KT_ERR_MAGIC;
    }
    out->version = rd16(raw + 4);
    if (out->version != 1) {
        return KT_ERR_VERSION;
    }
    out->zoom      = raw[6];
    out->x0        = rd32(raw + 8);
    out->y0        = rd32(raw + 12);
    out->w         = rd32(raw + 16);
    out->h         = rd32(raw + 20);
    out->tile_size = rd16(raw + 24);
    out->index_off = rd32(raw + 28);
    out->data_off  = rd32(raw + 32);
    memcpy(out->attribution, raw + 36, KT_ATTR_BYTES);
    out->attribution[KT_ATTR_BYTES] = '\0';

    if (out->tile_size != 256 || out->w == 0 || out->h == 0 || out->zoom > 20) {
        return KT_ERR_LAYOUT;
    }
    /* Widen before multiplying: w*h*8 can exceed 32 bits for corrupt files. */
    uint64_t index_bytes = (uint64_t)out->w * out->h * KT_INDEX_ENTRY;
    if (out->index_off < KT_HEADER_BYTES ||
        (uint64_t)out->index_off + index_bytes > out->data_off ||
        out->data_off > file_size) {
        return KT_ERR_LAYOUT;
    }
    return KT_OK;
}

bool kt_cell(const kt_header_t *h, int32_t x, int32_t y, uint32_t *cell)
{
    if (x < 0 || y < 0) {
        return false;
    }
    uint32_t ux = (uint32_t)x, uy = (uint32_t)y;
    if (ux < h->x0 || uy < h->y0 || ux - h->x0 >= h->w || uy - h->y0 >= h->h) {
        return false;
    }
    *cell = (uy - h->y0) * h->w + (ux - h->x0);
    return true;
}

bool kt_entry_decode(const uint8_t raw[KT_INDEX_ENTRY], uint32_t file_size,
                     uint32_t *offset, uint32_t *length)
{
    uint32_t off = rd32(raw);
    uint32_t len = rd32(raw + 4);
    if (len == 0) {
        *offset = 0;
        *length = 0;
        return true;                       /* "no tile" is valid */
    }
    if (len > KT_MAX_TILE_BYTES || off >= file_size || len > file_size - off) {
        return false;
    }
    *offset = off;
    *length = len;
    return true;
}
