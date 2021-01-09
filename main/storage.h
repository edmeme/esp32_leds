#pragma once
#include "rgb.h"

typedef struct {
  hsv_t hsv;
  rgb_t rgb;
  int cursor_mode;
} persistent_state_t;

typedef void (*default_initializer_fn) (persistent_state_t *);

// initialize storage
// Read or write into the returned structure state that is to be persisted.
persistent_state_t * storage_initialize(default_initializer_fn di);
