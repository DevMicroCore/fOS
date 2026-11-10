#pragma once
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

// Percentage coordinates are relative to the actual parent's content box.
// The result is pixels, so the same parser works with LVGL 8 and LVGL 9,
// including canvases (which need a concrete allocation) and overlay keyboards.
inline int fos_layout_pixels(const char *text, int parent, int fallback) {
  if (!text || !*text) return fallback;
  char *end = nullptr;
  double value = strtod(text, &end);
  if (end == text || !isfinite(value)) return fallback;
  while (isspace(static_cast<unsigned char>(*end))) ++end;
  if (*end == '%') {
    if (parent <= 0) return fallback;
    value = value * parent / 100.0; ++end;
  } else if (end[0] == 'p' && end[1] == 'x') end += 2;
  while (isspace(static_cast<unsigned char>(*end))) ++end;
  if (*end || value < -32767.0 || value > 32767.0) return fallback;
  return static_cast<int>(lround(value));
}
