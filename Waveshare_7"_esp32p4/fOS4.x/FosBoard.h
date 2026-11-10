#pragma once
#include "src/board_display.h"
#include "src/board_touch.h"
#include "FosStorage.h"
void board_display_brightness(uint8_t level);
void board_display_power(bool enabled);
void board_network_prepare();
bool board_audio_codec_begin();
