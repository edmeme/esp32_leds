#include "rgb.h"

#include <driver/ledc.h>
#include <esp_err.h>
#include <esp_log.h>

const char * TAG = "RGB";

static rgb_calibration_t rgb_cal = {128, 128, 128};

/* channels */
#define LED_R_PWM_CHANNEL LEDC_CHANNEL_1 
#define LED_G_PWM_CHANNEL LEDC_CHANNEL_2
#define LED_B_PWM_CHANNEL LEDC_CHANNEL_3

/* timer */
#define LED_PWM_TIMER LEDC_TIMER_1
// #define LED_PWM_BIT_NUM LEDC_TIMER_10_BIT  // 1024 ( 1023 )
#define LED_PWM_BIT_NUM LEDC_TIMER_8_BIT    // 256 ( 255 )

static rgb_t correct_rgb(rgb_t in, rgb_calibration_t cal){
  unsigned int r = (in.r * (cal.r_scale + 128)) / 256;
  unsigned int g = (in.g * (cal.g_scale + 128)) / 256;
  unsigned int b = (in.b * (cal.b_scale + 128)) / 256;
  rgb_t color = {
    r>0xFF ? 0xFF : r,
    g>0xFF ? 0xFF : g,
    b>0xFF ? 0xFF : b};
  return color;
}

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

void rgb_set(rgb_t rgb_in)
{
  rgb_t rgb = correct_rgb(rgb_in, rgb_cal);
  ESP_LOGI(TAG, "RGB: Color set to r:%d g:%d b:%d ",
	   rgb_in.r, rgb_in.g, rgb_in.b);
  ESP_LOGI(TAG, "RGB: Color corrected to r:%d g:%d b:%d ",
	   rgb.r, rgb.g, rgb.b);
  /* LED R */
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LED_R_PWM_CHANNEL, rgb.r));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LED_R_PWM_CHANNEL) );
  
  /* LED G */
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LED_G_PWM_CHANNEL, rgb.g));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LED_G_PWM_CHANNEL) );
  
  /* LED B */
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LED_B_PWM_CHANNEL, rgb.b));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LED_B_PWM_CHANNEL) );
}

void rgb_set_calib(rgb_calibration_t cal){
  rgb_cal = cal;
}

rgb_t hsv_to_rgb(hsv_t hsv){
  rgb_t rgb;
  unsigned char region, p, q, t;
  unsigned int h, s, v, remainder;
  
  if (hsv.s == 0){
    rgb.r = hsv.v;
    rgb.g = hsv.v;
    rgb.b = hsv.v;
    return rgb;
  }
  
  // converting to 16 bit to prevent overflow
  h = hsv.h;
  s = hsv.s;
  v = hsv.v;

  region = h / 43;
  remainder = (h - (region * 43)) * 6; 

  p = (v * (255 - s)) >> 8;
  q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  
  switch (region){
  case 0:
    rgb.r = v;
    rgb.g = t;
    rgb.b = p;
    break;
  case 1:
    rgb.r = q;
    rgb.g = v;
    rgb.b = p;
    break;
  case 2:
    rgb.r = p;
    rgb.g = v;
    rgb.b = t;
    break;
  case 3:
    rgb.r = p;
    rgb.g = q;
    rgb.b = v;
    break;
  case 4:
    rgb.r = t;
    rgb.g = p;
    rgb.b = v;
    break;
  default:
    rgb.r = v;
    rgb.g = p;
    rgb.b = q;
    break;
  }
  return rgb;
}

hsv_t rgb_to_hsv(rgb_t rgb)
{
  hsv_t hsv;
  unsigned char rgbMin, rgbMax;
  
  rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
  rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
  
  hsv.v = rgbMax;
  if (hsv.v == 0){
    hsv.h = 0;
    hsv.s = 0;
    return hsv;
  }
  
  hsv.s = 255 * ((long)(rgbMax - rgbMin)) / hsv.v;
  if (hsv.s == 0){
    hsv.h = 0;
    return hsv;
  }
  
  if (rgbMax == rgb.r)
    hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
  else if (rgbMax == rgb.g)
    hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
  else
    hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);
  
  return hsv;
}
