idf_component_register(
    SRCS "display_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd user_assets
    PRIV_REQUIRES driver freertos heap
)
