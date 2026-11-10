#pragma once
#include "lvgl/lvgl.h"

void board_display_begin();
void board_display_backlight(bool on);
void board_display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels);
