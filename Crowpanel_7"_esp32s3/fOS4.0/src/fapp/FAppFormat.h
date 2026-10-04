#pragma once

#include <stddef.h>
#include <stdint.h>

namespace FAppFormat {

constexpr uint8_t kFormatMajor = 1;
constexpr uint8_t kFormatMinor = 0;
constexpr uint8_t kBytecodeMajor = 1;
constexpr uint8_t kBytecodeMinor = 6;
constexpr uint16_t kHeaderSize = 80;
constexpr size_t kCrcOffset = 16;
constexpr size_t kCrcSize = 4;
constexpr uint32_t kFunctionRecordSize = 16;
constexpr uint32_t kEventRecordSize = 8;

inline uint16_t readU16(const uint8_t * bytes)
{
  return static_cast<uint16_t>(bytes[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8u);
}

inline uint32_t readU32(const uint8_t * bytes)
{
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8u) |
         (static_cast<uint32_t>(bytes[2]) << 16u) |
         (static_cast<uint32_t>(bytes[3]) << 24u);
}

inline bool rangeIsValid(uint32_t offset, uint32_t size, uint32_t fileSize)
{
  return offset <= fileSize && size <= fileSize - offset;
}

inline uint32_t crc32Update(uint32_t crc, uint8_t byte)
{
  crc ^= byte;
  for (uint8_t bit = 0; bit < 8; ++bit) {
    crc = (crc >> 1u) ^ (0xEDB88320u & static_cast<uint32_t>(-(crc & 1u)));
  }
  return crc;
}

}  // namespace FAppFormat
