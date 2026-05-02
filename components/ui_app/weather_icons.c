#include "weather_icons.h"

#include <stddef.h>

#define KEY 0xF81F

#define C_SUN   0xFFE0
#define C_SUN_D 0xFD20
#define C_CLOUD 0x8C71
#define C_CLOUD_D 0x632C
#define C_RAIN  0x051D
#define C_FOG   0xB596
#define C_SNOW  0xEF7D
#define C_BOLT  0xFFE0

static void fill_key(uint16_t *p, int n)
{
    for (int i = 0; i < n; i++) {
        p[i] = KEY;
    }
}

static void pix(uint16_t *buf, int W, int H, int x, int y, uint16_t c)
{
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H && c != KEY) {
        buf[y * W + x] = c;
    }
}

static void fill_circle(uint16_t *buf, int W, int H, int cx, int cy, int r, uint16_t c)
{
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r + r) {
                pix(buf, W, H, cx + dx, cy + dy, c);
            }
        }
    }
}

static void fill_rect(uint16_t *buf, int W, int H, int x0, int y0, int x1, int y1, uint16_t c)
{
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            pix(buf, W, H, x, y, c);
        }
    }
}

static void icon_clear(uint16_t *buf, int W, int H)
{
    static const int8_t rays[8][2] = {
        {0, -14}, {10, -10}, {14, 0}, {10, 10},
        {0, 14}, {-10, 10}, {-14, 0}, {-10, -10},
    };
    fill_key(buf, W * H);
    fill_circle(buf, W, H, W / 2, H / 2 - 2, H / 5 + 2, C_SUN);
    for (size_t i = 0; i < 8; i++) {
        fill_circle(buf, W, H, W / 2 + rays[i][0], H / 2 - 2 + rays[i][1], 2, C_SUN_D);
    }
}

static void cloud_base(uint16_t *buf, int W, int H)
{
    fill_circle(buf, W, H, W / 4, H / 2, H / 6, C_CLOUD);
    fill_circle(buf, W, H, W / 2, H / 2 - 3, H / 5, C_CLOUD);
    fill_circle(buf, W, H, 3 * W / 4, H / 2, H / 6, C_CLOUD);
    fill_rect(buf, W, H, W / 8, H / 2, 7 * W / 8, H / 2 + H / 10, C_CLOUD);
}

static void icon_cloudy(uint16_t *buf, int W, int H)
{
    fill_key(buf, W * H);
    cloud_base(buf, W, H);
}

static void icon_partly(uint16_t *buf, int W, int H)
{
    fill_key(buf, W * H);
    fill_circle(buf, W, H, W / 5, H / 4, H / 8, C_SUN);
    cloud_base(buf, W, H);
}

static void icon_fog(uint16_t *buf, int W, int H)
{
    fill_key(buf, W * H);
    for (int band = 0; band < 5; band++) {
        int yy = H / 4 + band * (H / 10);
        fill_rect(buf, W, H, W / 10, yy, 9 * W / 10, yy + H / 22, C_FOG);
    }
}

static void icon_rain(uint16_t *buf, int W, int H)
{
    fill_key(buf, W * H);
    cloud_base(buf, W, H);
    for (int i = 0; i < 9; i++) {
        int x = W / 8 + (i % 3) * (W / 4);
        int y0 = H / 2 + H / 12 + (i / 3) * (H / 14);
        fill_rect(buf, W, H, x, y0, x + 1, y0 + H / 8, C_RAIN);
    }
}

static void icon_shower(uint16_t *buf, int W, int H)
{
    icon_rain(buf, W, H);
}

static void icon_snow(uint16_t *buf, int W, int H)
{
    fill_key(buf, W * H);
    cloud_base(buf, W, H);
    for (int i = 0; i < 12; i++) {
        int x = W / 10 + (i * 17) % (8 * W / 10);
        int y = H / 2 + H / 14 + (i * 11) % (H / 3);
        fill_circle(buf, W, H, x, y, 2, C_SNOW);
    }
}

static void icon_storm(uint16_t *buf, int W, int H)
{
    fill_key(buf, W * H);
    cloud_base(buf, W, H);
    fill_rect(buf, W, H, W / 2 - 2, H / 2 + H / 14, W / 2 + 2, H - H / 8, C_BOLT);
    fill_rect(buf, W, H, W / 2 + 2, H / 2 + H / 8, W / 2 + 8, H / 2 + H / 6, C_BOLT);
    fill_rect(buf, W, H, W / 2 - 8, H / 2 + H / 6, W / 2 - 2, H / 2 + H / 5, C_BOLT);
}

static void icon_mixed(uint16_t *buf, int W, int H)
{
    icon_cloudy(buf, W, H);
}

void weather_icon_render(uint16_t *pixels, int w, int h, int wmo_code)
{
    if (!pixels || w < 16 || h < 16) {
        return;
    }

    int code = wmo_code;
    if (code == 0) {
        icon_clear(pixels, w, h);
    } else if (code >= 1 && code <= 3) {
        if (code == 1) {
            icon_partly(pixels, w, h);
        } else {
            icon_cloudy(pixels, w, h);
        }
    } else if (code >= 45 && code <= 48) {
        icon_fog(pixels, w, h);
    } else if (code >= 51 && code <= 67) {
        icon_rain(pixels, w, h);
    } else if (code >= 71 && code <= 77) {
        icon_snow(pixels, w, h);
    } else if (code >= 80 && code <= 82) {
        icon_shower(pixels, w, h);
    } else if (code >= 85 && code <= 86) {
        icon_snow(pixels, w, h);
    } else if (code >= 95 && code <= 99) {
        icon_storm(pixels, w, h);
    } else {
        icon_mixed(pixels, w, h);
    }
}
