#include "ota_recovery_manager.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ui_Update.h"

namespace {

#define OTA_SD_CS 10
#define OTA_MAX_FILES 24U
#define OTA_MAX_RECOVERY_FILES 16U
#define OTA_MAX_BOOT_ATTEMPTS 3U
#define OTA_VALIDATION_DELAY_MS 30000UL
#define OTA_LIST_RETRY_MS 15000UL
#define OTA_WIFI_STABLE_BEFORE_TLS_MS 3000UL
#define OTA_HTTP_CONNECT_TIMEOUT_MS 15000U
#define OTA_HTTP_READ_TIMEOUT_MS 30000U
#define OTA_HTTP_RETRIES 3U
#define OTA_SD_WRITE_RETRIES 4U
#define OTA_SD_RESERVE_BYTES (64UL * 1024UL)
#define OTA_MIN_APP_BIN_SIZE (32U * 1024U)
#define OTA_MAX_API_PAYLOAD (32U * 1024U)
#define OTA_MIN_TLS_FREE_INTERNAL_HEAP (40U * 1024U)
// A TLS record needs a contiguous block above 16 KiB. Keep additional
// headroom, but accept the ~31 KiB block measured on the running panel.
#define OTA_MIN_TLS_LARGEST_BLOCK (24U * 1024U)
#define OTA_PREFS_NAMESPACE "ota_state"
#define OTA_KEY_PENDING "pending_update"
#define OTA_KEY_BOOT_COUNTER "boot_attempt_counter"
#define OTA_API_LIST "https://api.github.com/repos/DevMicroCore/fOS/contents/Crowpanel_7%22_esp32s3/ota"
#define OTA_API_RECOVERY_LIST "https://api.github.com/repos/DevMicroCore/fOS/contents/Crowpanel_7%22_esp32s3/update"
#define OTA_RAW_BASE "https://raw.githubusercontent.com/DevMicroCore/fOS/main/Crowpanel_7%22_esp32s3/ota/"
#define OTA_RAW_RECOVERY_BASE "https://raw.githubusercontent.com/DevMicroCore/fOS/main/Crowpanel_7%22_esp32s3/update/"
#define OTA_UPDATE_DIR "/system/update"
#define OTA_SD_UPDATE_FILE "/system/update/update.bin"
#define OTA_SD_RECOVERY_FILE "/system/update/recovery.bin"
#define OTA_SD_TEMP_SUFFIX ".part"
static const char * const kRecoveryFallbackNames[] = {
  "recovery.ino.bin",
  "recovery.bin",
  "update.ino.bin",
  "update.bin"
};

struct GithubFileEntry {
  String name;
  String downloadUrl;
  uint32_t size;
};

Preferences gPrefs;
bool gPrefsOpen = false;
uint32_t gBootStartMs = 0;
bool gBootConfirmed = false;
bool gOtaListLoaded = false;
uint32_t gLastListTryMs = 0;
uint32_t gWifiConnectedSinceMs = 0;
volatile bool gWifiBusy = false;
uint8_t gOtaCount = 0;
GithubFileEntry gOtaFiles[OTA_MAX_FILES];
lv_obj_t *gBoundDropdown = nullptr;
TaskHandle_t gListTaskHandle = nullptr;
volatile bool gListTaskRunning = false;
volatile bool gListRequested = false;
volatile bool gListDone = false;
volatile bool gListSuccess = false;
uint8_t gListCount = 0;
GithubFileEntry gListFiles[OTA_MAX_FILES];

TaskHandle_t gInstallTaskHandle = nullptr;
volatile bool gInstallTaskRunning = false;
volatile bool gInstallRequested = false;
volatile bool gInstallDone = false;
volatile bool gInstallSuccess = false;
volatile bool gRebootPending = false;
uint32_t gRebootAtMs = 0;
volatile bool gProgressDirty = false;
volatile uint8_t gProgressValue = 0;
String gLastInstallError = "";

void logLine(const String& line) {
  Serial.println("[OTA] " + line);
}

void setInstallError(const String& err) {
  gLastInstallError = err;
  logLine("ERROR: " + err);
}

String resetReasonToText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

bool hasBinExtension(const String& name) {
  if (name.length() < 4) {
    return false;
  }
  String low = name;
  low.toLowerCase();
  return low.endsWith(".bin");
}

void postProgress(uint8_t value) {
  if (value > 100) {
    value = 100;
  }
  gProgressValue = value;
  gProgressDirty = true;
}

String u64ToString(uint64_t value) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(value));
  return String(buf);
}

