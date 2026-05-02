#pragma once

#include <stdbool.h>
#include <stdint.h>

void geo_client_init(void);

/** Non-blocking HTTPS lookup (no-op if a request is already running). */
void geo_client_request_refresh(void);

/** Last successful lat/lon in microdegrees (e.g. 37.7749° → 37774900). */
bool geo_client_get_location(int32_t *lat_e6, int32_t *lon_e6);

/** True after at least one lookup finished and none is in flight (coords may still be unavailable). */
bool geo_client_is_lookup_settled(void);
