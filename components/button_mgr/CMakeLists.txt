idf_component_register(
    SRCS "button_mgr.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES driver
)
