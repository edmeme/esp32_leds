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

void rgb_init(gpio_num_t red_pin,
	      gpio_num_t green_pin,
	      gpio_num_t blue_pin);

void rgb_set(rgb_t rgb);

rgb_t hsv_to_rgb(hsv_t hsv);
hsv_t rgb_to_hsv(rgb_t rgb);
