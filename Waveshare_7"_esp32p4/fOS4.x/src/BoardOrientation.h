#pragma once
#include <stdint.h>
#include <stddef.h>

// Logical SquareLine coordinates remain 1024x600; rotate only the board output.
static inline void fos_copy_rgb565_rotated_180(uint16_t *framebuffer, int width,
    int height, int x1, int y1, int x2, int y2, const uint16_t *pixels) {
  const int areaWidth = x2 - x1 + 1;
  for (int y = y1; y <= y2; ++y) {
    uint16_t *target = framebuffer + (height - 1 - y) * width + (width - 1 - x1);
    for (int x = 0; x < areaWidth; ++x) target[-x] = *pixels++;
  }
}
