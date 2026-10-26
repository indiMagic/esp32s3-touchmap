#include "hud.h"
#include "gfx.h"
#include "ui_theme.h"
#include "kernel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- button layout ----------------------------------------------------- */
#define BTN_X   (UI_W - UI_BTN - 8)
#define BTN_Y0  100

static void btn_rect(int i, int *x, int *y)
{
    *x = BTN_X;
    *y = BTN_Y0 + i * (UI_BTN + UI_BTN_GAP);
}

hud_button_t hud_hit_test(int px, int py)
{
    for (int i = 0; i < HUD_BTN_COUNT; i++) {
        int x, y;
        btn_rect(i, &x, &y);
        if (px >= x && px < x + UI_BTN && py >= y && py < y + UI_BTN) {
            return (hud_button_t)i;
        }
    }
    return HUD_BTN_NONE;
}

static void draw_gear(int cx, int cy)
{
    for (int i = 0; i < 8; i++) {                      /* eight teeth */
        static const int8_t d[8][2] = { {0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1} };
        int dx = d[i][0] * 16, dy = d[i][1] * 16;
        if (d[i][0] && d[i][1]) { dx = d[i][0] * 12; dy = d[i][1] * 12; }
        gfx_line(cx, cy, cx + dx, cy + dy, 6, C_BTN_ICON);
    }
    gfx_fill_circle(cx, cy, 12, C_BTN_ICON);
    gfx_fill_circle(cx, cy, 5, C_BTN_FACE);
}

static void draw_buttons(bool following)
{
    for (int i = 0; i < HUD_BTN_COUNT; i++) {
        int x, y;
        btn_rect(i, &x, &y);
        gfx_fill_rect(x, y, UI_BTN, UI_BTN, C_BTN_FACE);
        gfx_rect(x, y, UI_BTN, UI_BTN, C_BTN_EDGE);
        int cx = x + UI_BTN / 2, cy = y + UI_BTN / 2;
        switch (i) {
        case HUD_BTN_ZOOM_IN:
            gfx_line(cx - 14, cy, cx + 14, cy, 5, C_BTN_ICON);
            gfx_line(cx, cy - 14, cx, cy + 14, 5, C_BTN_ICON);
            break;
        case HUD_BTN_ZOOM_OUT:
            gfx_line(cx - 14, cy, cx + 14, cy, 5, C_BTN_ICON);
            break;
        case HUD_BTN_RECENTER: {
            uint16_t col = following ? C_ACCENT : C_BTN_ICON;
            gfx_circle(cx, cy, 14, col);
            gfx_circle(cx, cy, 13, col);
            gfx_fill_circle(cx, cy, 6, col);
            gfx_line(cx, cy - 24, cx, cy - 16, 3, col);
            gfx_line(cx, cy + 16, cx, cy + 24, 3, col);
            gfx_line(cx - 24, cy, cx - 16, cy, 3, col);
            gfx_line(cx + 16, cy, cx + 24, cy, 3, col);
            break;
        }
        case HUD_BTN_SETTINGS:
            draw_gear(cx, cy);
            break;
        }
    }
}

