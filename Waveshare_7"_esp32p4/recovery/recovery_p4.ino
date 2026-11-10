#include <Arduino.h>
#include "FosStorage.h"
#include <SPI.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <string.h>

#include "src/lvgl/lvgl.h"
#include "src/board_display.h"
#include "src/FirmwareTarget.h"
#include <esp_heap_caps.h>

namespace {

#define RECOVERY_SD_CS 10
#define RECOVERY_UPDATE_PATH "/system/update/update.bin"
#define RECOVERY_TARGET_LABEL "app0"
#define RECOVERY_MIN_BIN_SIZE (32U * 1024U)
#define RECOVERY_FLASH_SECTOR_SIZE 4096U
#define RECOVERY_IO_BUFFER_SIZE 1024U
#define RECOVERY_VERIFY_BUFFER_SIZE 512U
#define RECOVERY_BOOT_GUARD_SIZE 16U

static lv_obj_t *statusLabel;
static lv_obj_t *progressBar;
static uint32_t tickMillis() { return millis(); }

void drawFrame() {
  board_display_begin();
  lv_init();
  lv_tick_set_cb(tickMillis);
  void *buffer=heap_caps_malloc(1024U*40U*2U, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!buffer) { Serial.println("Recovery: PSRAM allocation failed"); while(true) delay(1000); }
  lv_display_t *display=lv_display_create(1024,600);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display,board_display_flush);
  lv_display_set_buffers(display,buffer,nullptr,1024U*40U*2U,LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_obj_t *title=lv_label_create(lv_screen_active());
  lv_label_set_text(title,"fOS Recovery - Waveshare ESP32-P4");
  lv_obj_align(title,LV_ALIGN_TOP_LEFT,24,24);
  progressBar=lv_bar_create(lv_screen_active());
  lv_obj_set_size(progressBar,976,28);
  lv_obj_align(progressBar,LV_ALIGN_TOP_LEFT,24,90);
  statusLabel=lv_label_create(lv_screen_active());
  lv_obj_set_width(statusLabel,976);
  lv_label_set_long_mode(statusLabel,LV_LABEL_LONG_WRAP);
  lv_obj_align(statusLabel,LV_ALIGN_TOP_LEFT,24,145);
  lv_timer_handler();
  board_display_backlight(true);
}
void drawStatus(const String& line) {
  lv_label_set_text(statusLabel,line.c_str());
  lv_timer_handler();
  Serial.println("[RECOVERY] "+line);
}
void drawProgress(uint8_t percent) {
  lv_bar_set_value(progressBar,percent>100?100:percent,LV_ANIM_OFF);
  lv_timer_handler();
}

bool validateBinFile(File& bin, size_t maxPartitionSize) {
  if (!bin || bin.isDirectory()) {
    Serial.println("[RECOVERY] Image validation: file is unavailable or is a directory");
    return false;
  }

  const size_t sz = static_cast<size_t>(bin.size());
  if (sz < RECOVERY_MIN_BIN_SIZE || sz > maxPartitionSize) {
    Serial.printf("[RECOVERY] Image validation: invalid size=%u (allowed %u..%u)\n",
      static_cast<unsigned int>(sz),
      static_cast<unsigned int>(RECOVERY_MIN_BIN_SIZE),
      static_cast<unsigned int>(maxPartitionSize));
    return false;
  }

  uint8_t hdr[24] = {0};
  if (!bin.seek(0)) {
    Serial.println("[RECOVERY] Image validation: cannot seek to header");
    return false;
  }
  if (bin.read(hdr, sizeof(hdr)) != sizeof(hdr)) {
    Serial.println("[RECOVERY] Image validation: cannot read complete header");
    return false;
  }
  if (!bin.seek(0)) {
    Serial.println("[RECOVERY] Image validation: cannot rewind image");
    return false;
  }

  if (!fosFirmwareHeaderMatchesTarget(hdr, sizeof(hdr))) {
    Serial.println("[RECOVERY] Wrong target chip/revision; flash remains unchanged.");
    return false;
  }
  if (hdr[0] != 0xE9) {
    Serial.printf("[RECOVERY] Image validation: invalid magic=0x%02x (expected 0xe9)\n",
      hdr[0]);
    return false;
  }
  const uint8_t segmentCount = hdr[1];
  if (segmentCount == 0 || segmentCount > 16) {
    Serial.printf("[RECOVERY] Image validation: invalid segment count=%u\n",
      segmentCount);
    return false;
  }

  Serial.printf("[RECOVERY] Image header OK: size=%u, magic=0x%02x, segments=%u, hash=%u\n",
    static_cast<unsigned int>(sz), hdr[0], segmentCount, hdr[23]);

  size_t offset = sizeof(hdr);
  uint8_t segmentHeader[8];
  for (uint8_t segment = 0; segment < segmentCount; ++segment) {
    if (offset > sz || sz - offset < sizeof(segmentHeader) || !bin.seek(offset)) {
      Serial.printf("[RECOVERY] Image validation: missing segment %u header at 0x%08x\n",
        segment, static_cast<unsigned int>(offset));
      return false;
    }
    if (bin.read(segmentHeader, sizeof(segmentHeader)) != sizeof(segmentHeader)) {
      Serial.printf("[RECOVERY] Image validation: cannot read segment %u header\n", segment);
      return false;
    }

    const uint32_t loadAddress =
      static_cast<uint32_t>(segmentHeader[0]) |
      (static_cast<uint32_t>(segmentHeader[1]) << 8) |
      (static_cast<uint32_t>(segmentHeader[2]) << 16) |
      (static_cast<uint32_t>(segmentHeader[3]) << 24);
    const uint32_t segmentLength =
      static_cast<uint32_t>(segmentHeader[4]) |
      (static_cast<uint32_t>(segmentHeader[5]) << 8) |
      (static_cast<uint32_t>(segmentHeader[6]) << 16) |
      (static_cast<uint32_t>(segmentHeader[7]) << 24);
    offset += sizeof(segmentHeader);

    Serial.printf("[RECOVERY] Segment %u: file=0x%08x, load=0x%08x, length=%u\n",
      segment,
      static_cast<unsigned int>(offset),
      static_cast<unsigned int>(loadAddress),
      static_cast<unsigned int>(segmentLength));

    if (segmentLength == 0 || segmentLength > sz - offset) {
      Serial.printf("[RECOVERY] Image validation: segment %u exceeds file (%u > %u)\n",
        segment,
        static_cast<unsigned int>(segmentLength),
        static_cast<unsigned int>(sz - offset));
      return false;
    }
    offset += segmentLength;
  }

  // One checksum byte follows the segments, padded to a 16-byte boundary.
  // A header flag of 1 adds the 32-byte SHA-256 digest.
  if (offset >= sz) {
    Serial.println("[RECOVERY] Image validation: checksum is missing");
    return false;
  }
  const size_t paddedEnd = (offset + 1U + 15U) & ~static_cast<size_t>(15U);
  const size_t requiredEnd = paddedEnd + (hdr[23] == 1U ? 32U : 0U);
  if (requiredEnd > sz) {
    Serial.printf("[RECOVERY] Image validation: trailer is incomplete (%u required, %u present)\n",
      static_cast<unsigned int>(requiredEnd), static_cast<unsigned int>(sz));
    return false;
  }
  if (requiredEnd != sz) {
    Serial.printf("[RECOVERY] Image validation: %u trailing bytes after ESP image\n",
      static_cast<unsigned int>(sz - requiredEnd));
  }
  if (!bin.seek(0)) {
    Serial.println("[RECOVERY] Image validation: cannot rewind validated image");
    return false;
  }
  return true;
}

bool verifyPartitionAgainstFile(
  File& source,
  const esp_partition_t *partition,
  size_t imageSize) {
  if (!source.seek(0)) return false;
  uint8_t sourceBuffer[RECOVERY_VERIFY_BUFFER_SIZE];
  uint8_t flashBuffer[RECOVERY_VERIFY_BUFFER_SIZE];
  size_t offset = 0;
  while (offset < imageSize) {
    size_t chunk = imageSize - offset;
    if (chunk > sizeof(sourceBuffer)) chunk = sizeof(sourceBuffer);
    if (source.read(sourceBuffer, chunk) != chunk) return false;
    const esp_err_t readResult = esp_partition_read(partition, offset, flashBuffer, chunk);
    if (readResult != ESP_OK) {
      Serial.printf("[RECOVERY] Partition read failed at 0x%08x: %d\n",
        static_cast<unsigned int>(offset), static_cast<int>(readResult));
      return false;
    }
    if (memcmp(sourceBuffer, flashBuffer, chunk) != 0) {
      Serial.printf("[RECOVERY] Verify mismatch at 0x%08x\n",
        static_cast<unsigned int>(offset));
      return false;
    }
    offset += chunk;
    if ((offset & 0xFFFFU) == 0U) delay(1);
  }
  return true;
}

bool writePartitionFromFile(
  File& source,
  const esp_partition_t *partition,
  size_t imageSize) {
  if (partition == nullptr || imageSize < RECOVERY_BOOT_GUARD_SIZE) return false;
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running != nullptr && running->address == partition->address) {
    Serial.println("[RECOVERY] Refusing to overwrite the running partition");
    return false;
  }

