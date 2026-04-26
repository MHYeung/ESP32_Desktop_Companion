#include "button_mgr.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include <stdbool.h>

#define PIN_NUM_BUTTON 1
#define LONG_PRESS_MS 500
#define POLL_MS 50
#define MULTI_TAP_WINDOW_MS 350

static QueueHandle_t btn_queue;

static void button_task(void *arg) {
    gpio_set_direction(PIN_NUM_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_NUM_BUTTON, GPIO_PULLUP_ONLY);

    uint32_t press_time = 0;
    uint32_t release_time = 0;
    uint8_t tap_count = 0;
    bool is_pressed = false;

    while(1) {
        int state = gpio_get_level(PIN_NUM_BUTTON);

        if (state == 0 && !is_pressed) {
            is_pressed = true;
            press_time = 0;
        } 
        else if (state == 0 && is_pressed) {
            press_time += POLL_MS;
        } 
        else if (state == 1 && is_pressed) {
            is_pressed = false;
            if (press_time >= LONG_PRESS_MS) {
                button_event_t ev = BTN_EVENT_LONG_PRESS;
                tap_count = 0;
                release_time = 0;
                xQueueSend(btn_queue, &ev, 0);
            } else if (tap_count < 3) {
                tap_count++;
                release_time = 0;
            }
        } else if (state == 1 && tap_count > 0) {
            release_time += POLL_MS;
            if (release_time >= MULTI_TAP_WINDOW_MS || tap_count == 3) {
                button_event_t ev = BTN_EVENT_SHORT_PRESS;
                if (tap_count == 2) ev = BTN_EVENT_DOUBLE_PRESS;
                if (tap_count >= 3) ev = BTN_EVENT_TRIPLE_PRESS;
                tap_count = 0;
                release_time = 0;
                xQueueSend(btn_queue, &ev, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS)); // Polling also acts as a simple debouncer.
    }
}

QueueHandle_t button_mgr_init(void) {
    btn_queue = xQueueCreate(5, sizeof(button_event_t));
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
    return btn_queue;
}
