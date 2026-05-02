#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/** Starts Wi‑Fi station + SNTP. Clock uses UTC until IP geo updates TZ. */
esp_err_t wifi_time_init(const char *ssid, const char *pass);

/** Apply TZ from ipapi-style offset: local_time = UTC + utc_offset_sec (DST reflected when API updates). */
void wifi_time_set_tz_from_utc_offset_sec(int32_t utc_offset_sec);

bool wifi_time_is_connected(void);
