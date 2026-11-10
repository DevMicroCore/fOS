#include "FosUsbStorage.h"
#include "UsbSectors.h"
#include "../FosStorage.h"
#include <Arduino.h>
#include <Preferences.h>
#include <USB.h>
#include <USBMSC.h>
#include <tusb.h>
#include <freertos/semphr.h>
#include <atomic>

#if ARDUINO_USB_MODE
#error "Select USB Mode: USB-OTG (TinyUSB) for SD-card USB storage."
#endif
#if ARDUINO_USB_MSC_ON_BOOT
#error "Disable MSC On Boot; fOS supplies its own SD-card MSC, not FirmwareMSC."
#endif

namespace {
USBMSC msc;
Preferences prefs;
SemaphoreHandle_t ioMutex;
FosUsbStorageHooks hooks{};
std::atomic<bool> enabled{false}, connected{false}, ejected{false}, rawReady{false};
bool initialized, attached, geometryReady;
uint32_t sectorCount;

void usbEvent(void *, esp_event_base_t base, int32_t event, void *) {
  if (base != ARDUINO_USB_EVENTS) return;
  if (event == ARDUINO_USB_STARTED_EVENT) {
    ejected.store(false); connected.store(true);
  } else if (event == ARDUINO_USB_STOPPED_EVENT) {
    connected.store(false); ejected.store(false);
  }
  // USB suspend is not eject; keep the card owned by the computer while asleep.
}
bool startStop(uint8_t, bool start, bool loadEject) {
  if (loadEject) {
    if (!start) {
      // Actual release is deferred to the Arduino task; it waits for ongoing I/O.
      ejected.store(true);
      msc.mediaPresent(false);
    } else ejected.store(false);
  }
  return true;
}
int32_t transfer(bool writing, uint32_t lba, uint32_t offset, void *buffer, uint32_t bytes) {
  if (!ioMutex || xSemaphoreTake(ioMutex, pdMS_TO_TICKS(3000)) != pdTRUE) return -1;
  int32_t result = -1;
  if (rawReady.load() && enabled.load() && !ejected.load()) {
    result = fos_usb_sector_transfer(writing, sectorCount, lba, offset, buffer, bytes,
        [](uint8_t *p, uint32_t n) { return SD_MMC.readRAW(p, n); },
        [](uint8_t *p, uint32_t n) { return SD_MMC.writeRAW(p, n); });
  }
  xSemaphoreGive(ioMutex);
  return result;
}
int32_t read(uint32_t lba, uint32_t offset, void *buffer, uint32_t bytes) {
  return transfer(false, lba, offset, buffer, bytes);
}
int32_t write(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bytes) {
  return transfer(true, lba, offset, buffer, bytes);
}
bool attach() {
  if (!geometryReady) {
    if (SD_MMC.cardType() == CARD_NONE || SD_MMC.sectorSize() != 512) return false;
    const uint64_t count = SD_MMC.cardSize() / 512U;
    if (!count || count > UINT32_MAX) return false;
    sectorCount = static_cast<uint32_t>(count); // physical card, including MBR
    msc.vendorID("fOS"); msc.productID("Waveshare SD"); msc.productRevision("4.01");
    msc.onRead(read); msc.onWrite(write); msc.onStartStop(startStop);
    msc.isWritable(true); msc.mediaPresent(false);
    if (!msc.begin(sectorCount, 512)) return false;
    geometryReady = true;
  }
  if (!initialized) {
    USB.productName("fOS SD Card");
    USB.manufacturerName("DevMicroCore");
    USB.onEvent(usbEvent);
    if (!USB.begin()) return false;
    initialized = true;
  }
  tud_connect(); attached = true;
  connected.store(tud_mounted());
  Serial.println("[USB] SD storage enabled on the P4 native USB OTG HS port.");
  return true;
}
void releaseCard() {
  msc.mediaPresent(false);
  // Never remount while a TinyUSB callback is still reading or writing a sector.
  xSemaphoreTake(ioMutex, portMAX_DELAY);
  rawReady.store(false);
  const bool mounted = board_storage_usb_release();
  xSemaphoreGive(ioMutex);
  if (hooks.released) hooks.released(mounted);
  Serial.println(mounted ? "[USB] SD card returned to fOS (fresh FAT mount)."
                         : "[USB] SD remount failed; local storage unavailable.");
}
}
void fos_usb_storage_begin(FosUsbStorageHooks value) {
  hooks = value;
  ioMutex = xSemaphoreCreateMutex();
  const bool opened = prefs.begin("fos_usb", false);
  enabled.store(opened && prefs.getBool("enabled", false));
  if (!opened || !ioMutex) {
    enabled.store(false);
    Serial.println("[USB] Preferences or I/O mutex unavailable; MSC disabled.");
  }
}
bool fos_usb_storage_enabled() { return enabled.load(); }
bool fos_usb_storage_set_enabled(bool value) {
  if (prefs.putBool("enabled", value) != 1) {
    Serial.println("[USB] Could not save USB storage preference.");
    return false;
  }
  enabled.store(value);
  return true;
}
bool fos_usb_storage_active() { return rawReady.load(); }
bool fos_usb_storage_blocking() {
  return rawReady.load() || (enabled.load() && connected.load() && !ejected.load());
}
void fos_usb_storage_poll() {
  if (!ioMutex) return;
  const bool desired = enabled.load();
  if (desired && !attached) attach();
  if (!desired && attached) {
    msc.mediaPresent(false);
    tud_disconnect(); attached = false; connected.store(false);
  }
  const bool wanted = desired && connected.load() && !ejected.load();
  if (!wanted && rawReady.load()) releaseCard();
  if (!wanted || rawReady.load()) return;
  if (hooks.canClaim && !hooks.canClaim()) return;
  if (hooks.prepareClaim) hooks.prepareClaim();
  xSemaphoreTake(ioMutex, portMAX_DELAY);
  const bool claimed = board_storage_usb_claim();
  rawReady.store(claimed);
  xSemaphoreGive(ioMutex);
  if (!claimed) {
    ejected.store(true);
    if (hooks.released) hooks.released(board_storage_begin());
    Serial.println("[USB] SD ownership transfer failed; disk stays unavailable.");
    return;
  }
  msc.mediaPresent(true);
  Serial.println("[USB] Computer owns SD card; fOS file access paused.");
}
