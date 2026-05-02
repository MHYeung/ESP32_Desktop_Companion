#include "geo_client.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "wifi_time.h"

static const char *TAG = "geo_client";

#define HTTP_BUF_SIZE 3072

typedef struct {
    SemaphoreHandle_t lock;
    int32_t lat_e6;
    int32_t lon_e6;
    bool valid;
    volatile bool busy;
    bool settled;
} geo_state_t;

static geo_state_t s_geo;

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

static bool parse_utc_offset(cJSON *root, int32_t *utc_offset_sec_out)
{
    cJSON *off_j = cJSON_GetObjectItem(root, "utc_offset");
    if (off_j == NULL) {
        return false;
    }
    long long v = 0;
    if (cJSON_IsNumber(off_j)) {
        v = (long long)llround(off_j->valuedouble);
    } else if (cJSON_IsString(off_j) && off_j->valuestring != NULL) {
        v = strtoll(off_j->valuestring, NULL, 10);
    } else {
        return false;
    }
    if (v < -50400 || v > 50400) {
        return false;
    }
    *utc_offset_sec_out = (int32_t)v;
    return true;
}

static void geo_fetch_task(void *arg)
{
    (void)arg;

    char *body = malloc(HTTP_BUF_SIZE);
    if (!body) {
        ESP_LOGW(TAG, "oom http buffer");
        goto finish;
    }
    http_accum_t acc = { .buf = body, .len = 0, .cap = HTTP_BUF_SIZE - 1 };

    esp_http_client_config_t cfg = {
        .url = "https://ipapi.co/json/",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_on_data,
        .user_data = &acc,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(body);
        goto finish;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "http err %s status %d", esp_err_to_name(err), status);
        free(body);
        goto finish;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        ESP_LOGW(TAG, "geo json root parse failed");
        free(body);
        goto finish;
    }

    int32_t utc_off = 0;
    bool have_off = parse_utc_offset(root, &utc_off);
    if (have_off) {
        wifi_time_set_tz_from_utc_offset_sec(utc_off);
    } else {
        ESP_LOGW(TAG, "no utc_offset in geo response");
    }

    cJSON *lat_j = cJSON_GetObjectItem(root, "latitude");
    cJSON *lon_j = cJSON_GetObjectItem(root, "longitude");
    double lat = NAN;
    double lon = NAN;
    if (cJSON_IsNumber(lat_j)) {
        lat = lat_j->valuedouble;
    } else if (cJSON_IsString(lat_j) && lat_j->valuestring != NULL) {
        lat = strtod(lat_j->valuestring, NULL);
    }
    if (cJSON_IsNumber(lon_j)) {
        lon = lon_j->valuedouble;
    } else if (cJSON_IsString(lon_j) && lon_j->valuestring != NULL) {
        lon = strtod(lon_j->valuestring, NULL);
    }

    if (!isnan(lat) && !isnan(lon) && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 &&
        lon <= 180.0) {
        int32_t lat_e6 = (int32_t)llround(lat * 1e6);
        int32_t lon_e6 = (int32_t)llround(lon * 1e6);
        xSemaphoreTake(s_geo.lock, portMAX_DELAY);
        s_geo.lat_e6 = lat_e6;
        s_geo.lon_e6 = lon_e6;
        s_geo.valid = true;
        xSemaphoreGive(s_geo.lock);
        ESP_LOGI(TAG, "geo: lat_e6 %ld lon_e6 %ld", (long)lat_e6, (long)lon_e6);
    } else {
        ESP_LOGW(TAG, "geo lat/lon invalid or missing");
    }

    cJSON_Delete(root);
    free(body);

finish:
    xSemaphoreTake(s_geo.lock, portMAX_DELAY);
    s_geo.settled = true;
    s_geo.busy = false;
    xSemaphoreGive(s_geo.lock);
    vTaskDelete(NULL);
}

void geo_client_init(void)
{
    memset(&s_geo, 0, sizeof(s_geo));
    s_geo.lock = xSemaphoreCreateMutex();
}

void geo_client_request_refresh(void)
{
    xSemaphoreTake(s_geo.lock, portMAX_DELAY);
    if (s_geo.busy) {
        xSemaphoreGive(s_geo.lock);
        return;
    }
    s_geo.busy = true;
    s_geo.settled = false;
    xSemaphoreGive(s_geo.lock);

    if (xTaskCreate(geo_fetch_task, "geo_http", 8192, NULL, 5, NULL) != pdPASS) {
        xSemaphoreTake(s_geo.lock, portMAX_DELAY);
        s_geo.busy = false;
        s_geo.settled = true;
        xSemaphoreGive(s_geo.lock);
        ESP_LOGW(TAG, "geo task create failed");
    }
}

bool geo_client_get_location(int32_t *lat_e6, int32_t *lon_e6)
{
    if (lat_e6 == NULL || lon_e6 == NULL) {
        return false;
    }
    xSemaphoreTake(s_geo.lock, portMAX_DELAY);
    bool ok = s_geo.valid;
    if (ok) {
        *lat_e6 = s_geo.lat_e6;
        *lon_e6 = s_geo.lon_e6;
    }
    xSemaphoreGive(s_geo.lock);
    return ok;
}

bool geo_client_is_lookup_settled(void)
{
    xSemaphoreTake(s_geo.lock, portMAX_DELAY);
    bool settled = s_geo.settled && !s_geo.busy;
    xSemaphoreGive(s_geo.lock);
    return settled;
}
