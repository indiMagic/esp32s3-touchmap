/**
 * @file ui_theme.h
 * @brief Colours (RGB565), sizes and spacing shared by every screen.
 *
 * Touch targets are at least 64 px and text at least 16 px high (SRS quality
 * table, "Usability").
 */
#pragma once

#include <stdint.h>

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/* map */
#define C_MAP_BG        RGB565(0xE6, 0xE4, 0xDC)
#define C_TILE_MISSING  RGB565(0xD0, 0xD0, 0xD4)
#define C_ROUTE_CASING  RGB565(0xFF, 0xFF, 0xFF)
#define C_ROUTE         RGB565(0x1F, 0x6F, 0xFF)
#define C_POS           RGB565(0x1F, 0x6F, 0xFF)
#define C_DEST          RGB565(0xE5, 0x20, 0x20)

/* chrome */
#define C_PANEL         RGB565(0x10, 0x18, 0x2C)
#define C_PANEL_ALT     RGB565(0x1E, 0x28, 0x44)
#define C_PANEL_HI      RGB565(0x2A, 0x38, 0x5C)
#define C_TEXT          RGB565(0xFF, 0xFF, 0xFF)
#define C_TEXT_DIM      RGB565(0x9A, 0xA6, 0xC0)
#define C_ACCENT        RGB565(0x1F, 0x6F, 0xFF)
#define C_OK            RGB565(0x22, 0xC5, 0x5E)
#define C_WARN          RGB565(0xF2, 0xA6, 0x1F)
#define C_DANGER        RGB565(0xD0, 0x30, 0x30)
#define C_BTN_FACE      RGB565(0xFA, 0xFA, 0xFA)
#define C_BTN_EDGE      RGB565(0x30, 0x30, 0x38)
#define C_BTN_ICON      RGB565(0x10, 0x18, 0x2C)

/* layout */
#define UI_W            800
#define UI_H            480
#define UI_HEADER_H     56
#define UI_ROW_H        64
#define UI_BTN          64
#define UI_BTN_GAP      8
#define UI_BANNER_H     64
#define UI_STATUS_H     34
#define UI_FONT_H       16      /* body text line box; title is 32 */
