#include "gfx.h"
#include "font_ui.h"
#include "ui_theme.h"
#include "kernel.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uint16_t *s_fb;

void gfx_init(void)
{
    s_fb = k_display_backbuffer();
}

void gfx_begin_frame(void)
{
    s_fb = k_display_backbuffer();
}

uint16_t *gfx_fb(void)
{
    return s_fb;
}

/* ---- rectangles -------------------------------------------------------- */
void gfx_clear(uint16_t c)
{
    gfx_fill_rect(0, 0, UI_W, UI_H, c);
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t c)
{
    if (!s_fb) {
        return;
    }
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > UI_W) x1 = UI_W;
    if (y1 > UI_H) y1 = UI_H;
    for (int yy = y; yy < y1; yy++) {
        uint16_t *row = s_fb + yy * UI_W;
        for (int xx = x; xx < x1; xx++) {
            row[xx] = c;
        }
    }
}

void gfx_rect(int x, int y, int w, int h, uint16_t c)
{
    gfx_fill_rect(x, y, w, 1, c);
    gfx_fill_rect(x, y + h - 1, w, 1, c);
    gfx_fill_rect(x, y, 1, h, c);
    gfx_fill_rect(x + w - 1, y, 1, h, c);
}

void gfx_blend_rect(int x, int y, int w, int h, uint16_t c, uint8_t a)
{
    if (!s_fb) {
        return;
    }
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > UI_W) x1 = UI_W;
    if (y1 > UI_H) y1 = UI_H;

    uint32_t sr = (c >> 11) & 0x1F, sg = (c >> 5) & 0x3F, sb = c & 0x1F;
    uint32_t ia = 255 - a;
    for (int yy = y; yy < y1; yy++) {
        uint16_t *row = s_fb + yy * UI_W;
        for (int xx = x; xx < x1; xx++) {
            uint16_t d = row[xx];
            uint32_t r = (sr * a + ((d >> 11) & 0x1F) * ia) / 255;
            uint32_t g = (sg * a + ((d >> 5) & 0x3F) * ia) / 255;
            uint32_t b = (sb * a + (d & 0x1F) * ia) / 255;
            row[xx] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
}

void gfx_blit(int x, int y, int w, int h, const uint16_t *src, int stride)
{
    if (!s_fb || !src) {
        return;
    }
    int sx = 0, sy = 0;
    if (x < 0) { sx = -x; w += x; x = 0; }
    if (y < 0) { sy = -y; h += y; y = 0; }
    if (x + w > UI_W) w = UI_W - x;
    if (y + h > UI_H) h = UI_H - y;
    if (w <= 0 || h <= 0) {
        return;
    }
    for (int r = 0; r < h; r++) {
        memcpy(s_fb + (y + r) * UI_W + x, src + (sy + r) * stride + sx, (size_t)w * 2);
    }
}

/* ---- lines ------------------------------------------------------------- */
static inline void put(int x, int y, uint16_t c)
{
    if ((unsigned)x < UI_W && (unsigned)y < UI_H) {
        s_fb[y * UI_W + x] = c;
    }
}

void gfx_fill_circle(int cx, int cy, int r, uint16_t c)
{
    if (!s_fb) {
        return;
    }
    if (r <= 0) {
        put(cx, cy, c);
        return;
    }
    for (int dy = -r; dy <= r; dy++) {
        int yy = cy + dy;
        if ((unsigned)yy >= UI_H) {
            continue;
        }
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r + r) {
            dx++;
        }
        int x0 = cx - dx, x1 = cx + dx;
        if (x0 < 0) x0 = 0;
        if (x1 > UI_W - 1) x1 = UI_W - 1;
        for (int xx = x0; xx <= x1; xx++) {
            s_fb[yy * UI_W + xx] = c;
        }
    }
}

void gfx_circle(int cx, int cy, int r, uint16_t c)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        put(cx + x, cy + y, c); put(cx - x, cy + y, c);
        put(cx + x, cy - y, c); put(cx - x, cy - y, c);
        put(cx + y, cy + x, c); put(cx - y, cy + x, c);
        put(cx + y, cy - x, c); put(cx - y, cy - x, c);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* Cohen-Sutherland against the screen grown by a margin, in 64-bit so route
 * points far outside the view cannot overflow. */
#define OUT_L 1
#define OUT_R 2
#define OUT_T 4
#define OUT_B 8

static int outcode(int64_t x, int64_t y, int64_t xmin, int64_t ymin, int64_t xmax, int64_t ymax)
{
    int c = 0;
    if (x < xmin) c |= OUT_L; else if (x > xmax) c |= OUT_R;
    if (y < ymin) c |= OUT_T; else if (y > ymax) c |= OUT_B;
    return c;
}

