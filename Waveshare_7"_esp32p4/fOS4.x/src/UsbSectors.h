#pragma once
#include <stdint.h>
#include <string.h>
#include <limits.h>

// TinyUSB can request partial sectors with a nonzero byte offset. SD_MMC's
// raw API transfers complete sectors, so writes use read/modify/write as needed.
// The callbacks below must be serialized against SD ownership changes.
template<class Read, class Write>
int32_t fos_usb_sector_transfer(bool writing, uint32_t sectors, uint32_t lba,
    uint32_t offset, void *buffer, uint32_t bytes, Read read, Write write) {
  const uint64_t capacity = uint64_t(sectors) * 512U;
  uint64_t position = uint64_t(lba) * 512U + offset;
  if (bytes > INT32_MAX || position > capacity || bytes > capacity - position ||
      (bytes && buffer == nullptr)) return -1;
  uint8_t *data = static_cast<uint8_t *>(buffer);
  const uint32_t result = bytes;
  alignas(4) uint8_t sector[512];
  while (bytes) {
    const uint32_t index = position / 512U;
    const uint32_t within = position % 512U;
    const uint32_t chunk = bytes < 512U - within ? bytes : 512U - within;
    if (!writing || within || chunk != 512U) {
      if (!read(sector, index)) return -1;
    }
    if (writing) {
      memcpy(sector + within, data, chunk);
      if (!write(sector, index)) return -1;
    } else memcpy(data, sector + within, chunk);
    position += chunk; data += chunk; bytes -= chunk;
  }
  return static_cast<int32_t>(result);
}
