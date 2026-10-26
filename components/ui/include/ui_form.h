/**
 * @file ui_form.h
 * @brief Declarative "settings page": a header with a back button plus a list
 *        of 64 px rows. Most screens are just a table of rows.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROW_INFO,      /* label + value, not tappable                       */
    ROW_TOGGLE,    /* label + switch; get() gives the state, act() flips */
    ROW_CHOICE,    /* label + value text; tap calls act() to cycle       */
    ROW_ACTION,    /* full-width button; tap calls act()                 */
    ROW_DANGER,    /* red full-width button                              */
} ui_row_kind_t;

typedef struct {
    const char    *label;
    ui_row_kind_t  kind;
    bool         (*get)(void);                       /* ROW_TOGGLE */
    void         (*value)(char *buf, size_t n);      /* INFO / CHOICE text */
    void         (*act)(void);
} ui_row_t;

typedef struct {
    const ui_row_t *rows;
    int             count;
    const char     *footnote;                         /* dim text under the rows, optional */
} ui_form_t;

/** Generic draw/touch for screens whose .data points to a ui_form_t. */
void ui_form_draw(const ui_screen_t *self);
void ui_form_touch(const ui_screen_t *self, const k_touch_evt_t *e);

/** Header with title and a back button (shared by every non-map screen). */
void ui_draw_header(const char *title);
bool ui_header_back_hit(const k_touch_evt_t *e);   /* true if an UP landed on the back button */

/** True for a tap: UP close to where the touch began. */
bool ui_is_tap(const k_touch_evt_t *e);

#ifdef __cplusplus
}
#endif
