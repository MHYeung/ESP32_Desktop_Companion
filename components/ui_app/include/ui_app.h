#pragma once
#include "display_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void ui_app_start(esp_lcd_panel_handle_t display, QueueHandle_t btn_queue,
                  uint32_t rotation_interval_sec,
                  uint32_t pomodoro_focus_sec,
                  uint32_t pomodoro_short_break_sec,
                  uint32_t pomodoro_long_break_sec,
                  uint32_t pomodoro_long_break_every,
                  int32_t weather_lat_e6, int32_t weather_lon_e6);
