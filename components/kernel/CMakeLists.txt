# Layer 2: kernel services. Depends only on hw_hal and ESP-IDF system services.
# The portable sources (proto, geo, tiles/k_tpk.c) have no ESP-IDF includes so
# the host tests in test/host can compile them directly.
idf_component_register(
    SRCS
        # portable
        "src/proto/k_proto.c"
        "src/geo/k_geo.c"
        "src/tiles/k_tpk.c"
        # system
        "src/system/k_sys.c"
        "src/system/k_events.c"
        "src/system/k_settings.c"
        "src/system/k_power.c"
        # navigation state
        "src/nav/k_nav.c"
        "src/nav/k_route.c"
        # storage + tiles
        "src/storage/k_storage.c"
        "src/tiles/k_tiles.c"
        # display + input
        "src/display/k_display.c"
        "src/display/k_input.c"
        # bluetooth
        "src/ble/k_ble.c"
    INCLUDE_DIRS "include"
    REQUIRES
        hw_hal
        freertos
        nvs_flash
        bt
        esp_timer
)