void flushProgressToUi() {
  if (!gProgressDirty) {
    return;
  }
  if (uic_InstallProgressBar != nullptr) {
    lv_bar_set_value(uic_InstallProgressBar, gProgressValue, LV_ANIM_OFF);
  }
  gProgressDirty = false;
}

bool fetchGithubListing(const char *apiUrl, GithubFileEntry *entries, uint8_t maxEntries, uint8_t *outCount) {
  if (outCount == nullptr) {
    return false;
  }
  *outCount = 0;

  String body;
  bool ok = false;
  for (uint8_t attempt = 1; attempt <= OTA_HTTP_RETRIES; ++attempt) {
    if (WiFi.status() != WL_CONNECTED) {
      logLine("GitHub API deferred: WiFi disconnected");
      break;
    }

    const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    logLine(
      "TLS heap before try " + String(attempt) + ": free=" +
      String(freeInternal) + ", largest=" + String(largestInternal));
    if (freeInternal < OTA_MIN_TLS_FREE_INTERNAL_HEAP || largestInternal < OTA_MIN_TLS_LARGEST_BLOCK) {
      logLine("GitHub API deferred: insufficient internal TLS heap");
      break;
    }
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(OTA_HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(OTA_HTTP_READ_TIMEOUT_MS);
    http.useHTTP10(true);

    if (!http.begin(client, apiUrl)) {
      logLine("HTTP begin failed for API listing (try " + String(attempt) + ")");
      delay(120);
      continue;
    }

    http.addHeader("User-Agent", "fOS/4.0.0 OTA");
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("Connection", "close");

    const int code = http.GET();
    if (code == HTTP_CODE_OK) {
      body = http.getString();
      http.end();
      if (!body.isEmpty() && body.length() <= OTA_MAX_API_PAYLOAD) {
        ok = true;
        break;
      }
      logLine("API payload empty/large (try " + String(attempt) + ")");
    } else {
      logLine(
        "GitHub API HTTP error: " + String(code) + " (" +
        HTTPClient::errorToString(code) + ", try " + String(attempt) + ")");
    }
    http.end();
    delay(150);
  }

  if (!ok) {
    return false;
  }

  const String nameMarker = "\"name\":\"";
  const String dlMarker = "\"download_url\":\"";
  const String typeMarker = "\"type\":\"";
  const String sizeMarker = "\"size\":";

  size_t scanPos = 0;
  uint8_t count = 0;
  while (count < maxEntries) {
    const int namePos = body.indexOf(nameMarker, static_cast<int>(scanPos));
    if (namePos < 0) {
      break;
    }
    const int nameStart = namePos + nameMarker.length();
    const int nameEnd = body.indexOf('"', nameStart);
    if (nameEnd < 0) {
      break;
    }
    const String nameValue = body.substring(nameStart, nameEnd);

    const int nextNamePos = body.indexOf(nameMarker, nameEnd + 1);
    const int windowEnd = nextNamePos >= 0 ? nextNamePos : body.length();

    const int dlPos = body.indexOf(dlMarker, nameEnd);
    if (dlPos < 0 || dlPos >= windowEnd) {
      scanPos = static_cast<size_t>(nameEnd + 1);
      continue;
    }
    const int dlStart = dlPos + dlMarker.length();
    const int dlEnd = body.indexOf('"', dlStart);
    if (dlEnd < 0 || dlEnd > windowEnd) {
      scanPos = static_cast<size_t>(nameEnd + 1);
      continue;
    }
    const String dlValue = body.substring(dlStart, dlEnd);

    const int typePos = body.indexOf(typeMarker, dlEnd);
    if (typePos < 0 || typePos >= windowEnd) {
      scanPos = static_cast<size_t>(nameEnd + 1);
      continue;
    }
    const int typeStart = typePos + typeMarker.length();
    const int typeEnd = body.indexOf('"', typeStart);
    if (typeEnd < 0 || typeEnd > windowEnd) {
      scanPos = static_cast<size_t>(nameEnd + 1);
      continue;
    }
    const String typeValue = body.substring(typeStart, typeEnd);

    uint32_t sizeValue = 0;
    const int sizePos = body.indexOf(sizeMarker, nameEnd);
    if (sizePos >= 0 && sizePos < windowEnd) {
      int numberStart = sizePos + sizeMarker.length();
      while (numberStart < windowEnd && body[numberStart] == ' ') {
        ++numberStart;
      }
      int numberEnd = numberStart;
      while (numberEnd < windowEnd && isdigit(static_cast<unsigned char>(body[numberEnd]))) {
        ++numberEnd;
      }
      if (numberEnd > numberStart) {
        sizeValue = static_cast<uint32_t>(strtoul(body.substring(numberStart, numberEnd).c_str(), nullptr, 10));
      }
    }

    scanPos = static_cast<size_t>(windowEnd);

    if (typeValue != "file" || nameValue.isEmpty() || dlValue.isEmpty() || !hasBinExtension(nameValue)) {
      continue;
    }

    entries[count].name = nameValue;
    entries[count].downloadUrl = dlValue;
    entries[count].size = sizeValue;
    ++count;
  }

  *outCount = count;
  return count > 0;
}

void sortEntriesByNameDesc(GithubFileEntry *entries, uint8_t count) {
  for (uint8_t i = 0; i < count; ++i) {
    for (uint8_t j = i + 1; j < count; ++j) {
      if (entries[j].name > entries[i].name) {
        const GithubFileEntry tmp = entries[i];
        entries[i] = entries[j];
        entries[j] = tmp;
      }
    }
  }
}

const esp_partition_t *findAppPartitionByLabel(const char *label) {
  return esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,
    ESP_PARTITION_SUBTYPE_ANY,
    label
  );
}

