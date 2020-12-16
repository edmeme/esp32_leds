#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <esp_log.h>

#include "rotary_encoder.h"
#include "button.h"
#include "rgb.h"

#define TAG "LED"

#define BUTTON_GPIO (CONFIG_BUTTON_GPIO)
#define ROT_ENC_A_GPIO (CONFIG_ROT_ENC_A_GPIO)
#define ROT_ENC_B_GPIO (CONFIG_ROT_ENC_B_GPIO)
#define RED_GPIO (CONFIG_RGB_RED)
#define GREEN_GPIO (CONFIG_RGB_GREEN)
#define BLUE_GPIO (CONFIG_RGB_BLUE)

#define ENABLE_HALF_STEPS true  // true: Full resol. but worse error recovery

enum mode {
  MODE_HUE,
  MODE_SAT,
  MODE_VALUE
};

typedef struct {
  short hue;
  short sat;
  short value;
  
  int cursor_mode;
} state_t;

typedef struct {
  rotary_encoder_info_t encoder;
  button_info_t button;
  QueueHandle_t queue;
} input_t;

void setup_input(input_t * input)
{
  // esp32-rotary-encoder and button require that the GPIO ISR service
  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  
  // Initialise the rotary encoder device with the GPIOs for A and B signals
  ESP_ERROR_CHECK(rotary_encoder_init(&input->encoder,
				      ROT_ENC_A_GPIO, ROT_ENC_B_GPIO));
  ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&input->encoder,
						   ENABLE_HALF_STEPS));
  ESP_ERROR_CHECK(rotary_encoder_flip_direction(&input->encoder));

  // Initialise the button
  ESP_ERROR_CHECK(button_init(&input->button, BUTTON_GPIO));
  
  // queue
  input->queue = rotary_encoder_create_queue();
  ESP_ERROR_CHECK(rotary_encoder_set_queue(&input->encoder,
					   input->queue));
  ESP_ERROR_CHECK(button_set_queue(&input->button,
				   input->queue));
  
}

esp_err_t unsetup_input(input_t * input){
  ESP_ERROR_CHECK(rotary_encoder_uninit(&input->encoder));
  return 0;
}

void handle_input(input_t * input, state_t * state) {
  // Wait for incoming events on the event queue.

  union {
    rotary_encoder_event_t re;
    button_event_t bt;
  } event;
  if (xQueueReceive(input->encoder.queue, &event,
		    1000/portTICK_PERIOD_MS) == pdTRUE){
    if(event.bt.id0 == BUTTON_EVID){
      ESP_LOGI(TAG, "Event: button");
    }else{
      state->value = event.re.state.position & 0xff;
      ESP_LOGI(TAG, "Event: position %d, direction %s",
	       event.re.state.position,
	       event.re.state.direction ? (event.re.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET");
    }
  }
  
}

void app_main()
{
  state_t state = {0};
  rgb_t color = {128,128,128};
  rgb_init(RED_GPIO, GREEN_GPIO, BLUE_GPIO);
  rgb_set(&color);
  
  input_t input = {0};
  setup_input(&input);
  while (1) {
    handle_input(&input, &state);
    color.r = color.g = color.b = state.value;
    rgb_set(&color);
  }
  ESP_LOGE(TAG, "queue receive failed");  
  ESP_ERROR_CHECK(unsetup_input(&input));
}

