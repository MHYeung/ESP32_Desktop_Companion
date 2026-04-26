#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t wifi_time_init(const char *ssid, const char *pass, const char *timezone);
bool wifi_time_is_connected(void);