bool validateBinHeader(File& file, size_t maxPartitionSize) {
  if (!file || file.isDirectory()) {
    return false;
  }

  const size_t fileSize = static_cast<size_t>(file.size());
  if (fileSize < OTA_MIN_APP_BIN_SIZE || fileSize > maxPartitionSize) {
    return false;
  }

  uint8_t header[8] = {0};
  if (!file.seek(0)) {
    return false;
  }
  const size_t readCount = file.read(header, sizeof(header));
  if (readCount != sizeof(header)) {
    return false;
  }
  if (!file.seek(0)) {
    return false;
  }

  if (header[0] != 0xE9) {
    return false;
  }

  const uint8_t segmentCount = header[1];
  if (segmentCount == 0 || segmentCount > 16) {
    return false;
  }

  return true;
}

void removeIfExists(const char *path) {
  if (SD.exists(path)) {
    SD.remove(path);
  }
}

bool writeAllWithRetries(File& out, const uint8_t *data, size_t len) {
  size_t offset = 0;
  uint8_t retryCount = 0;

  while (offset < len) {
    const size_t written = out.write(data + offset, len - offset);
    if (written > 0) {
      offset += written;
      retryCount = 0;
      continue;
    }

    if (retryCount >= OTA_SD_WRITE_RETRIES) {
      return false;
    }
    ++retryCount;
    delay(2);
  }
  return true;
}

