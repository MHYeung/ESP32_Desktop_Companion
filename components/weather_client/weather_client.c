#include "weather_client.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "weather_client";

#define HTTP_BUF_SIZE 4096

typedef struct {
    SemaphoreHandle_t lock;
    int temp_c;
    int wmo_code;
    time_t updated_at;
    bool valid;
    volatile bool busy;
} weather_state_t;

static weather_state_t s_state;

typedef struct {
    int32_t lat_e6;
    int32_t lon_e6;
} weather_fetch_args_t;

static esp_err_t parse_open_meteo_json(const char *json, int *temp_out, int *wmo_out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    cJSON *current = cJSON_GetObjectItem(root, "current");
    if (!cJSON_IsObject(current)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    cJSON *t = cJSON_GetObjectItem(current, "temperature_2m");
    cJSON *w = cJSON_GetObjectItem(current, "weather_code");
    if (!cJSON_IsNumber(t) || !cJSON_IsNumber(w)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    *temp_out = (int)lround(t->valuedouble);
    *wmo_out = w->valueint;
    cJSON_Delete(root);
    return ESP_OK;
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} http_accum_t;

static esp_err_t http_on_data(esp_http_client_event_t *evt)
{
    http_accum_t *acc = (http_accum_t *)evt->user_data;
    if (evt->data_len <= 0 || acc->buf == NULL) {
        return ESP_OK;
    }
    size_t next = acc->len + (size_t)evt->data_len;
    if (next >= acc->cap) {
        return ESP_FAIL;
    }
    memcpy(acc->buf + acc->len, evt->data, (size_t)evt->data_len);
    acc->len = next;
    acc->buf[acc->len] = '\0';
    return ESP_OK;
}

static void weather_fetch_task(void *arg)
{
    weather_fetch_args_t *params = (weather_fetch_args_t *)arg;
    double lat = (double)params->lat_e6 / 1e6;
    double lon = (double)params->lon_e6 / 1e6;
    free(params);

    char url[176];
    int n = snprintf(url, sizeof(url),
                     "https://api.open-meteo.com/v1/forecast?"
                     "latitude=%.5f&longitude=%.5f&current=temperature_2m,weather_code",
                     lat, lon);
    if (n <= 0 || n >= (int)sizeof(url)) {
        s_state.busy = false;
        vTaskDelete(NULL);
        return;
    }

    char *body = malloc(HTTP_BUF_SIZE);
    if (!body) {
        ESP_LOGW(TAG, "oom http buffer");
        s_state.busy = false;
        vTaskDelete(NULL);
        return;
    }
    http_accum_t acc = { .buf = body, .len = 0, .cap = HTTP_BUF_SIZE - 1 };

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_on_data,
        .user_data = &acc,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(body);
        s_state.busy = false;
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "http err %s status %d", esp_err_to_name(err), status);
        free(body);
        s_state.busy = false;
        vTaskDelete(NULL);
        return;
    }

    int temp_c = 0, wmo = 0;
    err = parse_open_meteo_json(body, &temp_c, &wmo);
    free(body);

    if (err == ESP_OK) {
        xSemaphoreTake(s_state.lock, portMAX_DELAY);
        s_state.temp_c = temp_c;
        s_state.wmo_code = wmo;
        s_state.updated_at = time(NULL);
        s_state.valid = true;
        xSemaphoreGive(s_state.lock);
        ESP_LOGI(TAG, "weather: %d C code %d", temp_c, wmo);
    } else {
        ESP_LOGW(TAG, "json parse failed");
    }

    s_state.busy = false;
    vTaskDelete(NULL);
}

void weather_client_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.lock = xSemaphoreCreateMutex();
}

void weather_client_request_refresh(int32_t lat_e6, int32_t lon_e6)
{
    if (lat_e6 == 0 && lon_e6 == 0) {
        return;
    }
    if (s_state.busy) {
        return;
    }
    weather_fetch_args_t *args = malloc(sizeof(weather_fetch_args_t));
    if (!args) {
        return;
    }
    args->lat_e6 = lat_e6;
    args->lon_e6 = lon_e6;
    s_state.busy = true;
    if (xTaskCreate(weather_fetch_task, "weather_http", 8192, args, 5, NULL) != pdPASS) {
        s_state.busy = false;
        free(args);
    }
}

bool weather_client_get_snapshot(int *temp_c, int *wmo_code, int *age_sec)
{
    if (temp_c == NULL || wmo_code == NULL || age_sec == NULL) {
        return false;
    }
    xSemaphoreTake(s_state.lock, portMAX_DELAY);
    bool ok = s_state.valid;
    if (ok) {
        *temp_c = s_state.temp_c;
        *wmo_code = s_state.wmo_code;
        time_t now = time(NULL);
        *age_sec = (int)(now - s_state.updated_at);
        if (*age_sec < 0) {
            *age_sec = 0;
        }
    }
    xSemaphoreGive(s_state.lock);
    return ok;
}
