#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    BTN_EVENT_SHORT_PRESS,
    BTN_EVENT_LONG_PRESS
} button_event_t;

// Initializes the button and returns a queue you can read events from
QueueHandle_t button_mgr_init(void);