bool downloadUrlToSdFile(const String& url,
                         const char *finalPath,
                         uint8_t progressStart,
                         uint8_t progressEnd,
                         const char *phaseText) {
  const String tempPath = String(finalPath) + OTA_SD_TEMP_SUFFIX;
  removeIfExists(tempPath.c_str());
  removeIfExists(finalPath);

  for (uint8_t attempt = 1; attempt <= OTA_HTTP_RETRIES; ++attempt) {
    removeIfExists(tempPath.c_str());
    removeIfExists(finalPath);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(OTA_HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(OTA_HTTP_READ_TIMEOUT_MS);
    http.useHTTP10(true);

    if (!http.begin(client, url)) {
      setInstallError(String(phaseText) + ": HTTP begin failed (try " + String(attempt) + ")");
      delay(150);
      continue;
    }

    http.addHeader("User-Agent", "fOS/4.0.0 OTA");
    http.addHeader("Connection", "close");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
      setInstallError(String(phaseText) + ": HTTP code " + String(code) + " (try " + String(attempt) + ")");
      http.end();
      delay(150);
      continue;
    }

    WiFiClient *stream = http.getStreamPtr();
    const int contentLen = http.getSize();
    File out = SD.open(tempPath.c_str(), FILE_WRITE);
    if (!out) {
      setInstallError(String(phaseText) + ": SD open failed");
      http.end();
      return false;
    }

    if (contentLen > 0) {
      uint64_t total = SD.totalBytes();
      uint64_t used = SD.usedBytes();
      if (total > 0 && used <= total) {
        uint64_t freeBytes = total - used;
        uint64_t needed = static_cast<uint64_t>(contentLen) + OTA_SD_RESERVE_BYTES;
        logLine("SD free=" + u64ToString(freeBytes) + " bytes, needed~" + u64ToString(needed) + " bytes");
        if (freeBytes < needed) {
          out.close();
          http.end();
          setInstallError("Not enough SD free space for download");
          return false;
        }
      }
    }

    uint8_t buffer[1024];
    size_t written = 0;
    int remaining = contentLen;
    bool transferFailed = false;
    uint8_t lastProgress = progressStart;
    postProgress(progressStart);

    while (http.connected() && (remaining > 0 || remaining == -1)) {
      const size_t availableBytes = stream->available();
      if (availableBytes == 0) {
        delay(2);
        continue;
      }

      const size_t chunk = availableBytes > sizeof(buffer) ? sizeof(buffer) : availableBytes;
      const int readLen = stream->readBytes(buffer, chunk);
      if (readLen <= 0) {
        break;
      }

      if (!writeAllWithRetries(out, buffer, static_cast<size_t>(readLen))) {
        out.close();
        http.end();
        setInstallError(String(phaseText) + ": SD write failed (try " + String(attempt) + ")");
        removeIfExists(tempPath.c_str());
        SD.begin(OTA_SD_CS);
        delay(150);
        transferFailed = true;
        break;
      }

      written += static_cast<size_t>(readLen);
      if (remaining > 0) {
        remaining -= readLen;
      }

      if (contentLen > 0) {
        const uint8_t p = static_cast<uint8_t>(
          progressStart + ((uint64_t)(progressEnd - progressStart) * written) / static_cast<uint64_t>(contentLen)
        );
        if (p != lastProgress) {
          postProgress(p);
          lastProgress = p;
        }
      }
    }

    out.flush();
    out.close();
    http.end();

    if (transferFailed) {
      continue;
    }

    if (contentLen > 0 && written != static_cast<size_t>(contentLen)) {
      setInstallError(String(phaseText) + ": incomplete download (try " + String(attempt) + ")");
      removeIfExists(tempPath.c_str());
      SD.begin(OTA_SD_CS);
      delay(150);
      continue;
    }

    if (!SD.exists(tempPath) || written == 0) {
      setInstallError(String(phaseText) + ": no data (try " + String(attempt) + ")");
      removeIfExists(tempPath.c_str());
      SD.begin(OTA_SD_CS);
      delay(150);
      continue;
    }

    if (!SD.rename(tempPath.c_str(), finalPath)) {
      setInstallError(String(phaseText) + ": SD rename failed");
      removeIfExists(tempPath.c_str());
      return false;
    }

    postProgress(progressEnd);
    logLine(String(phaseText) + ": downloaded " + String(written) + " bytes");
    return true;
  }

  return false;
}

class ProgressFileStream : public Stream {
public:
  ProgressFileStream(File& file, size_t total, uint8_t pStart, uint8_t pEnd)
  : _file(file), _total(total), _pStart(pStart), _pEnd(pEnd), _consumed(0), _lastProgress(pStart) {}

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
  uint8_t _pStart;
  uint8_t _pEnd;
  size_t _consumed;
  uint8_t _lastProgress;

  void advance(size_t delta) {
    _consumed += delta;
    if (_total == 0) {
      return;
    }
    const uint8_t p = static_cast<uint8_t>(
      _pStart + ((uint64_t)(_pEnd - _pStart) * _consumed) / static_cast<uint64_t>(_total)
    );
    if (p != _lastProgress) {
      postProgress(p);
      _lastProgress = p;
    }
  }
};

