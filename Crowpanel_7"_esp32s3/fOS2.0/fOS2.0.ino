#include <lvgl.h>
#include "LGFX_CrowPanel.h"
#include "ui.h"
#include "esp_system.h"
#include "Arduino.h"
#include <SD.h>
#include <SPI.h>
#include "Audio.h"
#include <WiFi.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void setup();
void loop();
void audio_eof_mp3(const char *info);

#define WIFI_DIR  "/system/wifi"
#define WIFI_FILE "/system/wifi/wlans.txt"
#define TIMEZONE_DIR  "/system/timezone"
#define TIMEZONE_FILE "/system/timezone/timezone.txt"
#define TEXT_DIR "/text"
#define MUSIC_FILES_DIR   "/music/files"
#define WEBRADIO_DIR "/music/webradio"
#define WEBRADIO_FILE "/music/webradio/webradio.txt"
#define APPS_DIR "/apps"

#define MAX_LAUNCHER_APPS 6


#define MAX_WIFI_PROFILES 5


extern "C" void deleteSelectedFile(void);

/* ================= DISPLAY ================= */
LGFX gfx;

/* ================= AUDIO ================= */
Audio audio;

struct LauncherAppEntry {
  String folderName;
  String iconText;
  String displayName;
  String contentFile;
  String layoutFile;
  String appType;
  bool scrollable;
  String buttonText;
  String buttonMessage;
};

static LauncherAppEntry gLauncherApps[MAX_LAUNCHER_APPS];
static int gLauncherAppCount = 0;
static int gLauncherAppIndices[MAX_LAUNCHER_APPS];
static String gDemoButtonMessage = "Button gedrueckt!";
static String gStorageManagerCurrentDir = "/";
static lv_obj_t * gCalcDisplay = NULL;
static String gCalcExpression = "";
static bool gCalcShowingError = false;
static lv_obj_t * gRadioTabView = NULL;
static lv_obj_t * gRadioFileRoller = NULL;
static lv_obj_t * gRadioWebRoller = NULL;
static lv_obj_t * gRadioToggleButton = NULL;
static lv_obj_t * gRadioToggleLabel = NULL;
static String gRadioCurrentFile = "";
static String gRadioCurrentWebUrl = "";

enum RadioSourceType {
  RADIO_SOURCE_NONE,
  RADIO_SOURCE_FILE,
  RADIO_SOURCE_WEB
};

struct WebRadioEntry {
  String name;
  String url;
};

static RadioSourceType gRadioSource = RADIO_SOURCE_NONE;
static bool gRadioPlaying = false;
static const int MAX_WEBRADIO_STATIONS = 40;
static WebRadioEntry gWebRadioStations[MAX_WEBRADIO_STATIONS];
static int gWebRadioCount = 0;

/* ================= LVGL ================= */
static uint32_t last_tick = 0;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[800 * 40];

/* ================= SD ================= */
#define SD_CS 10   // ggf. an dein CrowPanel anpassen

bool sd_ok = false;
uint64_t totalBytes = 0;
uint64_t usedBytes  = 0;
uint64_t freeBytes  = 0;
int usedPercent     = 0;

/* ================= Wifi ================= */
struct WifiProfile {
  String ssid;
  String pass;
};

struct TimeZoneEntry {
  const char* label;
  const char* tzRule;
};