/* ---- maneuver arrows --------------------------------------------------- */
/* Drawn in a 48x48 box. Angles in degrees clockwise from "up". */
static void draw_arrow(int ox, int oy, kp_maneuver_t m, uint16_t col)
{
    const int cx = ox + 24, cy = oy + 24;

    /* Straight-ish family: shaft + head rotated by the turn angle. */
    int angle = 0;
    bool simple = true;
    switch (m) {
    case KP_MAN_STRAIGHT:       angle = 0;    break;
    case KP_MAN_SLIGHT_RIGHT:   angle = 45;   break;
    case KP_MAN_RIGHT:          angle = 90;   break;
    case KP_MAN_SHARP_RIGHT:    angle = 135;  break;
    case KP_MAN_SHARP_LEFT:     angle = -135; break;
    case KP_MAN_LEFT:           angle = -90;  break;
    case KP_MAN_SLIGHT_LEFT:    angle = -45;  break;
    case KP_MAN_MERGE:          angle = 15;   break;
    case KP_MAN_FORK:           angle = 0;    break;
    default:                    simple = false; break;
    }

    if (simple) {
        /* Shaft goes from the bottom centre up to the bend, then out at 'angle'. */
        const float rad = (float)angle * 3.14159265f / 180.0f;
        const float ux = sinf(rad), uy = -cosf(rad);     /* unit direction, y grows down */
        /* Sharp turns double back, so the bend sits high and to the side and the exit leg
         * is longer; otherwise the arrow head would land on top of the tail. */
        const bool sharp = angle > 90 || angle < -90;
        const int side = angle < 0 ? -1 : 1;
        const int bx = sharp ? cx - side * 14 : cx;
        const int by = sharp ? cy - 16 : cy + 2;         /* bend point */
        const float leg = sharp ? 26.0f : 18.0f;
        gfx_line(bx, cy + 22, bx, by, 8, col);           /* tail */
        const int tx = bx + (int)lroundf(ux * leg);
        const int ty = by + (int)lroundf(uy * leg);
        gfx_line(bx, by, tx, ty, 8, col);                /* shaft towards the exit */
        const float px = -uy, py = ux;                   /* perpendicular */
        gfx_pt_t head[3] = {
            { tx + (int)lroundf(ux * 12.0f),                   ty + (int)lroundf(uy * 12.0f) },
            { tx - (int)lroundf(ux * 2.0f) + (int)lroundf(px * 13.0f),
              ty - (int)lroundf(uy * 2.0f) + (int)lroundf(py * 13.0f) },
            { tx - (int)lroundf(ux * 2.0f) - (int)lroundf(px * 13.0f),
              ty - (int)lroundf(uy * 2.0f) - (int)lroundf(py * 13.0f) },
        };
        gfx_fill_poly(head, 3, col);
        return;
    }

    switch (m) {
    case KP_MAN_UTURN:
        gfx_line(cx + 10, cy + 22, cx + 10, cy - 6, 8, col);
        gfx_line(cx + 10, cy - 6, cx - 10, cy - 6, 8, col);
        gfx_line(cx - 10, cy - 6, cx - 10, cy + 8, 8, col);
        gfx_fill_poly((gfx_pt_t[]){ { cx - 10, cy + 22 }, { cx - 22, cy + 6 }, { cx + 2, cy + 6 } }, 3, col);
        break;
    case KP_MAN_ROUNDABOUT_RIGHT:
    case KP_MAN_ROUNDABOUT_LEFT: {
        int sgn = m == KP_MAN_ROUNDABOUT_RIGHT ? 1 : -1;
        gfx_circle(cx, cy - 2, 11, col);
        gfx_circle(cx, cy - 2, 10, col);
        gfx_circle(cx, cy - 2, 9, col);
        gfx_line(cx, cy + 22, cx, cy + 9, 7, col);
        gfx_line(cx + sgn * 10, cy - 10, cx + sgn * 22, cy - 22, 6, col);
        gfx_fill_poly((gfx_pt_t[]){ { cx + sgn * 24, cy - 24 }, { cx + sgn * 12, cy - 22 }, { cx + sgn * 22, cy - 12 } }, 3, col);
        break;
    }
    case KP_MAN_ARRIVE:
        gfx_fill_circle(cx, cy - 8, 13, col);
        gfx_fill_poly((gfx_pt_t[]){ { cx - 11, cy - 2 }, { cx + 11, cy - 2 }, { cx, cy + 20 } }, 3, col);
        gfx_fill_circle(cx, cy - 8, 5, C_PANEL);
        break;
    case KP_MAN_DEPART:
    default:
        gfx_line(cx, cy + 22, cx, cy - 10, 8, col);
        gfx_fill_poly((gfx_pt_t[]){ { cx, cy - 24 }, { cx - 13, cy - 8 }, { cx + 13, cy - 8 } }, 3, col);
        break;
    }
}

/* ---- text helpers ------------------------------------------------------ */
void hud_fmt_distance(char *out, unsigned n, uint32_t m)
{
    if (k_settings()->units == K_UNITS_MI) {
        uint32_t ft = (uint32_t)((uint64_t)m * 3281 / 1000);
        if (ft < 528) {
            snprintf(out, n, "%u ft", (unsigned)((ft + 5) / 10 * 10));
        } else {
            uint32_t d = (uint32_t)((uint64_t)m * 10000 / 1609344);   /* hundredths of 0.1 mi */
            snprintf(out, n, "%u.%u mi", (unsigned)(d / 10), (unsigned)(d % 10));
        }
    } else if (m < 1000) {
        snprintf(out, n, "%u m", (unsigned)m);
    } else {
        snprintf(out, n, "%u.%u km", (unsigned)(m / 1000), (unsigned)((m % 1000) / 100));
    }
}

