# Layer 3: user interface. Depends ONLY on the kernel (never on hw_hal).
idf_component_register(
    SRCS
        "src/core/ui_task.c"
        "src/gfx/gfx.c"
        "src/gfx/font_ui.c"
        "src/map/map_view.c"
        "src/hud/hud.c"
        "src/widgets/ui_form.c"
        "src/screens/screen_map.c"
        "src/screens/screen_settings.c"
        "src/screens/screen_bluetooth.c"
        "src/screens/screen_wifi.c"
        "src/screens/screen_storage.c"
        "src/screens/screen_display.c"
        "src/screens/screen_mapopts.c"
        "src/screens/screen_about.c"
        "src/screens/screen_factory.c"
    INCLUDE_DIRS "include"
    REQUIRES
        kernel
        freertos
        log
)
