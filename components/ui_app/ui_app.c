#include "ui_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button_mgr.h"
#include "user_assets.h"
#include "weather_client.h"
#include "wifi_time.h"
#include <time.h>
#include <stdio.h>
#include <stdbool.h>

#define POMODORO_STEP_SEC (5 * 60)
#define POMODORO_MIN_SEC  (5 * 60)
#define POMODORO_MAX_SEC  (99 * 60)

typedef enum {
    OVERLAY_DATETIME,
    OVERLAY_POMODORO,
    OVERLAY_WEATHER,
} overlay_mode_t;

typedef struct {
    esp_lcd_panel_handle_t display;
    QueueHandle_t btn_queue;
    uint32_t rotation_interval_sec;
    uint32_t pomodoro_default_sec;
    int32_t weather_lat_e6;
    int32_t weather_lon_e6;
} ui_app_args_t;

static void format_time(char *out, size_t out_len, const struct tm *timeinfo)
{
    snprintf(out, out_len, "%02d:%02d:%02d", timeinfo->tm_hour,
             timeinfo->tm_min, timeinfo->tm_sec);
}

static void format_date(char *out, size_t out_len, const struct tm *timeinfo, bool wifi_connected)
{
    static const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    snprintf(out, out_len, "%s%s %02d/%02d", wifi_connected ? "" : "*",
             days[timeinfo->tm_wday],
             timeinfo->tm_mon + 1, timeinfo->tm_mday);
}

static void format_mmss(char *out, size_t out_len, uint32_t seconds)
{
    snprintf(out, out_len, "%02lu:%02lu",
             (unsigned long)(seconds / 60),
             (unsigned long)(seconds % 60));
}

static void wmo_short_label(int code, char *out, size_t out_len)
{
    const char *s = "MIXED";
    if (code == 0) {
        s = "CLEAR";
    } else if (code >= 1 && code <= 3) {
        s = "CLOUD";
    } else if (code >= 45 && code <= 48) {
        s = "FOG";
    } else if (code >= 51 && code <= 67) {
        s = "RAIN";
    } else if (code >= 71 && code <= 77) {
        s = "SNOW";
    } else if (code >= 80 && code <= 82) {
        s = "SHWR";
    } else if (code >= 85 && code <= 86) {
        s = "SNSH";
    } else if (code >= 95 && code <= 99) {
        s = "STORM";
    }
    snprintf(out, out_len, "%s", s);
}

static overlay_mode_t next_overlay(overlay_mode_t m)
{
    switch (m) {
    case OVERLAY_DATETIME:
        return OVERLAY_POMODORO;
    case OVERLAY_POMODORO:
        return OVERLAY_WEATHER;
    default:
        return OVERLAY_DATETIME;
    }
}

