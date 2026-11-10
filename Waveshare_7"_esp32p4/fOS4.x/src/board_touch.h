#pragma once
#include "lvgl/lvgl.h"

// Board output is rotated 180 degrees relative to the original LCD-7B profile.
// Invert both original touch mirrors as well, keeping touch aligned with the UI.
// Change these only if another panel revision has a different orientation.
#define TOUCH_SWAP_XY 0
#define TOUCH_MIRROR_X 0
#define TOUCH_MIRROR_Y 0

bool board_touch_begin(lv_display_t *display);

#include "driver/i2c_master.h"
bool board_i2c_begin();
i2c_master_bus_handle_t board_i2c_bus();
void board_touch_enable(bool enabled);