bool flashPartitionFromSd(const char *sdPath,
                          const char *partitionLabel,
                          uint8_t progressStart,
                          uint8_t progressEnd) {
  const esp_partition_t *target = findAppPartitionByLabel(partitionLabel);
  if (target == nullptr) {
    setInstallError(String("Partition not found: ") + partitionLabel);
    return false;
  }

  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running != nullptr && strcmp(running->label, partitionLabel) == 0) {
    setInstallError("Refusing to flash running partition");
    return false;
  }

  File in = SD.open(sdPath, FILE_READ);
  if (!in) {
    setInstallError(String("SD file missing: ") + sdPath);
    return false;
  }

  const size_t imageSize = static_cast<size_t>(in.size());
  if (!validateBinHeader(in, target->size)) {
    setInstallError(String("Invalid image for ") + partitionLabel);
    in.close();
    return false;
  }

  if (!Update.begin(imageSize, U_FLASH, -1, LOW, partitionLabel)) {
    setInstallError(String("Update.begin failed for ") + partitionLabel + " err=" + String(Update.getError()));
    in.close();
    return false;
  }

  postProgress(progressStart);
  ProgressFileStream stream(in, imageSize, progressStart, progressEnd);
  const size_t written = Update.writeStream(stream);
  const bool okEnd = Update.end(true);

  in.close();

  if (written != imageSize || !okEnd) {
    setInstallError(String("Flashing failed for ") + partitionLabel + " written=" + String(written) +
                    " size=" + String(imageSize) + " err=" + String(Update.getError()));
    Update.abort();
    return false;
  }

  postProgress(progressEnd);
  logLine(String("Flashed ") + partitionLabel + " with " + String(written) + " bytes");
  return true;
}

void setDropdownFallback(const char *text) {
  if (uic_DropdownUpdate == nullptr) {
    return;
  }
  lv_dropdown_set_options(uic_DropdownUpdate, text);
}

bool getSelectedOtaFile(String *nameOut, String *urlOut) {
  if (nameOut == nullptr || urlOut == nullptr) {
    return false;
  }
  if (uic_DropdownUpdate == nullptr || gOtaCount == 0) {
    return false;
  }

  const uint16_t idx = lv_dropdown_get_selected(uic_DropdownUpdate);
  if (idx >= gOtaCount) {
    return false;
  }

  *nameOut = gOtaFiles[idx].name;
  *urlOut = gOtaFiles[idx].downloadUrl;
  if (urlOut->isEmpty()) {
    *urlOut = String(OTA_RAW_BASE) + *nameOut;
  }
  return true;
}

bool ensurePrefsOpen() {
  if (gPrefsOpen) {
    return true;
  }
  gPrefsOpen = gPrefs.begin(OTA_PREFS_NAMESPACE, false);
  if (!gPrefsOpen) {
    logLine("Preferences begin failed");
  }
  return gPrefsOpen;
}

void setPendingUpdateState(bool pending, uint8_t bootCounter) {
  if (!ensurePrefsOpen()) {
    return;
  }
  gPrefs.putBool(OTA_KEY_PENDING, pending);
  gPrefs.putUChar(OTA_KEY_BOOT_COUNTER, bootCounter);
}

bool getPendingUpdate() {
  if (!ensurePrefsOpen()) {
    return false;
  }
  return gPrefs.getBool(OTA_KEY_PENDING, false);
}

uint8_t getBootCounter() {
  if (!ensurePrefsOpen()) {
    return 0;
  }
  return gPrefs.getUChar(OTA_KEY_BOOT_COUNTER, 0);
}

void markBootSuccessful() {
  if (!ensurePrefsOpen()) {
    return;
  }
  const esp_err_t rb = esp_ota_mark_app_valid_cancel_rollback();
  if (rb != ESP_OK) {
#ifdef ESP_ERR_OTA_ROLLBACK_INVALID_STATE
    if (rb != ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
      logLine("esp_ota_mark_app_valid_cancel_rollback failed: " + String(static_cast<int>(rb)));
    }
#else
    logLine("esp_ota_mark_app_valid_cancel_rollback failed: " + String(static_cast<int>(rb)));
#endif
  }
  gPrefs.putUChar(OTA_KEY_BOOT_COUNTER, 0);
  gPrefs.putBool(OTA_KEY_PENDING, false);
  gBootConfirmed = true;
  logLine("Boot confirmed as stable");
}