  const size_t eraseSize = (imageSize + RECOVERY_FLASH_SECTOR_SIZE - 1U) &
    ~(static_cast<size_t>(RECOVERY_FLASH_SECTOR_SIZE) - 1U);
  if (eraseSize > partition->size) return false;

  drawStatus("Deleting old fOS ...");
  const esp_err_t eraseResult = esp_partition_erase_range(partition, 0, eraseSize);
  if (eraseResult != ESP_OK) {
    Serial.printf("[RECOVERY] Partition erase failed: %d\n", static_cast<int>(eraseResult));
    return false;
  }

  uint8_t bootGuard[RECOVERY_BOOT_GUARD_SIZE];
  if (!source.seek(0) || source.read(bootGuard, sizeof(bootGuard)) != sizeof(bootGuard)) return false;
  if (!source.seek(RECOVERY_BOOT_GUARD_SIZE)) return false;

  drawStatus("Installing fOS..");
  drawProgress(0);
  uint8_t buffer[RECOVERY_IO_BUFFER_SIZE];
  size_t offset = RECOVERY_BOOT_GUARD_SIZE;
  while (offset < imageSize) {
    size_t chunk = imageSize - offset;
    if (chunk > sizeof(buffer)) chunk = sizeof(buffer);
    if (source.read(buffer, chunk) != chunk) {
      Serial.printf("[RECOVERY] SD read failed at 0x%08x\n", static_cast<unsigned int>(offset));
      return false;
    }
    const esp_err_t writeResult = esp_partition_write(partition, offset, buffer, chunk);
    if (writeResult != ESP_OK) {
      Serial.printf("[RECOVERY] Partition write failed at 0x%08x: %d\n",
        static_cast<unsigned int>(offset), static_cast<int>(writeResult));
      return false;
    }
    offset += chunk;
    drawProgress(static_cast<uint8_t>((100ULL * offset) / imageSize));
    if ((offset & 0xFFFFU) == 0U) delay(1);
  }

