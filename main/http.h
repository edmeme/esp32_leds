#pragma once
#include "rgb.h"
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>


// ToDo: change functions that receive a queue for functions that
// receive a function that in turns queues events, its a lot more flexible than
// hacky id's.
static const uint32_t WEB_COLOR_EVID = 0x177EB011;

typedef struct {
  uint32_t id0;
  rgb_t color;
} web_color_event_t;

/**
 * color points to a structure that holds the current rgb state of the leds.
 * The structure is read over time, so it needs to be in memory and up-to-date
 * queue  Is the queue that receives events from the web.
 */
void init_httpd(const rgb_t * color, QueueHandle_t queue);
