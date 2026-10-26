#include "k_power.h"
#include "k_settings.h"
#include "k_display.h"
#include "k_events.h"
#include "hal.h"

static volatile uint32_t s_last_ms;
static volatile bool     s_asleep;

void k_power_init(void)
{
    s_last_ms = hal_millis();
    s_asleep = false;
    k_display_backlight(k_settings()->backlight);
}

void k_power_poke(void)
{
    s_last_ms = hal_millis();
}

void k_power_tick(void)
{
    uint32_t to = k_settings_timeout_ms(k_settings()->timeout_idx);
    if (to && !s_asleep && (uint32_t)(hal_millis() - s_last_ms) > to) {
        s_asleep = true;
        k_display_backlight(false);
    }
}

bool k_power_wake_if_asleep(void)
{
    if (!s_asleep) {
        return false;
    }
    s_asleep = false;
    s_last_ms = hal_millis();
    k_display_backlight(k_settings()->backlight);
    k_events_post(KEV_ALL);
    return true;
}

bool k_power_asleep(void)
{
    return s_asleep;
}