  // Write the image header last. An interrupted update therefore never leaves
  // a partially written image marked as bootable.
  const esp_err_t headerResult = esp_partition_write(partition, 0, bootGuard, sizeof(bootGuard));
  if (headerResult != ESP_OK) {
    Serial.printf("[RECOVERY] Header write failed: %d\n", static_cast<int>(headerResult));
    return false;
  }

  drawStatus("Verifying fOS ...");
  return verifyPartitionAgainstFile(source, partition, imageSize);
}

void invalidatePartitionHeader(const esp_partition_t *partition) {
  if (partition == nullptr) return;
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running != nullptr && running->address == partition->address) return;
  const esp_err_t result = esp_partition_erase_range(
    partition, 0, RECOVERY_FLASH_SECTOR_SIZE);
  if (result != ESP_OK) {
    Serial.printf("[RECOVERY] Could not invalidate app0 header: %d\n", static_cast<int>(result));
  }
}

bool flashApp0FromSd() {
  drawStatus("Initialize SD_MMC...");
  if (!board_storage_begin()) {
    drawStatus("SD init failed");
    return false;
  }

  drawStatus("Check update file...");
  if (!SD_MMC.exists(RECOVERY_UPDATE_PATH)) {
    drawStatus("Missing /update.bin");
    return false;
  }

  const esp_partition_t *app0 = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,
    ESP_PARTITION_SUBTYPE_ANY,
    RECOVERY_TARGET_LABEL
  );
  if (app0 == nullptr) {
    drawStatus("fOS partition not found");
    return false;
  }

  File updateBin = SD_MMC.open(RECOVERY_UPDATE_PATH, FILE_READ);
  if (!updateBin) {
    drawStatus("Cannot open /update.bin");
    return false;
  }

  const size_t imageSize = static_cast<size_t>(updateBin.size());
  const esp_partition_t *running = esp_ota_get_running_partition();
  Serial.printf("[RECOVERY] Running=%s, target=%s, offset=0x%08x, capacity=%u, image=%u\n",
    running != nullptr ? running->label : "unknown",
    app0->label,
    static_cast<unsigned int>(app0->address),
    static_cast<unsigned int>(app0->size),
    static_cast<unsigned int>(imageSize));

  if (!validateBinFile(updateBin, app0->size)) {
    updateBin.close();
    drawStatus("Invalid update.bin");
    return false;
  }

  const bool flashed = writePartitionFromFile(updateBin, app0, imageSize);
  updateBin.close();

  if (!flashed) {
    invalidatePartitionHeader(app0);
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
    invalidatePartitionHeader(app0);
    drawStatus("Set boot to fOS failed");
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

  drawFrame();
  drawStatus("Recovery start...");

  if (!flashApp0FromSd()) {
    drawStatus("Recovery idle - restart to retry");
  }
}

void loop() {
  // Do not repeatedly erase and rewrite flash after a deterministic failure.
  // A manual restart performs one new, fully validated attempt.
  delay(1000);
}
