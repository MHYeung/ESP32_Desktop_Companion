#include "ui_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button_mgr.h"
#include <time.h>
#include <stdio.h>

typedef enum { MODE_IMAGE, MODE_CLOCK, MODE_POMO } app_mode_t;

static void ui_task(void *arg) {
    void **args = (void **)arg;
    esp_lcd_panel_handle_t display = (esp_lcd_panel_handle_t)args[0];
    QueueHandle_t btn_queue = (QueueHandle_t)args[1];

    app_mode_t current_mode = MODE_IMAGE;
    bool force_redraw = true;

    // Timer tracking
    bool pomo_running = false;
    int pomo_seconds = 25 * 60;
    
    TickType_t last_clock_time = xTaskGetTickCount();
    TickType_t last_pomo_time = xTaskGetTickCount();

    while(1) {
        button_event_t event;
        // Wait up to 100ms for a button press. This keeps the UI feeling fast!
        if (xQueueReceive(btn_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event == BTN_EVENT_SHORT_PRESS) {
                current_mode = (current_mode + 1) % 3; 
                force_redraw = true;
            } else if (event == BTN_EVENT_LONG_PRESS && current_mode == MODE_POMO) {
                pomo_running = !pomo_running; 
                if(pomo_seconds == 0) pomo_seconds = 25 * 60; 
            }
        }

        TickType_t now = xTaskGetTickCount();

        // =====================================
        // MODE 0: IMAGE
        // =====================================
        if (current_mode == MODE_IMAGE) {
            if (force_redraw) {
                display_draw_bin_image(display, "/littlefs/hk100.bin");
                force_redraw = false;
            }
        } 
        
        // =====================================
        // MODE 1: CLOCK
        // =====================================
        else if (current_mode == MODE_CLOCK) {
            bool update_text = false;

            if (force_redraw) {
                display_fill_rect(display, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0x0000); 
                force_redraw = false;
                update_text = true;
                last_clock_time = now; 
            }
            
            // Only update time text exactly once every 1000ms
            if (now - last_clock_time >= pdMS_TO_TICKS(1000)) {
                last_clock_time += pdMS_TO_TICKS(1000);
                update_text = true;
            }

            if (update_text) {
                time_t t; struct tm timeinfo;
                time(&t); localtime_r(&t, &timeinfo);
                char time_str[16];
                snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                display_draw_text(display, 100, time_str, 3, 0xFFFF, 0x0000);
            }
        } 
        
        // =====================================
        // MODE 2: POMODORO
        // =====================================
        else if (current_mode == MODE_POMO) {
            bool update_text = false;

            if (force_redraw) {
                display_fill_rect(display, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0xF800); 
                force_redraw = false;
                update_text = true;
                last_pomo_time = now; 
            }
            
            // Real-time tracking
            if (pomo_running && pomo_seconds > 0) {
                if (now - last_pomo_time >= pdMS_TO_TICKS(1000)) {
                    pomo_seconds--;
                    last_pomo_time += pdMS_TO_TICKS(1000); // Strict 1s subtraction
                    update_text = true;
                }
            } else {
                last_pomo_time = now; // Keep the timer base synced while paused
            }
            
            if (update_text) {
                char pomo_str[16];
                snprintf(pomo_str, sizeof(pomo_str), "%02d:%02d", pomo_seconds / 60, pomo_seconds % 60);
                display_draw_text(display, 100, pomo_str, 5, 0xFFFF, 0xF800);
            }
        }
    }
}

void ui_app_start(esp_lcd_panel_handle_t display, QueueHandle_t btn_queue) {
    static void *args[2];
    args[0] = display;
    args[1] = btn_queue;
    xTaskCreate(ui_task, "ui_task", 4096, args, 5, NULL);
}