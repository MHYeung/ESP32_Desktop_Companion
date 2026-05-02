#include "user_assets.h"

#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_partition.h"

typedef struct __attribute__((packed)) {
    char magic[4];
    uint16_t version;
    uint16_t header_size;
    uint16_t screen_width;
    uint16_t screen_height;
    uint16_t image_count;
    uint16_t reserved0;
    uint32_t image_size;
    uint32_t rotation_interval_sec;
    uint32_t pomodoro_focus_sec;
    uint32_t asset_id;
    uint32_t flags;
    char timezone[64];
    char ssid[33];
    char password[65];
    int32_t weather_lat_e6;
    int32_t weather_lon_e6;
    uint32_t pomodoro_short_break_sec;
    uint32_t pomodoro_long_break_sec;
    uint32_t pomodoro_long_break_every;
    uint8_t pad[38];
} user_assets_header_t;

_Static_assert(sizeof(user_assets_header_t) == USER_ASSETS_HEADER_SIZE,
               "asset header must stay 256 bytes");

static const char *TAG = "user_assets";
static const esp_partition_t *s_partition;
static user_assets_header_t s_header;
static bool s_ready;

static void copy_field(char *dst, size_t dst_len, const char *src, size_t src_len)
{
    size_t n = strnlen(src, src_len);
    if (n >= dst_len) {
        n = dst_len - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

esp_err_t user_assets_init(void)
{
    s_ready = false;
    s_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                           ESP_PARTITION_SUBTYPE_ANY, "assets");
    if (s_partition == NULL) {
        ESP_LOGW(TAG, "assets partition not found");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(esp_partition_read(s_partition, 0, &s_header,
                                           sizeof(s_header)), TAG,
                        "read assets header");
    if (memcmp(s_header.magic, USER_ASSETS_MAGIC, sizeof(s_header.magic)) != 0 ||
        (s_header.version != 1 && s_header.version != USER_ASSETS_FORMAT_VERSION) ||
        s_header.header_size != USER_ASSETS_HEADER_SIZE) {
        ESP_LOGW(TAG, "assets partition has no valid desktop companion header");
        return ESP_ERR_INVALID_VERSION;
    }

    uint32_t expected = (uint32_t)s_header.screen_width * s_header.screen_height * 2;
    if (s_header.image_size != expected ||
        USER_ASSETS_HEADER_SIZE + (uint64_t)s_header.image_count * expected > s_partition->size) {
        ESP_LOGE(TAG, "assets geometry/count does not fit partition");
        return ESP_ERR_INVALID_SIZE;
    }

    s_ready = true;
    ESP_LOGI(TAG, "loaded %u photos from assets partition (format v%u)", s_header.image_count,
             (unsigned)s_header.version);
    return ESP_OK;
}

bool user_assets_ready(void)
{
    return s_ready;
}

uint16_t user_assets_photo_count(void)
{
    return s_ready ? s_header.image_count : 0;
}

uint16_t user_assets_screen_width(void)
{
    return s_ready ? s_header.screen_width : 0;
}

uint16_t user_assets_screen_height(void)
{
    return s_ready ? s_header.screen_height : 0;
}

uint32_t user_assets_rotation_interval_sec(void)
{
    return (s_ready && s_header.rotation_interval_sec > 0) ?
           s_header.rotation_interval_sec : 60;
}

esp_err_t user_assets_get_config(user_assets_config_t *out_config)
{
    if (!s_ready || out_config == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(out_config, 0, sizeof(*out_config));
    copy_field(out_config->ssid, sizeof(out_config->ssid), s_header.ssid,
               sizeof(s_header.ssid));
    copy_field(out_config->password, sizeof(out_config->password), s_header.password,
               sizeof(s_header.password));
    copy_field(out_config->timezone, sizeof(out_config->timezone), s_header.timezone,
               sizeof(s_header.timezone));
    out_config->rotation_interval_sec = user_assets_rotation_interval_sec();
    out_config->asset_id = s_header.asset_id;
    out_config->weather_lat_e6 = s_header.weather_lat_e6;
    out_config->weather_lon_e6 = s_header.weather_lon_e6;

    uint32_t focus = s_header.pomodoro_focus_sec;
    out_config->pomodoro_focus_sec = focus > 0 ? focus : (uint32_t)(25 * 60);

    if (s_header.version >= 2) {
        out_config->pomodoro_short_break_sec = s_header.pomodoro_short_break_sec;
        out_config->pomodoro_long_break_sec = s_header.pomodoro_long_break_sec;
        out_config->pomodoro_long_break_every = s_header.pomodoro_long_break_every;
    }

    if (out_config->pomodoro_short_break_sec == 0) {
        out_config->pomodoro_short_break_sec = 300;
    }
    if (out_config->pomodoro_long_break_sec == 0) {
        out_config->pomodoro_long_break_sec = 900;
    }
    if (out_config->pomodoro_long_break_every == 0) {
        out_config->pomodoro_long_break_every = 4;
    }

    return ESP_OK;
}

esp_err_t user_assets_read_photo_rows(size_t photo_index, uint16_t start_row,
                                      uint16_t row_count, void *out,
                                      size_t out_len)
{
    if (!s_ready || out == NULL || photo_index >= s_header.image_count) {
        return ESP_ERR_INVALID_ARG;
    }
    if (start_row + row_count > s_header.screen_height) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t row_bytes = (size_t)s_header.screen_width * 2;
    size_t read_len = (size_t)row_count * row_bytes;
    if (out_len < read_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t offset = USER_ASSETS_HEADER_SIZE + photo_index * s_header.image_size +
                    (size_t)start_row * row_bytes;
    return esp_partition_read(s_partition, offset, out, read_len);
}
