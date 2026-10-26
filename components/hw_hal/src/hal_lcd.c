#include "hal_lcd.h"
#include "hal_ioexp.h"
#include "board_config.h"

#include <stdbool.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_lcd";
static esp_lcd_panel_handle_t s_panel;

esp_err_t hal_lcd_init(void)
{
    /* Pulse the LCD reset through the expander (EXIO3, assumed - OI-4). */
    hal_ioexp_set(BOARD_EXIO_LCD_RST, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    hal_ioexp_set(BOARD_EXIO_LCD_RST, true);
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = BOARD_LCD_PCLK_HZ,
            .h_res = BOARD_LCD_W,
            .v_res = BOARD_LCD_H,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags = { .pclk_active_neg = 1 },
        },
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 2,        /* draw into the hidden one, then flip: no visible repaint */
        /* Bounce buffer keeps the picture stable while SD and BLE hammer PSRAM. */
        .bounce_buffer_size_px = BOARD_LCD_W * BOARD_LCD_BOUNCE_LINES,
        .hsync_gpio_num = BOARD_LCD_HSYNC,
        .vsync_gpio_num = BOARD_LCD_VSYNC,
        .de_gpio_num = BOARD_LCD_DE,
        .pclk_gpio_num = BOARD_LCD_PCLK,
        .disp_gpio_num = -1,
        .data_gpio_nums = BOARD_LCD_DATA_PINS,
        .flags = { .fb_in_psram = 1 },
    };

    esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new_rgb_panel: %s", esp_err_to_name(err));
        return err;
    }
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_LOGI(TAG, "panel %dx%d @ %d Hz", BOARD_LCD_W, BOARD_LCD_H, BOARD_LCD_PCLK_HZ);
    return ESP_OK;
}

static void *s_fb[2];
static int s_back = 1;      /* index of the hidden buffer the UI draws into */

uint16_t *hal_lcd_framebuffer(void)
{
    if (!s_fb[0]) {
        if (!s_panel || esp_lcd_rgb_panel_get_frame_buffer(s_panel, 2, &s_fb[0], &s_fb[1]) != ESP_OK) {
            return NULL;
        }
    }
    return s_fb[s_back];
}

esp_err_t hal_lcd_flip(void)
{
    if (!s_panel || !s_fb[0]) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Passing one of the driver's own buffers just switches the displayed buffer (no copy). */
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_W, BOARD_LCD_H, s_fb[s_back]);
    if (err == ESP_OK) {
        s_back ^= 1;
    }
    return err;
}

esp_err_t hal_lcd_present(const uint16_t *frame)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_W, BOARD_LCD_H, frame);
}

void hal_lcd_backlight(bool on)
{
    hal_ioexp_backlight(on);
}
