#include "ui_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button_mgr.h"
#include "user_assets.h"
#include <time.h>
#include <stdio.h>
#include <stdbool.h>

typedef enum {
    OVERLAY_DATETIME,
    OVERLAY_COUNTDOWN,
    OVERLAY_STOPWATCH,
} overlay_mode_t;

typedef struct {
    esp_lcd_panel_handle_t display;
    QueueHandle_t btn_queue;
    uint32_t rotation_interval_sec;
    uint32_t countdown_default_sec;
} ui_app_args_t;

static void format_datetime(char *out, size_t out_len)
{
    time_t t;
    struct tm timeinfo;
    time(&t);
    localtime_r(&t, &timeinfo);
    snprintf(out, out_len, "%02d-%02d %02d:%02d",
             timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min);
}

static void format_hms(char *out, size_t out_len, uint32_t seconds)
{
    snprintf(out, out_len, "%02lu:%02lu:%02lu",
             (unsigned long)(seconds / 3600),
             (unsigned long)((seconds / 60) % 60),
             (unsigned long)(seconds % 60));
}

static void ui_task(void *arg) {
    ui_app_args_t *args = (ui_app_args_t *)arg;
    esp_lcd_panel_handle_t display = args->display;
    QueueHandle_t btn_queue = args->btn_queue;
    uint32_t rotation_interval_sec = args->rotation_interval_sec ?
                                     args->rotation_interval_sec : 60;
    uint32_t countdown_default = args->countdown_default_sec ?
                                 args->countdown_default_sec : 5 * 60;

    overlay_mode_t overlay = OVERLAY_DATETIME;
    bool redraw_photo = true;
    bool redraw_overlay = true;
    size_t current_photo = 0;

    bool countdown_running = false;
    bool stopwatch_running = false;
    uint32_t countdown_seconds = countdown_default;
    uint32_t stopwatch_seconds = 0;
    TickType_t last_photo_tick = xTaskGetTickCount();
    TickType_t last_second_tick = xTaskGetTickCount();

    while(1) {
        button_event_t event;
        // Wait up to 100ms for a button press. This keeps the UI feeling fast!
        if (xQueueReceive(btn_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event == BTN_EVENT_SHORT_PRESS) {
                overlay = (overlay + 1) % 3;
                redraw_photo = true;
            } else if (event == BTN_EVENT_LONG_PRESS) {
                if (overlay == OVERLAY_COUNTDOWN) {
                    if (countdown_seconds == 0) countdown_seconds = countdown_default;
                    countdown_running = !countdown_running;
                } else if (overlay == OVERLAY_STOPWATCH) {
                    stopwatch_running = !stopwatch_running;
                } else if (user_assets_photo_count() > 0) {
                    current_photo = (current_photo + 1) % user_assets_photo_count();
                    redraw_photo = true;
                }
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
            if (countdown_running && countdown_seconds > 0) countdown_seconds--;
            if (stopwatch_running) stopwatch_seconds++;
            redraw_overlay = true;
        }

        if (redraw_photo) {
            display_draw_asset_image(display, current_photo);
            redraw_photo = false;
            redraw_overlay = true;
        }

        if (redraw_overlay) {
            char text[20];
            if (overlay == OVERLAY_DATETIME) {
                format_datetime(text, sizeof(text));
                display_draw_text_on_photo(display, current_photo, 104, text, 2, 0xFFFF);
            } else if (overlay == OVERLAY_COUNTDOWN) {
                format_hms(text, sizeof(text), countdown_seconds);
                display_draw_text_on_photo(display, current_photo, 100, text, 3, 0xFFFF);
            } else {
                format_hms(text, sizeof(text), stopwatch_seconds);
                display_draw_text_on_photo(display, current_photo, 100, text, 3, 0xFFFF);
            }
            redraw_overlay = false;
        }
    }
}

void ui_app_start(esp_lcd_panel_handle_t display, QueueHandle_t btn_queue,
                  uint32_t rotation_interval_sec, uint32_t countdown_seconds) {
    static ui_app_args_t args;
    args.display = display;
    args.btn_queue = btn_queue;
    args.rotation_interval_sec = rotation_interval_sec;
    args.countdown_default_sec = countdown_seconds;
    xTaskCreate(ui_task, "ui_task", 4096, &args, 5, NULL);
}