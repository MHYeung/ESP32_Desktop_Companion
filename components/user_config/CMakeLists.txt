idf_component_register(
    SRCS "user_config.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash user_assets
)
