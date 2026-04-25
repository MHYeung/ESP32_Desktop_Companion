idf_component_register(
    SRCS "ui_app.c"
    INCLUDE_DIRS "include"
    REQUIRES display_hal freertos
    PRIV_REQUIRES button_mgr heap
)
