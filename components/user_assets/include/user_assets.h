#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define USER_ASSETS_MAGIC "DCAS"
/** Written by current web flasher; reader accepts v1 for migration. */
#define USER_ASSETS_FORMAT_VERSION 2
#define USER_ASSETS_HEADER_SIZE 256
#define USER_ASSETS_MAX_SSID_LEN 32
#define USER_ASSETS_MAX_PASSWORD_LEN 64
#define USER_ASSETS_MAX_TIMEZONE_LEN 63

typedef struct {
    char ssid[USER_ASSETS_MAX_SSID_LEN + 1];
    char password[USER_ASSETS_MAX_PASSWORD_LEN + 1];
    char timezone[USER_ASSETS_MAX_TIMEZONE_LEN + 1];
    uint32_t rotation_interval_sec;
    uint32_t pomodoro_focus_sec;
    uint32_t pomodoro_short_break_sec;
    uint32_t pomodoro_long_break_sec;
    uint32_t pomodoro_long_break_every;
    uint32_t asset_id;
    int32_t weather_lat_e6;
    int32_t weather_lon_e6;
} user_assets_config_t;

esp_err_t user_assets_init(void);
bool user_assets_ready(void);
uint16_t user_assets_photo_count(void);
uint16_t user_assets_screen_width(void);
uint16_t user_assets_screen_height(void);
uint32_t user_assets_rotation_interval_sec(void);
esp_err_t user_assets_get_config(user_assets_config_t *out_config);
esp_err_t user_assets_read_photo_rows(size_t photo_index, uint16_t start_row,
                                      uint16_t row_count, void *out,
                                      size_t out_len);
