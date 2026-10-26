#include "k_input.h"
#include "k_events.h"
#include "k_power.h"
#include "hal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define POLL_MS        20
#define UP_DEBOUNCE    3     /* polls without a touch before we call it released */

static QueueHandle_t s_q;

static void push(k_touch_type_t t, int x, int y, int dx, int dy, int sx, int sy)
{
    k_touch_evt_t e = { t, (int16_t)x, (int16_t)y, (int16_t)dx, (int16_t)dy, (int16_t)sx, (int16_t)sy };
    if (xQueueSend(s_q, &e, 0) != pdTRUE) {
        /* Queue full: drop the oldest so the newest state always gets through. */
        k_touch_evt_t junk;
        xQueueReceive(s_q, &junk, 0);
        xQueueSend(s_q, &e, 0);
    }
    k_events_post(KEV_INPUT);
}

static void input_task(void *arg)
{
    (void)arg;
    bool down = false;
    bool swallow = false;            /* first touch only wakes a sleeping screen */
    int miss = 0;
    int lx = 0, ly = 0, sx = 0, sy = 0;

    for (;;) {
        uint16_t x, y;
        bool t = hal_touch_read(&x, &y);

        if (t) {
            miss = 0;
            k_power_poke();
            if (!down) {
                down = true;
                if (k_power_wake_if_asleep()) {
                    swallow = true;
                } else {
                    swallow = false;
                    sx = lx = x; sy = ly = y;
                    push(K_TOUCH_DOWN, x, y, 0, 0, sx, sy);
                }
            } else if (!swallow && (x != lx || y != ly)) {
                push(K_TOUCH_MOVE, x, y, (int)x - lx, (int)y - ly, sx, sy);
                lx = x; ly = y;
            }
        } else if (down && ++miss >= UP_DEBOUNCE) {
            down = false;
            miss = 0;
            if (!swallow) {
                push(K_TOUCH_UP, lx, ly, 0, 0, sx, sy);
            }
            swallow = false;
        }

        k_power_tick();
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

bool k_input_poll(k_touch_evt_t *out)
{
    return s_q && xQueueReceive(s_q, out, 0) == pdTRUE;
}

void k_input_start(void)
{
    s_q = xQueueCreate(16, sizeof(k_touch_evt_t));
    xTaskCreatePinnedToCore(input_task, "input", 3072, NULL, 6, NULL, 0);
}
