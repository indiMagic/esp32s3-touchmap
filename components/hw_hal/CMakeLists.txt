idf_component_register(
    SRCS
        "src/hal_sys.c"
        "src/hal_i2c.c"
        "src/hal_ioexp.c"
        "src/hal_lcd.c"
        "src/hal_touch.c"
        "src/hal_sd.c"
        "src/hal_file.c"
        "src/hal_jpeg.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_driver_i2c
        esp_driver_gpio
        esp_driver_spi
        esp_lcd
        esp_timer
        esp_psram
        esp_rom
        fatfs
        sdmmc
        vfs
    PRIV_REQUIRES
        espressif__esp_jpeg      # managed component (see idf_component.yml)
)
