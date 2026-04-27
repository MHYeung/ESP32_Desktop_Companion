#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

// Include our custom modules
#include "display_hal.h"
#include "button_mgr.h"
#include "wifi_time.h"
#include "ui_app.h"
#include "user_assets.h"
#include "user_config.h"
#include "weather_client.h"

static const char *TAG = "main";

static void init_system_services(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    weather_client_init();
}

void app_main(void)
{
    ESP_LOGI(TAG, "desktop companion booting");
    init_system_services();

    // 1. Initialize Screen & Storage
    esp_lcd_panel_handle_t display = display_hal_init();
    esp_err_t assets_ret = user_assets_init();
    if (assets_ret != ESP_OK) {
        ESP_LOGW(TAG, "continuing without flashed photo assets: %s", esp_err_to_name(assets_ret));
    }

    // 2. Initialize Button (Gets the FreeRTOS event queue)
    QueueHandle_t btn_queue = button_mgr_init();

    user_config_t user_config;
    esp_err_t cfg_ret = user_config_load(&user_config);
    if (cfg_ret == ESP_OK) {
        ESP_LOGI(TAG, "connecting to configured Wi-Fi");
        ESP_ERROR_CHECK(wifi_time_init(user_config.ssid, user_config.password,
                                       user_config.timezone));
    } else {
        ESP_LOGW(TAG, "starting UI without Wi-Fi time: %s", esp_err_to_name(cfg_ret));
        user_config.rotation_interval_sec = 60;
        user_config.pomodoro_seconds = 25 * 60;
        user_config.weather_lat_e6 = 0;
        user_config.weather_lon_e6 = 0;
        user_assets_config_t ac;
        if (user_assets_get_config(&ac) == ESP_OK) {
            if (ac.rotation_interval_sec > 0) {
                user_config.rotation_interval_sec = ac.rotation_interval_sec;
            }
            if (ac.pomodoro_seconds > 0) {
                user_config.pomodoro_seconds = ac.pomodoro_seconds;
            }
            user_config.weather_lat_e6 = ac.weather_lat_e6;
            user_config.weather_lon_e6 = ac.weather_lon_e6;
        }
    }

    // 4. Start the Application UI Loop
    ui_app_start(display, btn_queue, user_config.rotation_interval_sec,
                 user_config.pomodoro_seconds, user_config.weather_lat_e6,
                 user_config.weather_lon_e6);
}