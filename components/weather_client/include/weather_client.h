#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

void weather_client_init(void);

/** Start a background HTTPS fetch (no-op if Wi-Fi is not up or a fetch is already running). */
void weather_client_request_refresh(int32_t lat_e6, int32_t lon_e6);

/** Snapshot of last successful fetch; returns false if never fetched successfully. */
bool weather_client_get_snapshot(int *temp_c, int *wmo_code, int *age_sec);