void forceBootRecoveryPartition() {
  const esp_partition_t *app1 = findAppPartitionByLabel("app1");
  if (app1 == nullptr) {
    logLine("Cannot switch to recovery: app1 partition missing");
    return;
  }
  const esp_err_t setErr = esp_ota_set_boot_partition(app1);
  if (setErr != ESP_OK) {
    logLine("esp_ota_set_boot_partition(app1) failed: " + String(static_cast<int>(setErr)));
    return;
  }
  logLine("Boot partition switched to app1 recovery");
  delay(250);
  esp_restart();
}

void handleBootPolicy() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  logLine(String("Running partition: ") + (running ? running->label : "unknown"));
  logLine(String("Boot partition   : ") + (boot ? boot->label : "unknown"));
  logLine(String("Reset reason     : ") + resetReasonToText(esp_reset_reason()));

  if (running == nullptr || strcmp(running->label, "app0") != 0) {
    return;
  }

  if (!ensurePrefsOpen()) {
    return;
  }

  if (!getPendingUpdate()) {
    gPrefs.putUChar(OTA_KEY_BOOT_COUNTER, 0);
    return;
  }

  const uint8_t current = getBootCounter();
  const uint8_t next = static_cast<uint8_t>(current + 1);
  gPrefs.putUChar(OTA_KEY_BOOT_COUNTER, next);
  logLine("Pending update boot attempt: " + String(next) + "/" + String(OTA_MAX_BOOT_ATTEMPTS));

  if (next >= OTA_MAX_BOOT_ATTEMPTS) {
    logLine("Boot attempts exceeded, switching to recovery");
    gPrefs.putUChar(OTA_KEY_BOOT_COUNTER, 0);
    forceBootRecoveryPartition();
  }
}

