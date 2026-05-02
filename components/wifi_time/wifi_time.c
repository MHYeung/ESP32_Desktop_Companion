#include "wifi_time.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "wifi_time";
static bool s_wifi_connected;
static char s_timezone[40];

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_connected = true;
        ESP_LOGI(TAG, "Wi-Fi connected");
    }
}

esp_err_t wifi_time_init(const char *ssid, const char *pass)
{
    s_wifi_connected = false;
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT,
                        WIFI_EVENT_STA_DISCONNECTED, wifi_event_handler, NULL, NULL),
                        TAG, "wifi disconnect handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
                        IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL),
                        TAG, "wifi got ip handler");

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_FLASH), TAG, "wifi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "wifi config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "wifi connect");

    ESP_LOGI(TAG, "starting SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    strlcpy(s_timezone, "UTC0", sizeof(s_timezone));
    setenv("TZ", s_timezone, 1);
    tzset();
    return ESP_OK;
}

void wifi_time_set_tz_from_utc_offset_sec(int32_t utc_offset_sec)
{
    /*
     * ipapi.co: local time = UTC + utc_offset_sec (seconds).
     * Match legacy POSIX TZ convention used by normalize_timezone():
     * e.g. east +8h -> TZ string "UTC-8"; west -5h -> "UTC+5".
     */
    int64_t sec = utc_offset_sec;
    bool east = sec >= 0;
    int64_t a = east ? sec : -sec;
    int hh = (int)(a / 3600);
    int mm = (int)((a % 3600) / 60);

    if (mm != 0) {
        if (east) {
            snprintf(s_timezone, sizeof(s_timezone), "UTC-%d:%02d", hh, mm);
        } else {
            snprintf(s_timezone, sizeof(s_timezone), "UTC+%d:%02d", hh, mm);
        }
    } else {
        if (east) {
            snprintf(s_timezone, sizeof(s_timezone), "UTC-%d", hh);
        } else {
            snprintf(s_timezone, sizeof(s_timezone), "UTC+%d", hh);
        }
    }

    setenv("TZ", s_timezone, 1);
    tzset();
    ESP_LOGI(TAG, "TZ from utc_offset=%lds -> %s", (long)utc_offset_sec, s_timezone);
}

bool wifi_time_is_connected(void)
{
    return s_wifi_connected;
}
