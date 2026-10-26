/**
 * @file gfx.h
 * @brief 2D drawing into the RGB565 back buffer. Every call clips to the screen.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int x, y; } gfx_pt_t;

void      gfx_init(void);
/** Call before drawing each frame: the draw target alternates between two buffers. */
void      gfx_begin_frame(void);
uint16_t *gfx_fb(void);

void gfx_clear(uint16_t c);
void gfx_fill_rect(int x, int y, int w, int h, uint16_t c);
void gfx_rect(int x, int y, int w, int h, uint16_t c);                  /* 1 px outline */
void gfx_blend_rect(int x, int y, int w, int h, uint16_t c, uint8_t a); /* a: 0..255 */

/** Copy w x h pixels (row stride in pixels) to (x, y). */
void gfx_blit(int x, int y, int w, int h, const uint16_t *src, int stride);

/** Thick line, clipped (safe with huge coordinates). */
void gfx_line(int x0, int y0, int x1, int y1, int thick, uint16_t c);

void gfx_fill_circle(int cx, int cy, int r, uint16_t c);
void gfx_circle(int cx, int cy, int r, uint16_t c);                     /* 1 px ring */
void gfx_fill_poly(const gfx_pt_t *p, int n, uint16_t c);               /* even-odd */

/* 8x16 font, ASCII 32..126, integer scale. */
void gfx_text(int x, int y, const char *s, uint16_t c, int scale);
int  gfx_text_w(const char *s, int scale);
void gfx_text_center(int cx, int y, const char *s, uint16_t c, int scale);

#ifdef __cplusplus
}
#endif
