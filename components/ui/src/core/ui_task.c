#include "ui.h"
#include "gfx.h"
#include "map_view.h"
#include "kernel.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ui";

#define STACK_MAX      6
#define FRAME_MS       33        /* about 30 frames per second at most */
#define IDLE_REDRAW_MS 5000      /* repaint at least every 5 s (stale-position check) */

static const ui_screen_t *s_stack[STACK_MAX];
static int                s_depth;
static volatile bool      s_dirty = true;

/* ---- screen stack (UI task only, except ui_invalidate) ----------------- */
void ui_invalidate(void)
{
    s_dirty = true;
    k_events_post(KEV_SETTINGS);          /* wakes the UI task if it is sleeping */
}

void ui_push(const ui_screen_t *s)
{
    if (s_depth < STACK_MAX) {
        s_stack[s_depth++] = s;
        if (s->enter) {
            s->enter(s);
        }
        s_dirty = true;
    }
}

void ui_pop(void)
{
    if (s_depth > 1) {
        s_depth--;
        const ui_screen_t *s = s_stack[s_depth - 1];
        if (s->enter) {
            s->enter(s);
        }
        s_dirty = true;
    }
}

void ui_pop_to_root(void)
{
    while (s_depth > 1) {
        s_depth--;
    }
    s_dirty = true;
}

/* ---- task -------------------------------------------------------------- */
static void ui_task(void *arg)
{
    (void)arg;
    uint32_t last_draw = 0;

    ui_push(&screen_map);

    for (;;) {
        uint32_t ev = k_events_wait(KEV_ALL, FRAME_MS);

        k_touch_evt_t t;
        while (k_input_poll(&t)) {
            const ui_screen_t *top = s_stack[s_depth - 1];
            if (top->touch) {
                top->touch(top, &t);
            }
            s_dirty = true;
        }

        if (k_power_asleep()) {
            continue;                       /* backlight is off: save the work */
        }

        uint32_t now = kernel_millis();
        if (ev || s_dirty || (uint32_t)(now - last_draw) >= IDLE_REDRAW_MS) {
            s_dirty = false;
            const ui_screen_t *top = s_stack[s_depth - 1];
            gfx_begin_frame();
            top->draw(top);
            k_display_present();
            last_draw = now;

            /* Keep to ~30 fps even when events arrive in bursts. */
            uint32_t spent = kernel_millis() - now;
            if (spent < FRAME_MS) {
                vTaskDelay(pdMS_TO_TICKS(FRAME_MS - spent));
            }
        }
    }
}

void ui_start(void)
{
    gfx_init();
    map_view_init();
    BaseType_t ok = xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, 7, NULL, 0);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "cannot start UI task");
    }
}
