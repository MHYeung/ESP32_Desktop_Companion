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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "wifi_time";
static bool s_wifi_connected;
static char s_timezone[32];

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

static const char *normalize_timezone(const char *input)
{
    if (input == NULL || input[0] == '\0') {
        return "UTC0";
    }

    char compact[16] = {0};
    size_t out = 0;
    for (size_t i = 0; input[i] != '\0' && out < sizeof(compact) - 1; i++) {
        if (input[i] != ' ') {
            compact[out++] = (char)toupper((unsigned char)input[i]);
        }
    }

    int hours = 0;
    if (sscanf(compact, "UTC+%d", &hours) == 1 ||
        sscanf(compact, "GMT+%d", &hours) == 1) {
        snprintf(s_timezone, sizeof(s_timezone), "UTC-%d", hours);
        return s_timezone;
    }
    if (sscanf(compact, "UTC-%d", &hours) == 1 ||
        sscanf(compact, "GMT-%d", &hours) == 1) {
        snprintf(s_timezone, sizeof(s_timezone), "UTC+%d", hours);
        return s_timezone;
    }
    return input;
}

esp_err_t wifi_time_init(const char *ssid, const char *pass, const char *timezone) {
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
    strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_FLASH), TAG, "wifi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "wifi config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "wifi connect");

    ESP_LOGI(TAG, "starting SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    setenv("TZ", normalize_timezone(timezone), 1);
    tzset();
    return ESP_OK;
}

bool wifi_time_is_connected(void)
{
    return s_wifi_connected;
}