void otaListTaskMain(void *param) {
  (void)param;

  uint8_t count = 0;
  gListSuccess = false;
  gListCount = 0;

  if (fetchGithubListing(OTA_API_LIST, gListFiles, OTA_MAX_FILES, &count)) {
    sortEntriesByNameDesc(gListFiles, count);
    gListCount = count;
    gListSuccess = (count > 0);
  }

  gListDone = true;
  gListTaskRunning = false;
  gListTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

bool startOtaListTask() {
  if (gListTaskRunning) {
    return true;
  }

  gListDone = false;
  gListSuccess = false;
  gListCount = 0;
  gListTaskRunning = true;

  BaseType_t created = xTaskCreatePinnedToCore(
    otaListTaskMain,
    "ota_list",
    8192,
    nullptr,
    1,
    &gListTaskHandle,
    1
  );

  if (created != pdPASS) {
    gListTaskRunning = false;
    gListTaskHandle = nullptr;
    logLine("Failed to create OTA list task");
    return false;
  }

  return true;
}

void renderCachedOtaListToUi() {
  if (uic_DropdownUpdate == nullptr || !gOtaListLoaded || gOtaCount == 0) return;
  String options;
  options.reserve(1024);
  for (uint8_t i = 0; i < gOtaCount; ++i) {
    options += gOtaFiles[i].name;
    if (i + 1 < gOtaCount) {
      options += "\n";
    }
  }
  lv_dropdown_set_options(uic_DropdownUpdate, options.c_str());
  lv_dropdown_set_selected(uic_DropdownUpdate, 0);
  gBoundDropdown = uic_DropdownUpdate;
}

void applyFetchedOtaListToUi() {
  if (!gListSuccess || gListCount == 0) {
    if (uic_DropdownUpdate != nullptr) {
      setDropdownFallback(WiFi.status() == WL_CONNECTED
        ? "OTA connection failed - retrying"
        : "WiFi disconnected - OTA waiting");
    }
    gOtaCount = 0;
    gOtaListLoaded = false;
    return;
  }

  gOtaCount = gListCount;
  for (uint8_t i = 0; i < gListCount; ++i) {
    gOtaFiles[i] = gListFiles[i];
  }
  gOtaListLoaded = true;
  logLine("Loaded " + String(gOtaCount) + " OTA files");
  renderCachedOtaListToUi();
}

bool downloadNewestRecoveryToSd() {
  GithubFileEntry recoveryFiles[OTA_MAX_RECOVERY_FILES];
  uint8_t recoveryCount = 0;
  if (!fetchGithubListing(OTA_API_RECOVERY_LIST, recoveryFiles, OTA_MAX_RECOVERY_FILES, &recoveryCount)) {
    logLine("Recovery listing unavailable, trying RAW fallback names");
    const size_t fallbackCount = sizeof(kRecoveryFallbackNames) / sizeof(kRecoveryFallbackNames[0]);
    for (size_t i = 0; i < fallbackCount; ++i) {
      const String url = String(OTA_RAW_RECOVERY_BASE) + kRecoveryFallbackNames[i];
      if (downloadUrlToSdFile(url, OTA_SD_RECOVERY_FILE, 70, 85, "Download recovery fallback")) {
        logLine("Recovery fallback success: " + String(kRecoveryFallbackNames[i]));
        return true;
      }
    }
    setInstallError("Recovery listing unavailable");
    return false;
  }

  sortEntriesByNameDesc(recoveryFiles, recoveryCount);
  const String recoveryName = recoveryFiles[0].name;
  String recoveryUrl = recoveryFiles[0].downloadUrl;
  if (recoveryUrl.isEmpty()) {
    recoveryUrl = String(OTA_RAW_RECOVERY_BASE) + recoveryName;
  }

  logLine("Selected recovery image: " + recoveryName);
  return downloadUrlToSdFile(recoveryUrl, OTA_SD_RECOVERY_FILE, 70, 85, "Download recovery");
}

bool ensureSdMounted() {
  if (SD.begin(OTA_SD_CS)) {
    return true;
  }
  setInstallError("SD init failed");
  return false;
}

bool ensureUpdateDirectory() {
  if (SD.exists(OTA_UPDATE_DIR)) {
    return true;
  }
  if (SD.mkdir(OTA_UPDATE_DIR)) {
    return true;
  }
  setInstallError("Cannot create /system/update");
  return false;
}

bool executeInstallFlow() {
  gLastInstallError = "";

  if (!ensureSdMounted()) {
    return false;
  }
  if (!ensureUpdateDirectory()) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setInstallError("Install aborted: WiFi not connected");
    return false;
  }

  String selectedName;
  String selectedUrl;
  if (!getSelectedOtaFile(&selectedName, &selectedUrl)) {
    setInstallError("Install aborted: no OTA file selected");
    return false;
  }

  logLine("Selected OTA image: " + selectedName);
  postProgress(1);

  if (!downloadUrlToSdFile(selectedUrl, OTA_SD_UPDATE_FILE, 1, 70, "Download app0 update")) {
    return false;
  }

  if (!downloadNewestRecoveryToSd()) {
    return false;
  }

  const esp_partition_t *app1 = findAppPartitionByLabel("app1");
  if (app1 == nullptr) {
    setInstallError("app1 partition not found");
    return false;
  }

  {
    File recoveryBin = SD.open(OTA_SD_RECOVERY_FILE, FILE_READ);
    if (!recoveryBin) {
      setInstallError("Missing recovery.bin after download");
      return false;
    }
    const bool validRecovery = validateBinHeader(recoveryBin, app1->size);
    recoveryBin.close();
    if (!validRecovery) {
      setInstallError("Recovery image validation failed");
      return false;
    }
  }

  if (!flashPartitionFromSd(OTA_SD_RECOVERY_FILE, "app1", 85, 97)) {
    return false;
  }

  setPendingUpdateState(true, 0);

  const esp_partition_t *bootBefore = esp_ota_get_boot_partition();
  logLine(String("Boot before switch: ") + (bootBefore ? bootBefore->label : "unknown"));

  const esp_err_t err = esp_ota_set_boot_partition(app1);
  if (err != ESP_OK) {
    setInstallError("Failed setting boot partition to app1: " + String(static_cast<int>(err)));
    return false;
  }

  const esp_partition_t *bootAfter = esp_ota_get_boot_partition();
  logLine(String("Boot after switch: ") + (bootAfter ? bootAfter->label : "unknown"));

  postProgress(100);
  delay(300);
  return true;
}

void installTaskMain(void *param) {
  (void)param;
  bool ok = executeInstallFlow();
  gInstallSuccess = ok;
  gInstallDone = true;
  gInstallTaskRunning = false;
  gInstallTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

bool startInstallTask() {
  if (gInstallTaskRunning) {
    return true;
  }

  gInstallDone = false;
  gInstallSuccess = false;
  gInstallTaskRunning = true;

  BaseType_t created = xTaskCreatePinnedToCore(
    installTaskMain,
    "ota_install",
    10240,
    nullptr,
    1,
    &gInstallTaskHandle,
    1
  );

  if (created != pdPASS) {
    gInstallTaskRunning = false;
    gInstallTaskHandle = nullptr;
    setInstallError("Failed to create install task");
    return false;
  }

  return true;
}

}  // namespace