static void ui_task(void *arg) {
    ui_app_args_t *args = (ui_app_args_t *)arg;
    esp_lcd_panel_handle_t display = args->display;
    QueueHandle_t btn_queue = args->btn_queue;
    uint32_t rotation_interval_sec = args->rotation_interval_sec ?
                                     args->rotation_interval_sec : 60;
    uint32_t pomodoro_duration = args->pomodoro_default_sec ?
                                 args->pomodoro_default_sec : 25 * 60;

    overlay_mode_t overlay = OVERLAY_DATETIME;
    bool redraw_photo = true;
    bool redraw_overlay = true;
    size_t current_photo = 0;

    bool pomodoro_running = false;
    uint32_t pomodoro_remaining = pomodoro_duration;
    TickType_t last_photo_tick = xTaskGetTickCount();
    TickType_t last_second_tick = xTaskGetTickCount();
    overlay_mode_t prev_overlay = OVERLAY_DATETIME;

    while(1) {
        button_event_t event;
        // Wait up to 100ms for a button press. This keeps the UI feeling fast!
        if (xQueueReceive(btn_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event == BTN_EVENT_SHORT_PRESS) {
                overlay = next_overlay(overlay);
                redraw_photo = true;
            } else if (event == BTN_EVENT_DOUBLE_PRESS) {
                if (pomodoro_duration <= POMODORO_MAX_SEC - POMODORO_STEP_SEC) {
                    pomodoro_duration += POMODORO_STEP_SEC;
                    pomodoro_remaining += POMODORO_STEP_SEC;
                }
                overlay = OVERLAY_POMODORO;
                redraw_photo = true;
            } else if (event == BTN_EVENT_TRIPLE_PRESS) {
                if (pomodoro_duration > POMODORO_MIN_SEC) {
                    pomodoro_duration -= POMODORO_STEP_SEC;
                    pomodoro_remaining = (pomodoro_remaining > POMODORO_STEP_SEC) ?
                                         pomodoro_remaining - POMODORO_STEP_SEC :
                                         pomodoro_duration;
                }
                overlay = OVERLAY_POMODORO;
                redraw_photo = true;
            } else if (event == BTN_EVENT_LONG_PRESS) {
                if (pomodoro_running) {
                    pomodoro_running = false;
                    pomodoro_remaining = pomodoro_duration;
                } else {
                    pomodoro_remaining = pomodoro_duration;
                    pomodoro_running = true;
                }
                overlay = OVERLAY_POMODORO;
                redraw_photo = true;
                redraw_overlay = true;
            }
        }

        TickType_t now = xTaskGetTickCount();

        if (user_assets_photo_count() > 0 &&
            now - last_photo_tick >= pdMS_TO_TICKS(rotation_interval_sec * 1000)) {
            current_photo = (current_photo + 1) % user_assets_photo_count();
            last_photo_tick = now;
            redraw_photo = true;
        }

        if (now - last_second_tick >= pdMS_TO_TICKS(1000)) {
            last_second_tick += pdMS_TO_TICKS(1000);
            if (pomodoro_running && pomodoro_remaining > 0) {
                pomodoro_remaining--;
                if (pomodoro_remaining == 0) {
                    pomodoro_running = false;
                    pomodoro_remaining = pomodoro_duration;
                }
            }
            redraw_overlay = true;
        }

        if (overlay == OVERLAY_WEATHER && wifi_time_is_connected()) {
            static TickType_t last_weather_req_ticks;
            if (overlay != prev_overlay ||
                (now - last_weather_req_ticks) >= pdMS_TO_TICKS(600000)) {
                weather_client_request_refresh(args->weather_lat_e6, args->weather_lon_e6);
                last_weather_req_ticks = now;
            }
        }

        if (redraw_photo) {
            if (overlay == OVERLAY_POMODORO || overlay == OVERLAY_WEATHER) {
                display_draw_asset_image_dimmed(display, current_photo, 45);
            } else {
                display_draw_asset_image(display, current_photo);
            }
            redraw_photo = false;
            redraw_overlay = true;
        }

        if (redraw_overlay) {
            char text[18];
            if (overlay == OVERLAY_DATETIME) {
                time_t t;
                struct tm timeinfo;
                time(&t);
                localtime_r(&t, &timeinfo);
                format_time(text, sizeof(text), &timeinfo);
                display_draw_text_on_photo(display, current_photo, 75, text, 4, 0xFFFF);
                format_date(text, sizeof(text), &timeinfo, wifi_time_is_connected());
                display_draw_text_on_photo(display, current_photo, 129, text, 2, 0xFFFF);
            } else if (overlay == OVERLAY_POMODORO) {
                display_draw_text_on_photo_dimmed(display, current_photo, 52, "POMODORO", 2, 0xFFFF,
                                                  45);
                display_draw_text_on_photo_dimmed(display, current_photo, 72, "----------------", 1,
                                                  0xFFFF, 45);
                format_mmss(text, sizeof(text), pomodoro_remaining);
                display_draw_text_on_photo_dimmed(display, current_photo, 108, text, 5, 0xFFFF, 45);
            } else {
                display_draw_text_on_photo_dimmed(display, current_photo, 52, "WEATHER", 2, 0xFFFF,
                                                  45);
                display_draw_text_on_photo_dimmed(display, current_photo, 72, "----------------", 1,
                                                  0xFFFF, 45);
                if (!wifi_time_is_connected()) {
                    display_draw_text_on_photo_dimmed(display, current_photo, 108, "NO WIFI", 3,
                                                      0xFFFF, 45);
                } else {
                    int temp_c = 0, wmo = 0, age = 0;
                    if (weather_client_get_snapshot(&temp_c, &wmo, &age)) {
                        snprintf(text, sizeof(text), "%d C", temp_c);
                        display_draw_text_on_photo_dimmed(display, current_photo, 92, text, 4, 0xFFFF,
                                                          45);
                        wmo_short_label(wmo, text, sizeof(text));
                        display_draw_text_on_photo_dimmed(display, current_photo, 128, text, 2,
                                                          0xFFFF, 45);
                    } else {
                        display_draw_text_on_photo_dimmed(display, current_photo, 108, "LOADING", 3,
                                                          0xFFFF, 45);
                    }
                }
            }
            redraw_overlay = false;
        }

        prev_overlay = overlay;
    }
}

void ui_app_start(esp_lcd_panel_handle_t display, QueueHandle_t btn_queue,
                  uint32_t rotation_interval_sec, uint32_t pomodoro_seconds,
                  int32_t weather_lat_e6, int32_t weather_lon_e6) {
    static ui_app_args_t args;
    args.display = display;
    args.btn_queue = btn_queue;
    args.rotation_interval_sec = rotation_interval_sec;
    args.pomodoro_default_sec = pomodoro_seconds;
    args.weather_lat_e6 = weather_lat_e6;
    args.weather_lon_e6 = weather_lon_e6;
    xTaskCreate(ui_task, "ui_task", 4096, &args, 5, NULL);
}