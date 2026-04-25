#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Include our custom modules
#include "display_hal.h"
#include "button_mgr.h"
#include "wifi_time.h"
#include "ui_app.h"

void app_main(void)
{
    printf("--- Desktop Companion Booting ---\n");

    // 1. Initialize Screen & Storage
    esp_lcd_panel_handle_t display = display_hal_init();

    // 2. Initialize Button (Gets the FreeRTOS event queue)
    QueueHandle_t btn_queue = button_mgr_init();

    // 3. Connect to WiFi and fetch NTP time
    // !!! IMPORTANT: CHANGE THESE TO YOUR WIFI CREDENTIALS !!!
    printf("Connecting to WiFi...\n");
    wifi_time_init("ran_gen_gang", "coinplusfire");

    // 4. Start the Application UI Loop
    ui_app_start(display, btn_queue);
}