void OTARecovery_Init(void) {
  gBootStartMs = millis();
  gBootConfirmed = false;
  gLastListTryMs = millis() - OTA_LIST_RETRY_MS;
  gWifiConnectedSinceMs = 0;
  gWifiBusy = false;
  gListRequested = false;
  gListDone = false;
  gListSuccess = false;
  gListTaskRunning = false;
  gBoundDropdown = nullptr;

  handleBootPolicy();

  if (uic_DropdownUpdate != nullptr) {
    setDropdownFallback("Loading OTA list...");
  }
  postProgress(0);
  flushProgressToUi();
}

void OTARecovery_Tick(void) {
  flushProgressToUi();

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (!wifiConnected || gWifiBusy) {
    gWifiConnectedSinceMs = 0;
  } else if (gWifiConnectedSinceMs == 0) {
    gWifiConnectedSinceMs = millis();
  }
  const bool wifiStable = wifiConnected && !gWifiBusy && gWifiConnectedSinceMs != 0 &&
    (millis() - gWifiConnectedSinceMs) >= OTA_WIFI_STABLE_BEFORE_TLS_MS;

  if (uic_DropdownUpdate == nullptr) {
    gBoundDropdown = nullptr;
  } else if (gBoundDropdown != uic_DropdownUpdate) {
    gBoundDropdown = uic_DropdownUpdate;
    if (gOtaListLoaded) {
      renderCachedOtaListToUi();
    } else {
      setDropdownFallback(WiFi.status() == WL_CONNECTED
        ? "Loading OTA list..."
        : "WiFi disconnected - OTA waiting");
      gLastListTryMs = millis() - OTA_LIST_RETRY_MS;
    }
  }

  if (gInstallRequested && !gInstallTaskRunning) {
    gInstallRequested = false;
    if (!startInstallTask()) {
      postProgress(0);
      logLine("Install start failed");
    }
  }

  if (gInstallDone) {
    gInstallDone = false;
    if (!gInstallSuccess) {
      postProgress(0);
      if (gLastInstallError.length() > 0) {
        logLine("InstallUpdate failed: " + gLastInstallError);
      } else {
        logLine("InstallUpdate failed: unknown");
      }
    } else {
      gRebootPending = true;
      gRebootAtMs = millis() + 20;
      logLine("InstallUpdate complete -> reboot into recovery");
    }
  }

  if (gRebootPending && millis() >= gRebootAtMs) {
    gRebootPending = false;
    esp_restart();
  }

  if (!gOtaListLoaded && wifiStable && (millis() - gLastListTryMs) >= OTA_LIST_RETRY_MS && !gListTaskRunning) {
    gLastListTryMs = millis();
    gListRequested = true;
    if (uic_DropdownUpdate != nullptr) {
      setDropdownFallback("Loading OTA list...");
    }
  }

  if (gListRequested && !gListTaskRunning) {
    gListRequested = false;
    startOtaListTask();
  }

  if (gListDone) {
    gListDone = false;
    applyFetchedOtaListToUi();
  }

  if (!gBootConfirmed && getPendingUpdate()) {
    if ((millis() - gBootStartMs) >= OTA_VALIDATION_DELAY_MS) {
      markBootSuccessful();
    }
  }
}

void OTARecovery_InstallUpdateEvent(lv_event_t * e) {
  if (e == nullptr || lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  if (gInstallTaskRunning || gInstallRequested) {
    logLine("Install already running");
    return;
  }

  postProgress(0);
  gInstallRequested = true;
  logLine("Install requested");
}

bool OTARecovery_IsBusy(void) {
  return gInstallTaskRunning || gInstallRequested || gRebootPending;
}

bool OTARecovery_IsNetworkBusy(void) {
  return gWifiBusy || gListTaskRunning || gInstallTaskRunning || gInstallRequested || gRebootPending;
}

void OTARecovery_SetWifiBusy(bool busy) {
  gWifiBusy = busy;
  if (busy) {
    gWifiConnectedSinceMs = 0;
  }
}