static void draw_banner(const k_nav_snapshot_t *nav)
{
    gfx_blend_rect(0, 0, UI_W, UI_BANNER_H, C_PANEL, 225);
    draw_arrow(8, 8, (kp_maneuver_t)nav->maneuver, C_TEXT);

    char d[24];
    hud_fmt_distance(d, sizeof(d), nav->to_turn_m);
    gfx_text(72, 4, d, C_TEXT, 2);                         /* distance, 32 px high */
    gfx_text(72, 42, nav->text, C_TEXT, 1);                /* street text, 16 px high */

    if (nav->flags & KP_NAVF_REROUTING) {
        gfx_text(UI_W - 8 - gfx_text_w("Rerouting", 1), 4, "Rerouting", C_WARN, 1);
    } else if (nav->flags & KP_NAVF_OFFROUTE) {
        gfx_text(UI_W - 8 - gfx_text_w("Off route", 1), 4, "Off route", C_WARN, 1);
    }
}

static void draw_waiting_bar(const k_nav_snapshot_t *nav)
{
    char line[64];
    if (nav->link == K_LINK_OFF) {
        snprintf(line, sizeof(line), "Bluetooth off");
    } else if (nav->link == K_LINK_ADVERTISING) {
        snprintf(line, sizeof(line), "Waiting for phone (BLE: %s)", k_settings()->device_name);
    } else {
        return;                                            /* connected, nothing to say */
    }
    gfx_blend_rect(0, 0, UI_W, UI_FONT_H, C_PANEL, 200);
    gfx_text(2, 0, line, C_TEXT, 1);
}

static void draw_status_bar(const k_nav_snapshot_t *nav, uint8_t zoom, bool following,
                            int32_t clat, int32_t clon)
{
    const int y = UI_H - UI_STATUS_H;
    gfx_blend_rect(0, y, UI_W, UI_STATUS_H, C_PANEL, 215);

    bool pos_ok = k_nav_pos_valid(nav, kernel_millis());
    char l1[96], l2[64], tmp[48];

    if (pos_ok) {                                          /* NAV-04 */
        int32_t lat = nav->lat_e6, lon = nav->lon_e6;
        int n = snprintf(l1, sizeof(l1), "Z%u  %d.%05d, %d.%05d", zoom,
                         (int)(lat / 1000000), (int)(abs(lat % 1000000) / 10),
                         (int)(lon / 1000000), (int)(abs(lon % 1000000) / 10));
        if (k_settings()->show_speed) {
            uint32_t kmh10 = (uint32_t)nav->speed_d10 * 36 / 10;       /* 0.1 km/h */
            if (k_settings()->units == K_UNITS_MI) {
                uint32_t mph10 = (uint32_t)((uint64_t)kmh10 * 1000 / 1609);
                n += snprintf(l1 + n, sizeof(l1) - n, "  %u mph", (unsigned)(mph10 / 10));
            } else {
                n += snprintf(l1 + n, sizeof(l1) - n, "  %u km/h", (unsigned)(kmh10 / 10));
            }
        }
        hud_fmt_distance(tmp, sizeof(tmp), nav->accuracy_m);
        snprintf(l1 + n, sizeof(l1) - n, "  +/-%s", tmp);
    } else {
        (void)clat; (void)clon;
        snprintf(l1, sizeof(l1), "Z%u  no position yet%s", zoom, following ? "" : "  (free pan)");
    }

    switch (nav->link) {
    case K_LINK_CONNECTED:   snprintf(l2, sizeof(l2), "BLE connected"); break;
    case K_LINK_ADVERTISING: snprintf(l2, sizeof(l2), "BLE not connected"); break;
    default:                 snprintf(l2, sizeof(l2), "BLE off"); break;
    }

    gfx_text(2, y, l1, C_TEXT, 1);
    gfx_text(2, y + 16, l2, nav->link == K_LINK_CONNECTED ? C_OK : C_TEXT_DIM, 1);

    if (nav->route_active && nav->has_nav) {               /* NAV-03 */
        char dist[24];
        hud_fmt_distance(dist, sizeof(dist), nav->remain_m);
        snprintf(tmp, sizeof(tmp), "%s  %u min", dist, (unsigned)((nav->remain_s + 30) / 60));
        gfx_text(UI_W - 2 - gfx_text_w(tmp, 1), y, tmp, C_TEXT, 1);
    }

    const char *attr = k_packs_attribution();              /* MAP-08: always visible */
    if (attr[0]) {
        gfx_text(UI_W - 2 - gfx_text_w(attr, 1), y + 16, attr, C_TEXT_DIM, 1);
    }
}

void hud_draw(const k_nav_snapshot_t *nav, uint8_t zoom, bool following,
              int32_t clat, int32_t clon)
{
    if (nav->route_active && nav->has_nav) {
        draw_banner(nav);
    } else {
        draw_waiting_bar(nav);
    }
    draw_buttons(following);
    draw_status_bar(nav, zoom, following, clat, clon);
}
