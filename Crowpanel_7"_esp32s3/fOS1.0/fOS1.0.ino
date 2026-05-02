#include <lvgl.h>
#include "LGFX_CrowPanel.h"
#include "ui.h"
#include "esp_system.h"
#include "Arduino.h"
#include <SD.h>
#include <SPI.h>
#include "Audio.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void audio_eof_mp3(const char *info);
void setup();
void loop();

#define WIFI_DIR  "/system/wifi"
#define WIFI_FILE "/system/wifi/wlans.txt"
#define TIMEZONE_DIR  "/system/timezone"
#define TIMEZONE_FILE "/system/timezone/timezone.txt"
#define TEXT_DIR "/text"
#define MUSIC_FILES_DIR   "/music/files"
#define WEBRADIO_DIR      "/music/webradio"
#define WEBRADIO_FILE     "/music/webradio/webradio.txt"
#define APPS_DIR "/apps"

#define MUSIC_DIR MUSIC_FILES_DIR   // wichtig: Rest vom Code bleibt kompatibel
#define MAX_WEBRADIOS 30
#define MAX_LAUNCHER_APPS 4


#define MAX_WIFI_PROFILES 5


extern "C" void deleteSelectedFile(void);

/* ================= DISPLAY ================= */
LGFX gfx;

/* ================= AUDIO ================= */
Audio audio;

enum AudioSource {
  AUDIO_NONE,
  AUDIO_FILE,
  AUDIO_WEB
};

AudioSource currentSource = AUDIO_NONE;
bool audioPlaying = false;

struct WebRadioStation {
  String name;
  String url;
};

WebRadioStation webRadios[MAX_WEBRADIOS];
int webRadioCount = 0;

struct LauncherAppEntry {
  String folderName;
  String displayName;
  String contentFile;
  String appType;
  String buttonText;
  String buttonMessage;
};

static LauncherAppEntry gLauncherApps[MAX_LAUNCHER_APPS];
static int gLauncherAppCount = 0;
static int gLauncherAppIndices[MAX_LAUNCHER_APPS];
static String gDemoButtonMessage = "Button gedrueckt!";

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
static bool stopwatchRunning = false;
static uint64_t stopwatchElapsedMs = 0;
static unsigned long stopwatchStartedAtMs = 0;
static unsigned long lastStopwatchUiUpdate = 0;

