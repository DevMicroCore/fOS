#include "FosRtc.h"
#include "RtcAnchor.h"
#include "p4_vbat/FosVbat.h"
#include <esp_chip_info.h>
#include <Arduino.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_newlib.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <esp_private/esp_clk.h>
#include <soc/rtc.h>
#include <sys/time.h>
#include <time.h>
#include <atomic>

#if !CONFIG_ESP_TIME_FUNCS_USE_RTC_TIMER
#error "This fOS build needs the ESP-IDF RTC timekeeping timer enabled."
#endif

// No initializer: retained across supported resets. CRC/commit marker reject
// random or partially written RAM. NVS is an additional backup of the anchor,
// never a fallback to a frozen last-known date after the RTC counter was lost.
RTC_NOINIT_ATTR volatile FosRtcAnchor fosRtcRetainedAnchor;
static portMUX_TYPE anchorMux = portMUX_INITIALIZER_UNLOCKED;
static FosRtcAnchor pendingAnchor;
static std::atomic<bool> syncPending{false};

static FosRtcAnchor readRetained() {
  FosRtcAnchor a;
  a.magic = fosRtcRetainedAnchor.magic; a.version = fosRtcRetainedAnchor.version;
  a.utcUs = fosRtcRetainedAnchor.utcUs; a.ticks = fosRtcRetainedAnchor.ticks;
  a.period = fosRtcRetainedAnchor.period; a.source = fosRtcRetainedAnchor.source;
  a.checksum = fosRtcRetainedAnchor.checksum; a.reserved = fosRtcRetainedAnchor.reserved;
  return a;
}
static void writeRetained(const FosRtcAnchor &a) {
  fosRtcRetainedAnchor.magic = 0;
  fosRtcRetainedAnchor.version = a.version; fosRtcRetainedAnchor.utcUs = a.utcUs;
  fosRtcRetainedAnchor.ticks = a.ticks; fosRtcRetainedAnchor.period = a.period;
  fosRtcRetainedAnchor.source = a.source; fosRtcRetainedAnchor.reserved = a.reserved;
  fosRtcRetainedAnchor.checksum = a.checksum;
  std::atomic_thread_fence(std::memory_order_release);
  fosRtcRetainedAnchor.magic = a.magic;
}
static bool snapshot(FosRtcAnchor *a) {
  struct timeval now;
  gettimeofday(&now, nullptr);
  a->magic = kFosRtcMagic; a->version = 1;
  a->utcUs = int64_t(now.tv_sec) * 1000000LL + now.tv_usec;
  a->ticks = rtc_time_get();
  a->period = esp_clk_slowclk_cal_get();
  a->source = static_cast<uint32_t>(rtc_clk_slow_src_get());
  a->reserved = 0; a->checksum = fos_rtc_anchor_crc(*a);
  return fos_rtc_anchor_valid(*a);
}
static bool loadBackup(FosRtcAnchor *a) {
  Preferences prefs;
  if (!prefs.begin("fos_rtc", true)) return false;
  const bool loaded = prefs.getBytesLength("anchor") == sizeof(*a) &&
      prefs.getBytes("anchor", a, sizeof(*a)) == sizeof(*a);
  prefs.end();
  return loaded && fos_rtc_anchor_valid(*a);
}
static void discardBackup() {
  fosRtcRetainedAnchor.magic = 0;
  Preferences prefs;
  if (prefs.begin("fos_rtc", false)) { prefs.remove("anchor"); prefs.end(); }
}
static void onNtpSynchronized(struct timeval *value) {
  if (!value || !fos_rtc_utc_valid(int64_t(value->tv_sec) * 1000000LL + value->tv_usec)) return;
  esp_sync_timekeeping_timers();
  FosRtcAnchor a{};
  if (!snapshot(&a)) return;
  // Capture the raw RTC counter immediately in the SNTP callback, not on a
  // later UI iteration which could be delayed by startup or a blocking task.
  portENTER_CRITICAL(&anchorMux);
  writeRetained(a); pendingAnchor = a;
  portEXIT_CRITICAL(&anchorMux);
  syncPending.store(true, std::memory_order_release);
}
void fos_rtc_begin() {
  syncPending.store(false);
  const bool vbatReady = fos_vbat_apply_power(false);
  esp_chip_info_t chip{}; esp_chip_info(&chip);
  Serial.printf("[RTC] VBAT AUTO=%s mode=%u chip-rev=%u; charger=OFF; PMU sleep patch=enabled.\n",
      vbatReady ? "latched" : "FAILED", (unsigned)fos_vbat_mode(), (unsigned)chip.revision);
  const esp_reset_reason_t reset = esp_reset_reason();
  const uint64_t ticks = rtc_time_get();
  const uint32_t source = static_cast<uint32_t>(rtc_clk_slow_src_get());
  struct timeval systemTime;
  gettimeofday(&systemTime, nullptr);
  const bool sdkValid = fos_rtc_utc_valid(int64_t(systemTime.tv_sec) * 1000000LL + systemTime.tv_usec);
  Serial.printf("[RTC] Boot: reset=%d counter=%llu SDK UTC=%lld.\n", int(reset),
      (unsigned long long)ticks, (long long)systemTime.tv_sec);

  // The IDF already initializes its RTC/high-resolution offset before setup().
  // Set the calendar explicitly from our independent anchor on retained resets.
  const bool canRetain = reset == ESP_RST_SW || reset == ESP_RST_EXT ||
                         reset == ESP_RST_DEEPSLEEP || reset == ESP_RST_PANIC ||
                         reset == ESP_RST_INT_WDT || reset == ESP_RST_TASK_WDT ||
                         reset == ESP_RST_WDT;
  FosRtcAnchor a = readRetained();
  FosRtcAnchor backup{};
  const bool backupValid = loadBackup(&backup);
  Serial.printf("[RTC] Anchors: RAM=%s NVS=%s saved-counter=%llu.\n",
      fos_rtc_anchor_valid(a) ? "valid" : "invalid", backupValid ? "valid" : "invalid",
      (unsigned long long)(backupValid ? backup.ticks : 0));
  bool counterLost = fos_rtc_anchor_valid(a) && (ticks < a.ticks || source != a.source);
  int64_t restoredUs = 0;
  bool restored = false;
  const char *origin = "RTC memory";
  if (canRetain) {
    if (fos_rtc_anchor_valid(a)) {
      restored = fos_rtc_restore_anchor(a, ticks, source, &restoredUs);
      // A newer RAM record can prove that the counter reset, even when the
      // restarted counter has already overtaken an older NVS record. Never
      // bypass that evidence by trying the older backup.
      if (!restored) counterLost = true;
    } else if (backupValid) {
      a = backup;
      origin = "NVS anchor + live RTC counter";
      restored = fos_rtc_restore_anchor(a, ticks, source, &restoredUs);
      counterLost = !restored;
    }
  }
  if (restored) {
    struct timeval restoredTime;
    restoredTime.tv_sec = restoredUs / 1000000LL;
    restoredTime.tv_usec = restoredUs % 1000000LL;
    if (settimeofday(&restoredTime, nullptr) == 0) {
      esp_sync_timekeeping_timers();
      Serial.printf("[RTC] Restored before Wi-Fi from %s: UTC=%lld; elapsed=%llu s.\n",
          origin, (long long)restoredTime.tv_sec,
          (unsigned long long)((restoredUs - a.utcUs) / 1000000LL));
    } else restored = false;
  }
  const bool trustSdk = sdkValid && !counterLost;
  if (!restored && trustSdk) {
    Serial.printf("[RTC] Using valid SDK RTC calendar: UTC=%lld.\n", (long long)systemTime.tv_sec);
  } else if (!restored) {
    // Invalidate old anchors so a later warm reset cannot reuse an old power
    // session after its new RTC counter has grown past the old saved value.
    discardBackup();
    if (counterLost) {
      struct timeval unset = {};
      settimeofday(&unset, nullptr);
      Serial.println("[RTC] RTC counter/source was reset; rejected stale calendar.");
    }
    Serial.println("[RTC] No retained calendar/counter; time unset until NTP.");
  }
  if (restored || trustSdk) {
    FosRtcAnchor current{};
    if (snapshot(&current)) writeRetained(current);
  }
  sntp_set_time_sync_notification_cb(onNtpSynchronized);
}
void fos_rtc_poll() {
  if (!syncPending.exchange(false, std::memory_order_acquire)) return;
  FosRtcAnchor a;
  portENTER_CRITICAL(&anchorMux); a = pendingAnchor; portEXIT_CRITICAL(&anchorMux);
  if (!fos_rtc_anchor_valid(a)) return;
  Preferences prefs;
  bool saved = false;
  if (prefs.begin("fos_rtc", false)) {
    saved = prefs.putBytes("anchor", &a, sizeof(a)) == sizeof(a);
    prefs.end();
  }
  Serial.printf("[RTC] NTP captured: UTC=%lld counter=%llu; anchor captured, NVS=%s; battery retention needs power-cut test.\n",
      (long long)(a.utcUs / 1000000LL), (unsigned long long)a.ticks,
      saved ? "OK" : "FAILED");
}

bool fos_rtc_prepare_backup_sleep() {
  if (!fos_vbat_apply_power(true)) {
    Serial.println("[RTC] Backup sleep cancelled: VBAT AUTO did not latch.");
    return false;
  }
  esp_sync_timekeeping_timers();
  FosRtcAnchor a{};
  if (!snapshot(&a)) {
    Serial.println("[RTC] Backup sleep cancelled: synchronize time once first.");
    fos_vbat_apply_power(false);
    return false;
  }
  portENTER_CRITICAL(&anchorMux); writeRetained(a); portEXIT_CRITICAL(&anchorMux);
  Preferences prefs;
  bool saved = false;
  if (prefs.begin("fos_rtc", false)) {
    saved = prefs.putBytes("anchor", &a, sizeof(a)) == sizeof(a); prefs.end();
  }
  if (!saved) { fos_vbat_apply_power(false); return false; }
  Serial.printf("[RTC] Backup sleep checkpoint: UTC=%lld counter=%llu; NVS=OK.\n",
      (long long)(a.utcUs / 1000000LL), (unsigned long long)a.ticks);
  return true;
}
