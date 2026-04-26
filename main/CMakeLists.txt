idf_component_register(SRCS "main.c" 
                    INCLUDE_DIRS "."
                    REQUIRES button_mgr display_hal ui_app wifi_time user_assets user_config
                             nvs_flash esp_netif esp_event)