/* Cross-language check: reads a .tpk written by tools/tilepack.py with the
 * firmware's own parser (k_tpk.c) and verifies every index entry and JPEG.
 *   build/pack_check path/to/z12.tpk
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "k_tpk.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s file.tpk\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t raw[KT_HEADER_BYTES];
    if (fread(raw, 1, sizeof(raw), f) != sizeof(raw)) {
        fprintf(stderr, "short header\n");
        return 1;
    }
    kt_header_t h;
    kt_err_t e = kt_header_parse(raw, (uint32_t)fsz, &h);
    if (e != KT_OK) {
        fprintf(stderr, "header rejected: %d\n", (int)e);
        return 1;
    }

    uint32_t tiles = 0;
    uint8_t *buf = malloc(KT_MAX_TILE_BYTES);
    for (uint32_t cell = 0; cell < h.w * h.h; cell++) {
        uint8_t ent[KT_INDEX_ENTRY];
        fseek(f, (long)kt_index_pos(&h, cell), SEEK_SET);
        if (fread(ent, 1, sizeof(ent), f) != sizeof(ent)) {
            return 1;
        }
        uint32_t off, len;
        if (!kt_entry_decode(ent, (uint32_t)fsz, &off, &len)) {
            fprintf(stderr, "cell %u: bad index entry\n", (unsigned)cell);
            return 1;
        }
        if (len == 0) {
            continue;
        }
        fseek(f, (long)off, SEEK_SET);
        if (fread(buf, 1, len, f) != len || buf[0] != 0xFF || buf[1] != 0xD8 ||
            buf[len - 2] != 0xFF || buf[len - 1] != 0xD9) {
            fprintf(stderr, "cell %u: not a complete JPEG\n", (unsigned)cell);
            return 1;
        }
        /* The tile at (x, y) must be found through kt_cell() at the same cell. */
        uint32_t x = h.x0 + cell % h.w, y = h.y0 + cell / h.w, c2;
        if (!kt_cell(&h, (int32_t)x, (int32_t)y, &c2) || c2 != cell) {
            fprintf(stderr, "cell mapping mismatch at %u\n", (unsigned)cell);
            return 1;
        }
        tiles++;
    }
    printf("%s: zoom %u, %ux%u cells, %u tiles, attribution \"%s\" - OK\n",
           argv[1], h.zoom, (unsigned)h.w, (unsigned)h.h, (unsigned)tiles, h.attribution);
    free(buf);
    fclose(f);
    return 0;
}
