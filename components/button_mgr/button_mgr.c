#include "button_mgr.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include <stdbool.h>

#define PIN_NUM_BUTTON 1
#define LONG_PRESS_MS 500

static QueueHandle_t btn_queue;

static void button_task(void *arg) {
    gpio_set_direction(PIN_NUM_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_NUM_BUTTON, GPIO_PULLUP_ONLY);

    uint32_t press_time = 0;
    bool is_pressed = false;

    while(1) {
        int state = gpio_get_level(PIN_NUM_BUTTON);

        if (state == 0 && !is_pressed) {
            is_pressed = true;
            press_time = 0;
        } 
        else if (state == 0 && is_pressed) {
            press_time += 50; 
        } 
        else if (state == 1 && is_pressed) {
            is_pressed = false;
            button_event_t ev = (press_time >= LONG_PRESS_MS) ? BTN_EVENT_LONG_PRESS : BTN_EVENT_SHORT_PRESS;
            xQueueSend(btn_queue, &ev, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Poll every 50ms (acts as debouncer)
    }
}

QueueHandle_t button_mgr_init(void) {
    btn_queue = xQueueCreate(5, sizeof(button_event_t));
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
    return btn_queue;
}
