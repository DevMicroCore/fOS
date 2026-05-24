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
#include <esp_partition.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ui_Update.h"

namespace {

constexpr int kSdCs = 10;
constexpr uint8_t kMaxOtaFiles = 24;
constexpr uint8_t kMaxRecoveryFiles = 16;
constexpr uint8_t kMaxBootAttempts = 3;
constexpr uint32_t kValidationDelayMs = 30000UL;
constexpr uint32_t kListRetryMs = 15000UL;
constexpr uint16_t kHttpConnectTimeoutMs = 15000;
constexpr uint16_t kHttpReadTimeoutMs = 30000;
constexpr uint8_t kHttpRetries = 3;
constexpr size_t kMinAppBinSize = 32 * 1024;
constexpr size_t kMaxApiPayload = 32 * 1024;

constexpr const char *kPrefsNamespace = "ota_state";
constexpr const char *kKeyPending = "pending_update";
constexpr const char *kKeyBootCounter = "boot_attempt_counter";

constexpr const char *kApiOtaList = "https://api.github.com/repos/DevMicroCore/fOS/contents/Crowpanel_7%22_esp32s3/ota";
constexpr const char *kApiRecoveryList = "https://api.github.com/repos/DevMicroCore/fOS/contents/Crowpanel_7%22_esp32s3/update";
constexpr const char *kRawOtaBase = "https://raw.githubusercontent.com/DevMicroCore/fOS/main/Crowpanel_7%22_esp32s3/ota/";
constexpr const char *kRawRecoveryBase = "https://raw.githubusercontent.com/DevMicroCore/fOS/main/Crowpanel_7%22_esp32s3/update/";

constexpr const char *kUpdateDir = "/system/update";
constexpr const char *kSdUpdateFile = "/system/update/update.bin";
constexpr const char *kSdRecoveryFile = "/system/update/recovery.bin";
constexpr const char *kSdTempSuffix = ".part";
constexpr const char *kRecoveryFallbackNames[] = {
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
uint8_t gOtaCount = 0;
GithubFileEntry gOtaFiles[kMaxOtaFiles];
TaskHandle_t gListTaskHandle = nullptr;
volatile bool gListTaskRunning = false;
volatile bool gListRequested = false;
volatile bool gListDone = false;
volatile bool gListSuccess = false;
uint8_t gListCount = 0;
GithubFileEntry gListFiles[kMaxOtaFiles];

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
  for (uint8_t attempt = 1; attempt <= kHttpRetries; ++attempt) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(kHttpConnectTimeoutMs);
    http.setTimeout(kHttpReadTimeoutMs);
    http.useHTTP10(true);

    if (!http.begin(client, apiUrl)) {
      logLine("HTTP begin failed for API listing (try " + String(attempt) + ")");
      delay(120);
      continue;
    }

    http.addHeader("User-Agent", "fOS2.0-OTA");
    http.addHeader("Accept", "application/vnd.github+json");

    const int code = http.GET();
    if (code == HTTP_CODE_OK) {
      body = http.getString();
      http.end();
      if (!body.isEmpty() && body.length() <= kMaxApiPayload) {
        ok = true;
        break;
      }
      logLine("API payload empty/large (try " + String(attempt) + ")");
    } else {
      logLine("GitHub API HTTP error: " + String(code) + " (try " + String(attempt) + ")");
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
  if (fileSize < kMinAppBinSize || fileSize > maxPartitionSize) {
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

bool downloadUrlToSdFile(const String& url,
                         const char *finalPath,
                         uint8_t progressStart,
                         uint8_t progressEnd,
                         const char *phaseText) {
  const String tempPath = String(finalPath) + kSdTempSuffix;
  removeIfExists(tempPath.c_str());
  removeIfExists(finalPath);

  for (uint8_t attempt = 1; attempt <= kHttpRetries; ++attempt) {
    removeIfExists(tempPath.c_str());
    removeIfExists(finalPath);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(kHttpConnectTimeoutMs);
    http.setTimeout(kHttpReadTimeoutMs);
    http.useHTTP10(true);

    if (!http.begin(client, url)) {
      setInstallError(String(phaseText) + ": HTTP begin failed (try " + String(attempt) + ")");
      delay(150);
      continue;
    }

    http.addHeader("User-Agent", "fOS2.0-OTA");
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

    uint8_t buffer[2048];
    size_t written = 0;
    int remaining = contentLen;
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

      const size_t writeLen = out.write(buffer, static_cast<size_t>(readLen));
      if (writeLen != static_cast<size_t>(readLen)) {
        out.close();
        http.end();
        setInstallError(String(phaseText) + ": SD write failed");
        removeIfExists(tempPath.c_str());
        return false;
      }

      written += writeLen;
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

    if (!SD.exists(tempPath) || written == 0) {
      setInstallError(String(phaseText) + ": no data (try " + String(attempt) + ")");
      removeIfExists(tempPath.c_str());
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
    *urlOut = String(kRawOtaBase) + *nameOut;
  }
  return true;
}

bool ensurePrefsOpen() {
  if (gPrefsOpen) {
    return true;
  }
  gPrefsOpen = gPrefs.begin(kPrefsNamespace, false);
  if (!gPrefsOpen) {
    logLine("Preferences begin failed");
  }
  return gPrefsOpen;
}

void setPendingUpdateState(bool pending, uint8_t bootCounter) {
  if (!ensurePrefsOpen()) {
    return;
  }
  gPrefs.putBool(kKeyPending, pending);
  gPrefs.putUChar(kKeyBootCounter, bootCounter);
}

bool getPendingUpdate() {
  if (!ensurePrefsOpen()) {
    return false;
  }
  return gPrefs.getBool(kKeyPending, false);
}

uint8_t getBootCounter() {
  if (!ensurePrefsOpen()) {
    return 0;
  }
  return gPrefs.getUChar(kKeyBootCounter, 0);
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
  gPrefs.putUChar(kKeyBootCounter, 0);
  gPrefs.putBool(kKeyPending, false);
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
    gPrefs.putUChar(kKeyBootCounter, 0);
    return;
  }

  const uint8_t current = getBootCounter();
  const uint8_t next = static_cast<uint8_t>(current + 1);
  gPrefs.putUChar(kKeyBootCounter, next);
  logLine("Pending update boot attempt: " + String(next) + "/" + String(kMaxBootAttempts));

  if (next >= kMaxBootAttempts) {
    logLine("Boot attempts exceeded, switching to recovery");
    gPrefs.putUChar(kKeyBootCounter, 0);
    forceBootRecoveryPartition();
  }
}

void otaListTaskMain(void *param) {
  (void)param;

  uint8_t count = 0;
  gListSuccess = false;
  gListCount = 0;

  if (fetchGithubListing(kApiOtaList, gListFiles, kMaxOtaFiles, &count)) {
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

void applyFetchedOtaListToUi() {
  if (uic_DropdownUpdate == nullptr) {
    return;
  }

  if (!gListSuccess || gListCount == 0) {
    setDropdownFallback("No OTA files found");
    gOtaCount = 0;
    gOtaListLoaded = false;
    return;
  }

  gOtaCount = gListCount;
  for (uint8_t i = 0; i < gListCount; ++i) {
    gOtaFiles[i] = gListFiles[i];
  }
  gOtaListLoaded = true;

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
  logLine("Loaded " + String(gOtaCount) + " OTA files");
}

bool downloadNewestRecoveryToSd() {
  GithubFileEntry recoveryFiles[kMaxRecoveryFiles];
  uint8_t recoveryCount = 0;
  if (!fetchGithubListing(kApiRecoveryList, recoveryFiles, kMaxRecoveryFiles, &recoveryCount)) {
    logLine("Recovery listing unavailable, trying RAW fallback names");
    const size_t fallbackCount = sizeof(kRecoveryFallbackNames) / sizeof(kRecoveryFallbackNames[0]);
    for (size_t i = 0; i < fallbackCount; ++i) {
      const String url = String(kRawRecoveryBase) + kRecoveryFallbackNames[i];
      if (downloadUrlToSdFile(url, kSdRecoveryFile, 70, 85, "Download recovery fallback")) {
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
    recoveryUrl = String(kRawRecoveryBase) + recoveryName;
  }

  logLine("Selected recovery image: " + recoveryName);
  return downloadUrlToSdFile(recoveryUrl, kSdRecoveryFile, 70, 85, "Download recovery");
}

bool ensureSdMounted() {
  if (SD.begin(kSdCs)) {
    return true;
  }
  setInstallError("SD init failed");
  return false;
}

bool ensureUpdateDirectory() {
  if (SD.exists(kUpdateDir)) {
    return true;
  }
  if (SD.mkdir(kUpdateDir)) {
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

  if (!downloadUrlToSdFile(selectedUrl, kSdUpdateFile, 1, 70, "Download app0 update")) {
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
    File recoveryBin = SD.open(kSdRecoveryFile, FILE_READ);
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

  if (!flashPartitionFromSd(kSdRecoveryFile, "app1", 85, 97)) {
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
  gLastListTryMs = millis() - kListRetryMs;
  gListRequested = false;
  gListDone = false;
  gListSuccess = false;
  gListTaskRunning = false;

  handleBootPolicy();

  if (uic_DropdownUpdate != nullptr) {
    setDropdownFallback("Loading OTA list...");
  }
  postProgress(0);
  flushProgressToUi();
}

void OTARecovery_Tick(void) {
  flushProgressToUi();

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

  if (!gOtaListLoaded && WiFi.status() == WL_CONNECTED && (millis() - gLastListTryMs) >= kListRetryMs && !gListTaskRunning) {
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
    if ((millis() - gBootStartMs) >= kValidationDelayMs) {
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
