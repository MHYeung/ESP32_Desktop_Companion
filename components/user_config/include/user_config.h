#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "user_assets.h"

typedef struct {
    char ssid[USER_ASSETS_MAX_SSID_LEN + 1];
    char password[USER_ASSETS_MAX_PASSWORD_LEN + 1];
    char timezone[USER_ASSETS_MAX_TIMEZONE_LEN + 1];
    uint32_t rotation_interval_sec;
    uint32_t countdown_seconds;
} user_config_t;

esp_err_t user_config_load(user_config_t *out_config);
