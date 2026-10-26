#include "k_display.h"
#include "hal.h"

#include "esp_log.h"

static uint16_t *s_back;
static bool s_direct;

uint16_t *k_display_backbuffer(void)
{
    if (s_direct) {
        return hal_lcd_framebuffer();       /* alternates after every present */
    }
    if (!s_back) {
        /* Draw straight into the panel frame buffer: no per-frame copy, less PSRAM traffic. */
        s_back = hal_lcd_framebuffer();
        s_direct = (s_back != NULL);
    }
    if (!s_back) {
        s_back = hal_psram_alloc((size_t)K_DISPLAY_W * K_DISPLAY_H * 2);
        if (!s_back) {
            ESP_LOGE("k_display", "back buffer: out of PSRAM");
        }
    }
    return s_back;
}

void k_display_present(void)
{
    if (s_direct) {
        hal_lcd_flip();
    } else if (s_back) {
        hal_lcd_present(s_back);
    }
}

void k_display_backlight(bool on)
{
    hal_lcd_backlight(on);
}
