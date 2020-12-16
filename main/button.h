#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <driver/rtc_io.h>


typedef struct {
  gpio_num_t pin;
  QueueHandle_t queue;
} button_info_t;

typedef struct {
  int32_t id0;
  int32_t id1;
} button_event_t;

#define BUTTON_EVID (0xAAFEC012)

esp_err_t button_init(button_info_t * info, gpio_num_t pin);
esp_err_t button_uninit(button_info_t * info);
QueueHandle_t button_create_queue(void);
esp_err_t button_set_queue(button_info_t * info, QueueHandle_t queue);
