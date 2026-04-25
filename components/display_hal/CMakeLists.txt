idf_component_register(
    SRCS "display_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd
    PRIV_REQUIRES driver littlefs freertos heap
)