static const TimeZoneEntry kTimeZones[] = {
  { "Europe/Berlin (CET/CEST)", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "UTC (GMT+0)", "UTC0" },
  { "Europe/London (GMT/BST)", "GMT0BST,M3.5.0/1,M10.5.0" },
  { "America/New_York (EST/EDT)", "EST5EDT,M3.2.0/2,M11.1.0/2" },
  { "America/Chicago (CST/CDT)", "CST6CDT,M3.2.0/2,M11.1.0/2" },
  { "America/Denver (MST/MDT)", "MST7MDT,M3.2.0/2,M11.1.0/2" },
  { "America/Los_Angeles (PST/PDT)", "PST8PDT,M3.2.0/2,M11.1.0/2" },
  { "Europe/Helsinki (EET/EEST)", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Asia/Tokyo (JST)", "JST-9" },
  { "Asia/Seoul (KST)", "KST-9" },
  { "Asia/Shanghai (CST)", "CST-8" },
  { "Asia/Kolkata (IST)", "IST-5:30" },
  { "Australia/Sydney (AEST/AEDT)", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Pacific/Auckland (NZST/NZDT)", "NZST-12NZDT,M9.5.0/2,M4.1.0/3" }
};

static const int kDefaultTimeZoneIndex = 0;
static int currentTimeZoneIndex = kDefaultTimeZoneIndex;
static unsigned long lastNtpSyncAttempt = 0;
static unsigned long lastClockUiUpdate = 0;

static const char* kWeekdaysEn[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char* kMonthsEn[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static bool ensureAppsDirectory();
static String pathBasename(const String& path);
static void clearLauncherPage(lv_obj_t * page);
static void clearAppContentArea();
static lv_obj_t * getLauncherSlotByIndex(int index);
static void parseAppConfigFile(const String& folderPath, LauncherAppEntry * app);
static void loadAppsFromSdCard();
static void drawLauncherApps();
static void showAppContentForIndex(int appIndex);
static void launcherAppTileEvent(lv_event_t * e);
static void demoButtonClicked(lv_event_t * e);
static bool readTextFileLimited(const String& path, String * out, size_t maxLen);
static String decodeEscapedText(const String& text);
static String getUiField(const String& line, const char* key);
static int getUiFieldInt(const String& line, const char* key, int defaultValue);
static uint32_t getUiFieldColor(const String& line, const char* key, uint32_t defaultValue);
static bool getUiFieldBool(const String& line, const char* key, bool defaultValue);
static bool renderAppLayout(const LauncherAppEntry& app);
static bool renderAppTextContent(const LauncherAppEntry& app);
static void resetCalculatorState();
static bool isCalcOperatorChar(char c);
static int calcOperatorPrecedence(char op);
static bool applyCalcOperator(double * values, int * valueTop, char op, bool * divisionByZero);
static bool evaluateCalculatorExpression(const String& expression, double * out, bool * divisionByZero);
static String formatCalculatorResult(double value);
static bool currentNumberHasComma(const String& expression);
static void updateCalculatorDisplay();
static void calculatorButtonEvent(lv_event_t * e);
static void renderCalculatorButton(const char * text, int x, int y, int w, int h);
static void renderCalculatorApp();
static void resetRadioState();
static bool isMusicFileName(const String& filename);
static void fillRadioFileRoller();
static bool loadWebRadioStations();
static void fillWebRadioRoller();
static String getRadioSelectedFile();
static int getSelectedWebRadioIndex();
static void stopRadioPlayback();
static void updateRadioToggleButtonLabel();
static void startOrStopSelectedRadio();
static void radioToggleButtonEvent(lv_event_t * e);
static void radioTabChangedEvent(lv_event_t * e);
static void renderRadioApp();
static String getStorageManagerParentPath(const String& path);
static String joinStorageManagerPath(const String& base, const String& name);

int getTimeZoneCount()
{
  return sizeof(kTimeZones) / sizeof(kTimeZones[0]);
}

int normalizeTimeZoneIndex(int index)
{
  if (index < 0 || index >= getTimeZoneCount()) {
    return kDefaultTimeZoneIndex;
  }
  return index;
}

int findTimeZoneIndex(const String& value)
{
  for (int i = 0; i < getTimeZoneCount(); i++) {
    if (value == kTimeZones[i].tzRule || value == kTimeZones[i].label) {
      return i;
    }
  }
  return -1;
}

bool isSystemTimeValid()
{
  return time(nullptr) > 1609459200;  // 2021-01-01
}

void requestNtpSync(bool force)
{
  if (WiFi.status() != WL_CONNECTED) return;
  if (!force && isSystemTimeValid()) return;

  if (!force && millis() - lastNtpSyncAttempt < 30000) return;
  lastNtpSyncAttempt = millis();

  configTzTime(
    kTimeZones[currentTimeZoneIndex].tzRule,
    "pool.ntp.org",
    "time.nist.gov",
    "time.google.com"
  );
}

void applyTimeZone(int index, bool syncNtp)
{
  currentTimeZoneIndex = normalizeTimeZoneIndex(index);

  setenv("TZ", kTimeZones[currentTimeZoneIndex].tzRule, 1);
  tzset();

  if (syncNtp) {
    requestNtpSync(true);
  }
}

bool saveCurrentTimeZone()
{
  if (!sd_ok) return false;

  if (!SD.exists(TIMEZONE_DIR)) {
    SD.mkdir(TIMEZONE_DIR);
  }

  if (SD.exists(TIMEZONE_FILE)) {
    SD.remove(TIMEZONE_FILE);
  }

  File f = SD.open(TIMEZONE_FILE, FILE_WRITE);
  if (!f) return false;

  f.println(kTimeZones[currentTimeZoneIndex].tzRule);
  f.close();
  return true;
}

void loadSavedTimeZone()
{
  int zoneIndex = kDefaultTimeZoneIndex;

  if (sd_ok && SD.exists(TIMEZONE_FILE)) {
    File f = SD.open(TIMEZONE_FILE, FILE_READ);
    if (f) {
      String rule = f.readStringUntil('\n');
      rule.trim();
      f.close();

      int found = findTimeZoneIndex(rule);
      if (found >= 0) {
        zoneIndex = found;
      }
    }
  }

  applyTimeZone(zoneIndex, false);

  if (uic_TimeZoneManager) {
    lv_roller_set_selected(
      uic_TimeZoneManager,
      currentTimeZoneIndex,
      LV_ANIM_OFF
    );
  }
}

void setupTimeZoneRoller()
{
  if (!uic_TimeZoneManager) return;

  String options;
  options.reserve(512);

  for (int i = 0; i < getTimeZoneCount(); i++) {
    options += kTimeZones[i].label;
    if (i + 1 < getTimeZoneCount()) {
      options += "\n";
    }
  }

  lv_roller_set_options(
    uic_TimeZoneManager,
    options.c_str(),
    LV_ROLLER_MODE_NORMAL
  );
  lv_roller_set_selected(
    uic_TimeZoneManager,
    currentTimeZoneIndex,
    LV_ANIM_OFF
  );
}

void updateClockUI()
{
  if (!isSystemTimeValid()) {
    if (uic_labelClockTopLine) {
      lv_label_set_text(uic_labelClockTopLine, "Mon. 01. Jan. 00:00");
    }
    return;
  }

  time_t nowTs = time(nullptr);
  struct tm localTm;
  localtime_r(&nowTs, &localTm);

  char topLine[40];

  snprintf(
    topLine,
    sizeof(topLine),
    "%s. %02d. %s. %02d:%02d",
    kWeekdaysEn[localTm.tm_wday],
    localTm.tm_mday,
    kMonthsEn[localTm.tm_mon],
    localTm.tm_hour,
    localTm.tm_min
  );

  if (uic_labelClockTopLine) {
    lv_label_set_text(uic_labelClockTopLine, topLine);
  }
}

extern "C" void SaveTimeZone_Data(lv_event_t * e)
{
  (void)e;
  if (!uic_TimeZoneManager) return;

  int selected = lv_roller_get_selected(uic_TimeZoneManager);
  applyTimeZone(selected, true);

  if (saveCurrentTimeZone()) {
    Serial.println("Zeitzone gespeichert");
  } else {
    Serial.println("Zeitzone nicht gespeichert (SD nicht bereit?)");
  }

  updateClockUI();
}

/* ================= Boot Progress ================= */
int mapPercent(int value, int inMin, int inMax, int outMin, int outMax)
{
  return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
}

void bootProgress(uint8_t percent, const char* text)
{
  lv_tick_inc(20);

  lv_bar_set_value(uic_BootProgressBar, percent, LV_ANIM_ON);
  lv_label_set_text(uic_BootProgressLabel, text);

  lv_timer_handler();
  delay(20);
}

int countTextFiles()
{
  int count = 0;

  File dir = SD.open(TEXT_DIR);
  if (!dir) return 0;

  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      count++;
    }
    file = dir.openNextFile();
  }

  dir.close();
  return count;
}

void fillFileRoller_WithLiveProgress(int bootStart, int bootEnd)
{
  if (!sd_ok) return;

  int totalFiles = countTextFiles();

  File dir = SD.open(TEXT_DIR);
  if (!dir) return;

  String rollerText;
  int processed = 0;

  if (totalFiles == 0) {
    bootProgress(bootEnd, "No files found");
    fillFileRoller();
    return;
  }

  File file = dir.openNextFile();
  while (file) {

    if (!file.isDirectory()) {
      rollerText += file.name();
      rollerText += "\n";
      processed++;

      // 🔁 Datei-Prozent (0–100)
      int filePercent = (processed * 100) / totalFiles;

      // 🔁 Umrechnen auf BOOT-Bereich (z. B. 30–70)
      int bootPercent = mapPercent(
        filePercent,
        0, 100,
        bootStart, bootEnd
      );

      char label[64];
      snprintf(label, sizeof(label),
               "Scan files (%d / %d)",
               processed, totalFiles);

      // 🔄 UI LIVE aktualisieren
      lv_tick_inc(15);
      lv_bar_set_value(uic_BootProgressBar, bootPercent, LV_ANIM_ON);
      lv_label_set_text(uic_BootProgressLabel, label);
      lv_timer_handler();
      delay(15);
    }

    file = dir.openNextFile();
  }

  dir.close();

  fillFileRoller();
}


/* ================= DISPLAY FLUSH ================= */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  gfx.startWrite();
  gfx.setAddrWindow(area->x1, area->y1, w, h);
  gfx.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
  gfx.endWrite();

  lv_disp_flush_ready(disp);
}

/* ================= TOUCH ================= */
void my_touchpad_read(lv_indev_drv_t * indev, lv_indev_data_t * data)
{
  uint16_t x, y;
  if (gfx.getTouch(&x, &y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

static bool ensureAppsDirectory()
{
  if (!sd_ok) return false;
  if (SD.exists(APPS_DIR)) return true;

  if (SD.mkdir(APPS_DIR)) {
    Serial.println("Ordner /apps erstellt");
    return true;
  }

  Serial.println("Ordner /apps konnte nicht erstellt werden");
  return false;
}

static String pathBasename(const String& path)
{
  int slash = path.lastIndexOf('/');
  if (slash >= 0 && slash + 1 < path.length()) {
    return path.substring(slash + 1);
  }
  return path;
}

static void clearLauncherPage(lv_obj_t * page)
{
  if (page == NULL) return;

  lv_obj_t * child = lv_obj_get_child(page, 0);
  while (child != NULL) {
    lv_obj_del(child);
    child = lv_obj_get_child(page, 0);
  }
}

static void clearAppContentArea()
{
  if (uic_AppContentArea == NULL) return;

  lv_obj_t * child = lv_obj_get_child(uic_AppContentArea, 0);
  while (child != NULL) {
    lv_obj_del(child);
    child = lv_obj_get_child(uic_AppContentArea, 0);
  }
}

static lv_obj_t * getLauncherSlotByIndex(int index)
{
  switch (index) {
    case 0: return ui_AppL1;
    case 1: return ui_AppL2;
    case 2: return ui_AppL3;
    case 3: return ui_AppL4;
    case 4: return ui_AppL5;
    case 5: return ui_AppL6;
    default: return NULL;
  }
}

static void parseAppConfigFile(const String& folderPath, LauncherAppEntry * app)
{
  if (app == NULL) return;

  String cfgPath = folderPath + "/app.cfg";
  if (!SD.exists(cfgPath)) return;

  File cfg = SD.open(cfgPath, FILE_READ);
  if (!cfg) return;

  while (cfg.available()) {
    String line = cfg.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.startsWith("#")) continue;

    int sep = line.indexOf('=');
    if (sep <= 0) continue;

    String key = line.substring(0, sep);
    String value = line.substring(sep + 1);
    key.trim();
    key.toLowerCase();
    value.trim();

    if (key == "name" && value.length() > 0) {
      app->displayName = value;
    } else if (key == "icon" && value.length() > 0) {
      app->iconText = value;
    } else if (key == "content" && value.length() > 0) {
      app->contentFile = value;
    } else if (key == "layout" && value.length() > 0) {
      app->layoutFile = value;
    } else if (key == "type" && value.length() > 0) {
      value.toLowerCase();
      app->appType = value;
    } else if (key == "scrollable" && value.length() > 0) {
      value.toLowerCase();
      app->scrollable = (value == "1" || value == "true" || value == "yes" || value == "on");
    } else if (key == "button_text" && value.length() > 0) {
      app->buttonText = value;
    } else if (key == "button_message" && value.length() > 0) {
      app->buttonMessage = value;
    }
  }

  cfg.close();
}

static void loadAppsFromSdCard()
{
  gLauncherAppCount = 0;

  if (!sd_ok) return;
  if (!ensureAppsDirectory()) return;

  File root = SD.open(APPS_DIR);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }

  File entry = root.openNextFile();
  while (entry && gLauncherAppCount < MAX_LAUNCHER_APPS) {
    if (entry.isDirectory()) {
      String folderName = pathBasename(String(entry.name()));
      folderName.trim();

      if (folderName.length() > 0) {
        LauncherAppEntry &app = gLauncherApps[gLauncherAppCount];
        app.folderName = folderName;
        app.iconText = "";
        app.displayName = folderName;
        app.contentFile = "content.txt";
        app.layoutFile = "layout.ui";
        app.appType = "ui";
        app.scrollable = true;
        app.buttonText = "Klick mich";
        app.buttonMessage = "Button gedrueckt!";

        parseAppConfigFile(String(APPS_DIR) + "/" + folderName, &app);
        gLauncherAppCount++;
      }
    }
    entry.close();
    entry = root.openNextFile();
  }

  root.close();
}

static bool readTextFileLimited(const String& path, String * out, size_t maxLen)
{
  if (out == NULL) return false;
  out->remove(0);
  if (!SD.exists(path)) return false;

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  while (f.available()) {
    *out += (char)f.read();
    if (out->length() >= maxLen) {
      break;
    }
  }
  f.close();
  return true;
}

static String decodeEscapedText(const String& text)
{
  String out;
  out.reserve(text.length());

  bool escaping = false;
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (escaping) {
      if (c == 'n') out += '\n';
      else if (c == 't') out += '\t';
      else out += c;
      escaping = false;
    } else if (c == '\\') {
      escaping = true;
    } else {
      out += c;
    }
  }

  if (escaping) out += '\\';
  return out;
}

static String getUiField(const String& line, const char* key)
{
  String needle = String(key) + "=";
  int start = line.indexOf(needle);
  if (start < 0) return "";

  start += needle.length();
  int end = line.indexOf(';', start);
  if (end < 0) end = line.length();

  String value = line.substring(start, end);
  value.trim();
  return value;
}

static int getUiFieldInt(const String& line, const char* key, int defaultValue)
{
  String value = getUiField(line, key);
  if (value.length() == 0) return defaultValue;
  return value.toInt();
}

static uint32_t getUiFieldColor(const String& line, const char* key, uint32_t defaultValue)
{
  String value = getUiField(line, key);
  if (value.length() == 0) return defaultValue;

  if (value.startsWith("0x") || value.startsWith("0X")) {
    value = value.substring(2);
  }
  char buf[16];
  value.toCharArray(buf, sizeof(buf));
  return (uint32_t)strtoul(buf, NULL, 16);
}

static bool getUiFieldBool(const String& line, const char* key, bool defaultValue)
{
  String value = getUiField(line, key);
  if (value.length() == 0) return defaultValue;

  value.toLowerCase();
  if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
  if (value == "0" || value == "false" || value == "no" || value == "off") return false;
  return defaultValue;
}

static bool renderAppLayout(const LauncherAppEntry& app)
{
  String layoutPath = String(APPS_DIR) + "/" + app.folderName + "/" + app.layoutFile;
  String layoutText;
  if (!readTextFileLimited(layoutPath, &layoutText, 32000)) {
    return false;
  }

  lv_obj_clear_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
  if (app.scrollable) {
    lv_obj_add_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(uic_AppContentArea, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_AUTO);
  } else {
    lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_OFF);
  }

  int lineStart = 0;
  bool createdAny = false;
  while (lineStart >= 0 && lineStart < (int)layoutText.length()) {
    int lineEnd = layoutText.indexOf('\n', lineStart);
    if (lineEnd < 0) lineEnd = layoutText.length();

    String line = layoutText.substring(lineStart, lineEnd);
    line.trim();
    if (line.length() > 0 && !line.startsWith("#")) {
      String type = getUiField(line, "type");
      type.toLowerCase();

      int x = getUiFieldInt(line, "x", 0);
      int y = getUiFieldInt(line, "y", 0);
      int w = getUiFieldInt(line, "w", 220);
      int h = getUiFieldInt(line, "h", 50);
      String text = decodeEscapedText(getUiField(line, "text"));
      uint32_t bg = getUiFieldColor(line, "bg", 0xFFFFFF);
      uint32_t fg = getUiFieldColor(line, "fg", 0x000000);

      if (type == "label") {
        lv_obj_t * obj = lv_label_create(uic_AppContentArea);
        lv_label_set_text(obj, text.c_str());
        if (w > 0) {
          lv_obj_set_width(obj, w);
          lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
        }
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_style_text_color(obj, lv_color_hex(fg), LV_PART_MAIN | LV_STATE_DEFAULT);
        createdAny = true;
      } else if (type == "button") {
        lv_obj_t * btn = lv_btn_create(uic_AppContentArea);
        lv_obj_set_size(btn, w, h);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, text.c_str());
        lv_obj_set_style_text_color(label, lv_color_hex(fg), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(label);
        createdAny = true;
      } else if (type == "textarea") {
        lv_obj_t * ta = lv_textarea_create(uic_AppContentArea);
        lv_obj_set_size(ta, w, h);
        lv_obj_set_pos(ta, x, y);
        lv_textarea_set_text(ta, text.c_str());
        lv_textarea_set_one_line(ta, getUiFieldBool(line, "one_line", false));
        createdAny = true;
      } else if (type == "switch") {
        lv_obj_t * sw = lv_switch_create(uic_AppContentArea);
        lv_obj_set_pos(sw, x, y);
        if (getUiFieldBool(line, "value", false)) {
          lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        createdAny = true;
      } else if (type == "checkbox") {
        lv_obj_t * cb = lv_checkbox_create(uic_AppContentArea);
        lv_obj_set_pos(cb, x, y);
        lv_checkbox_set_text(cb, text.c_str());
        if (getUiFieldBool(line, "value", false)) {
          lv_obj_add_state(cb, LV_STATE_CHECKED);
        }
        createdAny = true;
      } else if (type == "panel") {
        lv_obj_t * panel = lv_obj_create(uic_AppContentArea);
        lv_obj_set_size(panel, w, h);
        lv_obj_set_pos(panel, x, y);
        lv_obj_set_style_bg_color(panel, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
        createdAny = true;
      }
    }

    if (lineEnd >= (int)layoutText.length()) break;
    lineStart = lineEnd + 1;
  }

  return createdAny;
}

static bool renderAppTextContent(const LauncherAppEntry& app)
{
  lv_obj_add_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(uic_AppContentArea, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t * contentLabel = lv_label_create(uic_AppContentArea);
  lv_obj_set_width(contentLabel, lv_pct(100));
  lv_label_set_long_mode(contentLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(contentLabel, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_text_font(contentLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  String contentPath = String(APPS_DIR) + "/" + app.folderName + "/" + app.contentFile;
  String contentText;
  if (readTextFileLimited(contentPath, &contentText, 16000) && contentText.length() > 0) {
    lv_label_set_text(contentLabel, contentText.c_str());
    lv_obj_update_layout(uic_AppContentArea);
    return true;
  }

  contentText = "Keine Inhalte gefunden.\n\nLege eine Datei an:\n";
  contentText += contentPath;
  lv_label_set_text(contentLabel, contentText.c_str());
  lv_obj_update_layout(uic_AppContentArea);
  return false;
}

static void resetCalculatorState()
{
  gCalcDisplay = NULL;
  gCalcExpression = "";
  gCalcShowingError = false;
}

static bool isCalcOperatorChar(char c)
{
  return c == '+' || c == '-' || c == '*' || c == '/';
}

static int calcOperatorPrecedence(char op)
{
  if (op == '*' || op == '/') return 2;
  if (op == '+' || op == '-') return 1;
  return 0;
}

static bool applyCalcOperator(double * values, int * valueTop, char op, bool * divisionByZero)
{
  if (*valueTop < 2) return false;

  double right = values[--(*valueTop)];
  double left = values[--(*valueTop)];
  double result = 0.0;

  switch (op) {
    case '+': result = left + right; break;
    case '-': result = left - right; break;
    case '*': result = left * right; break;
    case '/':
      if (fabs(right) < 1e-12) {
        if (divisionByZero) *divisionByZero = true;
        return false;
      }
      result = left / right;
      break;
    default:
      return false;
  }

  values[(*valueTop)++] = result;
  return true;
}

static bool evaluateCalculatorExpression(const String& expression, double * out, bool * divisionByZero)
{
  if (out == NULL) return false;
  if (divisionByZero) *divisionByZero = false;

  String normalized = expression;
  normalized.replace(',', '.');
  normalized.trim();
  if (normalized.length() == 0) return false;

  const int kMaxCalcStack = 64;
  double values[kMaxCalcStack];
  char operators[kMaxCalcStack];
  int valueTop = 0;
  int operatorTop = 0;

  const char * p = normalized.c_str();
  bool expectNumber = true;

  while (*p != '\0') {
    if (isspace((unsigned char)*p)) {
      p++;
      continue;
    }

    if (expectNumber) {
      char * endPtr = NULL;
      double number = strtod(p, &endPtr);
      if (endPtr == p) return false;
      if (valueTop >= kMaxCalcStack) return false;

      values[valueTop++] = number;
      p = endPtr;
      expectNumber = false;
      continue;
    }

    char op = *p;
    if (!isCalcOperatorChar(op)) return false;

    while (operatorTop > 0 &&
           calcOperatorPrecedence(operators[operatorTop - 1]) >= calcOperatorPrecedence(op)) {
      if (!applyCalcOperator(values, &valueTop, operators[--operatorTop], divisionByZero)) {
        return false;
      }
    }

    if (operatorTop >= kMaxCalcStack) return false;
    operators[operatorTop++] = op;
    p++;
    expectNumber = true;
  }

  if (expectNumber) return false;

  while (operatorTop > 0) {
    if (!applyCalcOperator(values, &valueTop, operators[--operatorTop], divisionByZero)) {
      return false;
    }
  }

  if (valueTop != 1) return false;
  *out = values[0];
  return true;
}

static String formatCalculatorResult(double value)
{
  char buffer[40];
  snprintf(buffer, sizeof(buffer), "%.10g", value);
  String result = String(buffer);
  result.replace('.', ',');
  return result;
}

static bool currentNumberHasComma(const String& expression)
{
  for (int i = expression.length() - 1; i >= 0; i--) {
    char c = expression[i];
    if (c == ',') return true;
    if (isCalcOperatorChar(c)) break;
  }
  return false;
}

static void updateCalculatorDisplay()
{
  if (gCalcDisplay == NULL) return;

  if (gCalcShowingError) {
    lv_textarea_set_text(gCalcDisplay, "Math Error");
    return;
  }

  if (gCalcExpression.length() == 0) {
    lv_textarea_set_text(gCalcDisplay, "0");
  } else {
    lv_textarea_set_text(gCalcDisplay, gCalcExpression.c_str());
  }
}

static void calculatorButtonEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  const char * token = (const char *)lv_event_get_user_data(e);
  if (token == NULL) return;

  if (gCalcShowingError && strcmp(token, "C") != 0) {
    gCalcExpression = "";
    gCalcShowingError = false;
  }

  if (strcmp(token, "C") == 0) {
    gCalcExpression = "";
    gCalcShowingError = false;
    updateCalculatorDisplay();
    return;
  }

  if (strcmp(token, "<") == 0) {
    if (gCalcExpression.length() > 0) {
      gCalcExpression.remove(gCalcExpression.length() - 1);
    }
    updateCalculatorDisplay();
    return;
  }

  if (strcmp(token, "=") == 0) {
    bool divisionByZero = false;
    double value = 0.0;
    if (evaluateCalculatorExpression(gCalcExpression, &value, &divisionByZero)) {
      gCalcExpression = formatCalculatorResult(value);
      gCalcShowingError = false;
    } else {
      gCalcExpression = "";
      gCalcShowingError = divisionByZero;
      if (!gCalcShowingError) {
        gCalcExpression = "0";
      }
    }
    updateCalculatorDisplay();
    return;
  }

  if (strcmp(token, ",") == 0) {
    if (gCalcExpression.length() == 0 || isCalcOperatorChar(gCalcExpression[gCalcExpression.length() - 1])) {
      gCalcExpression += "0,";
    } else if (!currentNumberHasComma(gCalcExpression)) {
      gCalcExpression += ",";
    }
    updateCalculatorDisplay();
    return;
  }

  if (strlen(token) == 1 && isCalcOperatorChar(token[0])) {
    if (gCalcExpression.length() == 0) {
      if (token[0] == '-') {
        gCalcExpression += "-";
      }
      updateCalculatorDisplay();
      return;
    }

    char last = gCalcExpression[gCalcExpression.length() - 1];
    if (isCalcOperatorChar(last)) {
      gCalcExpression.setCharAt(gCalcExpression.length() - 1, token[0]);
    } else {
      gCalcExpression += token;
    }
    updateCalculatorDisplay();
    return;
  }

  gCalcExpression += token;
  updateCalculatorDisplay();
}

static void renderCalculatorButton(const char * text, int x, int y, int w, int h)
{
  lv_obj_t * btn = lv_btn_create(uic_AppContentArea);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_radius(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(btn, calculatorButtonEvent, LV_EVENT_CLICKED, (void *)text);

  lv_obj_t * label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(label);
}

static void renderCalculatorApp()
{
  resetCalculatorState();

  lv_obj_t * panel = lv_obj_create(uic_AppContentArea);
  lv_obj_set_size(panel, 760, 430);
  lv_obj_set_pos(panel, 20, 30);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(panel, 14, LV_PART_MAIN | LV_STATE_DEFAULT);

  gCalcDisplay = lv_textarea_create(uic_AppContentArea);
  lv_obj_set_size(gCalcDisplay, 680, 64);
  lv_obj_set_pos(gCalcDisplay, 60, 56);
  lv_textarea_set_one_line(gCalcDisplay, true);
  lv_obj_clear_flag(gCalcDisplay, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_add_state(gCalcDisplay, LV_STATE_DISABLED);

  const int bx = 60;
  const int by = 140;
  const int bw = 160;
  const int bh = 58;
  const int gap = 10;

  renderCalculatorButton("C", bx + 0 * (bw + gap), by + 0 * (bh + gap), bw, bh);
  renderCalculatorButton("<", bx + 1 * (bw + gap), by + 0 * (bh + gap), bw, bh);
  renderCalculatorButton("/", bx + 2 * (bw + gap), by + 0 * (bh + gap), bw, bh);
  renderCalculatorButton("*", bx + 3 * (bw + gap), by + 0 * (bh + gap), bw, bh);

  renderCalculatorButton("7", bx + 0 * (bw + gap), by + 1 * (bh + gap), bw, bh);
  renderCalculatorButton("8", bx + 1 * (bw + gap), by + 1 * (bh + gap), bw, bh);
  renderCalculatorButton("9", bx + 2 * (bw + gap), by + 1 * (bh + gap), bw, bh);
  renderCalculatorButton("-", bx + 3 * (bw + gap), by + 1 * (bh + gap), bw, bh);

  renderCalculatorButton("4", bx + 0 * (bw + gap), by + 2 * (bh + gap), bw, bh);
  renderCalculatorButton("5", bx + 1 * (bw + gap), by + 2 * (bh + gap), bw, bh);
  renderCalculatorButton("6", bx + 2 * (bw + gap), by + 2 * (bh + gap), bw, bh);
  renderCalculatorButton("+", bx + 3 * (bw + gap), by + 2 * (bh + gap), bw, bh);

  renderCalculatorButton("1", bx + 0 * (bw + gap), by + 3 * (bh + gap), bw, bh);
  renderCalculatorButton("2", bx + 1 * (bw + gap), by + 3 * (bh + gap), bw, bh);
  renderCalculatorButton("3", bx + 2 * (bw + gap), by + 3 * (bh + gap), bw, bh);
  renderCalculatorButton("=", bx + 3 * (bw + gap), by + 3 * (bh + gap), bw, bh);

  renderCalculatorButton("0", bx + 0 * (bw + gap), by + 4 * (bh + gap), bw * 2 + gap, bh);
  renderCalculatorButton(",", bx + 2 * (bw + gap), by + 4 * (bh + gap), bw, bh);

  gCalcExpression = "";
  gCalcShowingError = false;
  updateCalculatorDisplay();
}

static void resetRadioState()
{
  gRadioTabView = NULL;
  gRadioFileRoller = NULL;
  gRadioWebRoller = NULL;
  gRadioToggleButton = NULL;
  gRadioToggleLabel = NULL;
  gRadioCurrentFile = "";
  gRadioCurrentWebUrl = "";
}

static bool isMusicFileName(const String& filename)
{
  String name = filename;
  name.toLowerCase();
  return name.endsWith(".mp3") ||
         name.endsWith(".wav") ||
         name.endsWith(".ogg") ||
         name.endsWith(".aac") ||
         name.endsWith(".m4a");
}

static void fillRadioFileRoller()
{
  if (gRadioFileRoller == NULL) return;
  if (!sd_ok) {
    lv_roller_set_options(gRadioFileRoller, "SD Fehler", LV_ROLLER_MODE_NORMAL);
    return;
  }

  File dir = SD.open(MUSIC_FILES_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    lv_roller_set_options(gRadioFileRoller, "Keine Dateien", LV_ROLLER_MODE_NORMAL);
    return;
  }

  String options = "";
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String name = pathBasename(String(entry.name()));
      name.trim();
      if (name.length() > 0 && isMusicFileName(name)) {
        options += name;
        options += "\n";
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  if (options.length() == 0) {
    options = "Keine Dateien";
  } else if (options.endsWith("\n")) {
    options.remove(options.length() - 1);
  }

  lv_roller_set_options(gRadioFileRoller, options.c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(gRadioFileRoller, 0, LV_ANIM_OFF);
}

static bool loadWebRadioStations()
{
  gWebRadioCount = 0;
  if (!sd_ok) return false;
  if (!SD.exists(WEBRADIO_FILE)) return false;

  File f = SD.open(WEBRADIO_FILE, FILE_READ);
  if (!f) return false;

  while (f.available() && gWebRadioCount < MAX_WEBRADIO_STATIONS) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int sep = line.indexOf('|');
    if (sep <= 0) continue;

    String name = line.substring(0, sep);
    String url = line.substring(sep + 1);
    name.trim();
    url.trim();
    if (name.length() == 0 || url.length() == 0) continue;

    gWebRadioStations[gWebRadioCount].name = name;
    gWebRadioStations[gWebRadioCount].url = url;
    gWebRadioCount++;
  }

  f.close();
  return gWebRadioCount > 0;
}

static void fillWebRadioRoller()
{
  if (gRadioWebRoller == NULL) return;
  if (!loadWebRadioStations()) {
    lv_roller_set_options(gRadioWebRoller, "Keine Sender", LV_ROLLER_MODE_NORMAL);
    return;
  }

  String options = "";
  for (int i = 0; i < gWebRadioCount; i++) {
    options += gWebRadioStations[i].name;
    options += "\n";
  }

  if (options.endsWith("\n")) {
    options.remove(options.length() - 1);
  }

  lv_roller_set_options(gRadioWebRoller, options.c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(gRadioWebRoller, 0, LV_ANIM_OFF);
}

static String getRadioSelectedFile()
{
  if (gRadioFileRoller == NULL) return "";

  char buf[160];
  lv_roller_get_selected_str(gRadioFileRoller, buf, sizeof(buf));
  String file = String(buf);
  file.trim();
  if (file.length() == 0 || file == "Keine Dateien" || file == "SD Fehler") return "";
  return file;
}

static int getSelectedWebRadioIndex()
{
  if (gRadioWebRoller == NULL) return -1;
  if (gWebRadioCount <= 0) return -1;
  int index = lv_roller_get_selected(gRadioWebRoller);
  if (index < 0 || index >= gWebRadioCount) return -1;
  return index;
}

static void stopRadioPlayback()
{
  if (gRadioPlaying) {
    audio.stopSong();
  }
  gRadioPlaying = false;
  gRadioSource = RADIO_SOURCE_NONE;
  gRadioCurrentFile = "";
  gRadioCurrentWebUrl = "";
}

static void updateRadioToggleButtonLabel()
{
  if (gRadioToggleLabel == NULL) return;
  lv_label_set_text(gRadioToggleLabel, gRadioPlaying ? "Stop" : "Start");
}

static void startOrStopSelectedRadio()
{
  if (gRadioTabView == NULL) return;

  if (gRadioPlaying) {
    stopRadioPlayback();
    updateRadioToggleButtonLabel();
    return;
  }

  uint32_t activeTab = lv_tabview_get_tab_act(gRadioTabView);

  if (activeTab == 0) {
    String file = getRadioSelectedFile();
    if (file.length() == 0) return;

    String path = String(MUSIC_FILES_DIR) + "/" + file;
    if (!SD.exists(path)) return;

    stopRadioPlayback();
    audio.connecttoFS(SD, path.c_str());
    gRadioPlaying = true;
    gRadioSource = RADIO_SOURCE_FILE;
    gRadioCurrentFile = path;
    updateRadioToggleButtonLabel();
    return;
  }

  if (activeTab == 1) {
    int index = getSelectedWebRadioIndex();
    if (index < 0) return;

    String url = gWebRadioStations[index].url;
    if (url.length() == 0) return;

    stopRadioPlayback();
    audio.connecttohost(url.c_str());
    gRadioPlaying = true;
    gRadioSource = RADIO_SOURCE_WEB;
    gRadioCurrentWebUrl = url;
    updateRadioToggleButtonLabel();
  }
}

static void radioToggleButtonEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  startOrStopSelectedRadio();
}

static void radioTabChangedEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  updateRadioToggleButtonLabel();
}

static void renderRadioApp()
{
  resetRadioState();

  lv_obj_t * panel = lv_obj_create(uic_AppContentArea);
  lv_obj_set_size(panel, 760, 430);
  lv_obj_set_pos(panel, 20, 30);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(panel, 14, LV_PART_MAIN | LV_STATE_DEFAULT);

  gRadioTabView = lv_tabview_create(panel, LV_DIR_TOP, 44);
  lv_obj_set_size(gRadioTabView, 720, 300);
  lv_obj_set_pos(gRadioTabView, 20, 16);
  lv_obj_add_event_cb(gRadioTabView, radioTabChangedEvent, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t * tabFiles = lv_tabview_add_tab(gRadioTabView, "Dateien");
  lv_obj_t * tabWeb = lv_tabview_add_tab(gRadioTabView, "Webradio");

  gRadioFileRoller = lv_roller_create(tabFiles);
  lv_obj_set_size(gRadioFileRoller, 680, 230);
  lv_obj_center(gRadioFileRoller);
  lv_obj_set_style_text_font(gRadioFileRoller, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  gRadioWebRoller = lv_roller_create(tabWeb);
  lv_obj_set_size(gRadioWebRoller, 680, 230);
  lv_obj_center(gRadioWebRoller);
  lv_obj_set_style_text_font(gRadioWebRoller, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  gRadioToggleButton = lv_btn_create(panel);
  lv_obj_set_size(gRadioToggleButton, 240, 64);
  lv_obj_set_pos(gRadioToggleButton, 260, 348);
  lv_obj_set_style_radius(gRadioToggleButton, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(gRadioToggleButton, radioToggleButtonEvent, LV_EVENT_CLICKED, NULL);

  gRadioToggleLabel = lv_label_create(gRadioToggleButton);
  lv_obj_set_style_text_font(gRadioToggleLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(gRadioToggleLabel);

  fillRadioFileRoller();
  fillWebRadioRoller();
  updateRadioToggleButtonLabel();
}

void audio_eof_mp3(const char *info)
{
  (void)info;
  if (!gRadioPlaying) return;
  gRadioPlaying = false;
  gRadioSource = RADIO_SOURCE_NONE;
  gRadioCurrentFile = "";
  gRadioCurrentWebUrl = "";
  updateRadioToggleButtonLabel();
}

static void showAppContentForIndex(int appIndex)
{
  if (appIndex < 0 || appIndex >= gLauncherAppCount) return;
  LauncherAppEntry &app = gLauncherApps[appIndex];

  _ui_screen_change(&ui_AppContent, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_AppContent_screen_init);

  if (uic_AppContentArea == NULL) return;

  clearAppContentArea();

  lv_obj_set_style_pad_all(uic_AppContentArea, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  if (app.appType == "button") {
    lv_obj_add_flag(uic_AppContentArea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t * infoLabel = lv_label_create(uic_AppContentArea);
    lv_obj_set_width(infoLabel, lv_pct(100));
    lv_label_set_long_mode(infoLabel, LV_LABEL_LONG_WRAP);
    lv_obj_align(infoLabel, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(infoLabel, "Button Demo App");
    lv_obj_set_style_text_align(infoLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(infoLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * demoButton = lv_btn_create(uic_AppContentArea);
    lv_obj_set_size(demoButton, 260, 90);
    lv_obj_align(demoButton, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_radius(demoButton, 18, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * buttonLabel = lv_label_create(demoButton);
    lv_label_set_text(buttonLabel, app.buttonText.c_str());
    lv_obj_center(buttonLabel);
    lv_obj_set_style_text_font(buttonLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * statusLabel = lv_label_create(uic_AppContentArea);
    lv_obj_set_width(statusLabel, lv_pct(100));
    lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_WRAP);
    lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 80);
    lv_label_set_text(statusLabel, "Noch nicht gedrueckt.");
    lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    gDemoButtonMessage = app.buttonMessage;
    lv_obj_add_event_cb(demoButton, demoButtonClicked, LV_EVENT_CLICKED, statusLabel);
    return;
  }

  if (app.appType == "calculator") {
    renderCalculatorApp();
    return;
  }

  if (app.appType == "radio") {
    renderRadioApp();
    return;
  }

  if (app.appType == "ui") {
    if (renderAppLayout(app)) return;
    renderAppTextContent(app);
    return;
  }

  renderAppTextContent(app);
}

static void launcherAppTileEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  int * appIndex = (int *)lv_event_get_user_data(e);
  if (appIndex == NULL) return;

  showAppContentForIndex(*appIndex);
}

static void demoButtonClicked(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  lv_obj_t * statusLabel = (lv_obj_t *)lv_event_get_user_data(e);
  if (statusLabel == NULL) return;

  lv_label_set_text(statusLabel, gDemoButtonMessage.c_str());
}

static void drawLauncherApps()
{
  if (ui_AppL1 == NULL || ui_AppL2 == NULL || ui_AppL3 == NULL ||
      ui_AppL4 == NULL || ui_AppL5 == NULL || ui_AppL6 == NULL) return;

  for (int i = 0; i < MAX_LAUNCHER_APPS; i++) {
    lv_obj_t * slot = getLauncherSlotByIndex(i);
    if (slot == NULL) continue;

    clearLauncherPage(slot);

    if (i >= gLauncherAppCount) {
      lv_obj_t * emptyLabel = lv_label_create(slot);
      lv_label_set_text(emptyLabel, "Leer");
      lv_obj_center(emptyLabel);
      continue;
    }

    lv_obj_t * appBtn = lv_btn_create(slot);
    lv_obj_set_size(appBtn, lv_pct(100), lv_pct(100));
    lv_obj_center(appBtn);
    lv_obj_clear_flag(appBtn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(appBtn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * appLabel = lv_label_create(appBtn);
    String tileText = gLauncherApps[i].displayName;
    if (gLauncherApps[i].iconText.length() > 0) {
      tileText = gLauncherApps[i].iconText + "\n" + gLauncherApps[i].displayName;
    }
    lv_label_set_text(appLabel, tileText.c_str());
    lv_obj_set_width(appLabel, lv_pct(90));
    lv_label_set_long_mode(appLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(appLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(appLabel);

    gLauncherAppIndices[i] = i;
    lv_obj_add_event_cb(appBtn, launcherAppTileEvent, LV_EVENT_CLICKED, &gLauncherAppIndices[i]);
  }
}

extern "C" void StartAppLauncher_Data(lv_event_t * e)
{
  (void)e;
  loadAppsFromSdCard();
  drawLauncherApps();
}

extern "C" void UnloadApp_Data(lv_event_t * e)
{
  (void)e;
  stopRadioPlayback();
  clearAppContentArea();
  resetCalculatorState();
  resetRadioState();
  StartAppLauncher_Data(NULL);
}

/* ================= SD INIT ================= */
void initSD()
{
  if (SD.begin(SD_CS)) {
    sd_ok = true;
    Serial.println("SD Karte initialisiert");

    if (!SD.exists(TEXT_DIR)) {
      SD.mkdir(TEXT_DIR);
      Serial.println("Ordner /text erstellt");
    }

    if (!SD.exists(MUSIC_FILES_DIR)) {
      SD.mkdir(MUSIC_FILES_DIR);
      Serial.println("Ordner /music/files erstellt");
    }

    if (!SD.exists(WEBRADIO_DIR)) {
      SD.mkdir(WEBRADIO_DIR);
      Serial.println("Ordner /music/webradio erstellt");
    }

    ensureAppsDirectory();


    if (!SD.exists("/system")) {
      SD.mkdir("/system");
      Serial.println("Ordner /system erstellt");
    }

    if (!SD.exists(WIFI_DIR)) {
      SD.mkdir(WIFI_DIR);
      Serial.println("Ordner /system/wifi erstellt");
    }

    if (!SD.exists(TIMEZONE_DIR)) {
      SD.mkdir(TIMEZONE_DIR);
      Serial.println("Ordner /system/timezone erstellt");
    }

  } else {
    sd_ok = false;
    Serial.println("SD Karte NICHT gefunden");
  }
}


/* ================= SD READ ================= */
void readSDInfo()
{
  if (!sd_ok) return;

  totalBytes = SD.totalBytes();
  usedBytes  = SD.usedBytes();
  freeBytes  = totalBytes - usedBytes;

  if (totalBytes > 0) {
    usedPercent = (usedBytes * 100) / totalBytes;
  } else {
    usedPercent = 0;
  }
}

/* =========================================================
   WI-FI
   ========================================================= */
unsigned long lastReconnectAttempt = 0;
volatile bool wifiConnecting = false;
TaskHandle_t wifiReconnectTaskHandle = nullptr;
static const unsigned long kWifiReconnectIntervalMs = 30000;

extern "C" void SaveWifiConnection_Data(lv_event_t * e)
{
  if (!sd_ok) return;

  const char* ssid = lv_textarea_get_text(uic_TextAreaWifiSSID);
  const char* pass = lv_textarea_get_text(uic_TextAreaWifiPassword);

  if (strlen(ssid) == 0) return;

  File f = SD.open(WIFI_FILE, FILE_APPEND);
  if (!f) return;

  f.print(ssid);
  f.print("|");
  f.println(pass);
  f.close();

  Serial.println("WLAN Profil hinzugefügt");
}

int loadWifiProfiles(WifiProfile profiles[])
{
  if (!sd_ok) return 0;
  if (!SD.exists(WIFI_FILE)) return 0;

  File f = SD.open(WIFI_FILE, FILE_READ);
  if (!f) return 0;

  int count = 0;

  while (f.available() && count < MAX_WIFI_PROFILES) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int sep = line.indexOf('|');
    if (sep < 0) continue;

    profiles[count].ssid = line.substring(0, sep);
    profiles[count].pass = line.substring(sep + 1);

    profiles[count].ssid.trim();
    profiles[count].pass.trim();

    count++;
  }

  f.close();
  return count;
}

bool connectKnownWifi(bool updateUiState = true)
{
  WifiProfile profiles[MAX_WIFI_PROFILES];
  int profileCount = loadWifiProfiles(profiles);

  if (profileCount == 0) {
    Serial.println("Keine WLAN Profile gespeichert");
    return false;
  }

  Serial.println("Scanne WLANs...");
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println("Keine WLANs gefunden");
    return false;
  }

  int bestProfile = -1;
  int bestRSSI = -999;

  // 🔍 Alle gefundenen WLANs mit gespeicherten vergleichen
  for (int i = 0; i < n; i++) {
    String foundSSID = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);

    for (int p = 0; p < profileCount; p++) {
      if (foundSSID == profiles[p].ssid) {

        Serial.printf(
          "Bekanntes WLAN gefunden: %s (RSSI %d)\n",
          foundSSID.c_str(), rssi
        );

        if (rssi > bestRSSI) {
          bestRSSI = rssi;
          bestProfile = p;
        }
      }
    }
  }

  if (bestProfile < 0) {
    Serial.println("Kein bekanntes WLAN erreichbar");
    return false;
  }

  // 🚀 Nur EIN Verbindungsversuch
  Serial.println("Verbinde bestes WLAN: " + profiles[bestProfile].ssid);
  WiFi.begin(
    profiles[bestProfile].ssid.c_str(),
    profiles[bestProfile].pass.c_str()
  );

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 10000) {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWLAN verbunden!");
    Serial.println(WiFi.localIP());

    if (updateUiState) updateWifiIcon();
    return true;
  }

  Serial.println("\nVerbindung fehlgeschlagen");
  if (updateUiState) updateWifiIcon();
  return false;

}

void wifiReconnectTaskRunner(void* parameter)
{
  (void)parameter;

  WiFi.disconnect(true);
  delay(100);

  if (connectKnownWifi(false)) {
    requestNtpSync(true);
  }

  wifiConnecting = false;
  wifiReconnectTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void reconnectWifi()
{
  if (wifiConnecting) return;

  wifiConnecting = true;

  Serial.println("Starte WLAN Reconnect (async)...");

  BaseType_t created = xTaskCreate(
    wifiReconnectTaskRunner,
    "wifi_reconnect",
    8192,
    nullptr,
    1,
    &wifiReconnectTaskHandle
  );

  if (created != pdPASS) {
    Serial.println("WLAN Task konnte nicht gestartet werden");
    wifiConnecting = false;
    wifiReconnectTaskHandle = nullptr;
  }
}

extern "C" void ReloadWiFiConnection_Data(lv_event_t * e)
{
  Serial.println("Manueller WLAN Reload");

  reconnectWifi();
  updateWifiIcon();
}

void updateWifiIcon()
{
  if (WiFi.status() == WL_CONNECTED) {
    // Icon anzeigen
    lv_obj_clear_flag(uic_WiFiImage, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Icon ausblenden
    lv_obj_add_flag(uic_WiFiImage, LV_OBJ_FLAG_HIDDEN);
  }
}

/* =========================================================
   SYSTEM INFO
   ========================================================= */
extern "C" void updateSystemInfoData(void)
{
  char buf[256];

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  const char * model = "ESP32";
  switch (chip_info.model) {
    case CHIP_ESP32:   model = "ESP32"; break;
    case CHIP_ESP32S2: model = "ESP32-S2"; break;
    case CHIP_ESP32S3: model = "ESP32-S3"; break;
    case CHIP_ESP32C3: model = "ESP32-C3"; break;
    default:           model = "Unbekannt"; break;
  }

  uint64_t chip_id = ESP.getEfuseMac();

  snprintf(
    buf,
    sizeof(buf),
    "Modell : %s\n"
    "Kerne  : %d\n"
    "RAM    : %d Bytes\n"
    "CPU    : %d MHz\n"
    "ChipID : %04X%08X",
    model,
    chip_info.cores,
    esp_get_free_heap_size(),
    ESP.getCpuFreqMHz(),
    (uint16_t)(chip_id >> 32),
    (uint32_t)chip_id
  );

  lv_label_set_text(ui_systemInfoLabel, buf);
}

/* =========================================================
   SD UI UPDATE (LABEL + BAR)
   ========================================================= */
extern "C" void updateSDUIData(void)
{
  char buf[128];

  if (!sd_ok) {
    lv_label_set_text(uic_LabelSDInfo, "SD Karte nicht vorhanden");
    lv_bar_set_value(uic_BarSD, 0, LV_ANIM_OFF);
    return;
  }

  readSDInfo();

  float totalGB = totalBytes / 1024.0 / 1024.0 / 1024.0;
  float freeGB  = freeBytes  / 1024.0 / 1024.0 / 1024.0;

  snprintf(
    buf,
    sizeof(buf),
    "SD Karte\nGesamt: %.2f GB\nFrei: %.2f GB",
    totalGB,
    freeGB
  );

  lv_label_set_text(uic_LabelSDInfo, buf);
  lv_bar_set_value(uic_BarSD, usedPercent, LV_ANIM_ON);
}

static String getStorageManagerParentPath(const String& path)
{
  if (path.length() == 0 || path == "/") return "/";

  String clean = path;
  while (clean.length() > 1 && clean.endsWith("/")) {
    clean.remove(clean.length() - 1);
  }

  int slash = clean.lastIndexOf('/');
  if (slash <= 0) return "/";
  return clean.substring(0, slash);
}

static String joinStorageManagerPath(const String& base, const String& name)
{
  if (base.length() == 0 || base == "/") {
    return String("/") + name;
  }
  return base + "/" + name;
}

/* =========================================================
   FILE ROLLER FUNKTIONEN
   ========================================================= */
void fillFileRoller()
{
  if (uic_FileRollerFileManager == NULL) return;

  if (!sd_ok) {
    lv_roller_set_options(uic_FileRollerFileManager, "SD Fehler", LV_ROLLER_MODE_NORMAL);
    return;
  }

  if (gStorageManagerCurrentDir.length() == 0) {
    gStorageManagerCurrentDir = "/";
  }

  File dir = SD.open(gStorageManagerCurrentDir.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    gStorageManagerCurrentDir = "/";
    dir = SD.open(gStorageManagerCurrentDir.c_str());
  }

  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    lv_roller_set_options(uic_FileRollerFileManager, "Ordner nicht verfügbar", LV_ROLLER_MODE_NORMAL);
    return;
  }

  String folders = "";
  String files = "";

  if (gStorageManagerCurrentDir != "/") {
    folders += "..\n";
  }

  File entry = dir.openNextFile();
  while (entry) {
    String name = pathBasename(String(entry.name()));
    name.trim();

    if (name.length() > 0) {
      if (entry.isDirectory()) {
        folders += name;
        folders += "/\n";
      } else {
        files += name;
        files += "\n";
      }
    }

    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  String rollerText = folders + files;
  if (rollerText.length() == 0) {
    rollerText = "(Leer)";
  } else if (rollerText.endsWith("\n")) {
    rollerText.remove(rollerText.length() - 1);
  }

  lv_roller_set_options(uic_FileRollerFileManager, rollerText.c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(uic_FileRollerFileManager, 0, LV_ANIM_OFF);
}

String getSelectedFileFromRoller()
{
  char buf[128];
  lv_roller_get_selected_str(
    uic_FileRollerFileManager,
    buf,
    sizeof(buf)
  );
  return String(buf);
}

extern "C" void deleteSelectedFile(void)
{
  if (!sd_ok) return;

  String selected = getSelectedFileFromRoller();
  selected.trim();
  if (selected.length() == 0 || selected == "(Leer)" || selected == "..") return;
  if (selected.endsWith("/")) return;

  String path = joinStorageManagerPath(gStorageManagerCurrentDir, selected);

  if (!SD.exists(path)) {
    Serial.println("Datei nicht gefunden: " + path);
    return;
  }

  File target = SD.open(path, FILE_READ);
  if (!target) return;

  bool isDirectory = target.isDirectory();
  target.close();
  if (isDirectory) return;

  if (SD.remove(path)) {
    Serial.println("Datei gelöscht: " + path);
  }

  fillFileRoller();
  updateSDUIData();
}

extern "C" void StorageManagerSelect_Data(lv_event_t * e)
{
  (void)e;
  if (!sd_ok) return;

  String selected = getSelectedFileFromRoller();
  selected.trim();
  if (selected.length() == 0 || selected == "(Leer)") return;

  if (selected == "..") {
    gStorageManagerCurrentDir = getStorageManagerParentPath(gStorageManagerCurrentDir);
    fillFileRoller();
    return;
  }

  if (!selected.endsWith("/")) {
    return;
  }

  selected.remove(selected.length() - 1);
  if (selected.length() == 0) return;

  String nextPath = joinStorageManagerPath(gStorageManagerCurrentDir, selected);
  File dir = SD.open(nextPath.c_str());
  if (!dir) return;

  bool canEnter = dir.isDirectory();
  dir.close();
  if (!canEnter) return;

  gStorageManagerCurrentDir = nextPath;
  fillFileRoller();
}

extern "C" void ResetStorageManagerToRoot_Data(void)
{
  gStorageManagerCurrentDir = "/";
  fillFileRoller();
}


/* =========================================================
   FILE ROLLER – TEXT VIEWER SCREEN
   wird beim Laden des Text-Screens aufgerufen
   ========================================================= */
extern "C" void fillFileRoller_TextViewer_Data(void)
{
  if (!sd_ok) return;

  File root = SD.open(TEXT_DIR);
  if (!root) return;

  String rollerText = "";

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      rollerText += file.name();
      rollerText += "\n";
    }
    file = root.openNextFile();
  }

  root.close();

  if (rollerText.length() == 0) {
    rollerText = "Keine Dateien";
  }

  lv_roller_set_options(
    uic_FileRollerText,
    rollerText.c_str(),
    LV_ROLLER_MODE_NORMAL
  );
}


extern "C" void load_selected_file_Data(void)
{
  if (!sd_ok) {
    lv_textarea_set_text(uic_TextArea, "SD Karte nicht verfügbar");
    return;
  }

  /* ausgewählten Dateinamen aus dem Roller holen */
  char buf[128];
  lv_roller_get_selected_str(
    uic_FileRollerText,
    buf,
    sizeof(buf)
  );

  String filename = String(buf);

  if (filename.length() == 0 || filename == "Keine Dateien") {
    lv_textarea_set_text(uic_TextArea, "Keine Datei ausgewählt");
    return;
  }

  String path = String(TEXT_DIR) + "/" + filename;

  /* Existenz prüfen */
  if (!SD.exists(path)) {
    lv_textarea_set_text(uic_TextArea, "Datei nicht gefunden");
    return;
  }

  /* Datei öffnen */
  File file = SD.open(path, FILE_READ);
  if (!file) {
    lv_textarea_set_text(uic_TextArea, "Datei konnte nicht geöffnet werden");
    return;
  }

  /* Dateiinhalt lesen */
  String content;
  content.reserve(1024);   // RAM-schonend

  while (file.available()) {
    content += (char)file.read();

    /* RAM-Schutz für ESP32 */
    if (content.length() > 4000) {
      content += "\n\n[Datei gekürzt]";
      break;
    }
  }

  file.close();

  /* Textarea befüllen */
  lv_textarea_set_text(uic_TextArea, content.c_str());
}


/* =========================================================
   TEXTDATEI SPEICHERN (IMMER ÜBERSCHREIBEN)
   wird von SquareLine Button aufgerufen
   ========================================================= */
extern "C" void save_text_file_data(lv_event_t * e)
{
  if (!sd_ok) {
    Serial.println("SD Karte nicht verfügbar");
    return;
  }

  // Text aus der TextArea
  const char* text = lv_textarea_get_text(uic_TextArea);

  // Dateiname aus dem Input
  const char* filename = lv_textarea_get_text(uic_FileNameInput);

  if (strlen(filename) == 0) {
    Serial.println("Dateiname leer");
    return;
  }

  String path = String(TEXT_DIR) + "/" + filename + ".txt";


  // 🔥 Datei IMMER überschreiben
  if (SD.exists(path.c_str())) {
    SD.remove(path.c_str());
  }

  File file = SD.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Datei konnte nicht erstellt werden");
    return;
  }

  file.print(text);   // kein zusätzlicher Zeilenumbruch
  file.close();

  Serial.println("Datei gespeichert: " + path);

  // 🔄 UI aktualisieren
  fillFileRoller();
  updateSDUIData();
  fillFileRoller_TextViewer_Data();
}

extern "C" void OpenNewFile_Data(lv_event_t * e)
{
  lv_textarea_set_text(uic_TextArea, "");
}


/* ================= SETUP ================= */
void setup()
{
  Serial.begin(115200);

  /* ================= DISPLAY ================= */
  gfx.init();
  gfx.setRotation(0);
  gfx.setBrightness(255);

  /* ================= LVGL ================= */
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 800 * 40);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 800;
  disp_drv.ver_res = 480;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  /* ================= UI INIT ================= */
  ui_init();
  lv_scr_load(uic_ScreenHome);
  setupTimeZoneRoller();
  updateClockUI();

  /* ================= BOOT OVERLAY EIN ================= */
  lv_obj_move_foreground(uic_BootOverlay);
  lv_obj_clear_flag(uic_BootOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_timer_handler();
  delay(20);

  /* ================= BOOT PROGRESS ================= */
bootProgress(5,  "Start system");
bootProgress(10, "Initialize display");

bootProgress(15, "Initialize SD card");
initSD();
loadSavedTimeZone();
updateClockUI();
StartAppLauncher_Data(NULL);

bootProgress(20, "Scan files");
fillFileRoller_WithLiveProgress(20, 50);

bootProgress(60, "Initialize WiFi");
WiFi.mode(WIFI_STA);
reconnectWifi();

/* ================= AUDIO INIT ================= */
bootProgress(70, "Initialize Audio");
audio.setPinout(
  42,   // I2S_BCLK
  18,   // I2S_LRC
  17    // I2S_DOUT
);
audio.setVolume(21); // 0..21 (CrowPanel Lautsprecher brauchen meist 10–14)


bootProgress(80, "Update memory info");
updateSDUIData();

bootProgress(95, "Start user interface");
bootProgress(100, "Finished");
delay(500);

lv_obj_add_flag(uic_BootOverlay, LV_OBJ_FLAG_HIDDEN);
updateClockUI();


}


/* ================= LOOP ================= */
void loop()
{
  uint32_t now = millis();
  if (now - last_tick >= 5) {
    lv_tick_inc(now - last_tick);
    last_tick = now;
    lv_timer_handler();
  }

  // alle 30 Sekunden Wi-Fi prüfen (non-blocking Reconnect)
  if (millis() - lastReconnectAttempt > kWifiReconnectIntervalMs) {

    lastReconnectAttempt = millis();

    if (WiFi.status() != WL_CONNECTED) {

      Serial.println("WLAN verloren → suche neu...");
      reconnectWifi();
    }
  }

  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 2000) {   // alle 2 Sekunden
    lastWifiCheck = millis();
    updateWifiIcon();
  }

  if (millis() - lastClockUiUpdate > 1000) {
    lastClockUiUpdate = millis();
    requestNtpSync(false);
    updateClockUI();
  }

  // Audio
  audio.loop();

  delay(5);
}
