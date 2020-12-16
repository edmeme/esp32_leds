#pragma once
#include <stdint.h>
#include <driver/gpio.h>

typedef struct
{
  uint32_t r;
  uint32_t g;
  uint32_t b;
} rgb_t;

void rgb_init(gpio_num_t red_pin,
	      gpio_num_t green_pin,
	      gpio_num_t blue_pin);
void rgb_set(const rgb_t * rgb);
