#pragma once

#include <stdint.h>

/** Fill `pixels` (row-major RGB565) with a weather glyph for Open-Meteo WMO code `wmo_code`.
 *  Unused pixels are set to `0xF81F` (chroma key for sprite blit). */
void weather_icon_render(uint16_t *pixels, int w, int h, int wmo_code);
