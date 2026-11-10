#include "../FosBoard.h"
#include <Arduino.h>
#include <WiFi.h>
#include "codec/es8311.h"
#include "codec/codec_bus.h"
#include "ff.h"
#include "diskio.h"

static i2c_master_dev_handle_t codecDevice;

static bool mounted;
static bool usbOwnsCard;
bool board_storage_begin() {
  if (usbOwnsCard) return false;
  if (mounted) return true;
  // Core 3.3.12's P4 SD_MMC driver powers VO4 and active-low card power GPIO45.
  if (!SD_MMC.setPins(43, 44, 39, 40, 41, 42)) return false;
  mounted = SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_DEFAULT, 10);
  return mounted;
}

bool board_storage_usb_claim() {
  if (!mounted || usbOwnsCard) return false;
  // This project mounts exactly one FAT drive (SD_MMC), drive 0, as does
  // SDMMCFS::totalBytes() in Arduino core 3.3.12. SPIFFS is a separate VFS.
  // Removing the FAT mount keeps diskio/card registered for readRAW/writeRAW,
  // but prevents any stale VFS open from accessing the host-owned filesystem.
  if (disk_ioctl(0, CTRL_SYNC, nullptr) != RES_OK) return false;
  if (f_mount(nullptr, "0:", 0) != FR_OK) return false;
  usbOwnsCard = true;
  return true;
}
bool board_storage_usb_release() {
  if (!usbOwnsCard) return board_storage_begin();
  SD_MMC.end(); // also drops the old FatFs/VFS context and all cached FAT data
  mounted = false;
  usbOwnsCard = false;
  return board_storage_begin();
}

void board_network_prepare() {
  // Dedicated C6 SDIO bus, separate from the microSD bus.
  WiFi.setPins(18, 19, 14, 15, 16, 17, 54);
}

extern "C" esp_err_t fos_codec_write(const uint8_t *data, size_t size) {
  if (!codecDevice) return ESP_ERR_INVALID_STATE;
  return i2c_master_transmit(codecDevice, data, size, 100);
}
extern "C" esp_err_t fos_codec_read(uint8_t reg, uint8_t *value) {
  if (!codecDevice) return ESP_ERR_INVALID_STATE;
  return i2c_master_transmit_receive(codecDevice, &reg, 1, value, 1, 100);
}

bool board_audio_codec_begin() {
  pinMode(53, OUTPUT);
  digitalWrite(53, LOW);
  i2c_master_bus_handle_t bus = board_i2c_bus();
  if (!bus || i2c_master_probe(bus, 0x18, 100) != ESP_OK) {
    Serial.println("[ES8311] Codec not found at 0x18.");
    return false;
  }
  i2c_device_config_t device = {};
  device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  device.device_address = 0x18;
  device.scl_speed_hz = 100000;
  if (i2c_master_bus_add_device(bus, &device, &codecDevice) != ESP_OK) return false;
  es8311_handle_t codec = es8311_create(0, ES8311_ADDRRES_0);
  if (!codec) return false;
  es8311_clock_config_t clocks = {};
  clocks.mclk_from_mclk_pin = true;
  clocks.mclk_frequency = 48000 * 256;
  clocks.sample_frequency = 48000;
  esp_err_t result = es8311_init(codec, &clocks, ES8311_RESOLUTION_32, ES8311_RESOLUTION_32);
  if (result == ESP_OK) result = es8311_microphone_config(codec, false);
  if (result == ESP_OK) result = es8311_voice_volume_set(codec, 80, nullptr);
  // Codec registers retain their settings; the handle contains no running task.
  es8311_delete(codec);
  if (result != ESP_OK) {
    Serial.printf("[ES8311] Initialization failed: %s\n", esp_err_to_name(result));
    return false;
  }
  digitalWrite(53, HIGH);
  Serial.println("[ES8311] 48 kHz / 32-bit I2S; amplifier enabled on GPIO53.");
  return true;
}
