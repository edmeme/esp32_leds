#include "rgb.h"

#include <driver/ledc.h>
#include <esp_err.h>

/* channels */
#define LED_R_PWM_CHANNEL LEDC_CHANNEL_1 
#define LED_G_PWM_CHANNEL LEDC_CHANNEL_2
#define LED_B_PWM_CHANNEL LEDC_CHANNEL_3

/* timer */
#define LED_PWM_TIMER LEDC_TIMER_1
// #define LED_PWM_BIT_NUM LEDC_TIMER_10_BIT  // 1024 ( 1023 )
#define LED_PWM_BIT_NUM LEDC_TIMER_8_BIT    // 256 ( 255 )

void rgb_init(gpio_num_t red_pin,
	      gpio_num_t green_pin,
	      gpio_num_t blue_pin)
{
  ledc_channel_config_t ledc_channel_r = {0};
  ledc_channel_config_t ledc_channel_g = {0};
  ledc_channel_config_t ledc_channel_b = {0};
    
  /* set channel r */
  ledc_channel_r.gpio_num = red_pin; 
  ledc_channel_r.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_channel_r.channel = LED_R_PWM_CHANNEL;
  ledc_channel_r.intr_type = LEDC_INTR_DISABLE;
  ledc_channel_r.timer_sel = LED_PWM_TIMER;
  ledc_channel_r.duty = 0;
  
  /* set channel g */
  ledc_channel_g.gpio_num = green_pin; 
  ledc_channel_g.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_channel_g.channel = LED_G_PWM_CHANNEL;
  ledc_channel_g.intr_type = LEDC_INTR_DISABLE;
  ledc_channel_g.timer_sel = LED_PWM_TIMER;
  ledc_channel_g.duty = 0;
  
  /* set channel b */
  ledc_channel_b.gpio_num = blue_pin; 
  ledc_channel_b.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_channel_b.channel = LED_B_PWM_CHANNEL;
  ledc_channel_b.intr_type = LEDC_INTR_DISABLE;
  ledc_channel_b.timer_sel = LED_PWM_TIMER;
  ledc_channel_b.duty = 0;
  
  /* set timer  */	
  ledc_timer_config_t ledc_timer = {0};
  ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_timer.bit_num = LED_PWM_BIT_NUM;
  ledc_timer.timer_num = LED_PWM_TIMER;
  ledc_timer.freq_hz = 25000; 
  
  ESP_ERROR_CHECK( ledc_channel_config(&ledc_channel_r) );
  ESP_ERROR_CHECK( ledc_channel_config(&ledc_channel_g) );
  ESP_ERROR_CHECK( ledc_channel_config(&ledc_channel_b) );
  
  ESP_ERROR_CHECK( ledc_timer_config(&ledc_timer) );
}

void rgb_set(const rgb_t * rgb)
{ 
  /* LED R */
  ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LED_R_PWM_CHANNEL, rgb->r));
  ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LED_R_PWM_CHANNEL) );
  
  /* LED G */
  ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LED_G_PWM_CHANNEL, rgb->g));
  ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LED_G_PWM_CHANNEL) );
  
  /* LED B */
  ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LED_B_PWM_CHANNEL, rgb->b));
  ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LED_B_PWM_CHANNEL) );
}
