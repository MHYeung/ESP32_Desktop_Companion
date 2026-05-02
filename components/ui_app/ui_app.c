#include "ui_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button_mgr.h"
#include "geo_client.h"
#include "user_assets.h"
#include "weather_icons.h"
#include "weather_client.h"
#include "wifi_time.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define POMODORO_STEP_SEC (5 * 60)
#define POMODORO_MIN_SEC  (5 * 60)
#define POMODORO_MAX_SEC  (99 * 60)

#define WEATHER_ICON_SIZE 32

static uint16_t s_weather_icon_px[40 * 40];

typedef enum {
    OVERLAY_DATETIME,
    OVERLAY_POMODORO,
    OVERLAY_WEATHER,
} overlay_mode_t;

typedef enum {
    POMO_PHASE_FOCUS,
    POMO_PHASE_SHORT_BREAK,
    POMO_PHASE_LONG_BREAK,
} pomodoro_phase_t;

typedef struct {
    esp_lcd_panel_handle_t display;
    QueueHandle_t btn_queue;
    uint32_t rotation_interval_sec;
    uint32_t pomodoro_focus_sec;
    uint32_t pomodoro_short_break_sec;
    uint32_t pomodoro_long_break_sec;
    uint32_t pomodoro_long_break_every;
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

static void weather_resolve_refresh(ui_app_args_t *args)
{
    int32_t lat = 0;
    int32_t lon = 0;
    if (geo_client_get_location(&lat, &lon)) {
        weather_client_request_refresh(lat, lon);
        return;
    }
    if (args->weather_lat_e6 != 0 || args->weather_lon_e6 != 0) {
        weather_client_request_refresh(args->weather_lat_e6, args->weather_lon_e6);
    }
}

static uint32_t long_every_clamped(uint32_t every)
{
    return every > 0 ? every : 4;
}

static void ui_task(void *arg)
{
    ui_app_args_t *args = (ui_app_args_t *)arg;
    esp_lcd_panel_handle_t display = args->display;
    QueueHandle_t btn_queue = args->btn_queue;
    uint32_t rotation_interval_sec = args->rotation_interval_sec ?
                                     args->rotation_interval_sec : 60;
    uint32_t focus_sec = args->pomodoro_focus_sec ? args->pomodoro_focus_sec : 25 * 60;
    uint32_t short_sec = args->pomodoro_short_break_sec ? args->pomodoro_short_break_sec : 300;
    uint32_t long_sec = args->pomodoro_long_break_sec ? args->pomodoro_long_break_sec : 900;
    uint32_t long_every = long_every_clamped(args->pomodoro_long_break_every);

    overlay_mode_t overlay = OVERLAY_DATETIME;
    bool redraw_photo = true;
    bool redraw_overlay = true;
    size_t current_photo = 0;

    pomodoro_phase_t pomo_phase = POMO_PHASE_FOCUS;
    bool pomodoro_running = false;
    uint32_t pomodoro_remaining = focus_sec;
    uint32_t completed_focus_sessions = 0;
    TickType_t last_photo_tick = xTaskGetTickCount();
    TickType_t last_second_tick = xTaskGetTickCount();
    overlay_mode_t prev_overlay = OVERLAY_DATETIME;
    TickType_t last_weather_req_ticks = 0;
    TickType_t last_geo_retry_ticks = 0;

    while (1) {
        button_event_t event;
        if (xQueueReceive(btn_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event == BTN_EVENT_SHORT_PRESS) {
                overlay = next_overlay(overlay);
                redraw_photo = true;
            } else if (event == BTN_EVENT_DOUBLE_PRESS) {
                if (focus_sec <= POMODORO_MAX_SEC - POMODORO_STEP_SEC) {
                    focus_sec += POMODORO_STEP_SEC;
                    if (pomo_phase == POMO_PHASE_FOCUS) {
                        pomodoro_remaining += POMODORO_STEP_SEC;
                    }
                }
                overlay = OVERLAY_POMODORO;
                redraw_photo = true;
            } else if (event == BTN_EVENT_TRIPLE_PRESS) {
                if (focus_sec > POMODORO_MIN_SEC + POMODORO_STEP_SEC - 1) {
                    focus_sec -= POMODORO_STEP_SEC;
                    if (pomo_phase == POMO_PHASE_FOCUS) {
                        if (pomodoro_remaining > POMODORO_STEP_SEC) {
                            pomodoro_remaining -= POMODORO_STEP_SEC;
                        } else {
                            pomodoro_remaining = focus_sec;
                        }
                    }
                }
                overlay = OVERLAY_POMODORO;
                redraw_photo = true;
            } else if (event == BTN_EVENT_LONG_PRESS) {
                pomodoro_running = !pomodoro_running;
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

        if (overlay == OVERLAY_WEATHER && wifi_time_is_connected()) {
            int32_t geo_lat = 0, geo_lon = 0;
            bool has_geo_fix = geo_client_get_location(&geo_lat, &geo_lon);
            if (!has_geo_fix && geo_client_is_lookup_settled() &&
                now - last_geo_retry_ticks >= pdMS_TO_TICKS(600000)) {
                geo_client_request_refresh();
                last_geo_retry_ticks = now;
            }
            if (overlay != prev_overlay ||
                (now - last_weather_req_ticks) >= pdMS_TO_TICKS(600000)) {
                weather_resolve_refresh(args);
                last_weather_req_ticks = now;
            }
        }

        if (now - last_second_tick >= pdMS_TO_TICKS(1000)) {
            last_second_tick += pdMS_TO_TICKS(1000);
            if (pomodoro_running && pomodoro_remaining > 0) {
                pomodoro_remaining--;
                if (pomodoro_remaining == 0) {
                    if (pomo_phase == POMO_PHASE_FOCUS) {
                        completed_focus_sessions++;
                        if (completed_focus_sessions % long_every == 0) {
                            pomo_phase = POMO_PHASE_LONG_BREAK;
                            pomodoro_remaining = long_sec;
                        } else {
                            pomo_phase = POMO_PHASE_SHORT_BREAK;
                            pomodoro_remaining = short_sec;
                        }
                    } else {
                        pomo_phase = POMO_PHASE_FOCUS;
                        pomodoro_remaining = focus_sec;
                    }
                }
            }
            redraw_overlay = true;
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
                display_draw_text_on_photo_dimmed(display, current_photo, 44, "POMODORO", 2, 0xFFFF,
                                                  45);
                const char *phase_label = "WORK";
                if (pomo_phase == POMO_PHASE_SHORT_BREAK) {
                    phase_label = "BREAK";
                } else if (pomo_phase == POMO_PHASE_LONG_BREAK) {
                    phase_label = "LONG BRK";
                }
                display_draw_text_on_photo_dimmed(display, current_photo, 62, phase_label, 2, 0xFFFF,
                                                  45);
                display_draw_text_on_photo_dimmed(display, current_photo, 82, "----------------", 1,
                                                  0xFFFF, 45);
                format_mmss(text, sizeof(text), pomodoro_remaining);
                display_draw_text_on_photo_dimmed(display, current_photo, 114, text, 5, 0xFFFF, 45);
            } else {
                display_draw_text_on_photo_dimmed(display, current_photo, 44, "WEATHER", 2, 0xFFFF,
                                                  45);
                display_draw_text_on_photo_dimmed(display, current_photo, 58, "----------------", 1,
                                                  0xFFFF, 45);
                if (!wifi_time_is_connected()) {
                    display_draw_text_on_photo_dimmed(display, current_photo, 108, "NO WIFI", 3,
                                                      0xFFFF, 45);
                } else {
                    int32_t wlat = 0, wlon = 0;
                    bool has_geo_fix = geo_client_get_location(&wlat, &wlon);
                    bool have_coords = has_geo_fix || args->weather_lat_e6 != 0 ||
                                       args->weather_lon_e6 != 0;
                    int temp_c = 0, wmo = 0, age = 0;
                    bool have_wx = weather_client_get_snapshot(&temp_c, &wmo, &age);

                    if (have_wx) {
                        weather_icon_render(s_weather_icon_px, WEATHER_ICON_SIZE, WEATHER_ICON_SIZE,
                                            wmo);
                        display_draw_sprite_on_photo_dimmed(
                            display, current_photo, (LCD_WIDTH - WEATHER_ICON_SIZE) / 2, 68,
                            WEATHER_ICON_SIZE, WEATHER_ICON_SIZE, s_weather_icon_px, 45);
                        snprintf(text, sizeof(text), "%d C", temp_c);
                        display_draw_text_on_photo_dimmed(display, current_photo, 106, text, 4,
                                                          0xFFFF, 45);
                        wmo_short_label(wmo, text, sizeof(text));
                        display_draw_text_on_photo_dimmed(display, current_photo, 142, text, 2,
                                                          0xFFFF, 45);
                    } else if (!have_coords && geo_client_is_lookup_settled()) {
                        display_draw_text_on_photo_dimmed(display, current_photo, 108, "NO LOC", 3,
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
                  uint32_t rotation_interval_sec,
                  uint32_t pomodoro_focus_sec,
                  uint32_t pomodoro_short_break_sec,
                  uint32_t pomodoro_long_break_sec,
                  uint32_t pomodoro_long_break_every,
                  int32_t weather_lat_e6, int32_t weather_lon_e6)
{
    static ui_app_args_t args;
    args.display = display;
    args.btn_queue = btn_queue;
    args.rotation_interval_sec = rotation_interval_sec;
    args.pomodoro_focus_sec = pomodoro_focus_sec;
    args.pomodoro_short_break_sec = pomodoro_short_break_sec;
    args.pomodoro_long_break_sec = pomodoro_long_break_sec;
    args.pomodoro_long_break_every = pomodoro_long_break_every;
    args.weather_lat_e6 = weather_lat_e6;
    args.weather_lon_e6 = weather_lon_e6;
    xTaskCreate(ui_task, "ui_task", 4096, &args, 5, NULL);
}
