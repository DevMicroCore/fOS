#ifndef FOS_FIRMWARE_TARGET_H
#define FOS_FIRMWARE_TARGET_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <esp_app_format.h>
#include <esp_chip_info.h>
// Called before any app or recovery partition is erased/written.
static inline bool fosFirmwareHeaderMatchesTarget(const uint8_t *bytes, size_t size) {
  if (size < sizeof(esp_image_header_t)) return false;
  esp_image_header_t header;
  memcpy(&header, bytes, sizeof(header));
  if (header.magic != ESP_IMAGE_HEADER_MAGIC || header.chip_id != ESP_CHIP_ID_ESP32P4) return false;
  esp_chip_info_t chip;
  esp_chip_info(&chip);
  const uint16_t minimum = header.min_chip_rev_full ? header.min_chip_rev_full : uint16_t(header.min_chip_rev) * 100U;
  if (chip.revision < minimum) return false;
  if (header.max_chip_rev_full && header.max_chip_rev_full != 0xFFFF && chip.revision > header.max_chip_rev_full) return false;
  return true;
}
#endif
