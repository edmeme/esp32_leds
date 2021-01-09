#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <esp_log.h>

#include "rotary_encoder.h"
#include "button.h"
#include "rgb.h"
#include "wifi.h"
#include "http.h"
#include "storage.h"

#define TAG "LED"

#define BUTTON_GPIO (CONFIG_BUTTON_GPIO)
#define ROT_ENC_A_GPIO (CONFIG_ROT_ENC_A_GPIO)
#define ROT_ENC_B_GPIO (CONFIG_ROT_ENC_B_GPIO)
#define RED_GPIO (CONFIG_RGB_RED)
#define GREEN_GPIO (CONFIG_RGB_GREEN)
#define BLUE_GPIO (CONFIG_RGB_BLUE)

typedef persistent_state_t state_t; // All state is persistent state.

static const bool ENABLE_HALF_STEPS = true; // true: Full resol. encoder, worse error recovery
static const uint32_t DEBOUNCE_TIME = 400;   // in milliseconds

static inline int max(int a, int b){
  return (a > b) ? a : b;
}

static inline int min(int a, int b){
  return (a < b) ? a : b;
}

enum mode {
  MODE_HUE = 0,
  MODE_SAT,
  MODE_VALUE
};

typedef struct {
  rotary_encoder_info_t encoder;
  button_info_t button;
  QueueHandle_t queue; 
  int encoder_ref; // The encoder value is never reset, we use this to keep track of its delta.
  uint32_t btn_last_time; // Last time the button was pressed, for shitty debounce
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

static const char * const _color_fields[] = {"hue","sat","value"};

// Handles physical interface, returns true if something is updated.
bool handle_input(input_t * input, state_t * state) {
  union {
    rotary_encoder_event_t re;
    button_event_t bt;
    web_color_event_t web;
  } event;
  if (xQueueReceive(input->encoder.queue, &event,
		    1000/portTICK_PERIOD_MS) == pdTRUE){
    if(event.bt.id0 == BUTTON_EVID){
      // Ask/Google debouncing if you are looking at this code
      uint32_t now = esp_log_timestamp();
      if(now - input->btn_last_time > DEBOUNCE_TIME) {
	input->btn_last_time = now;
	
	state->cursor_mode = state->cursor_mode + 1;
	if(state->cursor_mode > 2) state->cursor_mode = 0;
	ESP_LOGI(TAG, "Encoder mode: %s", _color_fields[state->cursor_mode]);
      } else {
	ESP_LOGW(TAG, "Encoder mode debounce override");
	return false; // Nothing changed
      }
    }else if(event.bt.id0 == WEB_COLOR_EVID){
      ESP_LOGI(TAG, "Web color event");
      state->rgb = event.web.color;
      state->hsv = rgb_to_hsv(state->rgb);
      return true;
    }else{
      int delta = event.re.state.position - input->encoder_ref;
      input->encoder_ref = event.re.state.position;

      // Use an int to prevent overflow "wrap-around"
      uint8_t * target = &((uint8_t*)(&state->hsv))[state->cursor_mode];
      int new_value = max(0,min(0xff,*target + delta));
      
      ESP_LOGI(TAG, "Encoder: %s = %d + %d",
	       _color_fields[state->cursor_mode],
	       *target, delta);
      
      *target = new_value & 0xff;
      state->rgb = hsv_to_rgb(state->hsv);
    }
    return true;
  }
  return false;
}

void initialize_state(persistent_state_t * s){
  s->rgb.r = 255;
  s->rgb.g = 30;
  s->rgb.b = 0;
  s->hsv = rgb_to_hsv(s->rgb);
  s->cursor_mode = MODE_VALUE;
}

void app_main()
{
  // state and defaults
  state_t * state = storage_initialize(initialize_state);
  input_t input = {0};
  // init stuff
  rgb_init(RED_GPIO, GREEN_GPIO, BLUE_GPIO);
  wifi_main();
  setup_input(&input);
  init_httpd(&state->rgb, input.queue);

  while (1) {
    bool updated = handle_input(&input, state);
    if(updated){
      rgb_set(state->rgb);
      // ToDo notify clients
    }
  }
  
  ESP_LOGE(TAG, "queue receive failed");  
  ESP_ERROR_CHECK(unsetup_input(&input));
}

