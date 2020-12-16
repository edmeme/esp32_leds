#include "button.h"

#include "esp_log.h"
#include "driver/gpio.h"

#define TAG "button"

#define EVENT_QUEUE_LENGTH 1

static void _isr_button(void * args)
{
  button_info_t * info = (button_info_t *)args;
  button_event_t queue_event = {BUTTON_EVID,BUTTON_EVID};
  BaseType_t task_woken = pdFALSE;
  xQueueOverwriteFromISR(info->queue, &queue_event, &task_woken);
  if (task_woken){
    portYIELD_FROM_ISR();
  }
}

esp_err_t button_init(button_info_t * info, gpio_num_t pin)
{
  esp_err_t err = ESP_OK;
  if (info){
    info->pin = pin;
    
    // configure GPIOs
    gpio_pad_select_gpio(info->pin);
    gpio_set_pull_mode(info->pin, GPIO_FLOATING);
    gpio_set_direction(info->pin, GPIO_MODE_INPUT);
    gpio_set_intr_type(info->pin, GPIO_INTR_NEGEDGE);
    
    // install interrupt handlers
    gpio_isr_handler_add(info->pin, _isr_button, info);
  } else {
    ESP_LOGE(TAG, "info is NULL");
    err = ESP_ERR_INVALID_ARG;
  }
  return err;
}

esp_err_t button_uninit(button_info_t * info)
{
  esp_err_t err = ESP_OK;
  if (info){
    gpio_isr_handler_remove(info->pin);
  }else{
    ESP_LOGE(TAG, "info is NULL");
    err = ESP_ERR_INVALID_ARG;
  }
  return err;
}

QueueHandle_t button_create_queue(void)
{
  return xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(button_event_t));
}

esp_err_t button_set_queue(button_info_t * info, QueueHandle_t queue)
{
  esp_err_t err = ESP_OK;
  if (info){
      info->queue = queue;
  } else {
    ESP_LOGE(TAG, "info is NULL");
    err = ESP_ERR_INVALID_ARG;
  }
  return err;
}

