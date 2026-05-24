#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "LGFX_CrowPanel.h"

namespace {

constexpr int kSdCs = 10;
constexpr const char *kUpdatePath = "/system/update/update.bin";
constexpr const char *kTargetLabel = "app0";
constexpr size_t kMinBinSize = 32 * 1024;

LGFX gfx;

void drawFrame() {
  gfx.fillScreen(TFT_BLACK);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextSize(2);
  gfx.setCursor(16, 16);
  gfx.println("fOS Recovery");
  gfx.drawRect(16, 80, 768, 28, TFT_WHITE);
}

void drawStatus(const String& line) {
  gfx.fillRect(16, 120, 768, 32, TFT_BLACK);
  gfx.setCursor(16, 120);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.print(line);
  Serial.println("[RECOVERY] " + line);
}

void drawProgress(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  static uint8_t lastPercent = 255;
  static uint32_t lastDrawMs = 0;
  const uint32_t now = millis();
  if (lastPercent != 255 && percent < 100) {
    if (percent == lastPercent) {
      return;
    }
    const int delta = static_cast<int>(percent) - static_cast<int>(lastPercent);
    if (delta > 0 && delta < 2 && (now - lastDrawMs) < 120) {
      return;
    }
  }
  lastPercent = percent;
  lastDrawMs = now;

  const int x = 18;
  const int y = 82;
  const int w = 764;
  const int h = 24;
  const int fill = (w * percent) / 100;

  gfx.fillRect(x, y, w, h, TFT_BLACK);
  if (fill > 0) {
    gfx.fillRect(x, y, fill, h, TFT_WHITE);
  }

  gfx.fillRect(16, 156, 200, 32, TFT_BLACK);
  gfx.setCursor(16, 156);
  gfx.printf("%3u%%", percent);
}

bool validateBinFile(File& bin, size_t maxPartitionSize) {
  if (!bin || bin.isDirectory()) {
    return false;
  }

  const size_t sz = static_cast<size_t>(bin.size());
  if (sz < kMinBinSize || sz > maxPartitionSize) {
    return false;
  }

  uint8_t hdr[8] = {0};
  if (!bin.seek(0)) {
    return false;
  }
  if (bin.read(hdr, sizeof(hdr)) != sizeof(hdr)) {
    return false;
  }
  if (!bin.seek(0)) {
    return false;
  }

  if (hdr[0] != 0xE9) {
    return false;
  }
  if (hdr[1] == 0 || hdr[1] > 16) {
    return false;
  }
  return true;
}

class ProgressFileStream : public Stream {
public:
  ProgressFileStream(File& file, size_t totalSize)
  : _file(file), _total(totalSize), _done(0), _lastProgress(0), _lastBucket(255) {}

  int available() override { return _file.available(); }
  int read() override {
    int c = _file.read();
    if (c >= 0) {
      advance(1);
    }
    return c;
  }
  int peek() override { return _file.peek(); }
  void flush() override { _file.flush(); }
  size_t write(uint8_t) override { return 0; }

  size_t readBytes(char *buffer, size_t length) {
    const size_t n = _file.read(reinterpret_cast<uint8_t *>(buffer), length);
    if (n > 0) {
      advance(n);
    }
    return n;
  }

private:
  File& _file;
  size_t _total;
  size_t _done;
  uint8_t _lastProgress;
  uint8_t _lastBucket;

  void advance(size_t chunk) {
    _done += chunk;
    if (_total == 0) {
      return;
    }
    const uint8_t p = static_cast<uint8_t>((100ULL * _done) / _total);
    const uint8_t bucket = static_cast<uint8_t>(p / 5);  // Draw only every 5%
    if (p == 100 || bucket != _lastBucket) {
      _lastBucket = bucket;
      _lastProgress = p;
      drawProgress(p);
    }
  }
};

bool flashApp0FromSd() {
  drawStatus("Initialize SD...");
  if (!SD.begin(kSdCs)) {
    drawStatus("SD init failed");
    return false;
  }

  drawStatus("Check update file...");
  if (!SD.exists(kUpdatePath)) {
    drawStatus("Missing /update.bin");
    return false;
  }

  const esp_partition_t *app0 = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,
    ESP_PARTITION_SUBTYPE_ANY,
    kTargetLabel
  );
  if (app0 == nullptr) {
    drawStatus("Partition app0 not found");
    return false;
  }

  File updateBin = SD.open(kUpdatePath, FILE_READ);
  if (!updateBin) {
    drawStatus("Cannot open /update.bin");
    return false;
  }

  const size_t imageSize = static_cast<size_t>(updateBin.size());
  if (!validateBinFile(updateBin, app0->size)) {
    updateBin.close();
    drawStatus("Invalid update.bin");
    return false;
  }

  drawStatus("Flash app0...");
  drawProgress(0);

  if (!Update.begin(imageSize, U_FLASH, -1, LOW, kTargetLabel)) {
    updateBin.close();
    drawStatus("Update.begin failed");
    return false;
  }

  ProgressFileStream stream(updateBin, imageSize);
  const size_t written = Update.writeStream(stream);
  const bool endOk = Update.end(true);
  updateBin.close();

  if (written != imageSize || !endOk) {
    Update.abort();
    drawStatus("Flash failed");
    return false;
  }

  drawProgress(100);
  drawStatus("Flash OK, switch boot...");

  const esp_partition_t *bootBefore = esp_ota_get_boot_partition();
  if (bootBefore != nullptr) {
    Serial.printf("[RECOVERY] Boot before: %s\n", bootBefore->label);
  }

  const esp_err_t setErr = esp_ota_set_boot_partition(app0);
  if (setErr != ESP_OK) {
    drawStatus("Set boot app0 failed");
    Serial.printf("[RECOVERY] esp_ota_set_boot_partition err=%d\n", static_cast<int>(setErr));
    return false;
  }

  const esp_partition_t *bootAfter = esp_ota_get_boot_partition();
  if (bootAfter != nullptr) {
    Serial.printf("[RECOVERY] Boot after : %s\n", bootAfter->label);
  }

  drawStatus("Restarting...");
  delay(500);
  esp_restart();
  return true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  gfx.init();
  gfx.setRotation(0);
  gfx.setBrightness(255);
  drawFrame();
  drawStatus("Recovery start...");

  if (!flashApp0FromSd()) {
    drawStatus("Recovery idle (retry in 5s)");
  }
}

void loop() {
  delay(5000);
  drawStatus("Retry update...");
  if (!flashApp0FromSd()) {
    drawStatus("Recovery idle (retry in 5s)");
  }
}
