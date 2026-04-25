#pragma once
#include "display_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Starts the UI State Machine task
void ui_app_start(esp_lcd_panel_handle_t display, QueueHandle_t btn_queue);