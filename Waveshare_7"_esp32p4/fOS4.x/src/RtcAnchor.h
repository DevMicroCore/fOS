#pragma once
#include <stdint.h>
#include <limits.h>
#include <type_traits>

// POD, deliberately without initializers: the RTC_NOINIT instance must not be
// reinitialized by a C++ constructor during startup. UTC is independent of TZ.
struct FosRtcAnchor {
  uint32_t magic;
  uint32_t version;
  int64_t utcUs;
  uint64_t ticks;
  uint32_t period; // slow-clock period, microseconds in Q19
  uint32_t source;
  uint32_t checksum;
  uint32_t reserved;
};
static_assert(std::is_trivial<FosRtcAnchor>::value, "RTC anchor must remain trivial");
static_assert(sizeof(FosRtcAnchor) == 40, "Stable NVS record layout required");
static constexpr uint32_t kFosRtcMagic = 0x46525443U;
static constexpr int64_t kFosRtcMinUs = 1609459200LL * 1000000LL;
static constexpr int64_t kFosRtcMaxUs = 4102444800LL * 1000000LL;
inline bool fos_rtc_utc_valid(int64_t value) { return value >= kFosRtcMinUs && value < kFosRtcMaxUs; }
inline uint32_t fos_rtc_anchor_crc(const FosRtcAnchor &a) {
  uint32_t crc = 0xFFFFFFFFU;
  auto field = [&](uint64_t value, unsigned bytes) {
    while (bytes--) {
      crc ^= static_cast<uint8_t>(value); value >>= 8;
      for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
  };
  field(a.magic,4); field(a.version,4); field(uint64_t(a.utcUs),8);
  field(a.ticks,8); field(a.period,4); field(a.source,4); field(a.reserved,4);
  return ~crc;
}
inline bool fos_rtc_anchor_valid(const FosRtcAnchor &a) {
  return a.magic == kFosRtcMagic && a.version == 1 && a.reserved == 0 &&
         a.period != 0 && fos_rtc_utc_valid(a.utcUs) && a.checksum == fos_rtc_anchor_crc(a);
}
inline bool fos_rtc_restore_anchor(const FosRtcAnchor &a, uint64_t ticks,
    uint32_t source, int64_t *utcUs) {
  if (!utcUs || !fos_rtc_anchor_valid(a) || source != a.source || ticks < a.ticks) return false;
  const uint64_t delta = ticks - a.ticks;
  // Convert with the calibration saved alongside the raw counter, not the new
  // boot's recalibration. Split the product to avoid 64-bit overflow.
  const uint64_t whole = delta >> 19;
  if (whole > UINT64_MAX / a.period) return false;
  const uint64_t part = ((delta & ((1U << 19) - 1U)) * a.period) >> 19;
  const uint64_t base = whole * a.period;
  if (base > UINT64_MAX - part) return false;
  const uint64_t elapsed = base + part;
  if (elapsed >= uint64_t(kFosRtcMaxUs - a.utcUs)) return false;
  *utcUs = a.utcUs + static_cast<int64_t>(elapsed);
  return true;
}