static bool clip_line(int64_t *x0, int64_t *y0, int64_t *x1, int64_t *y1, int margin)
{
    const int64_t xmin = -margin, ymin = -margin;
    const int64_t xmax = UI_W - 1 + margin, ymax = UI_H - 1 + margin;
    int c0 = outcode(*x0, *y0, xmin, ymin, xmax, ymax);
    int c1 = outcode(*x1, *y1, xmin, ymin, xmax, ymax);
    for (int guard = 0; guard < 8; guard++) {
        if (!(c0 | c1)) {
            return true;
        }
        if (c0 & c1) {
            return false;
        }
        int co = c0 ? c0 : c1;
        int64_t x, y;
        int64_t dx = *x1 - *x0, dy = *y1 - *y0;
        if (co & OUT_B)      { y = ymax; x = *x0 + dx * (ymax - *y0) / dy; }
        else if (co & OUT_T) { y = ymin; x = *x0 + dx * (ymin - *y0) / dy; }
        else if (co & OUT_R) { x = xmax; y = *y0 + dy * (xmax - *x0) / dx; }
        else                 { x = xmin; y = *y0 + dy * (xmin - *x0) / dx; }
        if (co == c0) {
            *x0 = x; *y0 = y; c0 = outcode(x, y, xmin, ymin, xmax, ymax);
        } else {
            *x1 = x; *y1 = y; c1 = outcode(x, y, xmin, ymin, xmax, ymax);
        }
    }
    return false;
}

void gfx_line(int ix0, int iy0, int ix1, int iy1, int thick, uint16_t c)
{
    if (!s_fb) {
        return;
    }
    int64_t x0 = ix0, y0 = iy0, x1 = ix1, y1 = iy1;
    int r = thick / 2;
    if (!clip_line(&x0, &y0, &x1, &y1, r + 1)) {
        return;
    }
    int dx = (int)llabs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -(int)llabs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int x = (int)x0, y = (int)y0;
    for (;;) {
        if (r <= 0) put(x, y, c); else gfx_fill_circle(x, y, r, c);
        if (x == (int)x1 && y == (int)y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

/* ---- polygon (even-odd scanline) --------------------------------------- */
void gfx_fill_poly(const gfx_pt_t *p, int n, uint16_t c)
{
    if (!s_fb || n < 3 || n > 32) {
        return;
    }
    int ymin = p[0].y, ymax = p[0].y;
    for (int i = 1; i < n; i++) {
        if (p[i].y < ymin) ymin = p[i].y;
        if (p[i].y > ymax) ymax = p[i].y;
    }
    if (ymin < 0) ymin = 0;
    if (ymax > UI_H - 1) ymax = UI_H - 1;

    for (int y = ymin; y <= ymax; y++) {
        int xs[32], k = 0;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            int yi = p[i].y, yj = p[j].y;
            if ((yi <= y && yj > y) || (yj <= y && yi > y)) {
                xs[k++] = p[i].x + (int)((int64_t)(y - yi) * (p[j].x - p[i].x) / (yj - yi));
            }
        }
        for (int a = 1; a < k; a++) {                 /* tiny insertion sort */
            int v = xs[a], b = a - 1;
            while (b >= 0 && xs[b] > v) { xs[b + 1] = xs[b]; b--; }
            xs[b + 1] = v;
        }
        for (int a = 0; a + 1 < k; a += 2) {
            gfx_fill_rect(xs[a], y, xs[a + 1] - xs[a] + 1, 1, c);
        }
    }
}

/* ---- text -------------------------------------------------------------- */
/* Anti-aliased proportional fonts. scale 1 = body (16 px), scale >= 2 = title (30 px). */
static const font_t *face_for(int scale)
{
    return scale >= 2 ? &font_title : &font_body;
}

static inline const font_glyph_t *glyph_of(const font_t *f, unsigned char ch)
{
    if (ch < FONT_FIRST || ch > FONT_LAST) {
        ch = '?';
    }
    return &f->glyph[ch - FONT_FIRST];
}

int gfx_text_w(const char *s, int scale)
{
    const font_t *f = face_for(scale);
    int w = 0;
    for (; *s; s++) {
        w += glyph_of(f, (unsigned char)*s)->adv;
    }
    return w;
}

static inline uint16_t mix565(uint16_t fg, uint16_t bg, unsigned a)
{
    unsigned ia = 255 - a;
    unsigned r = (((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * ia + 127) / 255;
    unsigned g = (((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * ia + 127) / 255;
    unsigned b = ((fg & 0x1F) * a + (bg & 0x1F) * ia + 127) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void gfx_text(int x, int y, const char *s, uint16_t c, int scale)
{
    if (!s_fb || !s) {
        return;
    }
    const font_t *f = face_for(scale);
    const int base = y + f->baseline;
    for (; *s; s++) {
        const font_glyph_t *g = glyph_of(f, (unsigned char)*s);
        const int gx = x + g->xo, gy = base + g->yo;
        if (g->w && g->h && gx < UI_W && gx + g->w > 0) {
            const uint8_t *src = f->bitmap + g->off;
            for (int row = 0; row < g->h; row++, src += g->w) {
                int py = gy + row;
                if ((unsigned)py >= UI_H) {
                    continue;
                }
                uint16_t *dst = s_fb + py * UI_W;
                for (int col = 0; col < g->w; col++) {
                    unsigned a = src[col];
                    int px = gx + col;
                    if (a && (unsigned)px < UI_W) {
                        dst[px] = a == 255 ? c : mix565(c, dst[px], a);
                    }
                }
            }
        }
        x += g->adv;
    }
}

void gfx_text_center(int cx, int y, const char *s, uint16_t c, int scale)
{
    gfx_text(cx - gfx_text_w(s, scale) / 2, y, s, c, scale);
}
