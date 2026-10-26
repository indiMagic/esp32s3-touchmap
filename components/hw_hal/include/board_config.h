/**
 * @file board_config.h
 * @brief Waveshare ESP32-S3-Touch-LCD-4.3 pin map and panel constants.
 *
 * Layer 1 (hardware). This is the ONLY file that knows GPIO numbers.
 * Source: SRS Figure 9 (Waveshare documentation). Items marked ASSUMED are
 * open issues (see docs/ARCHITECTURE.md, OI-4).
 */
#pragma once

#include "driver/gpio.h"

/* ---- Panel geometry ---------------------------------------------------- */
#define BOARD_LCD_W            800
#define BOARD_LCD_H            480
#define BOARD_LCD_PCLK_HZ      (16 * 1000 * 1000)
#define BOARD_LCD_BOUNCE_LINES 30      /* bounce buffer = 30 lines of pixels */

/* ---- RGB LCD bus ------------------------------------------------------- */
#define BOARD_LCD_HSYNC   GPIO_NUM_46
#define BOARD_LCD_VSYNC   GPIO_NUM_3
#define BOARD_LCD_DE      GPIO_NUM_5
#define BOARD_LCD_PCLK    GPIO_NUM_7

/* Data lines in RGB565 order: B3..B7, G2..G7, R3..R7 */
#define BOARD_LCD_DATA_PINS { \
    14, 38, 18, 17, 10,        /* B3..B7 */ \
    39,  0, 45, 48, 47, 21,    /* G2..G7 */ \
     1,  2, 42, 41, 40         /* R3..R7 */ }

/* ---- I2C bus (CH422G expander + GT911 touch) --------------------------- */
#define BOARD_I2C_PORT        0
#define BOARD_I2C_SDA         GPIO_NUM_8
#define BOARD_I2C_SCL         GPIO_NUM_9
#define BOARD_I2C_FREQ_HZ     400000

/* CH422G: one address to configure, one to drive the output byte */
#define BOARD_CH422G_ADDR_SETUP  0x24
#define BOARD_CH422G_ADDR_OUT    0x38

/* CH422G output bit numbers (EXIOn) */
#define BOARD_EXIO_TOUCH_RST  1
#define BOARD_EXIO_BACKLIGHT  2
#define BOARD_EXIO_LCD_RST    3   /* ASSUMED: Waveshare page lists no function (OI-4) */
#define BOARD_EXIO_SD_CS      4   /* held LOW; SD runs as the only device on SPI2 */
#define BOARD_EXIO_USB_SEL    5   /* 0 = USB, 1 = CAN */

/* ---- GT911 touch controller ------------------------------------------- */
#define BOARD_TOUCH_INT       GPIO_NUM_4
#define BOARD_GT911_ADDR_A    0x5D
#define BOARD_GT911_ADDR_B    0x14

/* ---- microSD (SPI mode) ------------------------------------------------ */
#define BOARD_SD_HOST         SPI2_HOST
#define BOARD_SD_MOSI         GPIO_NUM_11
#define BOARD_SD_CLK          GPIO_NUM_12
#define BOARD_SD_MISO         GPIO_NUM_13
#define BOARD_SD_FREQ_KHZ     20000
#define BOARD_SD_MOUNT_POINT  "/sdcard"

/* ---- USB pins (do not use as GPIO) ------------------------------------- */
#define BOARD_USB_DM          GPIO_NUM_19
#define BOARD_USB_DP          GPIO_NUM_20
