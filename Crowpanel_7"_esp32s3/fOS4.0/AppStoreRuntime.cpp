#include "AppStoreRuntime.h"

#include <ctype.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ui.h"
#include "ui_AppStore.h"
#include "src/core/DefaultAppStoreIndex.h"
#include "src/core/FOSVersion.h"
#include "src/fapp/FAppManifest.h"

namespace {

#define APPSTORE_SYSTEM_DIRECTORY "/system/apps"
#define APPSTORE_STORES_FILE "/system/apps/stores.txt"
#define APPSTORE_STORES_TEMP_FILE "/system/apps/stores.tmp"
#define APPSTORE_STORES_BACKUP_FILE "/system/apps/stores.bak"
#define APPSTORE_SLOTS_FILE "/system/apps/slots.txt"
#define APPSTORE_SLOTS_TEMP_FILE "/system/apps/slots.tmp"
#define APPSTORE_APPS_DIRECTORY "/apps"
#define APPSTORE_DEFAULT_STORE \
  "https://github.com/DevMicroCore/fOS/tree/main/apps"
#define APPSTORE_DEFAULT_INDEX \
  "https://raw.githubusercontent.com/DevMicroCore/fOS/main/apps/fos-appstore.index"
#define APPSTORE_DEFAULT_API_FILE_BASE \
  "https://api.github.com/repos/DevMicroCore/fOS/contents/apps/"
#define APPSTORE_DEFAULT_API_REF "?ref=main"
#define APPSTORE_LEGACY_DEFAULT_STORE \
  "https://github.com/DevMicroCore/fOS/tree/main/Crowpanel_7%22_esp32s3/example%20app/apps"
#define APPSTORE_INDEX_FILE "fos-appstore.index"
#define APPSTORE_INDEX_HEADER "FOS_APPSTORE_INDEX_V1"
#define APPSTORE_INDEX_SOURCE_PREFIX "index:"
#define APPSTORE_LAUNCHER_SLOT_COUNT 7U
#define APPSTORE_MAX_REMOTE_APPS 40U
#define APPSTORE_MAX_INSTALLED_APPS 40U
#define APPSTORE_MAX_STORE_URLS 8U
#define APPSTORE_MAX_DOWNLOAD_DEPTH 5U
#define APPSTORE_HTTP_RETRIES 2U
#define APPSTORE_REFRESH_TASK_STACK 8192U

struct RemoteAppEntry {
  String folderName;
  String displayName;
  String version;
  String apiUrl;
  uint32_t sizeBytes;
  bool installed;
  String installedVersion;
};

struct RemoteVersionEntry {
  String name;
  String apiUrl;
  uint16_t major;
  uint16_t minor;
  uint16_t patch;
};

enum class StoreRefreshState : uint8_t {
  Idle = 0,
  Running,
  Complete,
  Failed
};

struct InstalledAppEntry {
  String folderName;
  String displayName;
  String version;
};

RemoteAppEntry gRemoteApps[APPSTORE_MAX_REMOTE_APPS];
InstalledAppEntry gInstalledApps[APPSTORE_MAX_INSTALLED_APPS];
String gSlotAssignments[APPSTORE_LAUNCHER_SLOT_COUNT];
uint8_t gRemoteAppCount = 0;
uint8_t gInstalledAppCount = 0;
bool gAssignmentsExplicit = false;
lv_obj_t * gStatusLabel = nullptr;
lv_timer_t * gStoreRefreshTimer = nullptr;
uint16_t gDownloadedFileCount = 0;
bool gStorePathChecked = false;
volatile StoreRefreshState gStoreRefreshState = StoreRefreshState::Idle;
volatile uint8_t gRefreshFoldersScanned = 0;
volatile uint8_t gRefreshStoreCount = 0;
volatile uint8_t gRefreshSuccessfulStores = 0;

extern "C" void StartAppLauncher_Data(lv_event_t * e);
extern "C" lv_obj_t * PrepareAppStoreContent_Data(void);

String basenameOf(const String& path)
{
  const int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

bool isSafePathPart(const String& value)
{
  if (value.length() == 0 || value.length() > 80 || value.startsWith(".")) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.') return false;
  }
  return value.indexOf("..") < 0;
}

bool isSafeRelativePath(const String& value)
{
  if (value.length() == 0 || value.startsWith("/") || value.endsWith("/")) return false;
  int start = 0;
  while (start < static_cast<int>(value.length())) {
    const int slash = value.indexOf('/', start);
    const int end = slash >= 0 ? slash : value.length();
    if (!isSafePathPart(value.substring(start, end))) return false;
    if (slash < 0) break;
    start = slash + 1;
  }
  return true;
}

bool tabField(const String& line, uint8_t wanted, String * value)
{
  if (value == nullptr) return false;
  int start = 0;
  uint8_t field = 0;
  while (start <= static_cast<int>(line.length())) {
    const int tab = line.indexOf('\t', start);
    const int end = tab >= 0 ? tab : line.length();
    if (field == wanted) {
      *value = line.substring(start, end);
      return true;
    }
    if (tab < 0) break;
    start = tab + 1;
    ++field;
  }
  value->remove(0);
  return false;
}

String encodeUrlPath(const String& path)
{
  static const char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(path.length() + 8U);
  for (size_t i = 0; i < path.length(); ++i) {
    const uint8_t value = static_cast<uint8_t>(path[i]);
    if (isalnum(value) || value == '-' || value == '_' || value == '.' ||
        value == '~' || value == '/') {
      encoded += static_cast<char>(value);
    } else {
      encoded += '%';
      encoded += hex[value >> 4];
      encoded += hex[value & 0x0F];
    }
  }
  return encoded;
}

String friendlyFolderName(String name)
{
  name.replace('_', ' ');
  name.replace('-', ' ');
  bool upper = true;
  for (size_t i = 0; i < name.length(); ++i) {
    if (upper && isalpha(static_cast<unsigned char>(name[i]))) {
      name.setCharAt(i, static_cast<char>(toupper(static_cast<unsigned char>(name[i]))));
      upper = false;
    } else if (name[i] == ' ') {
      upper = true;
    }
  }
  return name;
}

int compareVersion(
  uint16_t leftMajor, uint16_t leftMinor, uint16_t leftPatch,
  uint16_t rightMajor, uint16_t rightMinor, uint16_t rightPatch)
{
  if (leftMajor != rightMajor) return leftMajor < rightMajor ? -1 : 1;
  if (leftMinor != rightMinor) return leftMinor < rightMinor ? -1 : 1;
  if (leftPatch != rightPatch) return leftPatch < rightPatch ? -1 : 1;
  return 0;
}

bool parseVersion(const String& value, uint16_t * major, uint16_t * minor, uint16_t * patch)
{
  return FAppManifest::parseSemVer(value.c_str(), major, minor, patch);
}

bool compatibleWithCurrentFos(const String& minimumFos)
{
  if (minimumFos.length() == 0) return true;
  uint16_t major = 0;
  uint16_t minor = 0;
  uint16_t patch = 0;
  if (!parseVersion(minimumFos, &major, &minor, &patch)) return false;
  return compareVersion(
    major, minor, patch,
    FOSVersion::kMajor, FOSVersion::kMinor, FOSVersion::kPatch) <= 0;
}

bool extractJsonString(const String& json, const char * key, String * output)
{
  if (key == nullptr || output == nullptr) return false;
  String marker = "\"";
  marker += key;
  marker += "\"";
  int cursor = json.indexOf(marker);
  if (cursor < 0) return false;
  cursor = json.indexOf(':', cursor + marker.length());
  if (cursor < 0) return false;
  ++cursor;
  while (cursor < static_cast<int>(json.length()) && isspace(static_cast<unsigned char>(json[cursor]))) ++cursor;
  if (cursor >= static_cast<int>(json.length()) || json[cursor] != '"') return false;
  ++cursor;

  output->remove(0);
  bool escape = false;
  while (cursor < static_cast<int>(json.length())) {
    const char c = json[cursor++];
    if (escape) {
      if (c == 'n') *output += '\n';
      else if (c == 'r') *output += '\r';
      else if (c == 't') *output += '\t';
      else *output += c;
      escape = false;
    } else if (c == '\\') {
      escape = true;
    } else if (c == '"') {
      return true;
    } else {
      *output += c;
    }
  }
  return false;
}

bool extractJsonUnsigned(const String& json, const char * key, uint32_t * output)
{
  if (key == nullptr || output == nullptr) return false;
  String marker = "\"";
  marker += key;
  marker += "\"";
  int cursor = json.indexOf(marker);
  if (cursor < 0) return false;
  cursor = json.indexOf(':', cursor + marker.length());
  if (cursor < 0) return false;
  ++cursor;
  while (cursor < static_cast<int>(json.length()) && isspace(static_cast<unsigned char>(json[cursor]))) ++cursor;
  if (cursor >= static_cast<int>(json.length()) || !isdigit(static_cast<unsigned char>(json[cursor]))) return false;
  uint64_t value = 0;
  while (cursor < static_cast<int>(json.length()) && isdigit(static_cast<unsigned char>(json[cursor]))) {
    value = value * 10ULL + static_cast<uint8_t>(json[cursor++] - '0');
    if (value > UINT32_MAX) value = UINT32_MAX;
  }
  *output = static_cast<uint32_t>(value);
  return true;
}

bool extractConfigString(const String& config, const char * key, String * output)
{
  if (key == nullptr || output == nullptr) return false;
  int cursor = 0;
  while (cursor <= static_cast<int>(config.length())) {
    int end = config.indexOf('\n', cursor);
    if (end < 0) end = config.length();
    String line = config.substring(cursor, end);
    line.trim();
    cursor = end + 1;
    if (line.length() == 0 || line.startsWith("#")) continue;
    const int separator = line.indexOf('=');
    if (separator <= 0) continue;
    String option = line.substring(0, separator);
    option.trim();
    option.toLowerCase();
    if (option != key) continue;
    String value = line.substring(separator + 1);
    value.trim();
    if (value.length() == 0) return false;
    *output = value;
    return true;
  }
  return false;
}

bool nextJsonObject(const String& json, int * cursor, String * object)
{
  if (cursor == nullptr || object == nullptr) return false;
  int start = json.indexOf('{', *cursor);
  if (start < 0) return false;
  bool inString = false;
  bool escape = false;
  int depth = 0;
  for (int i = start; i < static_cast<int>(json.length()); ++i) {
    const char c = json[i];
    if (inString) {
      if (escape) escape = false;
      else if (c == '\\') escape = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') inString = true;
    else if (c == '{') ++depth;
    else if (c == '}') {
      --depth;
      if (depth == 0) {
        *object = json.substring(start, i + 1);
        *cursor = i + 1;
        return true;
      }
    }
  }
  return false;
}

bool httpGetText(
  const String& url,
  String * payload,
  bool githubRawContent = false,
  int * lastStatus = nullptr)
{
  if (payload == nullptr || WiFi.status() != WL_CONNECTED) return false;
  payload->remove(0);
  if (lastStatus != nullptr) *lastStatus = 0;
  for (uint8_t attempt = 1; attempt <= APPSTORE_HTTP_RETRIES; ++attempt) {
    if (WiFi.status() != WL_CONNECTED) return false;
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(20);
    HTTPClient http;
    http.setConnectTimeout(15000);
    http.setTimeout(18000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.useHTTP10(true);
    http.setUserAgent("fOS-AppStore/1.0");
    if (!http.begin(client, url)) {
      Serial.printf("[APPSTORE] HTTP begin failed (try %u): %s\n", attempt, url.c_str());
      delay(120);
      continue;
    }
    http.addHeader("Accept", githubRawContent
      ? "application/vnd.github.raw+json"
      : "application/vnd.github+json");
    if (url.startsWith("https://api.github.com/")) {
      http.addHeader("X-GitHub-Api-Version", "2022-11-28");
    }
    http.addHeader("Connection", "close");
    const int code = http.GET();
    if (lastStatus != nullptr) *lastStatus = code;
    if (code == HTTP_CODE_OK) {
      *payload = http.getString();
      if (payload->length() > 0) {
        http.end();
        return true;
      }
    } else {
      const String error = HTTPClient::errorToString(code);
      Serial.printf("[APPSTORE] GET failed (%d, %s, try %u): %s\n",
        code, error.c_str(), attempt, url.c_str());
    }
    http.end();
    // Permanent client errors and rate limits must not be retried immediately.
    // GitHub explicitly warns that repeated 403/429 requests can prolong a
    // block. Network errors, timeouts and server errors remain retryable.
    if (code > 0 && code != 408 && code != 429 && code < 500) break;
    if (code == 429) break;
    delay(120);
  }
  return false;
}

bool httpGetGithubFileText(
  const String& downloadUrl,
  const String& apiUrl,
  String * payload)
{
  if (downloadUrl.length() > 0 && httpGetText(downloadUrl, payload)) return true;
  if (apiUrl.length() > 0) {
    Serial.println("[APPSTORE] RAW download failed, retrying through GitHub API.");
    return httpGetText(apiUrl, payload, true);
  }
  return false;
}

String normalizeStoreUrl(String url)
{
  url.trim();
  url.replace(" ", "%20");
  url.replace("\"", "%22");
  while (url.endsWith("/")) url.remove(url.length() - 1);
  if (url.startsWith("https://api.github.com/repos/")) return url;
  if (!url.startsWith("https://github.com/")) return url;

  String tail = url.substring(strlen("https://github.com/"));
  const int firstSlash = tail.indexOf('/');
  if (firstSlash <= 0) return "";
  const int treeMarker = tail.indexOf("/tree/", firstSlash + 1);
  if (treeMarker < 0) return "";
  const String owner = tail.substring(0, firstSlash);
  const String repository = tail.substring(firstSlash + 1, treeMarker);
  String branchAndPath = tail.substring(treeMarker + 6);
  const int pathSlash = branchAndPath.indexOf('/');
  const String branch = pathSlash >= 0 ? branchAndPath.substring(0, pathSlash) : branchAndPath;
  const String path = pathSlash >= 0 ? branchAndPath.substring(pathSlash + 1) : "";
  if (owner.length() == 0 || repository.length() == 0 || branch.length() == 0) return "";

  String api = "https://api.github.com/repos/" + owner + "/" + repository + "/contents";
  if (path.length() > 0) api += "/" + path;
  api += "?ref=" + branch;
  return api;
}

String storeIndexUrl(String url)
{
  url.trim();
  url.replace(" ", "%20");
  url.replace("\"", "%22");
  while (url.endsWith("/")) url.remove(url.length() - 1);
  if (!url.startsWith("https://github.com/")) return "";

  String tail = url.substring(strlen("https://github.com/"));
  const int firstSlash = tail.indexOf('/');
  if (firstSlash <= 0) return "";
  const int treeMarker = tail.indexOf("/tree/", firstSlash + 1);
  if (treeMarker < 0) return "";
  const String owner = tail.substring(0, firstSlash);
  const String repository = tail.substring(firstSlash + 1, treeMarker);
  String branchAndPath = tail.substring(treeMarker + 6);
  const int pathSlash = branchAndPath.indexOf('/');
  const String branch = pathSlash >= 0 ? branchAndPath.substring(0, pathSlash) : branchAndPath;
  const String path = pathSlash >= 0 ? branchAndPath.substring(pathSlash + 1) : "";
  if (owner.length() == 0 || repository.length() == 0 ||
      branch.length() == 0 || path.length() == 0) return "";

  return "https://raw.githubusercontent.com/" + owner + "/" + repository +
    "/" + branch + "/" + path + "/" + APPSTORE_INDEX_FILE;
}

bool loadStoreIndex(const String& indexUrl, String * payload)
{
  if (payload == nullptr || indexUrl.length() == 0) return false;
  int status = 0;
  if (httpGetText(indexUrl, payload, false, &status) &&
      payload->startsWith(APPSTORE_INDEX_HEADER)) return true;

  if (indexUrl == APPSTORE_DEFAULT_INDEX) {
    *payload = FOSDefaultIndexes::kAppStore;
    Serial.printf("[APPSTORE] Online index unavailable (%d); using built-in catalog.\n", status);
    return true;
  }
  return false;
}

void showStatus(const String& text, lv_color_t color = lv_color_hex(0xFFFFFF))
{
  if (gStatusLabel == nullptr) return;
  lv_label_set_text(gStatusLabel, text.c_str());
  lv_obj_set_style_text_color(gStatusLabel, color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(gStatusLabel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(gStatusLabel);
}

void loadAssignments()
{
  for (uint8_t i = 0; i < APPSTORE_LAUNCHER_SLOT_COUNT; ++i) gSlotAssignments[i] = "";
  gAssignmentsExplicit = false;
  if (!SD.exists(APPSTORE_SLOTS_FILE)) return;
  File file = SD.open(APPSTORE_SLOTS_FILE, FILE_READ);
  if (!file) return;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;
    const int separator = line.indexOf('=');
    if (separator <= 0) continue;
    const int slot = line.substring(0, separator).toInt();
    String folder = line.substring(separator + 1);
    folder.trim();
    if (slot < 1 || slot > APPSTORE_LAUNCHER_SLOT_COUNT) continue;
    if (folder.length() > 0 && !isSafePathPart(folder)) continue;
    gSlotAssignments[slot - 1] = folder;
    gAssignmentsExplicit = true;
  }
  file.close();
}

bool saveAssignments()
{
  if (SD.exists(APPSTORE_SLOTS_TEMP_FILE)) SD.remove(APPSTORE_SLOTS_TEMP_FILE);
  File file = SD.open(APPSTORE_SLOTS_TEMP_FILE, FILE_WRITE);
  if (!file) return false;
  file.println("# fOS launcher slot assignments");
  file.println("# Empty value = empty launcher slot");
  for (uint8_t i = 0; i < APPSTORE_LAUNCHER_SLOT_COUNT; ++i) {
    file.print(i + 1);
    file.print('=');
    file.println(gSlotAssignments[i]);
  }
  file.close();
  if (SD.exists(APPSTORE_SLOTS_FILE) && !SD.remove(APPSTORE_SLOTS_FILE)) return false;
  if (!SD.rename(APPSTORE_SLOTS_TEMP_FILE, APPSTORE_SLOTS_FILE)) return false;
  gAssignmentsExplicit = true;
  return true;
}

String readManifestVersion(const String& folderPath, String * displayName)
{
  const String path = folderPath + "/app.json";
  if (!SD.exists(path)) return "0";
  File file = SD.open(path, FILE_READ);
  if (!file) return "0";
  String json;
  while (file.available() && json.length() < 4096) json += static_cast<char>(file.read());
  file.close();
  String value;
  if (displayName != nullptr && extractJsonString(json, "name", &value) && value.length() > 0) {
    *displayName = value;
  }
  if (!extractJsonString(json, "version", &value) || value.length() == 0) return "0";
  return value;
}

String readLegacyAppConfig(const String& folderPath, String * displayName)
{
  const String path = folderPath + "/app.cfg";
  if (!SD.exists(path)) return "0";
  File file = SD.open(path, FILE_READ);
  if (!file) return "0";
  String config;
  while (file.available() && config.length() < 4096) config += static_cast<char>(file.read());
  file.close();
  String value;
  if (displayName != nullptr && extractConfigString(config, "name", &value)) {
    *displayName = value;
  }
  if (!extractConfigString(config, "version", &value)) return "0";
  return value;
}

void scanInstalledApps()
{
  gInstalledAppCount = 0;
  File root = SD.open(APPSTORE_APPS_DIRECTORY);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }
  File entry = root.openNextFile();
  while (entry && gInstalledAppCount < APPSTORE_MAX_INSTALLED_APPS) {
    if (entry.isDirectory()) {
      const String folder = basenameOf(String(entry.name()));
      if (isSafePathPart(folder)) {
        InstalledAppEntry& app = gInstalledApps[gInstalledAppCount++];
        app.folderName = folder;
        app.displayName = friendlyFolderName(folder);
        const String appPath = String(APPSTORE_APPS_DIRECTORY) + "/" + folder;
        app.version = readManifestVersion(appPath, &app.displayName);
        if (app.version == "0") app.version = readLegacyAppConfig(appPath, &app.displayName);
      }
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
}

int installedIndexForFolder(const String& folder)
{
  for (uint8_t i = 0; i < gInstalledAppCount; ++i) {
    if (gInstalledApps[i].folderName == folder) return i;
  }
  return -1;
}

void fillAssignmentDropdowns()
{
  scanInstalledApps();
  if (uic_AppForAssigningRoller != nullptr) {
    String options = "Nothing";
    for (uint8_t i = 0; i < gInstalledAppCount; ++i) options += "\n" + gInstalledApps[i].displayName;
    lv_dropdown_set_options(uic_AppForAssigningRoller, options.c_str());
    lv_dropdown_set_selected(uic_AppForAssigningRoller, 0);
  }
  if (uic_SelectAppSlotRoller != nullptr) {
    lv_dropdown_set_options(uic_SelectAppSlotRoller,
      "Slot 1\nSlot 2\nSlot 3\nSlot 4\nSlot 5\nSlot 6\nSlot 7");
    lv_dropdown_set_selected(uic_SelectAppSlotRoller, 0);
  }
}

void addOrUpdateRemoteApp(const RemoteAppEntry& candidate)
{
  for (uint8_t i = 0; i < gRemoteAppCount; ++i) {
    if (gRemoteApps[i].folderName != candidate.folderName) continue;
    uint16_t oldMajor = 0, oldMinor = 0, oldPatch = 0;
    uint16_t newMajor = 0, newMinor = 0, newPatch = 0;
    if (parseVersion(candidate.version, &newMajor, &newMinor, &newPatch) &&
        (!parseVersion(gRemoteApps[i].version, &oldMajor, &oldMinor, &oldPatch) ||
         compareVersion(newMajor, newMinor, newPatch, oldMajor, oldMinor, oldPatch) > 0)) {
      gRemoteApps[i] = candidate;
    }
    return;
  }
  if (gRemoteAppCount < APPSTORE_MAX_REMOTE_APPS) gRemoteApps[gRemoteAppCount++] = candidate;
}

bool parseIndexedStore(const String& indexUrl, const String& payload)
{
  if (!payload.startsWith(APPSTORE_INDEX_HEADER)) return false;
  bool found = false;
  int cursor = 0;
  while (cursor < static_cast<int>(payload.length())) {
    int end = payload.indexOf('\n', cursor);
    if (end < 0) end = payload.length();
    String line = payload.substring(cursor, end);
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    cursor = end + 1;
    if (!line.startsWith("A\t")) continue;

    String folder;
    String version;
    String minimumFos;
    String displayName;
    String sizeText;
    String remoteDirectory;
    if (!tabField(line, 1, &folder) || !isSafePathPart(folder) ||
        !tabField(line, 2, &version) ||
        !tabField(line, 3, &minimumFos) ||
        !tabField(line, 4, &displayName) ||
        !tabField(line, 5, &sizeText) ||
        !tabField(line, 6, &remoteDirectory) ||
        !isSafeRelativePath(remoteDirectory)) continue;

    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t patch = 0;
    if (!parseVersion(version, &major, &minor, &patch) ||
        !compatibleWithCurrentFos(minimumFos)) continue;

    RemoteAppEntry candidate;
    candidate.folderName = folder;
    candidate.displayName = displayName.length() > 0 ? displayName : friendlyFolderName(folder);
    candidate.version = version;
    candidate.apiUrl = String(APPSTORE_INDEX_SOURCE_PREFIX) + indexUrl + "\t" +
      folder + "\t" + version + "\t" + remoteDirectory;
    candidate.sizeBytes = static_cast<uint32_t>(strtoul(sizeText.c_str(), nullptr, 10));
    const int installed = installedIndexForFolder(folder);
    candidate.installed = installed >= 0;
    candidate.installedVersion = installed >= 0 ? gInstalledApps[installed].version : "";
    addOrUpdateRemoteApp(candidate);
    found = true;
  }

  if (found) {
    for (uint8_t i = 0; i < gRemoteAppCount; ++i) {
      Serial.printf("[APPSTORE] %s: selected V%s for fOS %s (index).\n",
        gRemoteApps[i].folderName.c_str(), gRemoteApps[i].version.c_str(), FOSVersion::kString);
    }
  }
  return found;
}

bool inspectRemoteDirectory(
  const String& folderName,
  const String& apiUrl,
  RemoteAppEntry * app,
  String * minimumFos = nullptr)
{
  if (app == nullptr) return false;
  if (minimumFos != nullptr) minimumFos->remove(0);
  String payload;
  if (!httpGetText(apiUrl, &payload)) return false;
  app->folderName = folderName;
  app->displayName = friendlyFolderName(folderName);
  app->version = "0";
  app->apiUrl = apiUrl;
  app->sizeBytes = 0;

  String manifestDownloadUrl;
  String manifestApiUrl;
  String configDownloadUrl;
  String configApiUrl;
  int cursor = 0;
  String object;
  while (nextJsonObject(payload, &cursor, &object)) {
    String type;
    String name;
    if (!extractJsonString(object, "type", &type) || !extractJsonString(object, "name", &name)) continue;
    if (type == "file") {
      uint32_t size = 0;
      if (extractJsonUnsigned(object, "size", &size)) {
        const uint64_t sum = static_cast<uint64_t>(app->sizeBytes) + size;
        app->sizeBytes = sum > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(sum);
      }
      if (name == "app.json") {
        extractJsonString(object, "download_url", &manifestDownloadUrl);
        extractJsonString(object, "url", &manifestApiUrl);
      } else if (name == "app.cfg") {
        extractJsonString(object, "download_url", &configDownloadUrl);
        extractJsonString(object, "url", &configApiUrl);
      }
    }
  }

  const bool useManifest = manifestDownloadUrl.length() > 0 || manifestApiUrl.length() > 0;
  const String& metadataDownloadUrl = useManifest ? manifestDownloadUrl : configDownloadUrl;
  const String& metadataApiUrl = useManifest ? manifestApiUrl : configApiUrl;
  if (metadataDownloadUrl.length() > 0 || metadataApiUrl.length() > 0) {
    String metadata;
    if (!httpGetGithubFileText(metadataDownloadUrl, metadataApiUrl, &metadata)) {
      Serial.printf("[APPSTORE] Metadata unavailable: %s\n", folderName.c_str());
      return false;
    }
    String value;
    if (useManifest) {
      if (extractJsonString(metadata, "version", &value) && value.length() > 0) app->version = value;
      if (extractJsonString(metadata, "name", &value) && value.length() > 0) app->displayName = value;
      if (minimumFos != nullptr &&
          extractJsonString(metadata, "min_fos", &value) && value.length() > 0) {
        *minimumFos = value;
      }
    } else {
      if (extractConfigString(metadata, "version", &value)) app->version = value;
      if (extractConfigString(metadata, "name", &value)) app->displayName = value;
      if (minimumFos != nullptr && extractConfigString(metadata, "min_fos", &value)) {
        *minimumFos = value;
      }
    }
  }

  const int installed = installedIndexForFolder(folderName);
  app->installed = installed >= 0;
  app->installedVersion = installed >= 0 ? gInstalledApps[installed].version : "";
  return true;
}

bool inspectVersionDirectory(
  const String& folderName,
  const RemoteVersionEntry& version,
  RemoteAppEntry * app)
{
  String payload;
  if (!httpGetText(version.apiUrl, &payload)) return false;
  bool containsMetadata = false;
  String exactDirectoryUrl;
  String fallbackDirectoryUrl;
  uint8_t directoryCount = 0;
  int cursor = 0;
  String object;
  while (nextJsonObject(payload, &cursor, &object)) {
    String type;
    String name;
    if (!extractJsonString(object, "type", &type) ||
        !extractJsonString(object, "name", &name)) continue;
    if (type == "file" && (name == "app.json" || name == "app.cfg")) {
      containsMetadata = true;
    } else if (type == "dir" && isSafePathPart(name)) {
      String entryUrl;
      if (!extractJsonString(object, "url", &entryUrl) || entryUrl.length() == 0) continue;
      ++directoryCount;
      if (name == folderName) exactDirectoryUrl = entryUrl;
      if (fallbackDirectoryUrl.length() == 0) fallbackDirectoryUrl = entryUrl;
    }
  }

  String appDirectoryUrl;
  if (containsMetadata) appDirectoryUrl = version.apiUrl;
  else if (exactDirectoryUrl.length() > 0) appDirectoryUrl = exactDirectoryUrl;
  else if (directoryCount == 1) appDirectoryUrl = fallbackDirectoryUrl;
  else return false;

  String minimumFos;
  if (!inspectRemoteDirectory(folderName, appDirectoryUrl, app, &minimumFos)) return false;
  uint16_t manifestMajor = 0;
  uint16_t manifestMinor = 0;
  uint16_t manifestPatch = 0;
  if (!parseVersion(app->version, &manifestMajor, &manifestMinor, &manifestPatch) ||
      compareVersion(
        manifestMajor, manifestMinor, manifestPatch,
        version.major, version.minor, version.patch) != 0) {
    Serial.printf("[APPSTORE] %s/%s: metadata version does not match its folder.\n",
      folderName.c_str(), version.name.c_str());
    return false;
  }
  return compatibleWithCurrentFos(minimumFos);
}

bool findNewestVersionBefore(
  const String& payload,
  bool hasCeiling,
  const RemoteVersionEntry& ceiling,
  RemoteVersionEntry * result,
  bool * legacyFlatApp)
{
  if (result == nullptr) return false;
  bool found = false;
  int cursor = 0;
  String object;
  while (nextJsonObject(payload, &cursor, &object)) {
    String type;
    String name;
    if (!extractJsonString(object, "type", &type) ||
        !extractJsonString(object, "name", &name)) continue;
    if (type == "file" && (name == "app.json" || name == "app.cfg")) {
      if (legacyFlatApp != nullptr) *legacyFlatApp = true;
      continue;
    }
    if (type != "dir") continue;

    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t patch = 0;
    if (!parseVersion(name, &major, &minor, &patch)) continue;
    if (hasCeiling && compareVersion(
      major, minor, patch,
      ceiling.major, ceiling.minor, ceiling.patch) >= 0) continue;
    if (found && compareVersion(
      major, minor, patch,
      result->major, result->minor, result->patch) <= 0) continue;

    String entryUrl;
    if (!extractJsonString(object, "url", &entryUrl) || entryUrl.length() == 0) continue;
    *result = {name, entryUrl, major, minor, patch};
    found = true;
  }
  return found;
}

bool inspectRemoteAppRoot(const String& folderName, const String& apiUrl, RemoteAppEntry * app)
{
  if (app == nullptr) return false;
  String payload;
  if (!httpGetText(apiUrl, &payload)) return false;

  bool legacyFlatApp = false;
  bool foundVersionDirectory = false;
  bool hasCeiling = false;
  RemoteVersionEntry ceiling;
  while (true) {
    RemoteVersionEntry version;
    if (!findNewestVersionBefore(
      payload, hasCeiling, ceiling, &version, &legacyFlatApp)) break;
    foundVersionDirectory = true;
    RemoteAppEntry candidate;
    if (inspectVersionDirectory(folderName, version, &candidate)) {
      *app = candidate;
      Serial.printf("[APPSTORE] %s: selected V%s for fOS %s.\n",
        folderName.c_str(), app->version.c_str(), FOSVersion::kString);
      return true;
    }
    ceiling = version;
    hasCeiling = true;
  }

  if (!foundVersionDirectory && legacyFlatApp) {
    String minimumFos;
    return inspectRemoteDirectory(folderName, apiUrl, app, &minimumFos) &&
      compatibleWithCurrentFos(minimumFos);
  }
  Serial.printf("[APPSTORE] %s: no version compatible with fOS %s.\n",
    folderName.c_str(), FOSVersion::kString);
  return false;
}

bool fetchStore(const String& configuredUrl)
{
  const String indexUrl = storeIndexUrl(configuredUrl);
  if (indexUrl.length() > 0) {
    String indexPayload;
    if (loadStoreIndex(indexUrl, &indexPayload) &&
        parseIndexedStore(indexUrl, indexPayload)) {
      return true;
    }
    Serial.println("[APPSTORE] Store index unavailable; trying GitHub Contents API.");
  }

  const String apiUrl = normalizeStoreUrl(configuredUrl);
  if (apiUrl.length() == 0) return false;
  String payload;
  if (!httpGetText(apiUrl, &payload)) return false;
  bool found = false;
  int cursor = 0;
  String object;
  while (nextJsonObject(payload, &cursor, &object) && gRemoteAppCount < APPSTORE_MAX_REMOTE_APPS) {
    String type;
    String name;
    String entryUrl;
    if (!extractJsonString(object, "type", &type) || type != "dir") continue;
    if (!extractJsonString(object, "name", &name) || !isSafePathPart(name)) continue;
    if (!extractJsonString(object, "url", &entryUrl) || entryUrl.length() == 0) continue;
    ++gRefreshFoldersScanned;
    Serial.printf("[APPSTORE] Inspecting %s ...\n", name.c_str());
    RemoteAppEntry app;
    if (inspectRemoteAppRoot(name, entryUrl, &app)) {
      addOrUpdateRemoteApp(app);
      found = true;
    }
  }
  return found;
}

String formatSize(uint32_t bytes)
{
  if (bytes < 1024) return String(bytes) + " B";
  const uint32_t roundedKb = (bytes + 512U) / 1024U;
  return String(roundedKb) + " KB";
}

void fillRemoteRoller()
{
  if (uic_AppDownloadRoller == nullptr) return;
  if (gRemoteAppCount == 0) {
    lv_roller_set_options(uic_AppDownloadRoller, "No apps found", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(uic_AppDownloadRoller, 0, LV_ANIM_OFF);
    return;
  }
  String options;
  for (uint8_t i = 0; i < gRemoteAppCount; ++i) {
    const RemoteAppEntry& app = gRemoteApps[i];
    if (i > 0) options += '\n';
    options += app.displayName;
    options += " | ";
    if (app.installed) {
      options += "installed V" + app.installedVersion + " - compatible V" + app.version;
    } else {
      options += "V" + app.version;
    }
    options += " | " + formatSize(app.sizeBytes);
  }
  lv_roller_set_options(uic_AppDownloadRoller, options.c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(uic_AppDownloadRoller, 0, LV_ANIM_OFF);
}

void refreshInstalledStateForRemoteEntries()
{
  scanInstalledApps();
  for (uint8_t i = 0; i < gRemoteAppCount; ++i) {
    const int installed = installedIndexForFolder(gRemoteApps[i].folderName);
    gRemoteApps[i].installed = installed >= 0;
    gRemoteApps[i].installedVersion = installed >= 0 ? gInstalledApps[installed].version : "";
  }
  fillRemoteRoller();
  fillAssignmentDropdowns();
}

void storeRefreshTask(void *)
{
  File stores = SD.open(APPSTORE_STORES_FILE, FILE_READ);
  uint8_t storeCount = 0;
  uint8_t successfulStores = 0;
  if (stores) {
    while (stores.available() && storeCount < APPSTORE_MAX_STORE_URLS) {
      String url = stores.readStringUntil('\n');
      url.trim();
      if (url.length() == 0 || url.startsWith("#")) continue;
      ++storeCount;
      if (fetchStore(url)) ++successfulStores;
    }
    stores.close();
  }
  gRefreshStoreCount = storeCount;
  gRefreshSuccessfulStores = successfulStores;
  __sync_synchronize();
  gStoreRefreshState = successfulStores > 0
    ? StoreRefreshState::Complete
    : StoreRefreshState::Failed;
  vTaskDelete(nullptr);
}

void beginStoreRefresh()
{
  if (gStoreRefreshState == StoreRefreshState::Running) return;
  gRemoteAppCount = 0;
  gRefreshFoldersScanned = 0;
  gRefreshStoreCount = 0;
  gRefreshSuccessfulStores = 0;
  scanInstalledApps();
  if (WiFi.status() != WL_CONNECTED) {
    gStoreRefreshState = StoreRefreshState::Failed;
    return;
  }
  if (!SD.exists(APPSTORE_STORES_FILE)) AppStoreEnsureSystemFiles(SD);
  gStoreRefreshState = StoreRefreshState::Running;
  const BaseType_t created = xTaskCreate(
    storeRefreshTask, "appstore", APPSTORE_REFRESH_TASK_STACK,
    nullptr, 1, nullptr);
  if (created != pdPASS) {
    gStoreRefreshState = StoreRefreshState::Failed;
    Serial.println("[APPSTORE] Could not create refresh task.");
  }
}

void storeRefreshTimer(lv_timer_t * timer)
{
  if (timer == nullptr) return;
  const bool visible = ui_AppContent != nullptr && lv_scr_act() == ui_AppContent;
  if (!visible) {
    if (gStoreRefreshState != StoreRefreshState::Running) {
      if (gStoreRefreshTimer == timer) gStoreRefreshTimer = nullptr;
      lv_timer_del(timer);
    }
    return;
  }

  if (gStoreRefreshState == StoreRefreshState::Idle) {
    beginStoreRefresh();
    return;
  }
  if (gStoreRefreshState == StoreRefreshState::Running) {
    showStatus(String("Loading app lists ... ") +
      String(static_cast<unsigned int>(gRefreshFoldersScanned)));
    return;
  }
  __sync_synchronize();

  fillRemoteRoller();
  fillAssignmentDropdowns();
  if (gRemoteAppCount > 0) {
    if (gStatusLabel != nullptr) lv_obj_add_flag(gStatusLabel, LV_OBJ_FLAG_HIDDEN);
  } else if (WiFi.status() != WL_CONNECTED) {
    if (uic_AppDownloadRoller != nullptr) {
      lv_roller_set_options(uic_AppDownloadRoller, "No WiFi connection", LV_ROLLER_MODE_NORMAL);
    }
    showStatus("No WiFi connection", lv_color_hex(0xFF8080));
  } else if (gRefreshStoreCount == 0) {
    showStatus("No AppStore URL in stores.txt", lv_color_hex(0xFF8080));
  } else if (gRefreshSuccessfulStores == 0) {
    showStatus("AppStore could not be loaded", lv_color_hex(0xFF8080));
  }
  gStoreRefreshState = StoreRefreshState::Idle;
  if (gStoreRefreshTimer == timer) gStoreRefreshTimer = nullptr;
  lv_timer_del(timer);
}

bool removeRecursively(const String& path, uint8_t depth = 0)
{
  if (depth > APPSTORE_MAX_DOWNLOAD_DEPTH + 2) return false;
  File entry = SD.open(path);
  if (!entry) return !SD.exists(path);
  if (!entry.isDirectory()) {
    entry.close();
    return SD.remove(path);
  }
  File child = entry.openNextFile();
  while (child) {
    const String childPath = path + "/" + basenameOf(String(child.name()));
    const bool directory = child.isDirectory();
    child.close();
    if (directory) {
      if (!removeRecursively(childPath, depth + 1)) {
        entry.close();
        return false;
      }
    } else if (!SD.remove(childPath)) {
      entry.close();
      return false;
    }
    child = entry.openNextFile();
  }
  entry.close();
  return SD.rmdir(path);
}

bool downloadFileFromUrl(
  const String& url,
  const String& destination,
  bool githubRawContent)
{
  for (uint8_t attempt = 1; attempt <= APPSTORE_HTTP_RETRIES; ++attempt) {
    if (WiFi.status() != WL_CONNECTED) return false;
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(20);
    HTTPClient http;
    http.setConnectTimeout(15000);
    http.setTimeout(25000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.useHTTP10(true);
    http.setUserAgent("fOS-AppStore/1.0");
    if (!http.begin(client, url)) {
      Serial.printf("[APPSTORE] File HTTP begin failed (try %u): %s\n",
        attempt, url.c_str());
      delay(120);
      continue;
    }
    if (githubRawContent) {
      http.addHeader("Accept", "application/vnd.github.raw+json");
      http.addHeader("X-GitHub-Api-Version", "2022-11-28");
    }
    http.addHeader("Connection", "close");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
      const String error = HTTPClient::errorToString(code);
      Serial.printf("[APPSTORE] File GET failed (%d, %s, try %u): %s\n",
        code, error.c_str(), attempt, url.c_str());
      http.end();
      if (code > 0 && code != 408 && code != 429 && code < 500) break;
      if (code == 429) break;
      delay(120);
      continue;
    }
    if (SD.exists(destination)) SD.remove(destination);
    File file = SD.open(destination, FILE_WRITE);
    if (!file) {
      http.end();
      return false;
    }
    const int expected = http.getSize();
    const int written = http.writeToStream(&file);
    file.close();
    http.end();
    if (written >= 0 && (expected <= 0 || written == expected)) return true;
    Serial.printf("[APPSTORE] Incomplete file download (try %u): %s\n",
      attempt, url.c_str());
    SD.remove(destination);
    delay(120);
  }
  return false;
}

bool ensureParentDirectories(const String& root, const String& relativeFile)
{
  if (!isSafeRelativePath(relativeFile)) return false;
  int cursor = 0;
  while (true) {
    const int slash = relativeFile.indexOf('/', cursor);
    if (slash < 0) break;
    const String directory = root + "/" + relativeFile.substring(0, slash);
    if (!SD.exists(directory) && !SD.mkdir(directory)) return false;
    cursor = slash + 1;
  }
  return true;
}

bool downloadIndexedDirectory(const String& source, const String& destination)
{
  if (!source.startsWith(APPSTORE_INDEX_SOURCE_PREFIX)) return false;
  const String fields = source.substring(strlen(APPSTORE_INDEX_SOURCE_PREFIX));
  String indexUrl;
  String folder;
  String version;
  String remoteDirectory;
  if (!tabField(fields, 0, &indexUrl) ||
      !tabField(fields, 1, &folder) || !isSafePathPart(folder) ||
      !tabField(fields, 2, &version) ||
      !tabField(fields, 3, &remoteDirectory) || !isSafeRelativePath(remoteDirectory)) return false;

  String payload;
  if (!loadStoreIndex(indexUrl, &payload)) return false;
  const int lastSlash = indexUrl.lastIndexOf('/');
  if (lastSlash <= 8) return false;
  const String rawBase = indexUrl.substring(0, lastSlash);
  if (!SD.exists(destination) && !SD.mkdir(destination)) return false;

  uint16_t downloaded = 0;
  int cursor = 0;
  while (cursor < static_cast<int>(payload.length())) {
    int end = payload.indexOf('\n', cursor);
    if (end < 0) end = payload.length();
    String line = payload.substring(cursor, end);
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    cursor = end + 1;
    if (!line.startsWith("F\t")) continue;

    String fileFolder;
    String fileVersion;
    String relativeFile;
    if (!tabField(line, 1, &fileFolder) || fileFolder != folder ||
        !tabField(line, 2, &fileVersion) || fileVersion != version ||
        !tabField(line, 3, &relativeFile) || !isSafeRelativePath(relativeFile)) continue;
    if (!ensureParentDirectories(destination, relativeFile)) return false;

    const String remotePath = remoteDirectory + "/" + relativeFile;
    const String remoteUrl = rawBase + "/" + encodeUrlPath(remotePath);
    const String localPath = destination + "/" + relativeFile;
    bool fileDownloaded = downloadFileFromUrl(remoteUrl, localPath, false);
    if (!fileDownloaded && indexUrl == APPSTORE_DEFAULT_INDEX) {
      Serial.println("[APPSTORE] RAW file download failed; trying GitHub API once.");
      const String apiUrl = String(APPSTORE_DEFAULT_API_FILE_BASE) +
        encodeUrlPath(remotePath) + APPSTORE_DEFAULT_API_REF;
      fileDownloaded = downloadFileFromUrl(apiUrl, localPath, true);
    }
    if (!fileDownloaded) return false;
    ++downloaded;
    ++gDownloadedFileCount;
    if ((gDownloadedFileCount % 2U) == 0U) {
      showStatus(String("Downloading files: ") + gDownloadedFileCount);
    }
  }
  return downloaded > 0;
}

bool downloadFile(
  const String& downloadUrl,
  const String& apiUrl,
  const String& destination)
{
  bool downloaded = downloadUrl.length() > 0 &&
    downloadFileFromUrl(downloadUrl, destination, false);
  if (!downloaded && apiUrl.length() > 0) {
    Serial.println("[APPSTORE] RAW file download failed, retrying through GitHub API.");
    downloaded = downloadFileFromUrl(apiUrl, destination, true);
  }
  if (!downloaded) return false;
  ++gDownloadedFileCount;
  if ((gDownloadedFileCount % 2U) == 0U) showStatus(String("Downloading files: ") + gDownloadedFileCount);
  return true;
}

bool downloadDirectory(const String& apiUrl, const String& destination, uint8_t depth)
{
  if (depth > APPSTORE_MAX_DOWNLOAD_DEPTH) return false;
  if (!SD.exists(destination) && !SD.mkdir(destination)) return false;
  String payload;
  if (!httpGetText(apiUrl, &payload)) return false;
  int cursor = 0;
  String object;
  while (nextJsonObject(payload, &cursor, &object)) {
    String type;
    String name;
    if (!extractJsonString(object, "type", &type) || !extractJsonString(object, "name", &name)) continue;
    if (!isSafePathPart(name)) return false;
    const String localPath = destination + "/" + name;
    if (type == "file") {
      String downloadUrl;
      String fileApiUrl;
      extractJsonString(object, "download_url", &downloadUrl);
      extractJsonString(object, "url", &fileApiUrl);
      if (downloadUrl.length() == 0 && fileApiUrl.length() == 0) return false;
      if (!downloadFile(downloadUrl, fileApiUrl, localPath)) return false;
    } else if (type == "dir") {
      String childApiUrl;
      if (!extractJsonString(object, "url", &childApiUrl) || childApiUrl.length() == 0) return false;
      if (!downloadDirectory(childApiUrl, localPath, depth + 1)) return false;
    }
  }
  return true;
}

void clearAssignmentsForFolder(const String& folder)
{
  loadAssignments();
  if (!gAssignmentsExplicit) return;
  bool changed = false;
  for (uint8_t i = 0; i < APPSTORE_LAUNCHER_SLOT_COUNT; ++i) {
    if (gSlotAssignments[i] == folder) {
      gSlotAssignments[i] = "";
      changed = true;
    }
  }
  if (changed) saveAssignments();
}

void assignNewAppToFirstFreeSlot(const String& folder)
{
  loadAssignments();
  if (!gAssignmentsExplicit) return;

  for (uint8_t slot = 0; slot < APPSTORE_LAUNCHER_SLOT_COUNT; ++slot) {
    if (gSlotAssignments[slot] == folder) return;
  }

  scanInstalledApps();
  for (uint8_t slot = 0; slot < APPSTORE_LAUNCHER_SLOT_COUNT; ++slot) {
    const String& assigned = gSlotAssignments[slot];
    if (assigned.length() == 0 || installedIndexForFolder(assigned) < 0) {
      gSlotAssignments[slot] = folder;
      saveAssignments();
      return;
    }
  }
}

void preserveCurrentLauncherAssignments()
{
  loadAssignments();
  if (gAssignmentsExplicit) return;
  scanInstalledApps();
  for (uint8_t slot = 0; slot < APPSTORE_LAUNCHER_SLOT_COUNT; ++slot) {
    gSlotAssignments[slot] = slot < gInstalledAppCount ? gInstalledApps[slot].folderName : "";
  }
  saveAssignments();
}

void applySystemTheme()
{
  if (uic_AppContentArea != nullptr) {
    lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(uic_AppContentArea, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_OFF);
  }

  if (ui_TabView4 != nullptr) {
    lv_obj_set_style_bg_color(ui_TabView4, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TabView4, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t * tabButtons = lv_tabview_get_tab_btns(ui_TabView4);
    lv_obj_set_style_bg_color(tabButtons, lv_color_hex(0x20242A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(tabButtons, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tabButtons, lv_color_hex(0xF2F4F7), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(tabButtons, lv_color_hex(0x20242A), LV_PART_ITEMS | LV_STATE_CHECKED);
    ui_object_set_themeable_style_property(tabButtons, LV_PART_ITEMS | LV_STATE_CHECKED,
      LV_STYLE_TEXT_COLOR, _ui_theme_color_MainTheme);
    ui_object_set_themeable_style_property(tabButtons, LV_PART_ITEMS | LV_STATE_CHECKED,
      LV_STYLE_TEXT_OPA, _ui_theme_alpha_MainTheme);
    lv_obj_set_style_border_side(tabButtons, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tabButtons, 4, LV_PART_ITEMS | LV_STATE_CHECKED);
    ui_object_set_themeable_style_property(tabButtons, LV_PART_ITEMS | LV_STATE_CHECKED,
      LV_STYLE_BORDER_COLOR, _ui_theme_color_MainTheme);
    ui_object_set_themeable_style_property(tabButtons, LV_PART_ITEMS | LV_STATE_CHECKED,
      LV_STYLE_BORDER_OPA, _ui_theme_alpha_MainTheme);
  }

  lv_obj_t * darkObjects[] = {
    ui_TabPageDownload, ui_TabPageAssignApps, ui_AppDownloadRoller,
    ui_AppForAssigningRoller, ui_SelectAppSlotRoller
  };
  for (lv_obj_t * object : darkObjects) {
    if (object == nullptr) continue;
    lv_obj_set_style_bg_color(object, lv_color_hex(0x20242A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(object, lv_color_hex(0xF2F4F7), LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  if (ui_AppDownloadRoller != nullptr) {
    ui_object_set_themeable_style_property(ui_AppDownloadRoller, LV_PART_SELECTED | LV_STATE_DEFAULT,
      LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
    ui_object_set_themeable_style_property(ui_AppDownloadRoller, LV_PART_SELECTED | LV_STATE_DEFAULT,
      LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
    lv_obj_set_style_text_color(ui_AppDownloadRoller, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_DEFAULT);
  }

  lv_obj_t * dropdowns[] = { ui_AppForAssigningRoller, ui_SelectAppSlotRoller };
  for (lv_obj_t * dropdown : dropdowns) {
    if (dropdown == nullptr) continue;
    lv_obj_t * list = lv_dropdown_get_list(dropdown);
    if (list == nullptr) continue;
    lv_obj_set_style_bg_color(list, lv_color_hex(0x20242A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(list, lv_color_hex(0xF2F4F7), LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_object_set_themeable_style_property(list, LV_PART_SELECTED | LV_STATE_DEFAULT,
      LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
    ui_object_set_themeable_style_property(list, LV_PART_SELECTED | LV_STATE_DEFAULT,
      LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
    lv_obj_set_style_text_color(list, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_DEFAULT);
  }

  lv_obj_t * themeButtons[] = { ui_AppStoreDownload, ui_AssignAppsConfirm };
  for (lv_obj_t * button : themeButtons) {
    if (button == nullptr) continue;
    ui_object_set_themeable_style_property(button, LV_PART_MAIN | LV_STATE_DEFAULT,
      LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
    ui_object_set_themeable_style_property(button, LV_PART_MAIN | LV_STATE_DEFAULT,
      LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  }
}

void createRuntimeUi(lv_obj_t * host)
{
  if (host == nullptr) return;
  if (gStatusLabel == nullptr || !lv_obj_is_valid(gStatusLabel)) {
    gStatusLabel = lv_label_create(host);
    lv_obj_set_width(gStatusLabel, 520);
    lv_label_set_long_mode(gStatusLabel, LV_LABEL_LONG_DOT);
    lv_obj_align(gStatusLabel, LV_ALIGN_TOP_MID, 0, 57);
    lv_obj_set_style_text_align(gStatusLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(gStatusLabel, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  lv_obj_move_foreground(gStatusLabel);
  if (ui_HomeButton9 != nullptr) lv_obj_move_foreground(ui_HomeButton9);
}

void initializeAssignmentsWithInstalledDefaults()
{
  if (gAssignmentsExplicit) return;
  scanInstalledApps();
  for (uint8_t slot = 0; slot < APPSTORE_LAUNCHER_SLOT_COUNT; ++slot) {
    gSlotAssignments[slot] = slot < gInstalledAppCount ? gInstalledApps[slot].folderName : "";
  }
}

void migrateLegacyDefaultStore(fs::FS& filesystem)
{
  if (!filesystem.exists(APPSTORE_STORES_FILE)) return;
  if (filesystem.exists(APPSTORE_STORES_TEMP_FILE)) filesystem.remove(APPSTORE_STORES_TEMP_FILE);
  File input = filesystem.open(APPSTORE_STORES_FILE, FILE_READ);
  File output = filesystem.open(APPSTORE_STORES_TEMP_FILE, FILE_WRITE);
  if (!input || !output) {
    if (input) input.close();
    if (output) output.close();
    filesystem.remove(APPSTORE_STORES_TEMP_FILE);
    return;
  }

  bool changed = false;
  while (input.available()) {
    String line = input.readStringUntil('\n');
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    String comparison = line;
    comparison.trim();
    while (comparison.endsWith("/")) comparison.remove(comparison.length() - 1);
    if (comparison == APPSTORE_LEGACY_DEFAULT_STORE) {
      output.println(APPSTORE_DEFAULT_STORE);
      changed = true;
    } else {
      output.println(line);
    }
  }
  input.close();
  output.close();
  if (!changed) {
    filesystem.remove(APPSTORE_STORES_TEMP_FILE);
    return;
  }

  if (filesystem.exists(APPSTORE_STORES_BACKUP_FILE)) filesystem.remove(APPSTORE_STORES_BACKUP_FILE);
  if (!filesystem.rename(APPSTORE_STORES_FILE, APPSTORE_STORES_BACKUP_FILE)) {
    filesystem.remove(APPSTORE_STORES_TEMP_FILE);
    return;
  }
  if (!filesystem.rename(APPSTORE_STORES_TEMP_FILE, APPSTORE_STORES_FILE)) {
    filesystem.rename(APPSTORE_STORES_BACKUP_FILE, APPSTORE_STORES_FILE);
    return;
  }
  filesystem.remove(APPSTORE_STORES_BACKUP_FILE);
  Serial.println("[APPSTORE] Default store URL migrated to /apps.");
}

}  // namespace

void AppStoreEnsureSystemFiles(fs::FS& filesystem)
{
  if (!filesystem.exists("/system")) filesystem.mkdir("/system");
  if (!filesystem.exists(APPSTORE_SYSTEM_DIRECTORY)) filesystem.mkdir(APPSTORE_SYSTEM_DIRECTORY);
  if (!gStorePathChecked && !filesystem.exists(APPSTORE_STORES_FILE)) {
    File file = filesystem.open(APPSTORE_STORES_FILE, FILE_WRITE);
    if (file) {
      file.println("# fOS AppStores - one URL per line");
      file.println("# GitHub tree URLs and GitHub Contents API URLs are supported.");
      file.println(APPSTORE_DEFAULT_STORE);
      file.close();
      Serial.println("[APPSTORE] /system/apps/stores.txt created.");
    }
  } else if (!gStorePathChecked) {
    migrateLegacyDefaultStore(filesystem);
  }
  gStorePathChecked = true;
  loadAssignments();
}

bool AppStoreHasExplicitLauncherAssignments()
{
  return gAssignmentsExplicit;
}

String AppStoreGetAssignedFolder(uint8_t slot)
{
  return slot < APPSTORE_LAUNCHER_SLOT_COUNT ? gSlotAssignments[slot] : String();
}

extern "C" void StartAppStore_Data(lv_event_t * event)
{
  if (event != nullptr && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_obj_t * host = PrepareAppStoreContent_Data();
  if (host == nullptr) return;

  if (ui_AppStore == nullptr || ui_TabView4 == nullptr || !lv_obj_is_valid(ui_TabView4)) {
    if (ui_AppStore != nullptr) ui_AppStore_screen_destroy();
    ui_AppStore_screen_init();
    gStatusLabel = nullptr;
  }

  while (lv_obj_get_child(ui_AppStore, 0) != nullptr) {
    lv_obj_t * child = lv_obj_get_child(ui_AppStore, 0);
    lv_obj_set_parent(child, host);
  }

  applySystemTheme();
  createRuntimeUi(host);
  AppStoreEnsureSystemFiles(SD);
  fillAssignmentDropdowns();
  if (uic_AppDownloadRoller != nullptr) {
    lv_roller_set_options(uic_AppDownloadRoller, "Loading app lists ...", LV_ROLLER_MODE_NORMAL);
  }
  showStatus("Loading app lists ...");
  // Draw the AppStore before starting any GitHub request. Network loading runs
  // in a worker task while this timer only updates the visible status.
  lv_refr_now(nullptr);
  if (gStoreRefreshTimer != nullptr) {
    lv_timer_del(gStoreRefreshTimer);
    gStoreRefreshTimer = nullptr;
  }
  gStoreRefreshTimer = lv_timer_create(storeRefreshTimer, 60, nullptr);
  if (gStoreRefreshTimer == nullptr) {
    showStatus("App list loading could not be started", lv_color_hex(0xFF8080));
  }
}

extern "C" void AppStoreDownloadApp(lv_event_t * event)
{
  if (event != nullptr && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  if (gStoreRefreshState == StoreRefreshState::Running) {
    showStatus("App list is still loading", lv_color_hex(0xFFCC80));
    return;
  }
  if (gRemoteAppCount == 0 || uic_AppDownloadRoller == nullptr) {
    showStatus("No app selected", lv_color_hex(0xFF8080));
    return;
  }
  int selected = lv_roller_get_selected(uic_AppDownloadRoller);
  if (selected < 0 || selected >= gRemoteAppCount) selected = 0;
  const RemoteAppEntry app = gRemoteApps[selected];
  if (!isSafePathPart(app.folderName)) {
    showStatus("Invalid app folder", lv_color_hex(0xFF8080));
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    showStatus("No WiFi connection", lv_color_hex(0xFF8080));
    return;
  }

  const String target = String(APPSTORE_APPS_DIRECTORY) + "/" + app.folderName;
  const String temporary = String(APPSTORE_APPS_DIRECTORY) + "/.store_tmp_" + app.folderName;
  const String backup = String(APPSTORE_APPS_DIRECTORY) + "/.store_backup_" + app.folderName;
  if (SD.exists(temporary) && !removeRecursively(temporary)) {
    showStatus("Temporary folder could not be cleared", lv_color_hex(0xFF8080));
    return;
  }
  gDownloadedFileCount = 0;
  showStatus("Downloading " + app.displayName + " ...");
  const bool downloaded = app.apiUrl.startsWith(APPSTORE_INDEX_SOURCE_PREFIX)
    ? downloadIndexedDirectory(app.apiUrl, temporary)
    : downloadDirectory(app.apiUrl, temporary, 0);
  if (!downloaded) {
    removeRecursively(temporary);
    showStatus("Download failed", lv_color_hex(0xFF8080));
    return;
  }

  // Freeze the current launcher layout before changing /apps. This prevents a
  // newly downloaded directory from displacing an existing Home-screen app.
  preserveCurrentLauncherAssignments();
  if (SD.exists(backup)) removeRecursively(backup);
  bool hadExisting = SD.exists(target);
  if (hadExisting && !SD.rename(target, backup)) {
    removeRecursively(temporary);
    showStatus("Installed app could not be backed up", lv_color_hex(0xFF8080));
    return;
  }
  if (!SD.rename(temporary, target)) {
    if (hadExisting) SD.rename(backup, target);
    removeRecursively(temporary);
    showStatus("App could not be installed", lv_color_hex(0xFF8080));
    return;
  }
  if (hadExisting) removeRecursively(backup);
  if (!app.installed) assignNewAppToFirstFreeSlot(app.folderName);
  StartAppLauncher_Data(nullptr);
  refreshInstalledStateForRemoteEntries();
  showStatus(app.displayName + " installed");
}

extern "C" void AppStoreDeleteApp(lv_event_t * event)
{
  if (event != nullptr && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  if (gStoreRefreshState == StoreRefreshState::Running) {
    showStatus("App list is still loading", lv_color_hex(0xFFCC80));
    return;
  }
  if (gRemoteAppCount == 0 || uic_AppDownloadRoller == nullptr) {
    showStatus("No app selected", lv_color_hex(0xFF8080));
    return;
  }
  int selected = lv_roller_get_selected(uic_AppDownloadRoller);
  if (selected < 0 || selected >= gRemoteAppCount) selected = 0;
  const RemoteAppEntry app = gRemoteApps[selected];
  const String target = String(APPSTORE_APPS_DIRECTORY) + "/" + app.folderName;
  if (!app.installed || !SD.exists(target)) {
    showStatus("App is not installed", lv_color_hex(0xFFCC80));
    return;
  }
  showStatus("Deleting " + app.displayName + " ...");
  if (!removeRecursively(target)) {
    showStatus("App could not be deleted", lv_color_hex(0xFF8080));
    return;
  }
  clearAssignmentsForFolder(app.folderName);
  StartAppLauncher_Data(nullptr);
  refreshInstalledStateForRemoteEntries();
  showStatus(app.displayName + " deleted");
}

extern "C" void AssignAppsConfirm(lv_event_t * event)
{
  if (event != nullptr && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  if (gStoreRefreshState == StoreRefreshState::Running) {
    showStatus("App list is still loading", lv_color_hex(0xFFCC80));
    return;
  }
  if (uic_AppForAssigningRoller == nullptr || uic_SelectAppSlotRoller == nullptr) return;
  scanInstalledApps();
  loadAssignments();
  initializeAssignmentsWithInstalledDefaults();
  const int slot = lv_dropdown_get_selected(uic_SelectAppSlotRoller);
  const int appSelection = lv_dropdown_get_selected(uic_AppForAssigningRoller);
  if (slot < 0 || slot >= APPSTORE_LAUNCHER_SLOT_COUNT) {
    showStatus("Invalid launcher slot", lv_color_hex(0xFF8080));
    return;
  }
  if (appSelection <= 0) {
    gSlotAssignments[slot] = "";
  } else {
    const int appIndex = appSelection - 1;
    if (appIndex < 0 || appIndex >= gInstalledAppCount) {
      showStatus("Invalid app selection", lv_color_hex(0xFF8080));
      return;
    }
    gSlotAssignments[slot] = gInstalledApps[appIndex].folderName;
  }
  if (!saveAssignments()) {
    showStatus("Assignment could not be saved", lv_color_hex(0xFF8080));
    return;
  }
  StartAppLauncher_Data(nullptr);
  showStatus(String("Slot ") + (slot + 1) + " saved");
}
