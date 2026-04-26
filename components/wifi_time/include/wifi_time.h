#pragma once

#include "esp_err.h"

esp_err_t wifi_time_init(const char *ssid, const char *pass, const char *timezone);