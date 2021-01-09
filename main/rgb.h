#pragma once
#include <stdint.h>
#include <driver/gpio.h>


typedef struct
{
  uint8_t h;
  uint8_t s;
  uint8_t v;
} hsv_t;

typedef struct
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb_t;

typedef struct
{
  // color = value * (scale + 128) / 256
  uint8_t r_scale;
  uint8_t g_scale;
  uint8_t b_scale;
} rgb_calibration_t;

void rgb_init(gpio_num_t red_pin,
	      gpio_num_t green_pin,
	      gpio_num_t blue_pin);

void rgb_set(rgb_t rgb);

void rgb_set_calib(rgb_calibration_t cal);

rgb_t hsv_to_rgb(hsv_t hsv);
hsv_t rgb_to_hsv(rgb_t rgb);
