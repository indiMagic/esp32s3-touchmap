/**
 * @file k_tpk.h
 * @brief .tpk tile pack file format (PORTABLE, host-testable).
 *
 * One file per zoom level (zNN.tpk):
 *
 *   offset 0     header, 96 bytes
 *   index_off    w*h index entries of 8 bytes: { u32 offset, u32 length }
 *   data_off     baseline 256x256 JPEG tiles, back to back
 *
 * A lookup is one 8-byte read at  index_off + 8 * cell  followed by one tile
 * read. A length of 0 means "no tile here".
 *
 * Header layout (little-endian):
 *    0  char[4] magic "TPK1"
 *    4  u16     version (1)
 *    6  u8      zoom
 *    7  u8      flags (0)
 *    8  u32     x0   first tile column in the index
 *   12  u32     y0   first tile row
 *   16  u32     w    columns
 *   20  u32     h    rows
 *   24  u16     tile_size (256)
 *   26  u16     reserved
 *   28  u32     index_off (96)
 *   32  u32     data_off
 *   36  char[60] attribution, NUL padded
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KT_HEADER_BYTES   96
#define KT_ATTR_BYTES     60
#define KT_INDEX_ENTRY    8
#define KT_MAX_TILE_BYTES (192 * 1024)

typedef struct {
    uint16_t version;
    uint8_t  zoom;
    uint32_t x0, y0, w, h;
    uint16_t tile_size;
    uint32_t index_off;
    uint32_t data_off;
    char     attribution[KT_ATTR_BYTES + 1];
} kt_header_t;

typedef enum {
    KT_OK = 0,
    KT_ERR_MAGIC,
    KT_ERR_VERSION,
    KT_ERR_LAYOUT,      /* sizes/offsets inconsistent */
} kt_err_t;

/** Parse and validate a 96-byte header (SD-04: wrong magic is rejected). */
kt_err_t kt_header_parse(const uint8_t raw[KT_HEADER_BYTES], uint32_t file_size, kt_header_t *out);

/**
 * Index cell for tile (x, y).
 * @return false if the tile lies outside the pack's rectangle (SD-04).
 */
bool kt_cell(const kt_header_t *h, int32_t x, int32_t y, uint32_t *cell);

/** Byte position of an index entry in the file. */
static inline uint32_t kt_index_pos(const kt_header_t *h, uint32_t cell)
{
    return h->index_off + KT_INDEX_ENTRY * cell;
}

/**
 * Decode an 8-byte index entry and check it against the file.
 * @return false if offset/length point outside the file or exceed the limit.
 */
bool kt_entry_decode(const uint8_t raw[KT_INDEX_ENTRY], uint32_t file_size,
                     uint32_t *offset, uint32_t *length);

#ifdef __cplusplus
}
#endif
