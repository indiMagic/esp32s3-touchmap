#include "k_events.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static EventGroupHandle_t s_eg;

void k_events_init(void)
{
    if (!s_eg) {
        s_eg = xEventGroupCreate();
    }
}

void k_events_post(uint32_t bits)
{
    if (s_eg) {
        xEventGroupSetBits(s_eg, bits);
    }
}

uint32_t k_events_wait(uint32_t mask, uint32_t timeout_ms)
{
    if (!s_eg) {
        return 0;
    }
    return xEventGroupWaitBits(s_eg, mask, pdTRUE /* clear */, pdFALSE /* any */,
                               pdMS_TO_TICKS(timeout_ms)) & mask;
}