static const char* kWeekdaysEn[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char* kMonthsEn[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const int kWeatherDisplayDays = 7;
static const int kWeatherApiForecastDays = kWeatherDisplayDays + 1; // +1 wegen "ab morgen"
static const int kWeatherMaxForecastDays = 10;

#define CALC_STACK_SIZE 128
#define CALC_CHECKMARK_UTF8 "\xE2\x9C\x93"
#define CALC_BACKSPACE_UTF8 "\xE2\x8C\xAB"
static bool g_calc_division_by_zero = false;

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

static void ensure_calc_cursor_visible()
{
  if (uic_TextAreaCalculator == NULL) {
    return;
  }
  lv_obj_add_state(uic_TextAreaCalculator, LV_STATE_FOCUSED);
}

static bool is_operator_char(char c)
{
  return c == '+' || c == '-' || c == '*' || c == '/' || c == 'x' || c == 'X';
}

static int operator_precedence(char op)
{
  if (op == '*' || op == '/' || op == 'x' || op == 'X') {
    return 2;
  }
  if (op == '+' || op == '-') {
    return 1;
  }
  return 0;
}

static bool apply_operator(double *values, int *value_top, char op)
{
  double right;
  double left;
  double result;

  if (*value_top < 2) {
    return false;
  }

  right = values[--(*value_top)];
  left = values[--(*value_top)];

  switch (op) {
    case '+':
      result = left + right;
      break;
    case '-':
      result = left - right;
      break;
    case '*':
    case 'x':
    case 'X':
      result = left * right;
      break;
    case '/':
      if (right == 0.0) {
        g_calc_division_by_zero = true;
        return false;
      }
      result = left / right;
      break;
    default:
      return false;
  }

  values[(*value_top)++] = result;
  return true;
}

double evaluate_expression(const char * expr)
{
  double values[CALC_STACK_SIZE];
  char operators[CALC_STACK_SIZE];
  int value_top = 0;
  int operator_top = 0;
  const char *p = expr;
  bool expect_number = true;
  g_calc_division_by_zero = false;

  if (expr == NULL) {
    return 0.0;
  }

  while (*p != '\0') {
    if (isspace((unsigned char)*p)) {
      p++;
      continue;
    }

    if (expect_number) {
      char *endptr;
      double number = strtod(p, &endptr);

      if (endptr == p) {
        return 0.0;
      }
      if (value_top >= CALC_STACK_SIZE) {
        return 0.0;
      }

      values[value_top++] = number;
      p = endptr;
      expect_number = false;
      continue;
    }

    if (!is_operator_char(*p)) {
      return 0.0;
    }

    while (operator_top > 0 &&
           operator_precedence(operators[operator_top - 1]) >= operator_precedence(*p)) {
      if (!apply_operator(values, &value_top, operators[--operator_top])) {
        return 0.0;
      }
    }

    if (operator_top >= CALC_STACK_SIZE) {
      return 0.0;
    }

    operators[operator_top++] = *p;
    p++;
    expect_number = true;
  }

  if (expect_number) {
    return 0.0;
  }

  while (operator_top > 0) {
    if (!apply_operator(values, &value_top, operators[--operator_top])) {
      return 0.0;
    }
  }

  if (value_top != 1) {
    return 0.0;
  }

  return values[0];
}

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
    if (uic_RealTimeClock) {
      lv_label_set_text(uic_RealTimeClock, "00:00:00");
    }
    return;
  }

  time_t nowTs = time(nullptr);
  struct tm localTm;
  localtime_r(&nowTs, &localTm);

  char topLine[40];
  char clockLine[16];

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

  snprintf(
    clockLine,
    sizeof(clockLine),
    "%02d:%02d:%02d",
    localTm.tm_hour,
    localTm.tm_min,
    localTm.tm_sec
  );

  if (uic_labelClockTopLine) {
    lv_label_set_text(uic_labelClockTopLine, topLine);
  }
  if (uic_RealTimeClock) {
    lv_label_set_text(uic_RealTimeClock, clockLine);
  }

  static int lastYear = -1;
  static int lastMonth = -1;
  static int lastDay = -1;

  int year = localTm.tm_year + 1900;
  int month = localTm.tm_mon + 1;
  int day = localTm.tm_mday;

  if (uic_Calendar &&
      (year != lastYear || month != lastMonth || day != lastDay)) {
    lv_calendar_set_today_date(uic_Calendar, year, month, day);
    lv_calendar_set_showed_date(uic_Calendar, year, month);

    lastYear = year;
    lastMonth = month;
    lastDay = day;
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

uint64_t getStopwatchElapsedMs()
{
  if (!stopwatchRunning) {
    return stopwatchElapsedMs;
  }
  return stopwatchElapsedMs + (millis() - stopwatchStartedAtMs);
}

void updateStopwatchUI(bool force)
{
  unsigned long now = millis();
  if (!force && !stopwatchRunning) return;
  if (!force && (now - lastStopwatchUiUpdate < 100)) return;
  lastStopwatchUiUpdate = now;

  uint64_t elapsedMs = getStopwatchElapsedMs();
  uint64_t totalSeconds = elapsedMs / 1000ULL;

  uint32_t hours = totalSeconds / 3600ULL;
  uint32_t minutes = (totalSeconds % 3600ULL) / 60ULL;
  uint32_t seconds = totalSeconds % 60ULL;

  char stopwatchText[20];
  snprintf(
    stopwatchText,
    sizeof(stopwatchText),
    "%02lu:%02lu:%02lu",
    (unsigned long)hours,
    (unsigned long)minutes,
    (unsigned long)seconds
  );

  if (uic_LabelStopwatchDisplay) {
    lv_label_set_text(uic_LabelStopwatchDisplay, stopwatchText);
  }
  if (uic_LabelStopwatchPlay) {
    lv_label_set_text(uic_LabelStopwatchPlay, stopwatchRunning ? "||" : ">");
  }
}

extern "C" void StopwatchPlay_Data(lv_event_t * e)
{
  (void)e;

  if (stopwatchRunning) {
    stopwatchElapsedMs += millis() - stopwatchStartedAtMs;
    stopwatchRunning = false;
  } else {
    stopwatchStartedAtMs = millis();
    stopwatchRunning = true;
  }

  updateStopwatchUI(true);
}

extern "C" void StopwatchReset_Data(lv_event_t * e)
{
  (void)e;

  stopwatchRunning = false;
  stopwatchElapsedMs = 0;
  stopwatchStartedAtMs = millis();

  updateStopwatchUI(true);
}

extern "C" void keypad_event_handler_Data(lv_event_t * e)
{
  lv_obj_t * keyboard = lv_event_get_target(e);
  uint16_t button_id = lv_keyboard_get_selected_btn(keyboard);
  const char *button_text;

  if (button_id == LV_BTNMATRIX_BTN_NONE) {
    return;
  }

  button_text = lv_keyboard_get_btn_text(keyboard, button_id);

  if (button_text == NULL || uic_TextAreaCalculator == NULL) {
    return;
  }

  ensure_calc_cursor_visible();

  if (strcmp(button_text, LV_SYMBOL_OK) == 0 ||
      strcmp(button_text, CALC_CHECKMARK_UTF8) == 0 ||
      strcmp(button_text, "=") == 0 ||
      strcmp(button_text, "\n") == 0) {
    char result_text[32];
    const char *expression = lv_textarea_get_text(uic_TextAreaCalculator);
    double result = evaluate_expression(expression);

    if (g_calc_division_by_zero) {
      lv_textarea_set_text(uic_TextAreaCalculator, "Math Error");
    } else {
      snprintf(result_text, sizeof(result_text), "%.10g", result);
      lv_textarea_set_text(uic_TextAreaCalculator, result_text);
    }
    lv_textarea_set_cursor_pos(uic_TextAreaCalculator, LV_TEXTAREA_CURSOR_LAST);
    ensure_calc_cursor_visible();
    return;
  }

  if (strcmp(button_text, LV_SYMBOL_BACKSPACE) == 0 ||
      strcmp(button_text, CALC_BACKSPACE_UTF8) == 0) {
    lv_textarea_del_char(uic_TextAreaCalculator);
    return;
  }

  if (strcmp(button_text, LV_SYMBOL_LEFT) == 0 || strcmp(button_text, "<") == 0) {
    lv_textarea_cursor_left(uic_TextAreaCalculator);
    ensure_calc_cursor_visible();
    return;
  }

  if (strcmp(button_text, LV_SYMBOL_RIGHT) == 0 || strcmp(button_text, ">") == 0) {
    lv_textarea_cursor_right(uic_TextAreaCalculator);
    ensure_calc_cursor_visible();
    return;
  }

  lv_textarea_add_text(uic_TextAreaCalculator, button_text);
  ensure_calc_cursor_visible();
}

extern "C" void operator_event_handler_Data(lv_event_t * e)
{
  lv_obj_t *target = lv_event_get_target(e);

  if (uic_TextAreaCalculator == NULL) {
    return;
  }

  ensure_calc_cursor_visible();

  if (target == uic_ButtonCalculatorAdd) {
    lv_textarea_add_text(uic_TextAreaCalculator, "+");
  } else if (target == uic_ButtonCalculatorSubtract) {
    lv_textarea_add_text(uic_TextAreaCalculator, "-");
  } else if (target == uic_ButtonCalculatorMultiply) {
    lv_textarea_add_text(uic_TextAreaCalculator, "x");
  } else if (target == uic_ButtonCalculatorDivide) {
    lv_textarea_add_text(uic_TextAreaCalculator, "/");
  }
  ensure_calc_cursor_visible();
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

  if (rollerText.length() == 0) rollerText = "Keine Dateien";

  lv_roller_set_options(
    uic_FileRollerFileManager,
    rollerText.c_str(),
    LV_ROLLER_MODE_NORMAL
  );
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
    } else if (key == "content" && value.length() > 0) {
      app->contentFile = value;
    } else if (key == "type" && value.length() > 0) {
      value.toLowerCase();
      app->appType = value;
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
        app.displayName = folderName;
        app.contentFile = "content.txt";
        app.appType = "text";
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

static void showAppContentForIndex(int appIndex)
{
  if (appIndex < 0 || appIndex >= gLauncherAppCount) return;
  LauncherAppEntry &app = gLauncherApps[appIndex];

  _ui_screen_change(&ui_AppContent, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_AppContent_screen_init);

  if (uic_AppName != NULL) {
    lv_label_set_text(uic_AppName, app.displayName.c_str());
  }

  if (uic_AppContentArea == NULL) return;

  clearAppContentArea();

  lv_obj_set_style_pad_all(uic_AppContentArea, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

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

  lv_obj_add_flag(uic_AppContentArea, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(uic_AppContentArea, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t * contentLabel = lv_label_create(uic_AppContentArea);
  lv_obj_set_width(contentLabel, lv_pct(100));
  lv_label_set_long_mode(contentLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(contentLabel, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_text_font(contentLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  String contentPath = String(APPS_DIR) + "/" + app.folderName + "/" + app.contentFile;

  String contentText;
  if (SD.exists(contentPath)) {
    File contentFile = SD.open(contentPath, FILE_READ);
    if (contentFile) {
      while (contentFile.available()) {
        contentText += (char)contentFile.read();
        if (contentText.length() > 16000) {
          contentText += "\n\n[Inhalt gekuerzt]";
          break;
        }
      }
      contentFile.close();
    }
  }

  if (contentText.length() == 0) {
    contentText = "Keine Inhalte gefunden.\n\nLege eine Datei an:\n";
    contentText += contentPath;
  }

  lv_label_set_text(contentLabel, contentText.c_str());
  lv_obj_update_layout(uic_AppContentArea);
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
  if (ui_AppL1 == NULL || ui_AppL2 == NULL || ui_AppL3 == NULL || ui_AppL4 == NULL) return;

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
    lv_label_set_text(appLabel, gLauncherApps[i].displayName.c_str());
    lv_obj_set_width(appLabel, lv_pct(90));
    lv_label_set_long_mode(appLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(appLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(appLabel);

    gLauncherAppIndices[i] = i;
    lv_obj_add_event_cb(appBtn, launcherAppTileEvent, LV_EVENT_CLICKED, &gLauncherAppIndices[i]);
  }
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

    if (!SD.exists(MUSIC_DIR)) {
      SD.mkdir(MUSIC_DIR);
      Serial.println("Ordner /music erstellt");
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
   WEATHER (Current location via IP + Open-Meteo)
   ========================================================= */
bool weatherHttpGet(const String& url, String& response, int* statusOut = nullptr)
{
  response = "";
  HTTPClient http;
  int httpCode = -1;
  if (statusOut) *statusOut = httpCode;

  if (url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!http.begin(client, url)) return false;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("fOS1.0 Weather/1.0");
    http.setConnectTimeout(8000);
    http.setTimeout(12000);
    httpCode = http.GET();
  } else {
    if (!http.begin(url)) return false;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("fOS1.0 Weather/1.0");
    http.setConnectTimeout(8000);
    http.setTimeout(12000);
    httpCode = http.GET();
  }

  if (statusOut) *statusOut = httpCode;
  Serial.printf("Weather GET code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    response = http.getString();
  }
  http.end();

  return httpCode == HTTP_CODE_OK && response.length() > 0;
}

int findJsonArrayEnd(const String& json, int arrayStart)
{
  int depth = 0;
  for (int i = arrayStart; i < (int)json.length(); i++) {
    char c = json[i];
    if (c == '[') depth++;
    else if (c == ']') {
      depth--;
      if (depth == 0) return i;
    }
  }
  return -1;
}

int findJsonObjectEnd(const String& json, int objectStart)
{
  int depth = 0;
  for (int i = objectStart; i < (int)json.length(); i++) {
    char c = json[i];
    if (c == '{') depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0) return i;
    }
  }
  return -1;
}

bool extractJsonObject(const String& json, const char* key, String& out)
{
  out = "";
  String needle = "\"";
  needle += key;
  needle += "\":";

  int pos = json.indexOf(needle);
  if (pos < 0) return false;

  pos = json.indexOf('{', pos);
  if (pos < 0) return false;

  int objEnd = findJsonObjectEnd(json, pos);
  if (objEnd < 0) return false;

  out = json.substring(pos, objEnd + 1);
  return out.length() > 0;
}

bool extractJsonStringValue(const String& json, const char* key, String& out)
{
  out = "";

  String needle = "\"";
  needle += key;
  needle += "\":";

  int pos = json.indexOf(needle);
  if (pos < 0) return false;

  pos += needle.length();
  while (pos < (int)json.length() && isspace((unsigned char)json[pos])) pos++;
  if (pos >= (int)json.length() || json[pos] != '"') return false;
  pos++;

  int end = pos;
  while (end < (int)json.length()) {
    if (json[end] == '"' && json[end - 1] != '\\') break;
    end++;
  }
  if (end >= (int)json.length()) return false;

  out = json.substring(pos, end);
  return out.length() > 0;
}

bool extractJsonFloatValue(const String& json, const char* key, float& out)
{
  String needle = "\"";
  needle += key;
  needle += "\":";

  int pos = json.indexOf(needle);
  if (pos < 0) return false;

  pos += needle.length();
  while (pos < (int)json.length() && isspace((unsigned char)json[pos])) pos++;
  if (pos >= (int)json.length()) return false;

  int start = pos;
  while (pos < (int)json.length()) {
    char c = json[pos];
    if ((c >= '0' && c <= '9') ||
        c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
      pos++;
      continue;
    }
    break;
  }

  if (start == pos) return false;

  out = json.substring(start, pos).toFloat();
  return true;
}

int extractJsonFloatArray(const String& json, const char* key, float out[], int maxCount)
{
  String needle = "\"";
  needle += key;
  needle += "\":";

  int pos = json.indexOf(needle);
  if (pos < 0) return 0;

  pos = json.indexOf('[', pos);
  if (pos < 0) return 0;

  int arrayEnd = findJsonArrayEnd(json, pos);
  if (arrayEnd < 0) return 0;

  int count = 0;
  int i = pos + 1;

  while (i < arrayEnd && count < maxCount) {
    while (i < arrayEnd && (isspace((unsigned char)json[i]) || json[i] == ',')) i++;
    if (i >= arrayEnd) break;

    int start = i;
    while (i < arrayEnd) {
      char c = json[i];
      if ((c >= '0' && c <= '9') ||
          c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
        i++;
        continue;
      }
      break;
    }

    if (start == i) {
      while (i < arrayEnd && json[i] != ',') i++;
      continue;
    }

    out[count++] = json.substring(start, i).toFloat();
  }

  return count;
}

int extractJsonStringArray(const String& json, const char* key, String out[], int maxCount)
{
  String needle = "\"";
  needle += key;
  needle += "\":";

  int pos = json.indexOf(needle);
  if (pos < 0) return 0;

  pos = json.indexOf('[', pos);
  if (pos < 0) return 0;

  int arrayEnd = findJsonArrayEnd(json, pos);
  if (arrayEnd < 0) return 0;

  int count = 0;
  int i = pos + 1;

  while (i < arrayEnd && count < maxCount) {
    while (i < arrayEnd && (isspace((unsigned char)json[i]) || json[i] == ',')) i++;
    if (i >= arrayEnd) break;

    if (json[i] != '"') {
      while (i < arrayEnd && json[i] != ',') i++;
      continue;
    }

    i++;
    int start = i;
    while (i < arrayEnd) {
      if (json[i] == '"' && json[i - 1] != '\\') break;
      i++;
    }
    if (i >= arrayEnd) break;

    out[count++] = json.substring(start, i);
    i++;
  }

  return count;
}

const char* weatherCodeToText(int code)
{
  switch (code) {
    case 0: return "Clear sky";
    case 1: return "Mainly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: return "Fog";
    case 48: return "Rime fog";
    case 51: return "Light drizzle";
    case 53: return "Drizzle";
    case 55: return "Dense drizzle";
    case 56: return "Freezing drizzle";
    case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: return "Freezing rain";
    case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: return "Rain showers";
    case 81: return "Rain showers";
    case 82: return "Heavy showers";
    case 85: return "Snow showers";
    case 86: return "Heavy snow showers";
    case 95: return "Thunderstorm";
    case 96: return "Storm with hail";
    case 99: return "Storm with hail";
    default: return "Unknown";
  }
}

bool getCurrentLocation(float& lat, float& lon, String& city)
{
  lat = 0.0f;
  lon = 0.0f;
  city = "";

  String geoJson;
  if (!weatherHttpGet(
      "http://ip-api.com/json/?fields=status,message,lat,lon,city",
      geoJson)) {
    return false;
  }

  String status;
  if (!extractJsonStringValue(geoJson, "status", status)) return false;
  if (status != "success") return false;

  float latValue = 0.0f;
  float lonValue = 0.0f;
  if (!extractJsonFloatValue(geoJson, "lat", latValue)) return false;
  if (!extractJsonFloatValue(geoJson, "lon", lonValue)) return false;

  extractJsonStringValue(geoJson, "city", city);

  lat = latValue;
  lon = lonValue;
  return true;
}

void setWeatherUiLoadingState()
{
  if (uic_LabelWeatherTemperature) {
    lv_label_set_text(uic_LabelWeatherTemperature, "--°C   --%");
  }
  if (uic_LabelWeatherInformation) {
    lv_label_set_text(uic_LabelWeatherInformation, "Loading weather...");
  }
  if (uic_RollerWeatherData) {
    lv_roller_set_options(
      uic_RollerWeatherData,
      "Loading forecast...",
      LV_ROLLER_MODE_NORMAL
    );
  }
}

void setWeatherUiErrorState(const char* text)
{
  if (uic_LabelWeatherTemperature) {
    lv_label_set_text(uic_LabelWeatherTemperature, "--°C   --%");
  }
  if (uic_LabelWeatherInformation) {
    lv_label_set_text(uic_LabelWeatherInformation, text);
  }
  if (uic_RollerWeatherData) {
    lv_roller_set_options(uic_RollerWeatherData, "No forecast data", LV_ROLLER_MODE_NORMAL);
  }
}

void buildWeatherForecastRoller(
  const String dailyDates[],
  const float dailyCodes[],
  const float dailyMins[],
  const float dailyMaxs[],
  int dayCount,
  String& rollerText
)
{
  rollerText = "";
  int added = 0;

  for (int i = 1; i < dayCount && added < kWeatherDisplayDays; i++) {
    if (dailyDates[i].length() < 10) continue;

    String shortDate = dailyDates[i].substring(8, 10);
    shortDate += ".";
    shortDate += dailyDates[i].substring(5, 7);
    shortDate += ".";

    int minT = (int)roundf(dailyMins[i]);
    int maxT = (int)roundf(dailyMaxs[i]);
    const char* condition = weatherCodeToText((int)dailyCodes[i]);

    char line[96];
    snprintf(
      line,
      sizeof(line),
      "%s  %d/%d°C  %s",
      shortDate.c_str(),
      minT,
      maxT,
      condition
    );

    if (rollerText.length() > 0) rollerText += "\n";
    rollerText += line;
    added++;
  }

  if (rollerText.length() == 0) {
    rollerText = "No forecast data";
  }
}

extern "C" void StartWeatherApp_Data(lv_event_t * e)
{
  (void)e;
  setWeatherUiLoadingState();

  if (WiFi.status() != WL_CONNECTED) {
    setWeatherUiErrorState("WiFi not connected");
    return;
  }

  float lat = 0.0f;
  float lon = 0.0f;
  String city;

  if (!getCurrentLocation(lat, lon, city)) {
    setWeatherUiErrorState("Location not available");
    return;
  }

  String weatherQuery = "api.open-meteo.com/v1/forecast?latitude=";
  weatherQuery += String(lat, 4);
  weatherQuery += "&longitude=";
  weatherQuery += String(lon, 4);
  weatherQuery += "&current=temperature_2m,relative_humidity_2m,weather_code";
  weatherQuery += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
  weatherQuery += "&forecast_days=";
  weatherQuery += String(kWeatherApiForecastDays);
  weatherQuery += "&timezone=auto";

  String weatherUrl = "https://" + weatherQuery;

  String weatherJson;
  int weatherCodePrimary = -1;
  if (!weatherHttpGet(weatherUrl, weatherJson, &weatherCodePrimary)) {
    int weatherCodeFallback = -1;
    String weatherUrlFallback = "http://" + weatherQuery;
    if (!weatherHttpGet(weatherUrlFallback, weatherJson, &weatherCodeFallback)) {
      char err[64];
      snprintf(
        err,
        sizeof(err),
        "Weather unavailable (%d/%d)",
        weatherCodePrimary,
        weatherCodeFallback
      );
      setWeatherUiErrorState(err);
      return;
    }
  }

  String currentJson;
  String dailyJson;
  if (!extractJsonObject(weatherJson, "current", currentJson) ||
      !extractJsonObject(weatherJson, "daily", dailyJson)) {
    setWeatherUiErrorState("Weather data invalid");
    return;
  }

  float currentTemp = 0.0f;
  float currentHumidity = 0.0f;
  float currentCode = -1.0f;

  if (!extractJsonFloatValue(currentJson, "temperature_2m", currentTemp) ||
      !extractJsonFloatValue(currentJson, "relative_humidity_2m", currentHumidity) ||
      !extractJsonFloatValue(currentJson, "weather_code", currentCode)) {
    setWeatherUiErrorState("Weather data invalid");
    return;
  }

  String dailyDates[kWeatherMaxForecastDays];
  float dailyCodes[kWeatherMaxForecastDays];
  float dailyMaxs[kWeatherMaxForecastDays];
  float dailyMins[kWeatherMaxForecastDays];

  int dateCount = extractJsonStringArray(
    dailyJson, "time", dailyDates, kWeatherMaxForecastDays
  );
  int codeCount = extractJsonFloatArray(
    dailyJson, "weather_code", dailyCodes, kWeatherMaxForecastDays
  );
  int maxCount = extractJsonFloatArray(
    dailyJson, "temperature_2m_max", dailyMaxs, kWeatherMaxForecastDays
  );
  int minCount = extractJsonFloatArray(
    dailyJson, "temperature_2m_min", dailyMins, kWeatherMaxForecastDays
  );

  int dayCount = dateCount;
  if (codeCount < dayCount) dayCount = codeCount;
  if (maxCount < dayCount) dayCount = maxCount;
  if (minCount < dayCount) dayCount = minCount;

  if (uic_LabelWeatherTemperature) {
    char tempLabel[32];
    snprintf(
      tempLabel,
      sizeof(tempLabel),
      "%d°C   %d%%",
      (int)roundf(currentTemp),
      (int)roundf(currentHumidity)
    );
    lv_label_set_text(uic_LabelWeatherTemperature, tempLabel);
  }

  if (uic_LabelWeatherInformation) {
    const char* baseCondition = weatherCodeToText((int)currentCode);
    if (city.length() > 0) {
      String infoText = String(baseCondition) + " (" + city + ")";
      lv_label_set_text(uic_LabelWeatherInformation, infoText.c_str());
    } else {
      lv_label_set_text(uic_LabelWeatherInformation, baseCondition);
    }
  }

  if (uic_RollerWeatherData) {
    String rollerText;
    buildWeatherForecastRoller(
      dailyDates,
      dailyCodes,
      dailyMins,
      dailyMaxs,
      dayCount,
      rollerText
    );
    lv_roller_set_options(
      uic_RollerWeatherData,
      rollerText.c_str(),
      LV_ROLLER_MODE_NORMAL
    );
    lv_roller_set_selected(uic_RollerWeatherData, 0, LV_ANIM_OFF);
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

extern "C" void StartAppLauncher_Data(lv_event_t * e)
{
  (void)e;
  loadAppsFromSdCard();
  drawLauncherApps();
}

/* =========================================================
   FILE ROLLER FUNKTIONEN
   ========================================================= */
void fillFileRoller()
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
    uic_FileRollerFileManager,
    rollerText.c_str(),
    LV_ROLLER_MODE_NORMAL
  );
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

  String filename = getSelectedFileFromRoller();
  if (filename.length() == 0 || filename == "Keine Dateien") return;

  String path = String(TEXT_DIR) + "/" + filename;

  if (SD.exists(path)) {
    SD.remove(path);
    Serial.println("Datei gelöscht: " + path);
  } else {
    Serial.println("Datei nicht gefunden: " + path);
  }

  fillFileRoller();
  updateSDUIData();
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


/* =========================================================
   MUSIK
   MUSIK ROLLER – RADIO SCREEN
   wird beim Laden des Radio-Screens aufgerufen
   ========================================================= */
bool loadWebRadiosFromSD()
{
  webRadioCount = 0;

  if (!sd_ok) return false;
  if (!SD.exists(WEBRADIO_FILE)) return false;

  File f = SD.open(WEBRADIO_FILE, FILE_READ);
  if (!f) return false;

  while (f.available() && webRadioCount < MAX_WEBRADIOS) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int sep = line.indexOf('|');
    if (sep < 0) continue;

    webRadios[webRadioCount].name = line.substring(0, sep);
    webRadios[webRadioCount].url  = line.substring(sep + 1);

    webRadios[webRadioCount].name.trim();
    webRadios[webRadioCount].url.trim();

    webRadioCount++;
  }

  f.close();
  return webRadioCount > 0;
}


extern "C" void fillFileRoller_Radio_Data(void)
{
  if (!sd_ok) return;

  File root = SD.open(MUSIC_DIR);
  if (!root) {
    lv_roller_set_options(
      uic_RollerOptionRadio,
      "SD Fehler",
      LV_ROLLER_MODE_NORMAL
    );
    return;
  }

  String rollerText = "";
  File file = root.openNextFile();

  while (file) {
    if (!file.isDirectory()) {

      // optional: nur Musikdateien
      String name = file.name();
      name.toLowerCase();

      if (
        name.endsWith(".mp3") ||
        name.endsWith(".wav") ||
        name.endsWith(".ogg")
      ) {
        rollerText += file.name();
        rollerText += "\n";
      }
    }
    file = root.openNextFile();
  }

  root.close();

  if (rollerText.length() == 0) {
    rollerText = "Keine Musik";
  }

  lv_roller_set_options(
    uic_RollerOptionRadio,
    rollerText.c_str(),
    LV_ROLLER_MODE_NORMAL
  );

  // optional: immer erstes Element auswählen
  lv_roller_set_selected(uic_RollerOptionRadio, 0, LV_ANIM_OFF);
}

extern "C" void fillWebRadioRoller_Data(void)
{
  if (!loadWebRadiosFromSD()) {
    lv_roller_set_options(
      uic_RollerOptionWebRadio,
      "Keine Sender",
      LV_ROLLER_MODE_NORMAL
    );
    return;
  }

  String rollerText = "";
  for (int i = 0; i < webRadioCount; i++) {
    rollerText += webRadios[i].name;
    rollerText += "\n";
  }

  lv_roller_set_options(
    uic_RollerOptionWebRadio,
    rollerText.c_str(),
    LV_ROLLER_MODE_NORMAL
  );

  lv_roller_set_selected(uic_RollerOptionWebRadio, 0, LV_ANIM_OFF);
}


String getSelectedRadioFile()
{
  char buf[128];
  lv_roller_get_selected_str(
    uic_RollerOptionRadio,
    buf,
    sizeof(buf)
  );

  String file = String(buf);
  file.trim();

  if (
    file.length() == 0 ||
    file == "Keine Musik" ||
    file == "SD Fehler"
  ) {
    return "";
  }

  return file;
}

void playFilePlayer()
{
  if (!sd_ok) return;

  // STOP File Player
  if (audioPlaying && currentSource == AUDIO_FILE) {
    audio.stopSong();
    audioPlaying = false;
    currentSource = AUDIO_NONE;
    Serial.println("File Player stopped");
    return;
  }

  // Falls Webradio läuft → stoppen
  if (audioPlaying && currentSource == AUDIO_WEB) {
    audio.stopSong();
  }

  String file = getSelectedFilePlayerFile();
  if (file == "") return;

  String path = String(MUSIC_DIR) + "/" + file;
  if (!SD.exists(path)) return;

  audio.connecttoFS(SD, path.c_str());

  audioPlaying = true;
  currentSource = AUDIO_FILE;

  Serial.println("File Player play: " + path);
}


void audio_eof_mp3(const char *info)
{
  Serial.println("End of audio");

  audioPlaying = false;
  currentSource = AUDIO_NONE;
}


String getSelectedFilePlayerFile()
{
  char buf[128];
  lv_roller_get_selected_str(
    uic_RollerOptionRadio,   // File Player Roller
    buf,
    sizeof(buf)
  );

  String file = String(buf);
  file.trim();

  if (file.length() == 0 || file == "Keine Musik") return "";
  return file;
}

int getSelectedWebRadioIndex()
{
  return lv_roller_get_selected(uic_RollerOptionWebRadio);
}

void playWebRadio()
{
  int index = getSelectedWebRadioIndex();
  if (index < 0 || index >= webRadioCount) return;

  // Toggle Stop
  if (audioPlaying && currentSource == AUDIO_WEB) {
    audio.stopSong();
    audioPlaying = false;
    currentSource = AUDIO_NONE;
    Serial.println("Webradio stopped");
    return;
  }

  // Falls File Player läuft → stoppen
  if (audioPlaying && currentSource == AUDIO_FILE) {
    audio.stopSong();
  }

  Serial.println("Play Webradio: " + String(webRadios[index].name));
  Serial.println("URL: " + String(webRadios[index].url));

  audio.connecttohost(webRadios[index].url.c_str());

  audioPlaying = true;
  currentSource = AUDIO_WEB;
}



extern "C" void PlayRadio_data(lv_event_t * e)
{
  uint32_t tab = lv_tabview_get_tab_act(uic_TabViewMusicSelector);

  if (tab == 0) playFilePlayer();
  else if (tab == 1) playWebRadio();
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
  updateStopwatchUI(true);

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

  updateStopwatchUI(false);

  // Audio
  audio.loop();

  delay(5);
}
