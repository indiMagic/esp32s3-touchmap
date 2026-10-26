/**
 * @file ui.h
 * @brief Layer 3 entry point and screen stack.
 *
 * Screens are plain tables of callbacks. The UI task owns the stack; the map
 * is always the bottom of it ("the map is the home screen").
 */
#pragma once

#include <stdbool.h>
#include "k_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_screen ui_screen_t;

struct ui_screen {
    const char *title;
    void (*enter)(const ui_screen_t *self);                       /* optional */
    void (*draw)(const ui_screen_t *self);                        /* required */
    void (*touch)(const ui_screen_t *self, const k_touch_evt_t *e);   /* optional */
    const void *data;                                             /* screen-private */
};

/** Start the UI task. Call after kernel_start(). */
void ui_start(void);

void ui_push(const ui_screen_t *s);
void ui_pop(void);
void ui_pop_to_root(void);
void ui_invalidate(void);          /* request a redraw on the next frame */

/** The built-in screens (src/screens). */
extern const ui_screen_t screen_map;
extern const ui_screen_t screen_settings;
extern const ui_screen_t screen_bluetooth;
extern const ui_screen_t screen_wifi;
extern const ui_screen_t screen_storage;
extern const ui_screen_t screen_display;
extern const ui_screen_t screen_mapopts;
extern const ui_screen_t screen_about;
extern const ui_screen_t screen_factory;

#ifdef __cplusplus
}
#endif
