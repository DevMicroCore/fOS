#include <lvgl.h>
#include "LGFX_CrowPanel.h"
#include "ui.h"
#include "esp_system.h"
#include "Arduino.h"
#include <SD.h>
#include <SPI.h>
#include "Audio.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "ui_Email.h"
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_recovery_manager.h"
#include "SecureCredentials.h"

void setup();
void loop();
void audio_eof_mp3(const char *info);

#define WIFI_DIR  "/system/wifi"
#define WIFI_FILE "/system/wifi/wlans.txt"
#define WIFI_FILE_TEMP "/system/wifi/wlans.tmp"
#define WIFI_FILE_BACKUP "/system/wifi/wlans.bak"
#define WIFI_ENABLED_FILE "/system/wifi/enabled.txt"
#define TIMEZONE_DIR  "/system/timezone"
#define TIMEZONE_FILE "/system/timezone/timezone.txt"
#define DISPLAY_DIR "/system/display"
#define DISPLAY_FILE "/system/display/brightness.txt"
#define UPDATE_DIR "/system/update"
#define TEXT_DIR "/text"
#define MUSIC_FILES_DIR   "/music/files"
#define WEBRADIO_DIR "/music/webradio"
#define WEBRADIO_FILE "/music/webradio/webradio.txt"
#define APPS_DIR "/apps"
#define EMAIL_DIR "/email"
#define EMAIL_INBOX_DIR "/email/inbox"
#define EMAIL_OUTGOING_DIR "/email/outgoing"
#define EMAIL_SYSTEM_DIR "/system/email"
#define EMAIL_LOGIN_FILE "/system/email/login.txt"
#define EMAIL_LOGIN_TEMP "/system/email/login.tmp"
#define EMAIL_LOGIN_BACKUP "/system/email/login.bak"

#define MAX_LAUNCHER_APPS 7
#define MAX_EMAIL_MESSAGES 30
#define EMAIL_MAX_BODY_CHARS 2200
#define EMAIL_MAX_RAW_CHARS 24000
#define EMAIL_MAX_FETCH_PER_RUN 10


#define MAX_WIFI_PROFILES 5
#define MAX_SCANNED_NETWORKS 20

#define timerSound "/system/timer.mp3"


extern "C" void deleteSelectedFile(void);
extern "C" void fillFileRoller_TextViewer_Data(void);

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
static String gStorageManagerNewFolderDraft = "";
static lv_obj_t * gCalcDisplay = NULL;
static String gCalcExpression = "";
static bool gCalcShowingError = false;
static lv_obj_t * gRadioTabView = NULL;
static lv_obj_t * gRadioFileRoller = NULL;
static lv_obj_t * gRadioWebRoller = NULL;
static lv_obj_t * gRadioToggleButton = NULL;
static lv_obj_t * gRadioToggleLabel = NULL;
static lv_obj_t * gRadioVolumeSlider = NULL;
static lv_obj_t * gRadioProgressSlider = NULL;
static unsigned long gRadioProgressLastUpdateMs = 0;
static String gRadioFileCurrentDir = String(MUSIC_FILES_DIR);
static String gRadioCurrentFile = "";
static String gRadioCurrentWebUrl = "";
static lv_obj_t * gSdTextArea = NULL;
static lv_obj_t * gSdKeyboard = NULL;
static lv_obj_t * gSdPopupSave = NULL;
static lv_obj_t * gSdPopupList = NULL;
static lv_obj_t * gSdFileNameInput = NULL;
static lv_obj_t * gSdFileRoller = NULL;
static String gSdTextBrowserCurrentDir = TEXT_DIR;
static bool gSdOpenPopupShouldClose = false;
static lv_obj_t * gClockTabView = NULL;
static lv_obj_t * gClockCurrentPanel = NULL;
static lv_obj_t * gClockStopwatchPanel = NULL;
static lv_obj_t * gClockTimerPanel = NULL;
static lv_obj_t * gClockCurrentTimeLabel = NULL;
static lv_obj_t * gClockCalendar = NULL;
static lv_obj_t * gClockStopwatchTimeLabel = NULL;
static lv_obj_t * gClockStopwatchToggleButton = NULL;
static lv_obj_t * gClockStopwatchToggleLabel = NULL;
static lv_obj_t * gClockTimerTimeLabel = NULL;
static lv_obj_t * gClockTimerHoursRoller = NULL;
static lv_obj_t * gClockTimerMinutesRoller = NULL;
static lv_obj_t * gClockTimerSecondsRoller = NULL;
static lv_obj_t * gClockTimerToggleButton = NULL;
static lv_obj_t * gClockTimerToggleLabel = NULL;
static lv_obj_t * gClockTimerResetButton = NULL;
static uint8_t gClockActiveTab = 0;
static bool gClockAppVisible = false;
static bool gClockStopwatchRunning = false;
static uint64_t gClockStopwatchElapsedMs = 0;
static unsigned long gClockStopwatchLastMs = 0;
static bool gClockTimerRunning = false;
static bool gClockTimerHasBeenStarted = false;
static uint64_t gClockTimerSelectedMs = 0;
static uint64_t gClockTimerRemainingMs = 0;
static unsigned long gClockTimerLastMs = 0;
static int gClockCalendarYear = 0;
static int gClockCalendarMonth = 0;
static time_t gClockLastTimeLabelTs = 0;
static bool gClockCalendarSyncedWithRealTime = false;
static lv_obj_t * uic_LabelWeatherTemperature = NULL;
static lv_obj_t * uic_LabelWeatherInformation = NULL;
static lv_obj_t * uic_RollerWeatherData = NULL;
static lv_obj_t * uic_WeatherSearchPanel = NULL;
static lv_obj_t * uic_WeatherSearchTextArea = NULL;
static lv_obj_t * uic_WeatherSearchDropdown = NULL;
static lv_obj_t * uic_WeatherSearchKeyboard = NULL;
static lv_obj_t * uic_WeatherSearchConfirmButton = NULL;
static lv_obj_t * uic_WeatherSearchOpenButton = NULL;
static bool gWeatherAppVisible = false;
static unsigned long gWeatherLastFetchMs = 0;
static bool gWeatherFetchRunning = false;
static bool gWeatherUseCurrentLocation = true;
static String gWeatherSelectedLatitude = "";
static String gWeatherSelectedLongitude = "";
static String gWeatherSelectedLocationLabel = "Current Location";
static const int kMaxWeatherSearchResults = 6;
static String gWeatherSearchResultLabel[kMaxWeatherSearchResults + 1];
static String gWeatherSearchResultLatitude[kMaxWeatherSearchResults + 1];
static String gWeatherSearchResultLongitude[kMaxWeatherSearchResults + 1];
static int gWeatherSearchResultCount = 0;
static const unsigned long kWeatherRefreshIntervalMs = 15UL * 60UL * 1000UL;
static int gCurrentAppIndex = -1;

enum RadioSourceType {
  RADIO_SOURCE_NONE,
  RADIO_SOURCE_FILE,
  RADIO_SOURCE_WEB
};

struct WebRadioEntry {
  String name;
  String url;
};

struct EmailLoginConfig {
  String protocol;
  String email;
  String user;
  String password;
  String pop3Server;
  String imapServer;
  String smtpServer;
  String fromEmail;
  String replyTo;
  String senderName;
  String imapFolder;
  uint16_t pop3Port;
  uint16_t imapPort;
  uint16_t smtpPort;
  bool pop3Ssl;
  bool imapSsl;
  bool smtpSsl;
  bool smtpStartTls;
  bool allowTechnicalSender;
};

struct EmailMessageEntry {
  String uid;
  String filePath;
  String from;
  String to;
  String subject;
  String date;
  String status;
  String body;
};

static RadioSourceType gRadioSource = RADIO_SOURCE_NONE;
static bool gRadioPlaying = false;
static const int MAX_WEBRADIO_STATIONS = 40;
static WebRadioEntry gWebRadioStations[MAX_WEBRADIO_STATIONS];
static int gWebRadioCount = 0;
static EmailMessageEntry gEmailMessages[MAX_EMAIL_MESSAGES];
static int gEmailMessageCount = 0;
static EmailMessageEntry gOutgoingEmailMessages[MAX_EMAIL_MESSAGES];
static int gOutgoingEmailMessageCount = 0;
static bool gEmailFetchRunning = false;
static bool gEmailUiVisible = false;
static unsigned long gEmailLastSendEventMs = 0;

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

struct ScannedNetwork {
  String ssid;
  int rssi;
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
static const int kMinBrightnessPercent = 5;
static const int kDefaultBrightnessPercent = 100;
static int gBrightnessPercent = kDefaultBrightnessPercent;
static bool gWifiEnabled = false;
static ScannedNetwork gScannedNetworks[MAX_SCANNED_NETWORKS];
static int gScannedNetworkCount = 0;
static const gpio_num_t kSleepButtonGpio = GPIO_NUM_38;
static const bool kUseRealMcuSleep = false; // GPIO38 is unstable as sleep/wake pin on this board.
static const unsigned long kSleepButtonDebounceMs = 80;
static const unsigned long kSleepButtonReleaseTimeoutMs = 1500;
static const unsigned long kSleepButtonLongPressMs = 1500;
static const unsigned long kSleepButtonLongPressReleaseTimeoutMs = 10000;
static bool gDisplaySuspended = false;
static bool gSleepAfterBusy = false;
static int gSleepButtonLastReading = HIGH;
static int gSleepButtonStableState = HIGH;
static int gSleepButtonPressLevel = LOW;
static int gSleepButtonReleaseLevel = HIGH;
static unsigned long gSleepButtonLastChangeMs = 0;

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
void updateSDUIData(void);
static void parseAppConfigFile(const String& folderPath, LauncherAppEntry * app);
static void loadAppsFromSdCard();
static void drawLauncherApps();
static void showAppContentForIndex(int appIndex);
static void refreshVisibleSdAppForThemeChange();
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
static String normalizeRadioFileBrowserPath(const String& path);
static String getRadioFileBrowserParentPath(const String& path);
static void fillRadioFileRoller();
static void updateRadioProgressSlider();
static void radioVolumeSliderEvent(lv_event_t * e);
static void radioProgressSliderEvent(lv_event_t * e);
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
static void resetSdAppState();
static String normalizeTextBrowserPath(const String& path);
static String getTextBrowserParentPath(const String& path, const String& root);
static String joinTextBrowserPath(const String& base, const String& name);
static void fillTextRollerFor(lv_obj_t * roller);
static bool loadSelectedTextFileFor(lv_obj_t * roller, lv_obj_t * textArea);
static void saveTextFileFor(lv_obj_t * textArea, lv_obj_t * fileNameInput, lv_obj_t * roller);
static void openNewTextFileFor(lv_obj_t * textArea);
static void sdAppOpenNewFileEvent(lv_event_t * e);
static void sdAppShowSavePopupEvent(lv_event_t * e);
static void sdAppShowOpenPopupEvent(lv_event_t * e);
static void sdAppTextareaClickedEvent(lv_event_t * e);
static void sdAppFileNameClickedEvent(lv_event_t * e);
static void sdAppSaveCancelEvent(lv_event_t * e);
static void sdAppSaveConfirmEvent(lv_event_t * e);
static void sdAppOpenCancelEvent(lv_event_t * e);
static void sdAppOpenConfirmEvent(lv_event_t * e);
static void renderSdTextApp();
static void resetClockDashboardState();
static bool isLeapYearValue(int year);
static int getDaysInMonthValue(int year, int month);
static int getFirstWeekdayOfMonth(int year, int month);
static void formatHmsFromSeconds(uint32_t totalSeconds, char * out, size_t outLen);
static void refreshClockDashboardTabs();
static void updateClockDashboardCurrentTimeLabel();
static void refreshClockDashboardCalendar();
static void changeClockDashboardMonth(int delta);
static void showClockDashboardTab(uint8_t tabIndex);
static void clockDashboardTabCurrentEvent(lv_event_t * e);
static void clockDashboardTabStopwatchEvent(lv_event_t * e);
static void clockDashboardTabTimerEvent(lv_event_t * e);
static void clockDashboardMonthPrevEvent(lv_event_t * e);
static void clockDashboardMonthNextEvent(lv_event_t * e);
static void clockDashboardStopwatchToggleEvent(lv_event_t * e);
static void clockDashboardStopwatchResetEvent(lv_event_t * e);
static void updateClockDashboardStopwatchLabel();
static void updateClockDashboardStopwatchToggleLabel();
static String buildClockTimerRollerOptions(int maxValue);
static uint64_t readClockTimerSelectionMs();
static void updateClockTimerTimeLabel();
static void updateClockTimerToggleLabel();
static void syncClockTimerSelectionToUi();
static void clockDashboardTimerSelectionChangedEvent(lv_event_t * e);
static void clockDashboardTimerToggleEvent(lv_event_t * e);
static void clockDashboardTimerResetEvent(lv_event_t * e);
static void updateClockDashboardTick();
static void renderClockDashboardApp();
static void resetWeatherAppState();
static bool extractJsonStringField(const String& json, const char * key, String * out);
static bool extractJsonNumberField(const String& json, const char * key, String * out);
static bool extractJsonArrayRaw(const String& json, const char * key, String * out);
static String weatherCodeToText(int code);
static String urlEncode(const String& value);
static bool fetchWeatherData(String * temperatureHumidity, String * information, String * rollerOptions);
static bool performWeatherGeocodingSearch(const String& query);
static void updateWeatherSearchDropdownOptions();
static void showWeatherSearchPanel();
static void hideWeatherSearchPanel();
static void weatherSearchButtonEvent(lv_event_t * e);
static void weatherSearchKeyboardEvent(lv_event_t * e);
static void weatherSearchConfirmEvent(lv_event_t * e);
static void applyWeatherUiData(const String& temperatureHumidity, const String& information, const String& rollerOptions);
static void refreshWeatherDataIfNeeded(bool force);
static void renderWeatherApp();
static void resetEmailAppState();
static bool ensureEmailDirectories();
static void ensureEmailSdApp();
static bool loadEmailLoginConfig(EmailLoginConfig * cfg);
static void loadCachedEmails();
static void loadCachedOutgoingEmails();
static void fillEmailRollers();
static bool fetchNewPop3Emails();
static bool fetchNewImapEmails();
static bool fetchNewEmails();
static bool sendSmtpEmail(const String& recipient, const String& subject, const String& body, String * status);
static String emailFormatRfc2822Date();
static void emailSelectEvent(lv_event_t * e);
static void emailTextAreaEvent(lv_event_t * e);
static void emailSendButtonEvent(lv_event_t * e);
static void renderEmailApp();
extern "C" void SendEmail(lv_event_t * e);
static String getStorageManagerParentPath(const String& path);
static String joinStorageManagerPath(const String& base, const String& name);
static bool isProtectedStoragePath(const String& path);
static bool removeStoragePathRecursive(const String& path);
static int normalizeBrightnessPercent(int value);
static uint8_t brightnessPercentToLevel(int percent);
static void applyDisplayBrightnessPercent(int percent, bool updateSlider);
static bool saveCurrentBrightness();
static void loadSavedBrightness();
static bool saveDisplayThemeIndex(uint8_t themeIdx);
static int wifiRssiToQualityPercent(int rssi);
static bool saveWifiEnabledState(bool enabled);
static void loadWifiEnabledState();
static void applyWifiBootUiState();
static void updateWifiSelectorDropdown();
static void setupSleepButton();
static bool hasUninterruptibleProcess();
static void setDisplayPower(bool enabled);
static gpio_int_type_t getSleepButtonWakeInterrupt();
static bool waitForSleepButtonRelease(unsigned long timeoutMs);
static bool waitForSleepButtonLongPress(unsigned long holdMs);
static void enterButtonSleep(bool forceMcuSleep);
static void handleSleepButton();

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

static int normalizeBrightnessPercent(int value)
{
  if (value < kMinBrightnessPercent) {
    return kMinBrightnessPercent;
  }
  if (value > 100) {
    return 100;
  }
  return value;
}

static uint8_t brightnessPercentToLevel(int percent)
{
  const int normalized = normalizeBrightnessPercent(percent);
  return static_cast<uint8_t>((normalized * 255) / 100);
}

static void applyDisplayBrightnessPercent(int percent, bool updateSlider)
{
  gBrightnessPercent = normalizeBrightnessPercent(percent);
  const uint8_t level = brightnessPercentToLevel(gBrightnessPercent);
  gfx.setBrightness(level);

  if (updateSlider && uic_SliderBrightness) {
    lv_slider_set_value(uic_SliderBrightness, gBrightnessPercent, LV_ANIM_OFF);
  }
}

static bool saveCurrentBrightness()
{
  if (!sd_ok) return false;

  if (!SD.exists(DISPLAY_DIR)) {
    SD.mkdir(DISPLAY_DIR);
  }

  if (SD.exists(DISPLAY_FILE)) {
    SD.remove(DISPLAY_FILE);
  }

  File f = SD.open(DISPLAY_FILE, FILE_WRITE);
  if (!f) return false;

  f.println(gBrightnessPercent);
  f.close();
  return true;
}

static void loadSavedBrightness()
{
  int loadedPercent = kDefaultBrightnessPercent;

  if (uic_SliderBrightness) {
    lv_slider_set_range(uic_SliderBrightness, kMinBrightnessPercent, 100);
  }

  if (sd_ok && SD.exists(DISPLAY_FILE)) {
    File f = SD.open(DISPLAY_FILE, FILE_READ);
    if (f) {
      String line = f.readStringUntil('\n');
      line.trim();
      f.close();

      if (line.length() > 0) {
        loadedPercent = line.toInt();
      }
    }
  }

  applyDisplayBrightnessPercent(loadedPercent, true);
}

static uint8_t normalizeDisplayThemeIndex(int themeIdx)
{
  if (themeIdx < UI_THEME_DEFAULT || themeIdx > UI_THEME_GREENTHEME) {
    return UI_THEME_DEFAULT;
  }
  return (uint8_t)themeIdx;
}

static bool saveDisplayThemeIndex(uint8_t themeIdx)
{
  if (!sd_ok) return false;

  themeIdx = normalizeDisplayThemeIndex(themeIdx);

  if (!SD.exists("/system")) {
    SD.mkdir("/system");
  }
  if (!SD.exists(DISPLAY_DIR)) {
    SD.mkdir(DISPLAY_DIR);
  }
  if (SD.exists("/system/display/theme.txt")) {
    SD.remove("/system/display/theme.txt");
  }

  File themeFile = SD.open("/system/display/theme.txt", FILE_WRITE);
  if (!themeFile) return false;
  themeFile.print(themeIdx);
  themeFile.close();
  return true;
}

void loadAndApplyDisplayTheme() {
  uint8_t loaded_theme = UI_THEME_DEFAULT; // Standardwert (Theme 0), falls Datei nicht existiert

  // 1. Theme von SD-Karte auslesen
  if (SD.exists("/system/display/theme.txt")) {
    File themeFile = SD.open("/system/display/theme.txt", FILE_READ);
    if (themeFile) {
      String content = themeFile.readStringUntil('\n');
      content.trim();
      loaded_theme = normalizeDisplayThemeIndex(content.toInt());
      themeFile.close();
      Serial.printf("Loaded theme index from SD: %u\n", loaded_theme);
    }
  } else {
    Serial.println("No theme file found, using default (0)");
  }

  // 2. Das Theme in der Benutzeroberfläche aktivieren
  ui_theme_set(loaded_theme);
  refreshVisibleSdAppForThemeChange();

  // 3. Das Dropdown-Widget visuell auf das geladene Theme einstellen
  if (uic_DropdownTheme != NULL) {
    // Setzt das Dropdown auf den geladenen Index
    lv_dropdown_set_selected(uic_DropdownTheme, loaded_theme); 
    
    // Erzwingt, dass LVGL das Dropdown sofort neu zeichnet, um den Text zu aktualisieren
    lv_obj_invalidate(uic_DropdownTheme); 
    Serial.printf("Dropdown menu set to index: %u\n", loaded_theme);
  } else {
    Serial.println("Warning: uic_DropdownTheme not ready during boot setup!");
  }
}


static int wifiRssiToQualityPercent(int rssi)
{
  if (rssi >= -50) return 100;
  if (rssi <= -90) return 0;
  return ((rssi + 90) * 100) / 40;
}

static void recoverCredentialFileIfNeeded(const char * path, const char * backupPath)
{
  if (!sd_ok || path == NULL || backupPath == NULL) return;
  if (!SD.exists(path) && SD.exists(backupPath)) {
    SD.rename(backupPath, path);
  } else if (SD.exists(path) && SD.exists(backupPath)) {
    SD.remove(backupPath);
  }
}

static bool installMigratedCredentialFile(const char * path, const char * tempPath, const char * backupPath)
{
  if (path == NULL || tempPath == NULL || backupPath == NULL || !SD.exists(tempPath)) return false;

  if (SD.exists(backupPath)) SD.remove(backupPath);
  if (SD.exists(path) && !SD.rename(path, backupPath)) return false;

  if (SD.rename(tempPath, path)) {
    if (SD.exists(backupPath)) SD.remove(backupPath);
    return true;
  }

  if (SD.exists(backupPath)) SD.rename(backupPath, path);
  return false;
}

static bool decodeStoredCredential(const String& storedValue, String * plainText)
{
  if (plainText == NULL) return false;
  if (SecureCredentials::isEncrypted(storedValue)) {
    return SecureCredentials::decrypt(storedValue, plainText);
  }
  *plainText = storedValue;
  return true;
}

static bool saveWifiEnabledState(bool enabled)
{
  if (!sd_ok) return false;

  if (!SD.exists(WIFI_DIR)) {
    SD.mkdir(WIFI_DIR);
  }

  if (SD.exists(WIFI_ENABLED_FILE)) {
    SD.remove(WIFI_ENABLED_FILE);
  }

  File f = SD.open(WIFI_ENABLED_FILE, FILE_WRITE);
  if (!f) return false;

  f.println(enabled ? "1" : "0");
  f.close();
  return true;
}

static void loadWifiEnabledState()
{
  gWifiEnabled = false;

  if (sd_ok && SD.exists(WIFI_ENABLED_FILE)) {
    File f = SD.open(WIFI_ENABLED_FILE, FILE_READ);
    if (f) {
      String line = f.readStringUntil('\n');
      line.trim();
      f.close();
      gWifiEnabled = (line == "1" || line.equalsIgnoreCase("true"));
    }
  }
}

static void applyWifiBootUiState()
{
  if (!uic_WifiSwitch) return;

  if (gWifiEnabled) {
    lv_obj_add_state(uic_WifiSwitch, LV_STATE_CHECKED);
    if (uic_WifiSelectorDropdown) {
      lv_obj_clear_flag(uic_WifiSelectorDropdown, LV_OBJ_FLAG_HIDDEN);
    }
    if (uic_TextAreaWifiPassword) {
      lv_obj_clear_flag(uic_TextAreaWifiPassword, LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    lv_obj_clear_state(uic_WifiSwitch, LV_STATE_CHECKED);
    if (uic_WifiSelectorDropdown) {
      lv_obj_add_flag(uic_WifiSelectorDropdown, LV_OBJ_FLAG_HIDDEN);
    }
    if (uic_TextAreaWifiPassword) {
      lv_obj_add_flag(uic_TextAreaWifiPassword, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void updateWifiSelectorDropdown()
{
  if (!uic_WifiSelectorDropdown || !gWifiEnabled) return;

  gScannedNetworkCount = 0;
  int networkCount = WiFi.scanNetworks();

  for (int i = 0; i < networkCount; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;

    int rssi = WiFi.RSSI(i);
    int existing = -1;

    for (int j = 0; j < gScannedNetworkCount; j++) {
      if (gScannedNetworks[j].ssid == ssid) {
        existing = j;
        break;
      }
    }

    if (existing >= 0) {
      if (rssi > gScannedNetworks[existing].rssi) {
        gScannedNetworks[existing].rssi = rssi;
      }
      continue;
    }

    if (gScannedNetworkCount >= MAX_SCANNED_NETWORKS) continue;

    gScannedNetworks[gScannedNetworkCount].ssid = ssid;
    gScannedNetworks[gScannedNetworkCount].rssi = rssi;
    gScannedNetworkCount++;
  }

  for (int i = 0; i < gScannedNetworkCount - 1; i++) {
    for (int j = i + 1; j < gScannedNetworkCount; j++) {
      if (gScannedNetworks[j].rssi > gScannedNetworks[i].rssi) {
        ScannedNetwork tmp = gScannedNetworks[i];
        gScannedNetworks[i] = gScannedNetworks[j];
        gScannedNetworks[j] = tmp;
      }
    }
  }

  if (gScannedNetworkCount == 0) {
    lv_dropdown_set_options(uic_WifiSelectorDropdown, "Kein WLAN gefunden");
    lv_dropdown_set_selected(uic_WifiSelectorDropdown, 0);
    return;
  }

  String options;
  for (int i = 0; i < gScannedNetworkCount; i++) {
    if (i > 0) options += "\n";
    options += gScannedNetworks[i].ssid;
    options += " (";
    options += String(wifiRssiToQualityPercent(gScannedNetworks[i].rssi));
    options += "%)";
  }

  lv_dropdown_set_options(uic_WifiSelectorDropdown, options.c_str());
  lv_dropdown_set_selected(uic_WifiSelectorDropdown, 0);
}

static void setupSleepButton()
{
  pinMode(kSleepButtonGpio, INPUT_PULLUP);
  gpio_set_direction(kSleepButtonGpio, GPIO_MODE_INPUT);
  gpio_set_pull_mode(kSleepButtonGpio, GPIO_PULLUP_ONLY);
  gpio_pulldown_dis(kSleepButtonGpio);
  gpio_pullup_en(kSleepButtonGpio);

  delay(10);
  gSleepButtonLastReading = digitalRead(kSleepButtonGpio);
  gSleepButtonStableState = gSleepButtonLastReading;
  gSleepButtonReleaseLevel = gSleepButtonStableState;
  gSleepButtonPressLevel = gSleepButtonReleaseLevel == HIGH ? LOW : HIGH;
  gSleepButtonLastChangeMs = millis();

  Serial.printf(
    "Sleep button GPIO%d ready: idle=%s, first edge learns pressed level\n",
    static_cast<int>(kSleepButtonGpio),
    gSleepButtonStableState == HIGH ? "HIGH" : "LOW"
  );
}

static bool hasUninterruptibleProcess()
{
  return OTARecovery_IsBusy() || gRadioPlaying;
}

static gpio_int_type_t getSleepButtonWakeInterrupt()
{
  return gSleepButtonPressLevel == HIGH ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
}

static void setDisplayPower(bool enabled)
{
  if (enabled) {
    if (!gDisplaySuspended) return;

    gDisplaySuspended = false;
    gfx.wakeup();
    applyDisplayBrightnessPercent(gBrightnessPercent, true);
    lv_obj_invalidate(lv_scr_act());
    lv_timer_handler();
    Serial.println("Display eingeschaltet");
    return;
  }

  if (gDisplaySuspended) return;

  lv_timer_handler();
  gfx.sleep();
  gfx.setBrightness(0);
  gDisplaySuspended = true;
  Serial.println("Display ausgeschaltet");
}

static bool waitForSleepButtonRelease(unsigned long timeoutMs)
{
  const unsigned long startMs = millis();
  while (digitalRead(kSleepButtonGpio) == gSleepButtonPressLevel) {
    if (millis() - startMs >= timeoutMs) {
      return false;
    }
    delay(10);
  }

  delay(kSleepButtonDebounceMs);
  gSleepButtonLastReading = digitalRead(kSleepButtonGpio);
  gSleepButtonStableState = gSleepButtonLastReading;
  gSleepButtonReleaseLevel = gSleepButtonStableState;
  gSleepButtonLastChangeMs = millis();
  return true;
}

static bool waitForSleepButtonLongPress(unsigned long holdMs)
{
  const unsigned long startMs = millis();

  while (digitalRead(kSleepButtonGpio) == gSleepButtonPressLevel) {
    if (millis() - startMs >= holdMs) {
      Serial.println("Sleep button long press erkannt");
      return true;
    }
    delay(10);
  }

  delay(kSleepButtonDebounceMs);
  gSleepButtonLastReading = digitalRead(kSleepButtonGpio);
  gSleepButtonStableState = gSleepButtonLastReading;
  gSleepButtonReleaseLevel = gSleepButtonStableState;
  gSleepButtonLastChangeMs = millis();
  return false;
}

static void enterButtonSleep(bool forceMcuSleep)
{
  if (!gDisplaySuspended) {
    setDisplayPower(false);
  }

  const unsigned long releaseTimeout = forceMcuSleep
    ? kSleepButtonLongPressReleaseTimeoutMs
    : kSleepButtonReleaseTimeoutMs;

  if (!waitForSleepButtonRelease(releaseTimeout)) {
    Serial.println("Sleep abgebrochen: Button wurde nicht losgelassen");
    return;
  }

  if (!forceMcuSleep && !kUseRealMcuSleep) {
    Serial.println("MCU-Sleep deaktiviert: GPIO38 bleibt im sicheren Display-Off-Modus");
    return;
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  const esp_err_t gpioWakeResult = gpio_wakeup_enable(kSleepButtonGpio, getSleepButtonWakeInterrupt());
  const esp_err_t sleepWakeResult = esp_sleep_enable_gpio_wakeup();

  if (gpioWakeResult == ESP_OK && sleepWakeResult == ESP_OK) {
    Serial.println("Gehe in Light Sleep per Long Press");
    Serial.flush();
    esp_light_sleep_start();
  } else {
    Serial.printf(
      "Light-Sleep-Wakeup fehlgeschlagen: gpio=%d sleep=%d\n",
      static_cast<int>(gpioWakeResult),
      static_cast<int>(sleepWakeResult)
    );
  }

  gpio_wakeup_disable(kSleepButtonGpio);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  last_tick = millis();
  setDisplayPower(true);
  waitForSleepButtonRelease(kSleepButtonReleaseTimeoutMs);
}

static void handleSleepButton()
{
  const int reading = digitalRead(kSleepButtonGpio);

  if (reading != gSleepButtonLastReading) {
    gSleepButtonLastReading = reading;
    gSleepButtonLastChangeMs = millis();
    Serial.printf(
      "Sleep button GPIO%d changed: %s\n",
      static_cast<int>(kSleepButtonGpio),
      reading == HIGH ? "HIGH" : "LOW"
    );
  }

  if (millis() - gSleepButtonLastChangeMs < kSleepButtonDebounceMs) {
    return;
  }

  if (reading == gSleepButtonStableState) {
    return;
  }

  const int previousStableState = gSleepButtonStableState;
  gSleepButtonStableState = reading;

  if (gDisplaySuspended) {
    if (gSleepButtonStableState != gSleepButtonPressLevel) {
      return;
    }

    if (waitForSleepButtonLongPress(kSleepButtonLongPressMs)) {
      if (hasUninterruptibleProcess()) {
        Serial.println("Long-Press Light Sleep blockiert: Update oder Musik laeuft");
        waitForSleepButtonRelease(kSleepButtonLongPressReleaseTimeoutMs);
        return;
      }

      gSleepAfterBusy = false;
      enterButtonSleep(true);
      return;
    }

    gSleepAfterBusy = false;
    setDisplayPower(true);
    return;
  }

  gSleepButtonPressLevel = gSleepButtonStableState;
  gSleepButtonReleaseLevel = previousStableState;
  Serial.printf(
    "Sleep button press learned: pressed=%s released=%s\n",
    gSleepButtonPressLevel == HIGH ? "HIGH" : "LOW",
    gSleepButtonReleaseLevel == HIGH ? "HIGH" : "LOW"
  );

  setDisplayPower(false);

  if (hasUninterruptibleProcess()) {
    gSleepAfterBusy = true;
    Serial.println("Sleep wartet: Update oder Musik laeuft");
    return;
  }

  if (waitForSleepButtonLongPress(kSleepButtonLongPressMs)) {
    gSleepAfterBusy = false;
    enterButtonSleep(true);
    return;
  }

  gSleepAfterBusy = false;
  enterButtonSleep(false);
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

extern "C" void SaveDisplaySettings_Data(lv_event_t * e)
{
  (void)e;
  
  if (!uic_SliderBrightness || !uic_DropdownTheme) {
    Serial.println("Display settings not saved: slider or dropdown unavailable");
    return;
  }

  // === 1. HELLIGKEIT VERARBEITEN (Dein Code) ===
  lv_slider_set_range(uic_SliderBrightness, kMinBrightnessPercent, 100);
  int sliderPercent = lv_slider_get_value(uic_SliderBrightness);
  sliderPercent = normalizeBrightnessPercent(sliderPercent);
  applyDisplayBrightnessPercent(sliderPercent, true);

  const uint8_t brightness = brightnessPercentToLevel(sliderPercent);

  if (saveCurrentBrightness()) {
    Serial.println("Display brightness saved to /system/display/brightness.txt");
  } else {
    Serial.println("Display brightness applied but not saved (SD not ready?)");
  }

  Serial.printf(
    "Display brightness saved: slider=%d%% -> brightness=%u\n",
    sliderPercent,
    static_cast<unsigned>(brightness)
  );

  // === 2. NEU: THEME AUS DROPDOWN AUSLESEN & ANWENDEN ===
  uint8_t selected_theme = normalizeDisplayThemeIndex(lv_dropdown_get_selected(uic_DropdownTheme));
  ui_theme_set(selected_theme);
  refreshVisibleSdAppForThemeChange();
  Serial.printf("Display theme applied: index=%u\n", selected_theme);

  // === 3. NEU: THEME AUF SD-KARTE SPEICHERN ===
  if (saveDisplayThemeIndex(selected_theme)) {
    Serial.println("Display theme saved to /system/display/theme.txt");
  } else {
    Serial.println("Display theme applied but not saved (SD not ready?)");
  }
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

/* ================= DISPLAY FLUSH ================= */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  if (gDisplaySuspended) {
    lv_disp_flush_ready(disp);
    return;
  }

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
    case 6: return ui_AppL7;
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

  value.trim();
  value.toLowerCase();

  if (value == "theme" || value == "accent" || value == "main" || value == "main_theme") {
    return (uint32_t)ui_get_theme_value(_ui_theme_color_MainTheme);
  }

  if (value == "surface" || value == "theme_surface" || value == "panel") {
    const uint32_t accent = (uint32_t)ui_get_theme_value(_ui_theme_color_MainTheme);
    const uint8_t ar = (accent >> 16) & 0xFF;
    const uint8_t ag = (accent >> 8) & 0xFF;
    const uint8_t ab = accent & 0xFF;
    const uint32_t white = 0xFFFFFF;
    const uint8_t wr = (white >> 16) & 0xFF;
    const uint8_t wg = (white >> 8) & 0xFF;
    const uint8_t wb = white & 0xFF;
    const uint32_t accentWeight = 230; // mostly white, with a light tint of the accent color
    const uint32_t whiteWeight = 255 - accentWeight;
    return ((ar * accentWeight + wr * whiteWeight) / 255u << 16)
         | ((ag * accentWeight + wg * whiteWeight) / 255u << 8)
         |  ((ab * accentWeight + wb * whiteWeight) / 255u);
  }

  if (value == "contrast" || value == "theme_contrast" || value == "text") {
    const uint32_t accent = (uint32_t)ui_get_theme_value(_ui_theme_color_MainTheme);
    const uint8_t r = (accent >> 16) & 0xFF;
    const uint8_t g = (accent >> 8) & 0xFF;
    const uint8_t b = accent & 0xFF;
    const uint16_t brightness = (uint16_t)((r * 299u + g * 587u + b * 114u) / 1000u);
    return (brightness > 160u) ? 0x000000u : 0xFFFFFFu;
  }

  if (value.startsWith("0x") || value.startsWith("0X")) {
    value = value.substring(2);
  }
  char buf[16];
  value.toCharArray(buf, sizeof(buf));
  char * endPtr = NULL;
  unsigned long parsed = strtoul(buf, &endPtr, 16);
  if (endPtr == buf) return defaultValue;
  return (uint32_t)parsed;
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

static uint32_t getThemeAccentColor()
{
  return (uint32_t)ui_get_theme_value(_ui_theme_color_MainTheme);
}

static uint32_t getThemeSurfaceColor()
{
  const uint32_t accent = getThemeAccentColor();
  const uint8_t ar = (accent >> 16) & 0xFF;
  const uint8_t ag = (accent >> 8) & 0xFF;
  const uint8_t ab = accent & 0xFF;
  const uint32_t white = 0xFFFFFF;
  const uint8_t wr = (white >> 16) & 0xFF;
  const uint8_t wg = (white >> 8) & 0xFF;
  const uint8_t wb = white & 0xFF;
  const uint32_t accentWeight = 230; // mostly white, with a light tint of the accent color
  const uint32_t whiteWeight = 255 - accentWeight;
  return ((ar * accentWeight + wr * whiteWeight) / 255u << 16)
       | ((ag * accentWeight + wg * whiteWeight) / 255u << 8)
       |  ((ab * accentWeight + wb * whiteWeight) / 255u);
}

static uint32_t getThemeContrastColor(uint32_t color)
{
  const uint8_t r = (color >> 16) & 0xFF;
  const uint8_t g = (color >> 8) & 0xFF;
  const uint8_t b = color & 0xFF;
  const uint16_t brightness = (uint16_t)((r * 299u + g * 587u + b * 114u) / 1000u);
  return (brightness > 160u) ? 0x000000u : 0xFFFFFFu;
}

static bool renderAppLayout(const LauncherAppEntry& app)
{
  String layoutPath = String(APPS_DIR) + "/" + app.folderName + "/" + app.layoutFile;
  String layoutText;
  if (!readTextFileLimited(layoutPath, &layoutText, 32000)) {
    return false;
  }

  lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_AppContentArea, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
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
      uint32_t defaultBg = 0x000000;
      if (type == "button") defaultBg = getThemeAccentColor();
      uint32_t bg = getUiFieldColor(line, "bg", defaultBg);
      uint32_t fgDefault = 0x000000;
      if (type == "button" || type == "panel" || type == "textarea" || type == "checkbox" || type == "switch") {
        fgDefault = getThemeContrastColor(bg);
      }
      uint32_t fg = getUiFieldColor(line, "fg", fgDefault);

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
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

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
        lv_obj_set_style_bg_color(ta, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ta, lv_color_hex(fg), LV_PART_MAIN | LV_STATE_DEFAULT);
        createdAny = true;
      } else if (type == "switch") {
        lv_obj_t * sw = lv_switch_create(uic_AppContentArea);
        lv_obj_set_pos(sw, x, y);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(sw, lv_color_hex(getThemeAccentColor()), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (getUiFieldBool(line, "value", false)) {
          lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        createdAny = true;
      } else if (type == "checkbox") {
        lv_obj_t * cb = lv_checkbox_create(uic_AppContentArea);
        lv_obj_set_pos(cb, x, y);
        lv_checkbox_set_text(cb, text.c_str());
        lv_obj_set_style_text_color(cb, lv_color_hex(fg), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(cb, lv_color_hex(getThemeAccentColor()), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(cb, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (getUiFieldBool(line, "value", false)) {
          lv_obj_add_state(cb, LV_STATE_CHECKED);
        }
        createdAny = true;
      } else if (type == "panel") {
        lv_obj_t * panel = lv_obj_create(uic_AppContentArea);
        lv_obj_set_size(panel, w, h);
        lv_obj_set_pos(panel, x, y);
        lv_obj_set_style_bg_color(panel, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
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
  lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_AppContentArea, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t * contentLabel = lv_label_create(uic_AppContentArea);
  lv_obj_set_width(contentLabel, lv_pct(100));
  lv_label_set_long_mode(contentLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(contentLabel, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_text_font(contentLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(contentLabel, lv_color_hex(getThemeContrastColor(0x000000)), LV_PART_MAIN | LV_STATE_DEFAULT);

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
  ui_object_set_themeable_style_property(btn, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(btn, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);

  lv_obj_t * label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(label);
}

static void renderCalculatorApp()
{
  resetCalculatorState();

  gCalcDisplay = lv_textarea_create(uic_AppContentArea);
  lv_obj_set_size(gCalcDisplay, 680, 64);
  lv_obj_set_pos(gCalcDisplay, 60, 70);
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
  gRadioVolumeSlider = NULL;
  gRadioProgressSlider = NULL;
  gRadioFileCurrentDir = String(MUSIC_FILES_DIR);
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

static String normalizeRadioFileBrowserPath(const String& path)
{
  String clean = path;
  clean.trim();
  while (clean.length() > 1 && clean.endsWith("/")) {
    clean.remove(clean.length() - 1);
  }
  if (clean.length() == 0) {
    return String(MUSIC_FILES_DIR);
  }
  return clean;
}

static String getRadioFileBrowserParentPath(const String& path)
{
  String clean = normalizeRadioFileBrowserPath(path);
  String root = normalizeRadioFileBrowserPath(String(MUSIC_FILES_DIR));
  if (clean == root) {
    return root;
  }

  int slash = clean.lastIndexOf('/');
  if (slash <= 0) {
    return root;
  }
  String parent = clean.substring(0, slash);
  if (parent.length() == 0) {
    return root;
  }
  return parent;
}

static void fillRadioFileRoller()
{
  if (gRadioFileRoller == NULL) return;
  if (!sd_ok) {
    lv_roller_set_options(gRadioFileRoller, "SD Fehler", LV_ROLLER_MODE_NORMAL);
    return;
  }

  gRadioFileCurrentDir = normalizeRadioFileBrowserPath(gRadioFileCurrentDir);
  File dir = SD.open(gRadioFileCurrentDir.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    lv_roller_set_options(gRadioFileRoller, "Keine Dateien", LV_ROLLER_MODE_NORMAL);
    return;
  }

  String options = "";
  if (gRadioFileCurrentDir != String(MUSIC_FILES_DIR)) {
    options += "..\n";
  }

  File entry = dir.openNextFile();
  while (entry) {
    String name = pathBasename(String(entry.name()));
    name.trim();
    if (name.length() > 0 && !name.startsWith("._")) {
      if (entry.isDirectory()) {
        options += name;
        options += "/\n";
      } else if (isMusicFileName(name)) {
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

static void updateRadioProgressSlider()
{
  if (gRadioProgressSlider == NULL) return;
  if (gRadioSource != RADIO_SOURCE_FILE || !gRadioPlaying) {
    lv_slider_set_range(gRadioProgressSlider, 0, 1);
    lv_slider_set_value(gRadioProgressSlider, 0, LV_ANIM_OFF);
    return;
  }

  unsigned long now = millis();
  if (lv_obj_has_state(gRadioProgressSlider, LV_STATE_PRESSED)) {
    gRadioProgressLastUpdateMs = now;
    return;
  }

  if (now - gRadioProgressLastUpdateMs < 200) {
    return;
  }
  gRadioProgressLastUpdateMs = now;

  int totalSeconds = audio.getAudioFileDuration();
  int currentSeconds = audio.getAudioCurrentTime();

  if (totalSeconds <= 0) {
    totalSeconds = currentSeconds + 1;
    if (totalSeconds < 1) {
      totalSeconds = 1;
    }
  }

  if (currentSeconds > totalSeconds) {
    currentSeconds = totalSeconds;
  }

  lv_slider_set_range(gRadioProgressSlider, 0, totalSeconds);
  lv_slider_set_value(gRadioProgressSlider, currentSeconds, LV_ANIM_OFF);
}

static void radioVolumeSliderEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  if (gRadioVolumeSlider == NULL) return;

  int value = lv_slider_get_value(gRadioVolumeSlider);
  audio.setVolume((unsigned char)value);
}

static void radioProgressSliderEvent(lv_event_t * e)
{
  if (gRadioProgressSlider == NULL) return;
  if (gRadioSource != RADIO_SOURCE_FILE || !gRadioPlaying) return;

  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    if (!lv_obj_has_state(gRadioProgressSlider, LV_STATE_PRESSED)) {
      return;
    }
  } else if (code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
    return;
  }

  int position = lv_slider_get_value(gRadioProgressSlider);
  audio.setAudioPlayPosition((unsigned short)position);
  gRadioProgressLastUpdateMs = millis();
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
  lv_label_set_text(gRadioToggleLabel, gRadioPlaying ? "||" : ">");
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

    if (file == "..") {
      gRadioFileCurrentDir = getRadioFileBrowserParentPath(gRadioFileCurrentDir);
      fillRadioFileRoller();
      return;
    }

    if (file.endsWith("/")) {
      String folderName = file;
      folderName.remove(folderName.length() - 1);
      gRadioFileCurrentDir = normalizeRadioFileBrowserPath(gRadioFileCurrentDir + "/" + folderName);
      fillRadioFileRoller();
      return;
    }

    String path = normalizeRadioFileBrowserPath(gRadioFileCurrentDir) + "/" + file;
    if (!SD.exists(path)) return;

    if (gRadioPlaying && gRadioSource == RADIO_SOURCE_FILE && gRadioCurrentFile == path) {
      stopRadioPlayback();
      updateRadioToggleButtonLabel();
      return;
    }

    stopRadioPlayback();
    audio.connecttoFS(SD, path.c_str());
    gRadioPlaying = true;
    gRadioSource = RADIO_SOURCE_FILE;
    gRadioCurrentFile = path;
    updateRadioToggleButtonLabel();
    updateRadioProgressSlider();
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

  lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x050C14), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_AppContentArea, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  gRadioToggleButton = lv_btn_create(uic_AppContentArea);
  lv_obj_set_size(gRadioToggleButton, 162, 50);
  lv_obj_set_pos(gRadioToggleButton, 320, 18);
  lv_obj_set_style_radius(gRadioToggleButton, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(gRadioToggleButton, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(gRadioToggleButton, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_border_width(gRadioToggleButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(gRadioToggleButton, radioToggleButtonEvent, LV_EVENT_CLICKED, NULL);

  gRadioToggleLabel = lv_label_create(gRadioToggleButton);
  lv_obj_set_style_text_font(gRadioToggleLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(gRadioToggleLabel, lv_color_hex(0x0A1A2B), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(gRadioToggleLabel);

  gRadioVolumeSlider = lv_slider_create(uic_AppContentArea);
  lv_obj_set_size(gRadioVolumeSlider, 754, 18);
  lv_obj_set_pos(gRadioVolumeSlider, 24, 90);
  lv_slider_set_range(gRadioVolumeSlider, 0, 21);
  lv_slider_set_value(gRadioVolumeSlider, audio.getVolume(), LV_ANIM_OFF);
  lv_obj_add_event_cb(gRadioVolumeSlider, radioVolumeSliderEvent, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_set_style_bg_color(gRadioVolumeSlider, lv_color_hex(0x1F2937), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(gRadioVolumeSlider, lv_color_hex((uint32_t)ui_get_theme_value(_ui_theme_color_MainTheme)), LV_PART_INDICATOR | LV_STATE_DEFAULT);

  gRadioProgressSlider = lv_slider_create(uic_AppContentArea);
  lv_obj_set_size(gRadioProgressSlider, 754, 18);
  lv_obj_set_pos(gRadioProgressSlider, 24, 122);
  lv_slider_set_range(gRadioProgressSlider, 0, 1);
  lv_slider_set_value(gRadioProgressSlider, 0, LV_ANIM_OFF);
  lv_obj_add_event_cb(gRadioProgressSlider, radioProgressSliderEvent, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(gRadioProgressSlider, radioProgressSliderEvent, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(gRadioProgressSlider, radioProgressSliderEvent, LV_EVENT_PRESS_LOST, NULL);
  lv_obj_set_style_bg_color(gRadioProgressSlider, lv_color_hex(0x1F2937), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(gRadioProgressSlider, lv_color_hex((uint32_t)ui_get_theme_value(_ui_theme_color_MainTheme)), LV_PART_INDICATOR | LV_STATE_DEFAULT);

  gRadioTabView = lv_tabview_create(uic_AppContentArea, LV_DIR_TOP, 50);
  lv_obj_set_size(gRadioTabView, 800, 318);
  lv_obj_set_pos(gRadioTabView, 0, 160);
  lv_obj_add_event_cb(gRadioTabView, radioTabChangedEvent, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t * tabFiles = lv_tabview_add_tab(gRadioTabView, "File Player");
  lv_obj_t * tabWeb = lv_tabview_add_tab(gRadioTabView, "Web Radio");

  lv_obj_set_style_bg_color(gRadioTabView, lv_color_hex(0x050C14), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(gRadioTabView, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t * tabBtns = lv_tabview_get_tab_btns(gRadioTabView);
  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(0x2A2A39), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(tabBtns, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(tabBtns, &lv_font_montserrat_24, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(tabBtns, lv_color_hex(0xF1F3F7), LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(0x2A2A39), LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(0x23495F), LV_PART_ITEMS | LV_STATE_CHECKED);
  ui_object_set_themeable_style_property(tabBtns, LV_PART_ITEMS | LV_STATE_CHECKED, LV_STYLE_TEXT_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(tabBtns, LV_PART_ITEMS | LV_STATE_CHECKED, LV_STYLE_TEXT_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_border_side(tabBtns, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(tabBtns, 4, LV_PART_ITEMS | LV_STATE_CHECKED);
  ui_object_set_themeable_style_property(tabBtns, LV_PART_ITEMS | LV_STATE_CHECKED, LV_STYLE_BORDER_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(tabBtns, LV_PART_ITEMS | LV_STATE_CHECKED, LV_STYLE_BORDER_OPA, _ui_theme_alpha_MainTheme);

  gRadioFileRoller = lv_roller_create(tabFiles);
  lv_obj_set_size(gRadioFileRoller, 754, 225);
  lv_obj_set_pos(gRadioFileRoller, 12, 10);
  lv_obj_set_style_radius(gRadioFileRoller, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(gRadioFileRoller, lv_color_hex(0x2A2A39), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gRadioFileRoller, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(gRadioFileRoller, lv_color_hex(0xE9EEF6), LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(gRadioFileRoller, LV_PART_SELECTED| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(gRadioFileRoller, LV_PART_SELECTED| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_text_color(gRadioFileRoller, lv_color_hex(0xF5F8FC), LV_PART_SELECTED | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gRadioFileRoller, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(gRadioFileRoller, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_scrollbar_mode(gRadioFileRoller, LV_SCROLLBAR_MODE_OFF);
  lv_obj_center(gRadioFileRoller);

  gRadioWebRoller = lv_roller_create(tabWeb);
  lv_obj_set_size(gRadioWebRoller, 754, 225);
  lv_obj_set_pos(gRadioWebRoller, 12, 10);
  lv_obj_set_style_radius(gRadioWebRoller, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(gRadioWebRoller, lv_color_hex(0x2A2A39), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gRadioWebRoller, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(gRadioWebRoller, lv_color_hex(0xE9EEF6), LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(gRadioWebRoller, LV_PART_SELECTED| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(gRadioWebRoller, LV_PART_SELECTED| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_text_color(gRadioWebRoller, lv_color_hex(0xF5F8FC), LV_PART_SELECTED | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gRadioWebRoller, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(gRadioWebRoller, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_scrollbar_mode(gRadioWebRoller, LV_SCROLLBAR_MODE_OFF);
  lv_obj_center(gRadioWebRoller);

  fillRadioFileRoller();
  fillWebRadioRoller();
  updateRadioToggleButtonLabel();
  updateRadioProgressSlider();
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
  updateRadioProgressSlider();
}

static void resetSdAppState()
{
  gSdTextArea = NULL;
  gSdKeyboard = NULL;
  gSdPopupSave = NULL;
  gSdPopupList = NULL;
  gSdFileNameInput = NULL;
  gSdFileRoller = NULL;
  gSdTextBrowserCurrentDir = TEXT_DIR;
  gSdOpenPopupShouldClose = false;
}

static void fillTextRollerFor(lv_obj_t * roller)
{
  if (roller == NULL) return;

  if (!sd_ok) {
    lv_roller_set_options(roller, "SD Karte nicht verfuegbar", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, 0, LV_ANIM_OFF);
    return;
  }

  gSdTextBrowserCurrentDir = normalizeTextBrowserPath(gSdTextBrowserCurrentDir);

  File root = SD.open(gSdTextBrowserCurrentDir.c_str());
  if (!root || !root.isDirectory()) {
    if (root) {
      root.close();
    }
    gSdTextBrowserCurrentDir = TEXT_DIR;
    root = SD.open(gSdTextBrowserCurrentDir.c_str());
  }

  if (!root || !root.isDirectory()) {
    if (root) {
      root.close();
    }
    lv_roller_set_options(roller, "Keine Dateien", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, 0, LV_ANIM_OFF);
    return;
  }

  String folders = "";
  String files = "";

  if (gSdTextBrowserCurrentDir != TEXT_DIR) {
    folders += "..\n";
  }

  File file = root.openNextFile();
  while (file) {
    String name = pathBasename(String(file.name()));
    name.trim();

    if (name.length() > 0) {
      if (file.isDirectory()) {
        folders += name;
        folders += "/\n";
      } else {
        files += name;
        files += "\n";
      }
    }

    file.close();
    file = root.openNextFile();
  }
  root.close();

  String rollerText = folders + files;
  if (rollerText.length() == 0) {
    rollerText = "Keine Dateien";
  } else if (rollerText.endsWith("\n")) {
    rollerText.remove(rollerText.length() - 1);
  }

  lv_roller_set_options(roller, rollerText.c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(roller, 0, LV_ANIM_OFF);
}

static String normalizeTextBrowserPath(const String& path)
{
  String clean = path;
  clean.trim();

  while (clean.length() > 1 && clean.endsWith("/")) {
    clean.remove(clean.length() - 1);
  }

  if (clean.length() == 0) {
    return String(TEXT_DIR);
  }

  return clean;
}

static String getTextBrowserParentPath(const String& path, const String& root)
{
  const String cleanRoot = normalizeTextBrowserPath(root);
  String clean = normalizeTextBrowserPath(path);

  if (clean == cleanRoot) {
    return cleanRoot;
  }

  if (clean != cleanRoot && !clean.startsWith(cleanRoot + "/")) {
    return cleanRoot;
  }

  int slash = clean.lastIndexOf('/');
  if (slash <= cleanRoot.length()) {
    return cleanRoot;
  }

  return clean.substring(0, slash);
}

static String joinTextBrowserPath(const String& base, const String& name)
{
  String cleanBase = normalizeTextBrowserPath(base);
  String cleanName = name;
  cleanName.trim();

  if (cleanBase.length() == 0) {
    cleanBase = TEXT_DIR;
  }

  return cleanBase + "/" + cleanName;
}

static String normalizeTextSavePath(String path)
{
  path.trim();
  path.replace("\\", "/");

  while (path.startsWith("/")) {
    path.remove(0, 1);
  }

  while (path.indexOf("//") >= 0) {
    path.replace("//", "/");
  }

  while (path.endsWith("/")) {
    path.remove(path.length() - 1);
  }

  return path;
}

static bool hasTraversalSegment(const String& path)
{
  int start = 0;
  while (start < path.length()) {
    int slash = path.indexOf('/', start);
    String segment = (slash >= 0) ? path.substring(start, slash) : path.substring(start);
    segment.trim();
    if (segment == "." || segment == "..") {
      return true;
    }
    if (slash < 0) {
      break;
    }
    start = slash + 1;
  }
  return false;
}

static bool ensureTextSaveDirectories(const String& relativePath)
{
  if (!sd_ok) return false;

  if (!SD.exists(TEXT_DIR)) {
    if (!SD.mkdir(TEXT_DIR)) {
      return false;
    }
  }

  int lastSlash = relativePath.lastIndexOf('/');
  if (lastSlash < 0) {
    return true;
  }

  String dirPart = relativePath.substring(0, lastSlash);
  if (dirPart.length() == 0) {
    return true;
  }

  String currentDir = "";
  int start = 0;
  while (start < dirPart.length()) {
    int slash = dirPart.indexOf('/', start);
    String segment = (slash >= 0) ? dirPart.substring(start, slash) : dirPart.substring(start);
    segment.trim();

    if (segment.length() == 0 || segment == "." || segment == "..") {
      return false;
    }

    if (currentDir.length() > 0) {
      currentDir += "/";
    }
    currentDir += segment;

    String fullDir = String(TEXT_DIR) + "/" + currentDir;
    if (!SD.exists(fullDir.c_str())) {
      if (!SD.mkdir(fullDir.c_str())) {
        return false;
      }
    } else {
      File dir = SD.open(fullDir.c_str());
      if (!dir || !dir.isDirectory()) {
        if (dir) {
          dir.close();
        }
        return false;
      }
      dir.close();
    }

    if (slash < 0) {
      break;
    }
    start = slash + 1;
  }

  return true;
}

static bool loadSelectedTextFileFor(lv_obj_t * roller, lv_obj_t * textArea)
{
  if (roller == NULL || textArea == NULL) return false;

  if (!sd_ok) {
    lv_textarea_set_text(textArea, "SD Karte nicht verfuegbar");
    return false;
  }

  char buf[128];
  lv_roller_get_selected_str(roller, buf, sizeof(buf));

  String filename = String(buf);
  if (filename.length() == 0 ||
      filename == "Keine Dateien" ||
      filename == "SD Karte nicht verfuegbar") {
    lv_textarea_set_text(textArea, "Keine Datei ausgewaehlt");
    return false;
  }

  if (filename == "..") {
    gSdTextBrowserCurrentDir = getTextBrowserParentPath(gSdTextBrowserCurrentDir, TEXT_DIR);
    fillTextRollerFor(roller);
    return false;
  }

  bool selectedIsDirectory = filename.endsWith("/");
  if (selectedIsDirectory) {
    filename.remove(filename.length() - 1);
    if (filename.length() == 0) {
      return false;
    }

    String nextPath = joinTextBrowserPath(gSdTextBrowserCurrentDir, filename);
    File dir = SD.open(nextPath.c_str());
    if (!dir || !dir.isDirectory()) {
      if (dir) {
        dir.close();
      }
      lv_textarea_set_text(textArea, "Ordner konnte nicht geoeffnet werden");
      return false;
    }

    dir.close();
    gSdTextBrowserCurrentDir = nextPath;
    fillTextRollerFor(roller);
    return false;
  }

  String path = joinTextBrowserPath(gSdTextBrowserCurrentDir, filename);
  if (!SD.exists(path.c_str())) {
    lv_textarea_set_text(textArea, "Datei nicht gefunden");
    return false;
  }

  File file = SD.open(path, FILE_READ);
  if (!file) {
    lv_textarea_set_text(textArea, "Datei konnte nicht geoeffnet werden");
    return false;
  }

  String content;
  content.reserve(1024);

  while (file.available()) {
    content += (char)file.read();
    if (content.length() > 4000) {
      content += "\n\n[Datei gekuerzt]";
      break;
    }
  }
  file.close();

  lv_textarea_set_text(textArea, content.c_str());
  return true;
}

static void saveTextFileFor(lv_obj_t * textArea, lv_obj_t * fileNameInput, lv_obj_t * roller)
{
  if (textArea == NULL || fileNameInput == NULL) return;
  if (!sd_ok) {
    Serial.println("SD Karte nicht verfuegbar");
    return;
  }

  const char * text = lv_textarea_get_text(textArea);
  String relativePath = normalizeTextSavePath(lv_textarea_get_text(fileNameInput));
  if (relativePath.length() == 0) {
    Serial.println("Dateiname leer");
    return;
  }

  if (hasTraversalSegment(relativePath)) {
    Serial.println("Ungueltiger Pfad (.. nicht erlaubt)");
    return;
  }

  String lowerPath = relativePath;
  lowerPath.toLowerCase();
  if (!lowerPath.endsWith(".txt")) {
    relativePath += ".txt";
  }

  if (!ensureTextSaveDirectories(relativePath)) {
    Serial.println("Zielordner konnte nicht erstellt werden");
    return;
  }

  String path = String(TEXT_DIR) + "/" + relativePath;
  if (SD.exists(path.c_str())) {
    SD.remove(path.c_str());
  }

  File file = SD.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Datei konnte nicht erstellt werden");
    return;
  }

  file.print(text);
  file.close();

  lv_textarea_set_text(fileNameInput, relativePath.c_str());
  fillTextRollerFor(roller);
}

static void openNewTextFileFor(lv_obj_t * textArea)
{
  if (textArea == NULL) return;
  lv_textarea_set_text(textArea, "");
}

static void sdAppOpenNewFileEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  openNewTextFileFor(gSdTextArea);
}

static void sdAppShowSavePopupEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (gSdPopupSave == NULL) return;

  lv_obj_clear_flag(gSdPopupSave, LV_OBJ_FLAG_HIDDEN);
  if (gSdKeyboard && gSdFileNameInput) {
    lv_keyboard_set_textarea(gSdKeyboard, gSdFileNameInput);
  }
}

static void sdAppShowOpenPopupEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (gSdPopupList == NULL) return;

  gSdOpenPopupShouldClose = false;
  fillTextRollerFor(gSdFileRoller);
  lv_obj_clear_flag(gSdPopupList, LV_OBJ_FLAG_HIDDEN);
}

static void sdAppTextareaClickedEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (gSdKeyboard && gSdTextArea) {
    lv_keyboard_set_textarea(gSdKeyboard, gSdTextArea);
  }
}

static void sdAppFileNameClickedEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (gSdKeyboard && gSdFileNameInput) {
    lv_keyboard_set_textarea(gSdKeyboard, gSdFileNameInput);
  }
}

static void sdAppSaveCancelEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (gSdPopupSave) {
    lv_obj_add_flag(gSdPopupSave, LV_OBJ_FLAG_HIDDEN);
  }
}

static void sdAppSaveConfirmEvent(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    saveTextFileFor(gSdTextArea, gSdFileNameInput, gSdFileRoller);
    return;
  }
  if (code == LV_EVENT_RELEASED && gSdPopupSave) {
    lv_obj_add_flag(gSdPopupSave, LV_OBJ_FLAG_HIDDEN);
  }
}

static void sdAppOpenCancelEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (gSdPopupList) {
    gSdOpenPopupShouldClose = false;
    lv_obj_add_flag(gSdPopupList, LV_OBJ_FLAG_HIDDEN);
  }
}

static void sdAppOpenConfirmEvent(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    gSdOpenPopupShouldClose = loadSelectedTextFileFor(gSdFileRoller, gSdTextArea);
    return;
  }
  if (code == LV_EVENT_RELEASED && gSdPopupList && gSdOpenPopupShouldClose) {
    gSdOpenPopupShouldClose = false;
    lv_obj_add_flag(gSdPopupList, LV_OBJ_FLAG_HIDDEN);
  }
}

static void renderSdTextApp()
{
  resetSdAppState();

  lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_AppContentArea, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(uic_AppContentArea, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t * btnOpenNew = lv_btn_create(uic_AppContentArea);
  lv_obj_set_size(btnOpenNew, 200, 50);
  lv_obj_align(btnOpenNew, LV_ALIGN_CENTER, -134, -210);
  lv_obj_set_style_radius(btnOpenNew, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(btnOpenNew, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(btnOpenNew, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_clear_flag(btnOpenNew, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * lblOpenNew = lv_label_create(btnOpenNew);
  lv_label_set_text(lblOpenNew, "Open New File");
  lv_obj_set_style_text_font(lblOpenNew, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblOpenNew, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lblOpenNew);

  lv_obj_t * btnSaveAs = lv_btn_create(uic_AppContentArea);
  lv_obj_set_size(btnSaveAs, 200, 50);
  lv_obj_align(btnSaveAs, LV_ALIGN_CENTER, 76, -210);
  lv_obj_set_style_radius(btnSaveAs, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(btnSaveAs, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(btnSaveAs, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_clear_flag(btnSaveAs, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * lblSaveAs = lv_label_create(btnSaveAs);
  lv_label_set_text(lblSaveAs, "Save as . . .");
  lv_obj_set_style_text_font(lblSaveAs, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblSaveAs, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lblSaveAs);

  lv_obj_t * btnOpenFile = lv_btn_create(uic_AppContentArea);
  lv_obj_set_size(btnOpenFile, 200, 50);
  lv_obj_align(btnOpenFile, LV_ALIGN_CENTER, 286, -210);
  lv_obj_set_style_radius(btnOpenFile, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(btnOpenFile, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(btnOpenFile, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_clear_flag(btnOpenFile, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * lblOpenFile = lv_label_create(btnOpenFile);
  lv_label_set_text(lblOpenFile, "Open file");
  lv_obj_set_style_text_font(lblOpenFile, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblOpenFile, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lblOpenFile);

  gSdKeyboard = lv_keyboard_create(uic_AppContentArea);
  lv_obj_set_size(gSdKeyboard, 793, 197);
  lv_obj_align(gSdKeyboard, LV_ALIGN_CENTER, 0, 135);
  lv_obj_set_style_text_font(gSdKeyboard, &lv_font_montserrat_20, LV_PART_ITEMS | LV_STATE_DEFAULT);

  gSdTextArea = lv_textarea_create(uic_AppContentArea);
  lv_obj_set_size(gSdTextArea, 774, 203);
  lv_obj_align(gSdTextArea, LV_ALIGN_CENTER, 0, -69);
  lv_textarea_set_placeholder_text(gSdTextArea, "You can enter your text here ...");
  lv_obj_set_style_text_font(gSdTextArea, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gSdTextArea, &lv_font_montserrat_20, LV_PART_SELECTED | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gSdTextArea, &lv_font_montserrat_20, LV_PART_CURSOR | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gSdTextArea, &lv_font_montserrat_20, LV_PART_TEXTAREA_PLACEHOLDER | LV_STATE_DEFAULT);

  gSdPopupSave = lv_obj_create(uic_AppContentArea);
  lv_obj_remove_style_all(gSdPopupSave);
  lv_obj_set_size(gSdPopupSave, 600, 140);
  lv_obj_align(gSdPopupSave, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_add_flag(gSdPopupSave, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(gSdPopupSave, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(gSdPopupSave, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(gSdPopupSave, lv_color_hex(0xFAFAFA), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gSdPopupSave, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t * btnSaveCancel = lv_btn_create(gSdPopupSave);
  lv_obj_set_size(btnSaveCancel, 160, 50);
  lv_obj_align(btnSaveCancel, LV_ALIGN_CENTER, -34, 33);
  lv_obj_set_style_radius(btnSaveCancel, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(btnSaveCancel, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(btnSaveCancel, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_clear_flag(btnSaveCancel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * lblSaveCancel = lv_label_create(btnSaveCancel);
  lv_label_set_text(lblSaveCancel, "Cancel");
  lv_obj_set_style_text_font(lblSaveCancel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblSaveCancel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lblSaveCancel);

  lv_obj_t * btnSaveConfirm = lv_btn_create(gSdPopupSave);
  lv_obj_set_size(btnSaveConfirm, 160, 50);
  lv_obj_align(btnSaveConfirm, LV_ALIGN_CENTER, -206, 33);
  lv_obj_set_style_radius(btnSaveConfirm, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(btnSaveConfirm, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(btnSaveConfirm, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_clear_flag(btnSaveConfirm, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * lblSaveConfirm = lv_label_create(btnSaveConfirm);
  lv_label_set_text(lblSaveConfirm, "Save");
  lv_obj_set_style_text_font(lblSaveConfirm, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblSaveConfirm, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lblSaveConfirm);

  gSdFileNameInput = lv_textarea_create(gSdPopupSave);
  lv_obj_set_size(gSdFileNameInput, 577, 52);
  lv_obj_align(gSdFileNameInput, LV_ALIGN_CENTER, 0, -32);
  lv_textarea_set_placeholder_text(gSdFileNameInput, "folder/file.txt");
  lv_obj_set_style_text_font(gSdFileNameInput, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gSdFileNameInput, &lv_font_montserrat_20, LV_PART_SELECTED | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gSdFileNameInput, &lv_font_montserrat_20, LV_PART_CURSOR | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gSdFileNameInput, &lv_font_montserrat_20, LV_PART_TEXTAREA_PLACEHOLDER | LV_STATE_DEFAULT);

  gSdPopupList = lv_obj_create(uic_AppContentArea);
  lv_obj_remove_style_all(gSdPopupList);
  lv_obj_set_size(gSdPopupList, 600, 350);
  lv_obj_align(gSdPopupList, LV_ALIGN_TOP_MID, 0, 23);
  lv_obj_add_flag(gSdPopupList, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(gSdPopupList, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(gSdPopupList, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(gSdPopupList, lv_color_hex(0xFAFAFA), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gSdPopupList, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t * btnOpenCancel = lv_btn_create(gSdPopupList);
  lv_obj_set_size(btnOpenCancel, 160, 50);
  lv_obj_align(btnOpenCancel, LV_ALIGN_CENTER, -34, 140);
  lv_obj_set_style_radius(btnOpenCancel, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(btnOpenCancel, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(btnOpenCancel, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_clear_flag(btnOpenCancel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * lblOpenCancel = lv_label_create(btnOpenCancel);
  lv_label_set_text(lblOpenCancel, "Cancel");
  lv_obj_set_style_text_font(lblOpenCancel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblOpenCancel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lblOpenCancel);

  lv_obj_t * btnOpenConfirm = lv_btn_create(gSdPopupList);
  lv_obj_set_size(btnOpenConfirm, 160, 50);
  lv_obj_align(btnOpenConfirm, LV_ALIGN_CENTER, -206, 140);
  lv_obj_set_style_radius(btnOpenConfirm, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(btnOpenConfirm, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(btnOpenConfirm, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_clear_flag(btnOpenConfirm, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * lblOpenConfirm = lv_label_create(btnOpenConfirm);
  lv_label_set_text(lblOpenConfirm, "Select");
  lv_obj_set_style_text_font(lblOpenConfirm, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lblOpenConfirm, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lblOpenConfirm);

  gSdFileRoller = lv_roller_create(gSdPopupList);
  lv_roller_set_options(gSdFileRoller, "Keine Dateien", LV_ROLLER_MODE_NORMAL);
  lv_obj_set_size(gSdFileRoller, 576, 266);
  lv_obj_align(gSdFileRoller, LV_ALIGN_CENTER, 0, -30);
  lv_obj_set_style_text_align(gSdFileRoller, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gSdFileRoller, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_keyboard_set_textarea(gSdKeyboard, gSdTextArea);

  lv_obj_add_event_cb(btnOpenNew, sdAppOpenNewFileEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(btnSaveAs, sdAppShowSavePopupEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(btnOpenFile, sdAppShowOpenPopupEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(gSdTextArea, sdAppTextareaClickedEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(gSdFileNameInput, sdAppFileNameClickedEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(btnSaveCancel, sdAppSaveCancelEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(btnSaveConfirm, sdAppSaveConfirmEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(btnOpenCancel, sdAppOpenCancelEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(btnOpenConfirm, sdAppOpenConfirmEvent, LV_EVENT_ALL, NULL);

  fillTextRollerFor(gSdFileRoller);
}

static void resetClockDashboardState()
{
  gClockTabView = NULL;
  gClockCurrentPanel = NULL;
  gClockStopwatchPanel = NULL;
  gClockTimerPanel = NULL;
  gClockCurrentTimeLabel = NULL;
  gClockCalendar = NULL;
  gClockStopwatchTimeLabel = NULL;
  gClockStopwatchToggleButton = NULL;
  gClockStopwatchToggleLabel = NULL;
  gClockTimerTimeLabel = NULL;
  gClockTimerHoursRoller = NULL;
  gClockTimerMinutesRoller = NULL;
  gClockTimerSecondsRoller = NULL;
  gClockTimerToggleButton = NULL;
  gClockTimerToggleLabel = NULL;
  gClockTimerResetButton = NULL;
  gClockActiveTab = 0;
  gClockAppVisible = false;
  gClockStopwatchRunning = false;
  gClockStopwatchElapsedMs = 0;
  gClockStopwatchLastMs = 0;
  gClockTimerRunning = false;
  gClockTimerHasBeenStarted = false;
  gClockTimerSelectedMs = 0;
  gClockTimerRemainingMs = 0;
  gClockTimerLastMs = 0;
  gClockCalendarYear = 0;
  gClockCalendarMonth = 0;
  gClockLastTimeLabelTs = 0;
  gClockCalendarSyncedWithRealTime = false;
}

static bool isLeapYearValue(int year)
{
  if (year % 400 == 0) return true;
  if (year % 100 == 0) return false;
  return (year % 4 == 0);
}

static int getDaysInMonthValue(int year, int month)
{
  static const int kDaysPerMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (month < 1 || month > 12) return 30;
  if (month == 2 && isLeapYearValue(year)) return 29;
  return kDaysPerMonth[month - 1];
}

static int getFirstWeekdayOfMonth(int year, int month)
{
  struct tm tmValue;
  memset(&tmValue, 0, sizeof(tmValue));
  tmValue.tm_year = year - 1900;
  tmValue.tm_mon = month - 1;
  tmValue.tm_mday = 1;
  tmValue.tm_hour = 12;

  if (mktime(&tmValue) == (time_t)-1) return 0;
  return tmValue.tm_wday;
}

static void formatHmsFromSeconds(uint32_t totalSeconds, char * out, size_t outLen)
{
  if (out == NULL || outLen == 0) return;
  uint32_t hours = totalSeconds / 3600;
  uint32_t minutes = (totalSeconds % 3600) / 60;
  uint32_t seconds = totalSeconds % 60;
  if (hours > 99) hours = 99;
  snprintf(out, outLen, "%02lu:%02lu:%02lu",
           (unsigned long)hours,
           (unsigned long)minutes,
           (unsigned long)seconds);
}

static void refreshClockDashboardTabs()
{
  if (gClockTabView == NULL) return;

  lv_obj_t * tabBtns = lv_tabview_get_tab_btns(gClockTabView);
  if (tabBtns == NULL) return;

  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(0x2A2A39), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(tabBtns, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(tabBtns, &lv_font_montserrat_24, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(tabBtns, lv_color_hex(0xF1F3F7), LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(0x2A2A39), LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(0x23495F), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(tabBtns, lv_color_hex(0x2D95F5), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_side(tabBtns, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(tabBtns, 4, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(tabBtns, lv_color_hex(0x2D95F5), LV_PART_ITEMS | LV_STATE_CHECKED);
}

static void updateClockDashboardCurrentTimeLabel()
{
  if (gClockCurrentTimeLabel == NULL) return;

  if (!isSystemTimeValid()) {
    lv_label_set_text(gClockCurrentTimeLabel, "00:00:00");
    gClockLastTimeLabelTs = 0;
    return;
  }

  time_t nowTs = time(nullptr);
  if (nowTs == gClockLastTimeLabelTs) return;
  gClockLastTimeLabelTs = nowTs;

  struct tm localTm;
  localtime_r(&nowTs, &localTm);

  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
           localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
  lv_label_set_text(gClockCurrentTimeLabel, timeBuf);
}

static void refreshClockDashboardCalendar()
{
  if (gClockCalendar == NULL) return;
  if (gClockCalendarMonth < 1 || gClockCalendarMonth > 12) return;

  lv_calendar_set_showed_date(gClockCalendar, gClockCalendarYear, gClockCalendarMonth);

  time_t nowTs = time(nullptr);
  if (nowTs > 1609459200) {
    struct tm localTm;
    localtime_r(&nowTs, &localTm);
    lv_calendar_set_today_date(
      gClockCalendar,
      localTm.tm_year + 1900,
      localTm.tm_mon + 1,
      localTm.tm_mday
    );
  }
}

static void changeClockDashboardMonth(int delta)
{
  if (delta == 0) return;
  gClockCalendarMonth += delta;
  while (gClockCalendarMonth < 1) {
    gClockCalendarMonth += 12;
    gClockCalendarYear--;
  }
  while (gClockCalendarMonth > 12) {
    gClockCalendarMonth -= 12;
    gClockCalendarYear++;
  }
  refreshClockDashboardCalendar();
}

static void showClockDashboardTab(uint8_t tabIndex)
{
  if (tabIndex > 2) tabIndex = 2;
  gClockActiveTab = tabIndex;
  if (gClockTabView != NULL) {
    lv_tabview_set_act(gClockTabView, gClockActiveTab, LV_ANIM_OFF);
  }
  refreshClockDashboardTabs();
}

static void clockDashboardTabCurrentEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  showClockDashboardTab(0);
}

static void clockDashboardTabStopwatchEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  showClockDashboardTab(1);
}

static void clockDashboardTabTimerEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  showClockDashboardTab(2);
}

static void clockDashboardMonthPrevEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  changeClockDashboardMonth(-1);
}

static void clockDashboardMonthNextEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  changeClockDashboardMonth(1);
}

static void updateClockDashboardStopwatchLabel()
{
  if (gClockStopwatchTimeLabel == NULL) return;
  uint32_t seconds = (uint32_t)(gClockStopwatchElapsedMs / 1000ULL);
  char timeBuf[16];
  formatHmsFromSeconds(seconds, timeBuf, sizeof(timeBuf));
  lv_label_set_text(gClockStopwatchTimeLabel, timeBuf);
}

static void updateClockDashboardStopwatchToggleLabel()
{
  if (gClockStopwatchToggleLabel == NULL) return;
  lv_label_set_text(gClockStopwatchToggleLabel, gClockStopwatchRunning ? "||" : ">");
}

static void clockDashboardStopwatchToggleEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  if (gClockStopwatchRunning) {
    gClockStopwatchRunning = false;
  } else {
    gClockStopwatchRunning = true;
    gClockStopwatchLastMs = millis();
  }
  updateClockDashboardStopwatchToggleLabel();
}

static void clockDashboardStopwatchResetEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  gClockStopwatchElapsedMs = 0;
  gClockStopwatchLastMs = millis();
  updateClockDashboardStopwatchLabel();
}

static String buildClockTimerRollerOptions(int maxValue)
{
  String options;
  if (maxValue < 0) return options;

  options.reserve((maxValue + 1) * 3);
  for (int i = 0; i <= maxValue; i++) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", i);
    options += buf;
    if (i < maxValue) {
      options += "\n";
    }
  }
  return options;
}

static uint64_t readClockTimerSelectionMs()
{
  if (gClockTimerHoursRoller == NULL ||
      gClockTimerMinutesRoller == NULL ||
      gClockTimerSecondsRoller == NULL) {
    return 0;
  }

  int hours = lv_roller_get_selected(gClockTimerHoursRoller);
  int minutes = lv_roller_get_selected(gClockTimerMinutesRoller);
  int seconds = lv_roller_get_selected(gClockTimerSecondsRoller);

  if (hours < 0) hours = 0;
  if (minutes < 0) minutes = 0;
  if (seconds < 0) seconds = 0;

  return (((uint64_t)hours * 3600ULL) + ((uint64_t)minutes * 60ULL) + (uint64_t)seconds) * 1000ULL;
}

static void updateClockTimerTimeLabel()
{
  if (gClockTimerTimeLabel == NULL) return;

  char timeBuf[16];
  formatHmsFromSeconds((uint32_t)(gClockTimerRemainingMs / 1000ULL), timeBuf, sizeof(timeBuf));
  lv_label_set_text(gClockTimerTimeLabel, timeBuf);
}

static void updateClockTimerToggleLabel()
{
  if (gClockTimerToggleLabel == NULL) return;
  lv_label_set_text(gClockTimerToggleLabel, gClockTimerRunning ? "Stop" : "Start");
}

static void syncClockTimerSelectionToUi()
{
  gClockTimerSelectedMs = readClockTimerSelectionMs();
  if (!gClockTimerRunning && !gClockTimerHasBeenStarted) {
    gClockTimerRemainingMs = gClockTimerSelectedMs;
    updateClockTimerTimeLabel();
  }
}

static void clockDashboardTimerSelectionChangedEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  syncClockTimerSelectionToUi();
}

static void clockDashboardTimerToggleEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  if (gClockTimerRunning) {
    gClockTimerRunning = false;
    gClockTimerHasBeenStarted = true;
    gClockTimerLastMs = millis();
  } else {
    if (!gClockTimerHasBeenStarted) {
      gClockTimerSelectedMs = readClockTimerSelectionMs();
      gClockTimerRemainingMs = gClockTimerSelectedMs;
    }

    if (gClockTimerRemainingMs == 0) {
      gClockTimerSelectedMs = readClockTimerSelectionMs();
      gClockTimerRemainingMs = gClockTimerSelectedMs;
    }

    gClockTimerRunning = (gClockTimerRemainingMs > 0);
    gClockTimerHasBeenStarted = gClockTimerRunning;
    gClockTimerLastMs = millis();
  }

  updateClockTimerToggleLabel();
}

static void clockDashboardTimerResetEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  gClockTimerSelectedMs = readClockTimerSelectionMs();
  gClockTimerRemainingMs = gClockTimerSelectedMs;
  gClockTimerRunning = false;
  gClockTimerHasBeenStarted = false;
  gClockTimerLastMs = millis();

  updateClockTimerTimeLabel();
  updateClockTimerToggleLabel();
}

static void updateClockDashboardTick()
{
  if (!gClockAppVisible) return;

  updateClockDashboardCurrentTimeLabel();

  if (!gClockCalendarSyncedWithRealTime && isSystemTimeValid()) {
    time_t nowTs = time(nullptr);
    struct tm localTm;
    localtime_r(&nowTs, &localTm);
    gClockCalendarYear = localTm.tm_year + 1900;
    gClockCalendarMonth = localTm.tm_mon + 1;
    gClockCalendarSyncedWithRealTime = true;
    refreshClockDashboardCalendar();
  }

  if (gClockStopwatchRunning) {
    unsigned long nowMs = millis();
    if (nowMs >= gClockStopwatchLastMs) {
      gClockStopwatchElapsedMs += (uint64_t)(nowMs - gClockStopwatchLastMs);
    }
    gClockStopwatchLastMs = nowMs;
    updateClockDashboardStopwatchLabel();
  }

  if (!gClockTimerRunning) return;

  unsigned long nowMs = millis();
  unsigned long elapsedMs = nowMs - gClockTimerLastMs;
  gClockTimerLastMs = nowMs;

  if ((uint64_t)elapsedMs >= gClockTimerRemainingMs) {
      gClockTimerRemainingMs = 0;
      gClockTimerRunning = false;
      gClockTimerHasBeenStarted = false;

      audio.stopSong();
      if (SD.exists(timerSound)) {
        audio.setVolume(21);
        audio.connecttoFS(SD, timerSound);
      }

      updateClockTimerTimeLabel();
      updateClockTimerToggleLabel();
      return;
  }

  gClockTimerRemainingMs -= (uint64_t)elapsedMs;
  updateClockTimerTimeLabel();
}

static void renderClockDashboardApp()
{
  resetClockDashboardState();
  gClockAppVisible = true;

  lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x050C14), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_AppContentArea, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_OFF);
  gClockTabView = lv_tabview_create(uic_AppContentArea, LV_DIR_TOP, 65);
  if (gClockTabView == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_obj_set_size(gClockTabView, 800, 480);
  lv_obj_set_align(gClockTabView, LV_ALIGN_CENTER);
  lv_obj_clear_flag(gClockTabView, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(gClockTabView, lv_color_hex(0x050C14), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gClockTabView, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(gClockTabView, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gClockTabView, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);



  refreshClockDashboardTabs();

  lv_obj_t * tabCurrent = lv_tabview_add_tab(gClockTabView, "Current Time");
  lv_obj_t * tabStopwatch = lv_tabview_add_tab(gClockTabView, "Stopwatch");
  lv_obj_t * tabTimer = lv_tabview_add_tab(gClockTabView, "Timer");

  ui_object_set_themeable_style_property(lv_tabview_get_tab_btns(gClockTabView),  LV_PART_ITEMS| LV_STATE_CHECKED, LV_STYLE_TEXT_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(lv_tabview_get_tab_btns(gClockTabView),  LV_PART_ITEMS| LV_STATE_CHECKED, LV_STYLE_TEXT_OPA, _ui_theme_alpha_MainTheme);
  ui_object_set_themeable_style_property(lv_tabview_get_tab_btns(gClockTabView),  LV_PART_ITEMS| LV_STATE_CHECKED, LV_STYLE_BORDER_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(lv_tabview_get_tab_btns(gClockTabView),  LV_PART_ITEMS| LV_STATE_CHECKED, LV_STYLE_BORDER_OPA, _ui_theme_alpha_MainTheme);

  gClockCurrentPanel = tabCurrent;
  gClockStopwatchPanel = tabStopwatch;
  gClockTimerPanel = tabTimer;
  if (tabCurrent == NULL || tabStopwatch == NULL || tabTimer == NULL) {
    gClockAppVisible = false;
    return;
  }

  lv_obj_set_style_bg_color(tabCurrent, lv_color_hex(0x050C14), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(tabCurrent, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(tabCurrent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(tabStopwatch, lv_color_hex(0x050C14), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(tabStopwatch, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(tabStopwatch, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(tabTimer, lv_color_hex(0x050C14), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(tabTimer, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(tabTimer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  gClockCurrentTimeLabel = lv_label_create(tabCurrent);
  if (gClockCurrentTimeLabel == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_label_set_text(gClockCurrentTimeLabel, "00:00:00");
  lv_obj_align(gClockCurrentTimeLabel, LV_ALIGN_CENTER, 0, -130);
  lv_obj_set_style_text_color(gClockCurrentTimeLabel, lv_color_hex(0xF2F3F6), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gClockCurrentTimeLabel, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);

  gClockCalendar = lv_calendar_create(tabCurrent);
  if (gClockCalendar == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_obj_set_size(gClockCalendar, 757, 267);
  lv_obj_align(gClockCalendar, LV_ALIGN_CENTER, 0, 48);
  lv_obj_set_style_radius(gClockCalendar, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(gClockCalendar, lv_color_hex(0x2A2A39), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gClockCalendar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(gClockCalendar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(gClockCalendar, lv_color_hex(0xE6E9EF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(gClockCalendar, lv_color_hex(0x8A8F9A), LV_PART_ITEMS | LV_STATE_DISABLED);
  lv_obj_set_style_text_color(gClockCalendar, lv_color_hex(0xE6E9EF), LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gClockCalendar, 0, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(gClockCalendar, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(gClockCalendar, lv_color_hex(0x2E3340), LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gClockCalendar, 0, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(gClockCalendar, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(gClockCalendar, lv_color_hex(0x2D95F5), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(gClockCalendar, lv_color_hex(0xE6E9EF), LV_PART_ITEMS | LV_STATE_CHECKED);

  

  lv_obj_t * calendarHeader = lv_calendar_header_arrow_create(gClockCalendar);
  if (calendarHeader != NULL) {
    lv_obj_set_style_bg_opa(calendarHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(calendarHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(calendarHeader, lv_color_hex(0xE7EAF0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(calendarHeader, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  gClockStopwatchTimeLabel = lv_label_create(tabStopwatch);
  if (gClockStopwatchTimeLabel == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_label_set_text(gClockStopwatchTimeLabel, "00:00:00");
  lv_obj_align(gClockStopwatchTimeLabel, LV_ALIGN_CENTER, 0, -130);
  lv_obj_set_style_text_color(gClockStopwatchTimeLabel, lv_color_hex(0xF2F3F6), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gClockStopwatchTimeLabel, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t * resetBtn = lv_btn_create(tabStopwatch);
  if (resetBtn == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_obj_set_size(resetBtn, 191, 50);
  lv_obj_align(resetBtn, LV_ALIGN_CENTER, -102, 0);
  lv_obj_set_style_radius(resetBtn, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(resetBtn, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(resetBtn, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_border_width(resetBtn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(resetBtn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * resetLbl = lv_label_create(resetBtn);
  lv_label_set_text(resetLbl, "Reset");
  lv_obj_set_style_text_color(resetLbl, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(resetLbl, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(resetLbl);

  gClockStopwatchToggleButton = lv_btn_create(tabStopwatch);
  if (gClockStopwatchToggleButton == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_obj_set_size(gClockStopwatchToggleButton, 191, 50);
  lv_obj_align(gClockStopwatchToggleButton, LV_ALIGN_CENTER, 102, 0);
  lv_obj_set_style_radius(gClockStopwatchToggleButton, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(gClockStopwatchToggleButton, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(gClockStopwatchToggleButton, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_border_width(gClockStopwatchToggleButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(gClockStopwatchToggleButton, LV_OBJ_FLAG_SCROLLABLE);
  gClockStopwatchToggleLabel = lv_label_create(gClockStopwatchToggleButton);
  if (gClockStopwatchToggleLabel == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_label_set_text(gClockStopwatchToggleLabel, ">");
  lv_obj_set_style_text_color(gClockStopwatchToggleLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gClockStopwatchToggleLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(gClockStopwatchToggleLabel);

  lv_obj_add_event_cb(resetBtn, clockDashboardStopwatchResetEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(gClockStopwatchToggleButton, clockDashboardStopwatchToggleEvent, LV_EVENT_ALL, NULL);

  gClockTimerTimeLabel = lv_label_create(tabTimer);
  if (gClockTimerTimeLabel == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_label_set_text(gClockTimerTimeLabel, "00:01:00");
  lv_obj_align(gClockTimerTimeLabel, LV_ALIGN_TOP_MID, 0, 16);
  lv_obj_set_style_text_color(gClockTimerTimeLabel, lv_color_hex(0xF2F3F6), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gClockTimerTimeLabel, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t * timerCard = lv_obj_create(tabTimer);
  if (timerCard == NULL) {
    gClockAppVisible = false;
    return;
  }
  gClockTimerPanel = timerCard;
  lv_obj_set_size(timerCard, 740, 170);
  lv_obj_align(timerCard, LV_ALIGN_TOP_MID, 0, 72);
  lv_obj_set_style_radius(timerCard, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(timerCard, lv_color_hex(0x2A2A39), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(timerCard, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(timerCard, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(timerCard, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(timerCard, LV_OBJ_FLAG_SCROLLABLE);

  String hoursOptions = buildClockTimerRollerOptions(99);
  String minutesOptions = buildClockTimerRollerOptions(59);
  String secondsOptions = buildClockTimerRollerOptions(59);

  gClockTimerHoursRoller = lv_roller_create(timerCard);
  gClockTimerMinutesRoller = lv_roller_create(timerCard);
  gClockTimerSecondsRoller = lv_roller_create(timerCard);
  if (gClockTimerHoursRoller == NULL ||
      gClockTimerMinutesRoller == NULL ||
      gClockTimerSecondsRoller == NULL) {
    gClockAppVisible = false;
    return;
  }

  lv_obj_t * timerRollers[] = {
    gClockTimerHoursRoller,
    gClockTimerMinutesRoller,
    gClockTimerSecondsRoller
  };
  const char * timerRollerOptions[] = {
    hoursOptions.c_str(),
    minutesOptions.c_str(),
    secondsOptions.c_str()
  };
  const int timerRollerOffsets[] = { -220, 0, 220 };

  for (int i = 0; i < 3; i++) {
    lv_obj_set_size(timerRollers[i], 115, 120);
    lv_obj_align(timerRollers[i], LV_ALIGN_CENTER, timerRollerOffsets[i], -4);
    lv_roller_set_options(timerRollers[i], timerRollerOptions[i], LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(timerRollers[i], (i == 1) ? 1 : 0, LV_ANIM_OFF);
    lv_roller_set_visible_row_count(timerRollers[i], 3);
    ui_object_set_themeable_style_property(timerRollers[i], LV_PART_SELECTED| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
    ui_object_set_themeable_style_property(timerRollers[i], LV_PART_SELECTED| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
    lv_obj_set_style_border_width(timerRollers[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(timerRollers[i], lv_color_hex(0xF1F3F7), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(timerRollers[i], &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  lv_obj_t * timerHourLabel = lv_label_create(timerCard);
  lv_label_set_text(timerHourLabel, "h");
  lv_obj_align(timerHourLabel, LV_ALIGN_CENTER, -220, 70);
  lv_obj_set_style_text_color(timerHourLabel, lv_color_hex(0x8FA4B8), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(timerHourLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t * timerMinuteLabel = lv_label_create(timerCard);
  lv_label_set_text(timerMinuteLabel, "m");
  lv_obj_align(timerMinuteLabel, LV_ALIGN_CENTER, 0, 70);
  lv_obj_set_style_text_color(timerMinuteLabel, lv_color_hex(0x8FA4B8), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(timerMinuteLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t * timerSecondLabel = lv_label_create(timerCard);
  lv_label_set_text(timerSecondLabel, "s");
  lv_obj_align(timerSecondLabel, LV_ALIGN_CENTER, 220, 70);
  lv_obj_set_style_text_color(timerSecondLabel, lv_color_hex(0x8FA4B8), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(timerSecondLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  gClockTimerToggleButton = lv_btn_create(tabTimer);
  if (gClockTimerToggleButton == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_obj_set_size(gClockTimerToggleButton, 200, 50);
  lv_obj_align(gClockTimerToggleButton, LV_ALIGN_BOTTOM_MID, -108, -14);
  lv_obj_set_style_radius(gClockTimerToggleButton, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(gClockTimerToggleButton, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(gClockTimerToggleButton, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_border_width(gClockTimerToggleButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(gClockTimerToggleButton, LV_OBJ_FLAG_SCROLLABLE);
  gClockTimerToggleLabel = lv_label_create(gClockTimerToggleButton);
  if (gClockTimerToggleLabel == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_label_set_text(gClockTimerToggleLabel, "Start");
  lv_obj_set_style_text_color(gClockTimerToggleLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(gClockTimerToggleLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(gClockTimerToggleLabel);

  gClockTimerResetButton = lv_btn_create(tabTimer);
  if (gClockTimerResetButton == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_obj_set_size(gClockTimerResetButton, 200, 50);
  lv_obj_align(gClockTimerResetButton, LV_ALIGN_BOTTOM_MID, 108, -14);
  lv_obj_set_style_radius(gClockTimerResetButton, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(gClockTimerResetButton, lv_color_hex(0x3A4252), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(gClockTimerResetButton, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(gClockTimerResetButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(gClockTimerResetButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t * timerResetLabel = lv_label_create(gClockTimerResetButton);
  if (timerResetLabel == NULL) {
    gClockAppVisible = false;
    return;
  }
  lv_label_set_text(timerResetLabel, "Reset");
  lv_obj_set_style_text_color(timerResetLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(timerResetLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(timerResetLabel);

  lv_obj_add_event_cb(gClockTimerHoursRoller, clockDashboardTimerSelectionChangedEvent, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(gClockTimerMinutesRoller, clockDashboardTimerSelectionChangedEvent, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(gClockTimerSecondsRoller, clockDashboardTimerSelectionChangedEvent, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(gClockTimerToggleButton, clockDashboardTimerToggleEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(gClockTimerResetButton, clockDashboardTimerResetEvent, LV_EVENT_ALL, NULL);

  time_t nowTs = time(nullptr);
  struct tm localTm;
  if (isSystemTimeValid()) {
    localtime_r(&nowTs, &localTm);
    gClockCalendarYear = localTm.tm_year + 1900;
    gClockCalendarMonth = localTm.tm_mon + 1;
    gClockCalendarSyncedWithRealTime = true;
  } else {
    gClockCalendarYear = 2026;
    gClockCalendarMonth = 1;
    gClockCalendarSyncedWithRealTime = false;
  }

  gClockStopwatchRunning = false;
  gClockStopwatchElapsedMs = 0;
  gClockStopwatchLastMs = millis();
  gClockTimerSelectedMs = readClockTimerSelectionMs();
  gClockTimerRemainingMs = gClockTimerSelectedMs;
  gClockTimerRunning = false;
  gClockTimerHasBeenStarted = false;
  gClockTimerLastMs = millis();
  gClockLastTimeLabelTs = 0;

  updateClockDashboardCurrentTimeLabel();
  updateClockDashboardStopwatchLabel();
  updateClockDashboardStopwatchToggleLabel();
  updateClockTimerTimeLabel();
  updateClockTimerToggleLabel();
  refreshClockDashboardCalendar();

  if (ui_HomeButton9 != NULL) {
    lv_obj_move_foreground(ui_HomeButton9);
  }
}

static void resetWeatherAppState()
{
  uic_LabelWeatherTemperature = NULL;
  uic_LabelWeatherInformation = NULL;
  uic_RollerWeatherData = NULL;
  uic_WeatherSearchPanel = NULL;
  uic_WeatherSearchTextArea = NULL;
  uic_WeatherSearchDropdown = NULL;
  uic_WeatherSearchKeyboard = NULL;
  uic_WeatherSearchConfirmButton = NULL;
  uic_WeatherSearchOpenButton = NULL;
  gWeatherAppVisible = false;
  gWeatherLastFetchMs = 0;
  gWeatherFetchRunning = false;
  gWeatherUseCurrentLocation = true;
  gWeatherSelectedLatitude = "";
  gWeatherSelectedLongitude = "";
  gWeatherSelectedLocationLabel = "Current Location";
  gWeatherSearchResultCount = 0;
}

static bool extractJsonStringField(const String& json, const char * key, String * out)
{
  if (key == NULL || out == NULL) return false;
  String token = "\"";
  token += key;
  token += "\":";
  int pos = json.indexOf(token);
  if (pos < 0) return false;
  pos += token.length();
  while (pos < (int)json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  if (pos >= (int)json.length()) return false;
  if (json[pos] != '"') {
    pos = json.indexOf('"', pos);
    if (pos < 0) return false;
  }
  int end = json.indexOf('"', pos + 1);
  if (end < 0) return false;
  *out = json.substring(pos + 1, end);
  return true;
}

static bool extractJsonNumberField(const String& json, const char * key, String * out)
{
  if (key == NULL || out == NULL) return false;
  String token = "\"";
  token += key;
  token += "\":";
  int pos = json.indexOf(token);
  if (pos < 0) return false;
  pos += token.length();
  while (pos < (int)json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  if (pos >= (int)json.length()) return false;
  int end = pos;
  while (end < (int)json.length()) {
    char c = json[end];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
      end++;
      continue;
    }
    break;
  }
  if (end <= pos) return false;
  *out = json.substring(pos, end);
  out->trim();
  return out->length() > 0;
}

static bool extractJsonArrayRaw(const String& json, const char * key, String * out)
{
  if (key == NULL || out == NULL) return false;
  String token = "\"";
  token += key;
  token += "\":[";
  int pos = json.indexOf(token);
  if (pos < 0) return false;
  pos += token.length();
  int end = json.indexOf(']', pos);
  if (end < 0) return false;
  *out = json.substring(pos, end);
  return true;
}

static String weatherCodeToText(int code)
{
  switch (code) {
    case 0: return "clear";
    case 1:
    case 2:
    case 3: return "cloudy";
    case 45:
    case 48: return "fog";
    case 51:
    case 53:
    case 55: return "drizzle";
    case 56:
    case 57: return "freezing drizzle";
    case 61:
    case 63:
    case 65: return "rain";
    case 66:
    case 67: return "freezing rain";
    case 71:
    case 73:
    case 75:
    case 77: return "snow";
    case 80:
    case 81:
    case 82: return "rain showers";
    case 85:
    case 86: return "snow showers";
    case 95: return "thunderstorm";
    case 96:
    case 99: return "thunderstorm hail";
    default: return "unknown";
  }
}

static bool fetchWeatherData(String * temperatureHumidity, String * information, String * rollerOptions)
{
  if (temperatureHumidity == NULL || information == NULL || rollerOptions == NULL) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  String city;
  String country;
  String latStr;
  String lonStr;

  if (!gWeatherUseCurrentLocation && gWeatherSelectedLatitude.length() > 0 && gWeatherSelectedLongitude.length() > 0) {
    latStr = gWeatherSelectedLatitude;
    lonStr = gWeatherSelectedLongitude;
    city = gWeatherSelectedLocationLabel;
  }

  if (latStr.length() == 0 || lonStr.length() == 0) {
    HTTPClient http;
    http.setTimeout(12000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("fOS2.0-weather/1.0");
    bool started = http.begin("http://ip-api.com/json/?fields=status,message,country,city,lat,lon");
    if (!started) return false;

    int code = http.GET();
    Serial.printf("[WEATHER] ip-api code=%d\n", code);
    if (code != HTTP_CODE_OK) {
      http.end();
      return false;
    }

    String payload = http.getString();
    http.end();

    String status;
    if (!extractJsonStringField(payload, "status", &status)) return false;
    status.toLowerCase();
    if (status != "success") return false;
    if (!extractJsonNumberField(payload, "lat", &latStr)) return false;
    if (!extractJsonNumberField(payload, "lon", &lonStr)) return false;
    if (!extractJsonStringField(payload, "city", &city)) city = "unknown location";
    if (!extractJsonStringField(payload, "country", &country)) country = "";
  }

  String weatherJson;
  {
    String url = "https://api.open-meteo.com/v1/forecast?latitude=";
    url += latStr;
    url += "&longitude=";
    url += lonStr;
    url += "&current=temperature_2m,relative_humidity_2m,weather_code";
    url += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
    url += "&timezone=auto&forecast_days=7";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("fOS2.0-weather/1.0");
    bool started = http.begin(client, url);
    if (!started) return false;

    int code = http.GET();
    Serial.printf("[WEATHER] open-meteo code=%d\n", code);
    if (code != HTTP_CODE_OK) {
      http.end();
      return false;
    }

    weatherJson = http.getString();
    http.end();
  }

  int currentPos = weatherJson.indexOf("\"current\":{");
  int dailyPos = weatherJson.indexOf("\"daily\":{");
  if (currentPos < 0 || dailyPos < 0) return false;

  auto extractNumberFrom = [&](const char * key, int startPos, String * out) -> bool {
    if (key == NULL || out == NULL) return false;
    String token = "\"";
    token += key;
    token += "\":";
    int pos = weatherJson.indexOf(token, startPos);
    if (pos < 0) return false;
    pos += token.length();
    while (pos < (int)weatherJson.length() && (weatherJson[pos] == ' ' || weatherJson[pos] == '\t')) pos++;
    int end = pos;
    while (end < (int)weatherJson.length()) {
      char c = weatherJson[end];
      if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') end++;
      else break;
    }
    if (end <= pos) return false;
    *out = weatherJson.substring(pos, end);
    out->trim();
    return out->length() > 0;
  };

  String tempStr;
  String humidityStr;
  String weatherCodeStr;
  if (!extractNumberFrom("temperature_2m", currentPos, &tempStr)) return false;
  if (!extractNumberFrom("relative_humidity_2m", currentPos, &humidityStr)) return false;
  if (!extractNumberFrom("weather_code", currentPos, &weatherCodeStr)) return false;

  String arrDatesRaw;
  String arrMaxRaw;
  String arrMinRaw;
  String arrCodeRaw;
  if (!extractJsonArrayRaw(weatherJson.substring(dailyPos), "time", &arrDatesRaw)) return false;
  if (!extractJsonArrayRaw(weatherJson.substring(dailyPos), "temperature_2m_max", &arrMaxRaw)) return false;
  if (!extractJsonArrayRaw(weatherJson.substring(dailyPos), "temperature_2m_min", &arrMinRaw)) return false;
  if (!extractJsonArrayRaw(weatherJson.substring(dailyPos), "weather_code", &arrCodeRaw)) return false;

  String dates[7];
  String maxs[7];
  String mins[7];
  String codes[7];
  int dateCount = 0;
  int maxCount = 0;
  int minCount = 0;
  int codeCount = 0;

  int pos = 0;
  while (dateCount < 7) {
    int q1 = arrDatesRaw.indexOf('"', pos);
    if (q1 < 0) break;
    int q2 = arrDatesRaw.indexOf('"', q1 + 1);
    if (q2 < 0) break;
    dates[dateCount++] = arrDatesRaw.substring(q1 + 1, q2);
    pos = q2 + 1;
  }

  auto fillNumberArray = [](const String& raw, String outArr[7], int * outCount) {
    if (outCount == NULL) return;
    *outCount = 0;
    int start = 0;
    while (*outCount < 7 && start < (int)raw.length()) {
      int comma = raw.indexOf(',', start);
      if (comma < 0) comma = raw.length();
      String token = raw.substring(start, comma);
      token.trim();
      if (token.length() > 0) {
        outArr[*outCount] = token;
        (*outCount)++;
      }
      start = comma + 1;
    }
  };

  fillNumberArray(arrMaxRaw, maxs, &maxCount);
  fillNumberArray(arrMinRaw, mins, &minCount);
  fillNumberArray(arrCodeRaw, codes, &codeCount);

  int usable = dateCount;
  if (maxCount < usable) usable = maxCount;
  if (minCount < usable) usable = minCount;
  if (codeCount < usable) usable = codeCount;
  if (usable <= 0) return false;

  int tempI = (int)round(tempStr.toFloat());
  int humI = (int)round(humidityStr.toFloat());
  *temperatureHumidity = String(tempI) + "C   " + String(humI) + "%";

  int currentCode = weatherCodeStr.toInt();
  String currentPhenomenon = weatherCodeToText(currentCode);
  if (country.length() > 0) {
    *information = currentPhenomenon + " | " + city + ", " + country;
  } else {
    *information = currentPhenomenon + " | " + city;
  }

  rollerOptions->remove(0);
  for (int i = 0; i < usable; i++) {
    String weekday = dates[i];
    if (dates[i].length() >= 10) {
      int year = dates[i].substring(0, 4).toInt();
      int month = dates[i].substring(5, 7).toInt();
      int day = dates[i].substring(8, 10).toInt();
      struct tm tmValue;
      memset(&tmValue, 0, sizeof(tmValue));
      tmValue.tm_year = year - 1900;
      tmValue.tm_mon = month - 1;
      tmValue.tm_mday = day;
      tmValue.tm_hour = 12;
      if (mktime(&tmValue) != (time_t)-1) {
        weekday = String(kWeekdaysEn[tmValue.tm_wday]);
      }
    }

    String line = weekday + " " + dates[i] + " | " + mins[i] + "-" + maxs[i] + "C | " + weatherCodeToText(codes[i].toInt());
    *rollerOptions += line;
    if (i + 1 < usable) *rollerOptions += "\n";
  }

  return true;
}

static void applyWeatherUiData(const String& temperatureHumidity, const String& information, const String& rollerOptions)
{
  if (uic_LabelWeatherTemperature) {
    lv_label_set_text(uic_LabelWeatherTemperature, temperatureHumidity.c_str());
  }
  if (uic_LabelWeatherInformation) {
    lv_label_set_text(uic_LabelWeatherInformation, information.c_str());
  }
  if (uic_RollerWeatherData) {
    lv_roller_set_options(uic_RollerWeatherData, rollerOptions.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(uic_RollerWeatherData, 0, LV_ANIM_OFF);
  }
}

static void refreshWeatherDataIfNeeded(bool force)
{
  if (!gWeatherAppVisible) return;
  if (gWeatherFetchRunning) return;

  unsigned long nowMs = millis();
  if (!force && (nowMs - gWeatherLastFetchMs < kWeatherRefreshIntervalMs)) return;

  gWeatherFetchRunning = true;

  String tempHum;
  String info;
  String roller;
  bool ok = fetchWeatherData(&tempHum, &info, &roller);
  if (ok) {
    applyWeatherUiData(tempHum, info, roller);
    gWeatherLastFetchMs = nowMs;
  } else {
    if (force) {
      applyWeatherUiData("--C   --%", "No weather data found", "No Data");
    }
    if (kWeatherRefreshIntervalMs > 60000UL) {
      gWeatherLastFetchMs = nowMs - (kWeatherRefreshIntervalMs - 60000UL);
    } else {
      gWeatherLastFetchMs = nowMs;
    }
  }

  gWeatherFetchRunning = false;
}

static void updateWeatherSearchDropdownOptions()
{
  if (uic_WeatherSearchDropdown == NULL) return;

  String options = "Current Location";
  for (int i = 0; i < gWeatherSearchResultCount; i++) {
    options += "\n";
    options += gWeatherSearchResultLabel[i];
  }

  if (gWeatherSearchResultCount == 0) {
    options += "\nNo results";
  }

  lv_dropdown_set_options(uic_WeatherSearchDropdown, options.c_str());
  lv_dropdown_set_selected(uic_WeatherSearchDropdown, 0);
}

static void showWeatherSearchPanel()
{
  if (uic_WeatherSearchPanel == NULL) return;
  lv_obj_clear_flag(uic_WeatherSearchPanel, LV_OBJ_FLAG_HIDDEN);
  if (uic_WeatherSearchKeyboard != NULL && uic_WeatherSearchTextArea != NULL) {
    lv_keyboard_set_textarea(uic_WeatherSearchKeyboard, uic_WeatherSearchTextArea);
    lv_obj_clear_flag(uic_WeatherSearchKeyboard, LV_OBJ_FLAG_HIDDEN);
  }
  if (uic_WeatherSearchDropdown != NULL) {
    updateWeatherSearchDropdownOptions();
  }
  if (uic_WeatherSearchConfirmButton != NULL) {
    lv_obj_move_foreground(uic_WeatherSearchConfirmButton);
  }
}

static void hideWeatherSearchPanel()
{
  if (uic_WeatherSearchPanel != NULL) {
    lv_obj_add_flag(uic_WeatherSearchPanel, LV_OBJ_FLAG_HIDDEN);
  }
  if (uic_WeatherSearchKeyboard != NULL) {
    lv_keyboard_set_textarea(uic_WeatherSearchKeyboard, NULL);
    lv_obj_add_flag(uic_WeatherSearchKeyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

static bool performWeatherGeocodingSearch(const String& query)
{
  if (query.length() == 0) {
    gWeatherSearchResultCount = 0;
    updateWeatherSearchDropdownOptions();
    return false;
  }

  String encoded = urlEncode(query);
  String geocodeUrl = "https://geocoding-api.open-meteo.com/v1/search?name=" + encoded + "&count=6&language=en&format=json";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("fOS3.0-weather-search/1.0");
  bool started = http.begin(client, geocodeUrl);
  if (!started) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  String resultsRaw;
  if (!extractJsonArrayRaw(payload, "results", &resultsRaw)) {
    gWeatherSearchResultCount = 0;
    updateWeatherSearchDropdownOptions();
    return false;
  }

  gWeatherSearchResultCount = 0;
  int pos = 0;
  while (gWeatherSearchResultCount < kMaxWeatherSearchResults) {
    int objStart = resultsRaw.indexOf('{', pos);
    if (objStart < 0) break;
    int objEnd = resultsRaw.indexOf('}', objStart);
    if (objEnd < 0) break;
    String item = resultsRaw.substring(objStart, objEnd + 1);

    String name;
    String country;
    String lat;
    String lon;
    if (extractJsonStringField(item, "name", &name) && extractJsonNumberField(item, "latitude", &lat) && extractJsonNumberField(item, "longitude", &lon)) {
      if (!extractJsonStringField(item, "country", &country)) {
        country = "";
      }
      String label = name;
      if (country.length() > 0) {
        label += ", ";
        label += country;
      }
      gWeatherSearchResultLabel[gWeatherSearchResultCount] = label;
      gWeatherSearchResultLatitude[gWeatherSearchResultCount] = lat;
      gWeatherSearchResultLongitude[gWeatherSearchResultCount] = lon;
      gWeatherSearchResultCount++;
    }

    pos = objEnd + 1;
  }

  updateWeatherSearchDropdownOptions();
  return gWeatherSearchResultCount > 0;
}

static void applyWeatherSearchSelection()
{
  if (uic_WeatherSearchDropdown == NULL) return;

  int selected = lv_dropdown_get_selected(uic_WeatherSearchDropdown);
  if (selected <= 0) {
    gWeatherUseCurrentLocation = true;
    gWeatherSelectedLocationLabel = "Current Location";
    gWeatherSelectedLatitude = "";
    gWeatherSelectedLongitude = "";
  } else {
    int index = selected - 1;
    if (index < 0 || index >= gWeatherSearchResultCount) return;
    gWeatherUseCurrentLocation = false;
    gWeatherSelectedLatitude = gWeatherSearchResultLatitude[index];
    gWeatherSelectedLongitude = gWeatherSearchResultLongitude[index];
    gWeatherSelectedLocationLabel = gWeatherSearchResultLabel[index];
  }

  refreshWeatherDataIfNeeded(true);
}

static void weatherSearchKeyboardEvent(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) return;
  if (code == LV_EVENT_CANCEL) {
    hideWeatherSearchPanel();
    return;
  }

  if (uic_WeatherSearchTextArea == NULL) return;
  String query = lv_textarea_get_text(uic_WeatherSearchTextArea);
  query.trim();
  if (query.length() == 0) return;
  performWeatherGeocodingSearch(query);
}

static void weatherSearchButtonEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (uic_WeatherSearchPanel != NULL && !lv_obj_has_flag(uic_WeatherSearchPanel, LV_OBJ_FLAG_HIDDEN)) {
    applyWeatherSearchSelection();
    hideWeatherSearchPanel();
  } else {
    showWeatherSearchPanel();
  }
}

static void weatherSearchConfirmEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  applyWeatherSearchSelection();
  hideWeatherSearchPanel();
}

static String urlEncode(const String& value)
{
  String encoded;
  encoded.reserve(value.length() * 3);
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '%';
      encoded += '2';
      encoded += '0';
    } else {
      const char hexDigits[] = "0123456789ABCDEF";
      encoded += '%';
      encoded += hexDigits[(c >> 4) & 0xF];
      encoded += hexDigits[c & 0xF];
    }
  }
  return encoded;
}

static void renderWeatherApp()
{
  resetWeatherAppState();
  gWeatherAppVisible = true;

  lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x0B121B), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_AppContentArea, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_OFF);

  uic_WeatherSearchOpenButton = lv_btn_create(uic_AppContentArea);
  if (uic_WeatherSearchOpenButton != NULL) {
    lv_obj_set_size(uic_WeatherSearchOpenButton, 120, 50);
    lv_obj_align(uic_WeatherSearchOpenButton, LV_ALIGN_CENTER, 325, -210);
    lv_obj_set_style_radius(uic_WeatherSearchOpenButton, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(uic_WeatherSearchOpenButton, lv_color_hex(0x1A82FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(uic_WeatherSearchOpenButton, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_object_set_themeable_style_property(uic_WeatherSearchOpenButton, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
    ui_object_set_themeable_style_property(uic_WeatherSearchOpenButton, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
    lv_obj_set_style_border_width(uic_WeatherSearchOpenButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(uic_WeatherSearchOpenButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t * searchLabel = lv_label_create(uic_WeatherSearchOpenButton);
    lv_label_set_text(searchLabel, "Search");
    lv_obj_set_style_text_color(searchLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_text_font(searchLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(searchLabel);
    lv_obj_add_event_cb(uic_WeatherSearchOpenButton, weatherSearchButtonEvent, LV_EVENT_CLICKED, NULL);
  }

  uic_LabelWeatherTemperature = lv_label_create(uic_AppContentArea);
  if (uic_LabelWeatherTemperature == NULL) {
    gWeatherAppVisible = false;
    return;
  }
  lv_label_set_text(uic_LabelWeatherTemperature, "--C   --%");
  lv_obj_align(uic_LabelWeatherTemperature, LV_ALIGN_CENTER, 0, -183);
  lv_obj_set_style_text_font(uic_LabelWeatherTemperature, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(uic_LabelWeatherTemperature, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

  uic_LabelWeatherInformation = lv_label_create(uic_AppContentArea);
  if (uic_LabelWeatherInformation == NULL) {
    gWeatherAppVisible = false;
    return;
  }
  lv_label_set_text(uic_LabelWeatherInformation, "No weather data found");
  lv_obj_align(uic_LabelWeatherInformation, LV_ALIGN_CENTER, 0, -124);
  lv_obj_set_style_text_font(uic_LabelWeatherInformation, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(uic_LabelWeatherInformation, lv_color_hex(0xB7C1DA), LV_PART_MAIN | LV_STATE_DEFAULT);

  uic_RollerWeatherData = lv_roller_create(uic_AppContentArea);
  if (uic_RollerWeatherData == NULL) {
    gWeatherAppVisible = false;
    return;
  }
  lv_roller_set_options(uic_RollerWeatherData, "No Data", LV_ROLLER_MODE_NORMAL);
  lv_obj_set_size(uic_RollerWeatherData, 758, 303);
  lv_obj_align(uic_RollerWeatherData, LV_ALIGN_CENTER, 0, 70);
  lv_obj_set_style_text_font(uic_RollerWeatherData, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(uic_RollerWeatherData, &lv_font_montserrat_20, LV_PART_SELECTED | LV_STATE_DEFAULT);
  ui_object_set_themeable_style_property(uic_RollerWeatherData, LV_PART_SELECTED| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(uic_RollerWeatherData, LV_PART_SELECTED| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_border_width(uic_RollerWeatherData, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  uic_WeatherSearchPanel = lv_obj_create(uic_AppContentArea);
  if (uic_WeatherSearchPanel == NULL) {
    gWeatherAppVisible = false;
    return;
  }
  lv_obj_set_size(uic_WeatherSearchPanel, 780, 340);
  lv_obj_align(uic_WeatherSearchPanel, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_radius(uic_WeatherSearchPanel, 22, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(uic_WeatherSearchPanel, lv_color_hex(0x101820), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_WeatherSearchPanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(uic_WeatherSearchPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(uic_WeatherSearchPanel, LV_OBJ_FLAG_HIDDEN);

  uic_WeatherSearchTextArea = lv_textarea_create(uic_WeatherSearchPanel);
  if (uic_WeatherSearchTextArea == NULL) {
    gWeatherAppVisible = false;
    return;
  }
  lv_obj_set_size(uic_WeatherSearchTextArea, 740, LV_SIZE_CONTENT);
  lv_obj_align(uic_WeatherSearchTextArea, LV_ALIGN_TOP_MID, 0, 15);
  lv_textarea_set_placeholder_text(uic_WeatherSearchTextArea, "Search for a place ...");
  lv_textarea_set_one_line(uic_WeatherSearchTextArea, true);
  lv_textarea_set_align(uic_WeatherSearchTextArea, LV_TEXT_ALIGN_LEFT);
  lv_obj_set_style_bg_color(uic_WeatherSearchTextArea, lv_color_hex(0x172032), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_WeatherSearchTextArea, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(uic_WeatherSearchTextArea, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(uic_WeatherSearchTextArea, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

  uic_WeatherSearchDropdown = lv_dropdown_create(uic_WeatherSearchPanel);
  if (uic_WeatherSearchDropdown == NULL) {
    gWeatherAppVisible = false;
    return;
  }
  lv_obj_set_size(uic_WeatherSearchDropdown, 740, LV_SIZE_CONTENT);
  lv_obj_align(uic_WeatherSearchDropdown, LV_ALIGN_TOP_MID, 0, 95);
  lv_dropdown_set_options(uic_WeatherSearchDropdown, "Current Location");
  ui_object_set_themeable_style_property(lv_dropdown_get_list(uic_WeatherSearchDropdown),  LV_PART_SELECTED| LV_STATE_CHECKED, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
  ui_object_set_themeable_style_property(lv_dropdown_get_list(uic_WeatherSearchDropdown),  LV_PART_SELECTED| LV_STATE_CHECKED, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);
  lv_obj_set_style_text_color(uic_WeatherSearchDropdown, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(uic_WeatherSearchDropdown, &lv_font_montserrat_20, LV_PART_MAIN| LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(uic_WeatherSearchDropdown, &lv_font_montserrat_20, LV_PART_INDICATOR| LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(lv_dropdown_get_list(uic_WeatherSearchDropdown), &lv_font_montserrat_20,  LV_PART_MAIN| LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(lv_dropdown_get_list(uic_WeatherSearchDropdown), &lv_font_montserrat_20,  LV_PART_SELECTED| LV_STATE_DEFAULT);

  uic_WeatherSearchConfirmButton = lv_btn_create(uic_WeatherSearchPanel);
  if (uic_WeatherSearchConfirmButton == NULL) {
    gWeatherAppVisible = false;
    return;
  }
  lv_obj_set_size(uic_WeatherSearchConfirmButton, 220, 70);
  lv_obj_align(uic_WeatherSearchConfirmButton, LV_ALIGN_BOTTOM_MID, 0, -16);
  lv_obj_set_style_radius(uic_WeatherSearchConfirmButton, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
/*   lv_obj_set_style_bg_color(uic_WeatherSearchConfirmButton, lv_color_hex(0x1A82FF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_WeatherSearchConfirmButton, 255, LV_PART_MAIN | LV_STATE_DEFAULT); */
  lv_obj_set_style_border_width(uic_WeatherSearchConfirmButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t * confirmLabel = lv_label_create(uic_WeatherSearchConfirmButton);
  lv_label_set_text(confirmLabel, "Confirm");
  lv_obj_set_style_text_font(confirmLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(confirmLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(confirmLabel);
  lv_obj_add_event_cb(uic_WeatherSearchConfirmButton, weatherSearchConfirmEvent, LV_EVENT_CLICKED, NULL);

  uic_WeatherSearchKeyboard = lv_keyboard_create(uic_AppContentArea);
  if (uic_WeatherSearchKeyboard == NULL) {
    gWeatherAppVisible = false;
    return;
  }
  lv_obj_set_size(uic_WeatherSearchKeyboard, 793, 190);
  lv_obj_align(uic_WeatherSearchKeyboard, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(uic_WeatherSearchKeyboard, weatherSearchKeyboardEvent, LV_EVENT_ALL, NULL);
  lv_obj_add_flag(uic_WeatherSearchKeyboard, LV_OBJ_FLAG_HIDDEN);

  if (ui_HomeButton9 != NULL) {
    lv_obj_move_foreground(ui_HomeButton9);
  }

  updateWeatherSearchDropdownOptions();
  refreshWeatherDataIfNeeded(true);
}

static String emailReadLine(WiFiClientSecure& client, unsigned long timeoutMs = 10000)
{
  String line;
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (client.available()) {
      char c = (char)client.read();
      if (c == '\r') continue;
      if (c == '\n') return line;
      line += c;
      if (line.length() > 512) return line;
    }
    if (!client.connected() && !client.available()) break;
    delay(2);
  }
  return line;
}

static bool emailReadExpectedPrefix(WiFiClientSecure& client, const char * prefix, String * answer = NULL)
{
  String line = emailReadLine(client);
  if (answer != NULL) *answer = line;
  if (prefix == NULL) return line.length() > 0;
  return line.startsWith(prefix);
}

static bool smtpExpectCode(WiFiClientSecure& client, int expectedCode, String * answer = NULL)
{
  bool gotExpected = false;
  String lastLine;
  unsigned long start = millis();
  do {
    lastLine = emailReadLine(client);
    if (lastLine.length() >= 3 && lastLine.substring(0, 3).toInt() == expectedCode) {
      gotExpected = true;
    }
    if (millis() - start > 15000) break;
  } while (lastLine.length() >= 4 && lastLine.charAt(3) == '-');

  if (answer != NULL) *answer = lastLine;
  return gotExpected;
}

static String emailBase64Encode(const String& input)
{
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  int val = 0;
  int valb = -6;
  for (size_t i = 0; i < input.length(); i++) {
    val = (val << 8) + (uint8_t)input[i];
    valb += 8;
    while (valb >= 0) {
      out += table[(val >> valb) & 0x3F];
      valb -= 6;
    }
  }
  if (valb > -6) out += table[((val << 8) >> (valb + 8)) & 0x3F];
  while (out.length() % 4) out += '=';
  return out;
}

static int emailBase64Value(char c)
{
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static String emailBase64Decode(const String& input)
{
  String out;
  int val = 0;
  int valb = -8;
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '=') break;
    int d = emailBase64Value(c);
    if (d < 0) continue;
    val = (val << 6) + d;
    valb += 6;
    if (valb >= 0) {
      out += (char)((val >> valb) & 0xFF);
      valb -= 8;
    }
  }
  return out;
}

static String emailDecodeQuotedPrintableWord(const String& input)
{
  String out;
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '_') {
      out += ' ';
    } else if (c == '=' && i + 2 < input.length()) {
      char h1 = input[i + 1];
      char h2 = input[i + 2];
      if (isxdigit(h1) && isxdigit(h2)) {
        char hex[3] = { h1, h2, 0 };
        out += (char)strtol(hex, NULL, 16);
        i += 2;
      } else {
        out += c;
      }
    } else {
      out += c;
    }
  }
  return out;
}

static String emailDecodeMimeHeader(const String& input)
{
  String out;
  int pos = 0;
  while (pos < (int)input.length()) {
    int start = input.indexOf("=?", pos);
    if (start < 0) {
      out += input.substring(pos);
      break;
    }
    out += input.substring(pos, start);
    int q1 = input.indexOf('?', start + 2);
    int q2 = input.indexOf('?', q1 + 1);
    int end = input.indexOf("?=", q2 + 1);
    if (q1 < 0 || q2 < 0 || end < 0) {
      out += input.substring(start);
      break;
    }
    String encoding = input.substring(q1 + 1, q2);
    String payload = input.substring(q2 + 1, end);
    encoding.toUpperCase();
    if (encoding == "B") {
      out += emailBase64Decode(payload);
    } else if (encoding == "Q") {
      out += emailDecodeQuotedPrintableWord(payload);
    } else {
      out += payload;
    }
    pos = end + 2;
    while (pos < (int)input.length() && input[pos] == ' ') pos++;
  }
  out.trim();
  return out;
}

static String emailSanitizeFileName(const String& value)
{
  String out;
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_') {
      out += c;
    } else {
      out += '_';
    }
    if (out.length() >= 80) break;
  }
  if (out.length() == 0) out = String(millis());
  return out;
}

static String emailTrimForRoller(String value, int maxLen)
{
  value.replace("\r", " ");
  value.replace("\n", " ");
  value.trim();
  if ((int)value.length() > maxLen) {
    value = value.substring(0, maxLen - 3) + "...";
  }
  return value;
}

static void resetEmailAppState()
{
  gEmailUiVisible = false;
  gEmailFetchRunning = false;
  gEmailMessageCount = 0;
  gOutgoingEmailMessageCount = 0;
  if (ui_Email != NULL) {
    ui_Email_screen_destroy();
  }
}

static bool ensureEmailDirectories()
{
  if (!sd_ok) return false;

  if (!SD.exists("/system")) SD.mkdir("/system");
  if (!SD.exists(EMAIL_SYSTEM_DIR)) SD.mkdir(EMAIL_SYSTEM_DIR);
  if (!SD.exists(EMAIL_DIR)) SD.mkdir(EMAIL_DIR);
  if (!SD.exists(EMAIL_INBOX_DIR)) SD.mkdir(EMAIL_INBOX_DIR);
  if (!SD.exists(EMAIL_OUTGOING_DIR)) SD.mkdir(EMAIL_OUTGOING_DIR);

  return SD.exists(EMAIL_INBOX_DIR) && SD.exists(EMAIL_OUTGOING_DIR);
}

static void ensureEmailSdApp()
{
  if (!sd_ok) return;
  if (!ensureAppsDirectory()) return;

  const char * appDir = "/apps/email";
  const char * appCfg = "/apps/email/app.cfg";
  if (!SD.exists(appDir)) {
    SD.mkdir(appDir);
  }
  if (!SD.exists(appCfg)) {
    File f = SD.open(appCfg, FILE_WRITE);
    if (f) {
      f.println("name=E-Mail");
      f.println("icon=@");
      f.println("type=email");
      f.println("scrollable=false");
      f.close();
    }
  }
}

static bool emailParseBool(String value, bool defaultValue)
{
  value.trim();
  value.toLowerCase();
  if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
  if (value == "0" || value == "false" || value == "no" || value == "off") return false;
  return defaultValue;
}

static void emailParseServerValue(String value, String * host, uint16_t * port, bool * ssl, bool * startTls)
{
  value.trim();
  if (value.length() == 0) return;

  String lower = value;
  lower.toLowerCase();
  if (lower.startsWith("smtps://") || lower.startsWith("imaps://") || lower.startsWith("pop3s://") || lower.startsWith("ssl://")) {
    int schemeEnd = value.indexOf("://");
    value = value.substring(schemeEnd + 3);
    if (ssl != NULL) *ssl = true;
    if (startTls != NULL) *startTls = false;
  } else if (lower.startsWith("smtp://") || lower.startsWith("imap://") || lower.startsWith("pop3://")) {
    int schemeEnd = value.indexOf("://");
    value = value.substring(schemeEnd + 3);
  } else if (lower.startsWith("starttls://") || lower.startsWith("tls://")) {
    int schemeEnd = value.indexOf("://");
    value = value.substring(schemeEnd + 3);
    if (ssl != NULL) *ssl = false;
    if (startTls != NULL) *startTls = true;
  }

  int slash = value.indexOf('/');
  if (slash >= 0) value = value.substring(0, slash);
  value.trim();

  int colon = value.lastIndexOf(':');
  if (colon > 0 && value.indexOf(':') == colon) {
    String portText = value.substring(colon + 1);
    portText.trim();
    int parsedPort = portText.toInt();
    if (parsedPort > 0 && parsedPort <= 65535) {
      value = value.substring(0, colon);
      value.trim();
      if (port != NULL) *port = (uint16_t)parsedPort;
    }
  }

  if (host != NULL) *host = value;
}

static int emailKeyValueSeparator(const String& line)
{
  const int equals = line.indexOf('=');
  const int colon = line.indexOf(':');
  int separator = -1;

  if (equals > 0) separator = equals;
  if (colon > 0 && (separator < 0 || colon < separator)) separator = colon;

  // In the legacy one-line format, ':' can occur later in a server URL or
  // host:port value. Only accept a key/value separator before the first '|'.
  const int pipe = line.indexOf('|');
  if (pipe >= 0 && (separator < 0 || pipe < separator)) return -1;
  return separator;
}

static void applyEmailLoginKeyValue(EmailLoginConfig * cfg, String key, String value)
{
  if (cfg == NULL) return;
  key.trim();
  value.trim();
  key.toLowerCase();

  if (key == "protocol" || key == "incoming_protocol") {
    value.toLowerCase();
    cfg->protocol = value;
  } else if (key == "email" || key == "address" || key == "mail") cfg->email = value;
  else if (key == "from_email" || key == "sender_email" || key == "from") cfg->fromEmail = value;
  else if (key == "reply_to" || key == "replyto") cfg->replyTo = value;
  else if (key == "user" || key == "username" || key == "login") cfg->user = value;
  else if (key == "password" || key == "pass") {
    String decrypted;
    if (decodeStoredCredential(value, &decrypted)) cfg->password = decrypted;
    else Serial.println("[SECURITY] E-Mail-Passwort kann auf diesem Gerät nicht entschlüsselt werden.");
    SecureCredentials::clear(&decrypted);
  }
  else if (key == "incoming_server") {
    emailParseServerValue(value, &cfg->pop3Server, &cfg->pop3Port, &cfg->pop3Ssl, NULL);
    emailParseServerValue(value, &cfg->imapServer, &cfg->imapPort, &cfg->imapSsl, NULL);
  }
  else if (key == "pop3_server" || key == "pop_server") emailParseServerValue(value, &cfg->pop3Server, &cfg->pop3Port, &cfg->pop3Ssl, NULL);
  else if (key == "imap_server" || key == "imap_host") emailParseServerValue(value, &cfg->imapServer, &cfg->imapPort, &cfg->imapSsl, NULL);
  else if (key == "incoming_port") {
    cfg->pop3Port = value.toInt();
    cfg->imapPort = value.toInt();
  }
  else if (key == "pop3_port" || key == "pop_port") cfg->pop3Port = value.toInt();
  else if (key == "imap_port") cfg->imapPort = value.toInt();
  else if (key == "imap_folder" || key == "imap_mailbox" || key == "mailbox") cfg->imapFolder = value;
  else if (key == "smtp_server" || key == "outgoing_server") emailParseServerValue(value, &cfg->smtpServer, &cfg->smtpPort, &cfg->smtpSsl, &cfg->smtpStartTls);
  else if (key == "smtp_port" || key == "outgoing_port") cfg->smtpPort = value.toInt();
  else if (key == "name" || key == "sender_name") cfg->senderName = value;
  else if (key == "pop3_ssl") {
    cfg->pop3Ssl = emailParseBool(value, true);
  } else if (key == "imap_ssl") {
    cfg->imapSsl = emailParseBool(value, true);
  } else if (key == "smtp_ssl") {
    cfg->smtpSsl = emailParseBool(value, true);
  } else if (key == "smtp_starttls" || key == "smtp_tls" || key == "starttls") {
    cfg->smtpStartTls = emailParseBool(value, false);
    if (cfg->smtpStartTls) cfg->smtpSsl = false;
  } else if (key == "allow_technical_sender") {
    cfg->allowTechnicalSender = emailParseBool(value, false);
  }
}

static bool migrateEmailPasswordIfNeeded()
{
  recoverCredentialFileIfNeeded(EMAIL_LOGIN_FILE, EMAIL_LOGIN_BACKUP);
  if (!sd_ok || !SD.exists(EMAIL_LOGIN_FILE)) return false;

  bool needsMigration = false;
  File inspection = SD.open(EMAIL_LOGIN_FILE, FILE_READ);
  if (!inspection) return false;
  while (inspection.available() && !needsMigration) {
    String line = inspection.readStringUntil('\n');
    line.replace("\r", "");
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;

    int separator = emailKeyValueSeparator(line);
    if (separator > 0) {
      String key = line.substring(0, separator);
      key.trim();
      key.toLowerCase();
      if (key == "password" || key == "pass") {
        String value = line.substring(separator + 1);
        value.trim();
        needsMigration = value.length() > 0 && !SecureCredentials::isEncrypted(value);
        SecureCredentials::clear(&value);
      }
    } else {
      int first = line.indexOf('|');
      int second = first < 0 ? -1 : line.indexOf('|', first + 1);
      int third = second < 0 ? -1 : line.indexOf('|', second + 1);
      if (second >= 0 && third > second) {
        String value = line.substring(second + 1, third);
        value.trim();
        needsMigration = value.length() > 0 && !SecureCredentials::isEncrypted(value);
        SecureCredentials::clear(&value);
      }
    }
  }
  inspection.close();
  if (!needsMigration) return true;

  File source = SD.open(EMAIL_LOGIN_FILE, FILE_READ);
  if (!source) return false;
  if (SD.exists(EMAIL_LOGIN_TEMP)) SD.remove(EMAIL_LOGIN_TEMP);
  File target = SD.open(EMAIL_LOGIN_TEMP, FILE_WRITE);
  if (!target) {
    source.close();
    return false;
  }

  bool migrationNeeded = false;
  bool migrationFailed = false;
  while (source.available()) {
    String originalLine = source.readStringUntil('\n');
    originalLine.replace("\r", "");
    String line = originalLine;
    line.trim();

    bool handled = false;
    if (line.length() > 0 && !line.startsWith("#")) {
      int separator = emailKeyValueSeparator(line);
      if (separator > 0) {
        String key = line.substring(0, separator);
        key.trim();
        key.toLowerCase();
        if (key == "password" || key == "pass") {
          String storedPassword = line.substring(separator + 1);
          storedPassword.trim();
          if (storedPassword.length() > 0 && !SecureCredentials::isEncrypted(storedPassword)) {
            String encryptedPassword;
            if (SecureCredentials::encrypt(storedPassword, &encryptedPassword)) {
              int originalSeparator = emailKeyValueSeparator(originalLine);
              target.print(originalLine.substring(0, originalSeparator + 1));
              target.println(encryptedPassword);
              migrationNeeded = true;
              handled = true;
            } else {
              migrationFailed = true;
            }
            SecureCredentials::clear(&encryptedPassword);
            SecureCredentials::clear(&storedPassword);
          }
        }
      } else {
        // Legacy one-line format: email|user|password|incoming|port|smtp|port
        int first = line.indexOf('|');
        int second = first < 0 ? -1 : line.indexOf('|', first + 1);
        int third = second < 0 ? -1 : line.indexOf('|', second + 1);
        if (second >= 0 && third > second) {
          String storedPassword = line.substring(second + 1, third);
          storedPassword.trim();
          if (storedPassword.length() > 0 && !SecureCredentials::isEncrypted(storedPassword)) {
            String encryptedPassword;
            if (SecureCredentials::encrypt(storedPassword, &encryptedPassword)) {
              target.print(line.substring(0, second + 1));
              target.print(encryptedPassword);
              target.println(line.substring(third));
              migrationNeeded = true;
              handled = true;
            } else {
              migrationFailed = true;
            }
            SecureCredentials::clear(&encryptedPassword);
            SecureCredentials::clear(&storedPassword);
          }
        }
      }
    }

    if (!handled) target.println(originalLine);
  }

  target.flush();
  target.close();
  source.close();

  if (!migrationNeeded || migrationFailed) {
    SD.remove(EMAIL_LOGIN_TEMP);
    if (migrationFailed) Serial.println("[SECURITY] Klartext-E-Mail-Passwort konnte nicht migriert werden.");
    return !migrationFailed;
  }

  const bool installed = installMigratedCredentialFile(EMAIL_LOGIN_FILE, EMAIL_LOGIN_TEMP, EMAIL_LOGIN_BACKUP);
  Serial.println(installed
    ? "[SECURITY] E-Mail-Passwort auf der SD-Karte verschlüsselt."
    : "[SECURITY] Migration des E-Mail-Passworts konnte nicht abgeschlossen werden.");
  return installed;
}

static bool loadEmailLoginConfig(EmailLoginConfig * cfg)
{
  if (cfg == NULL) return false;
  cfg->protocol = "pop3";
  cfg->email = "";
  cfg->user = "";
  cfg->password = "";
  cfg->pop3Server = "";
  cfg->imapServer = "";
  cfg->smtpServer = "";
  cfg->fromEmail = "";
  cfg->replyTo = "";
  cfg->senderName = "fOS";
  cfg->imapFolder = "INBOX";
  cfg->pop3Port = 995;
  cfg->imapPort = 993;
  cfg->smtpPort = 465;
  cfg->pop3Ssl = true;
  cfg->imapSsl = true;
  cfg->smtpSsl = true;
  cfg->smtpStartTls = false;
  cfg->allowTechnicalSender = false;

  if (!sd_ok) return false;
  if (!migrateEmailPasswordIfNeeded()) {
    Serial.println("[SECURITY] E-Mail-Konfiguration wird wegen fehlgeschlagener Passwortmigration nicht verwendet.");
    return false;
  }
  if (!SD.exists(EMAIL_LOGIN_FILE)) return false;
  File f = SD.open(EMAIL_LOGIN_FILE, FILE_READ);
  if (!f) return false;

  bool foundKeyValue = false;
  String flatLine = "";
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;
    int sep = emailKeyValueSeparator(line);
    if (sep > 0) {
      foundKeyValue = true;
      applyEmailLoginKeyValue(cfg, line.substring(0, sep), line.substring(sep + 1));
    } else {
      if (flatLine.length() > 0) flatLine += "|";
      flatLine += line;
    }
  }
  f.close();

  if (!foundKeyValue && flatLine.length() > 0) {
    String parts[7];
    int count = 0;
    int start = 0;
    while (count < 7) {
      int sep = flatLine.indexOf('|', start);
      if (sep < 0) sep = flatLine.length();
      parts[count++] = flatLine.substring(start, sep);
      if (sep >= (int)flatLine.length()) break;
      start = sep + 1;
    }
    if (count >= 6) {
      cfg->email = parts[0];
      cfg->user = parts[1];
      String decrypted;
      if (decodeStoredCredential(parts[2], &decrypted)) cfg->password = decrypted;
      else Serial.println("[SECURITY] E-Mail-Passwort kann auf diesem Gerät nicht entschlüsselt werden.");
      SecureCredentials::clear(&decrypted);
      emailParseServerValue(parts[3], &cfg->pop3Server, &cfg->pop3Port, &cfg->pop3Ssl, NULL);
      emailParseServerValue(parts[3], &cfg->imapServer, &cfg->imapPort, &cfg->imapSsl, NULL);
      cfg->pop3Port = parts[4].toInt();
      cfg->imapPort = parts[4].toInt();
      emailParseServerValue(parts[5], &cfg->smtpServer, &cfg->smtpPort, &cfg->smtpSsl, &cfg->smtpStartTls);
      if (count >= 7) cfg->smtpPort = parts[6].toInt();
    }
  }

  if (cfg->user.length() == 0) cfg->user = cfg->email;
  if (cfg->pop3Port == 0) cfg->pop3Port = 995;
  if (cfg->imapPort == 0) cfg->imapPort = 993;
  if (cfg->smtpPort == 0) cfg->smtpPort = 465;
  if (cfg->smtpPort == 587) {
    cfg->smtpStartTls = true;
    cfg->smtpSsl = false;
  } else if (cfg->smtpPort == 465 && !cfg->smtpStartTls) {
    cfg->smtpSsl = true;
  }
  cfg->protocol.toLowerCase();
  if (cfg->protocol != "imap") cfg->protocol = "pop3";
  if (cfg->imapServer.length() == 0 && cfg->protocol == "imap") cfg->imapServer = cfg->pop3Server;
  if (cfg->pop3Server.length() == 0 && cfg->protocol == "pop3") cfg->pop3Server = cfg->imapServer;
  if (cfg->imapFolder.length() == 0) cfg->imapFolder = "INBOX";

  bool incomingOk = (cfg->protocol == "imap")
    ? (cfg->imapServer.length() > 0)
    : (cfg->pop3Server.length() > 0);

  return cfg->email.length() > 0 &&
         cfg->user.length() > 0 &&
         cfg->password.length() > 0 &&
         incomingOk &&
         cfg->smtpServer.length() > 0;
}

static bool parseCachedEmailFile(const String& path, EmailMessageEntry * msg)
{
  if (msg == NULL) return false;
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) return false;

  msg->uid = "";
  msg->filePath = path;
  msg->from = "";
  msg->to = "";
  msg->subject = "";
  msg->date = "";
  msg->status = "";
  msg->body = "";
  bool inBody = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.replace("\r", "");
    if (inBody) {
      if (msg->body.length() < EMAIL_MAX_BODY_CHARS) {
        msg->body += line;
        msg->body += "\n";
      }
      continue;
    }
    if (line == "body:") {
      inBody = true;
      continue;
    }
    int sep = line.indexOf(':');
    if (sep <= 0) continue;
    String key = line.substring(0, sep);
    String value = line.substring(sep + 1);
    key.trim();
    value.trim();
    if (key == "uid") msg->uid = value;
    else if (key == "from") msg->from = value;
    else if (key == "to") msg->to = value;
    else if (key == "subject") msg->subject = value;
    else if (key == "date") msg->date = value;
    else if (key == "status") msg->status = value;
  }
  f.close();

  if (msg->from.length() == 0 && msg->to.length() > 0) msg->from = "To: " + msg->to;
  if (msg->from.length() == 0) msg->from = "(unknown)";
  if (msg->subject.length() == 0) msg->subject = "(no subject)";
  return true;
}

static int emailMonthNumber(const char * monthText)
{
  if (monthText == NULL) return 0;
  static const char * months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  String month(monthText);
  for (int i = 0; i < 12; i++) {
    if (month.equalsIgnoreCase(months[i])) return i + 1;
  }
  return 0;
}

static int64_t emailDaysFromCivil(int year, unsigned month, unsigned day)
{
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = (unsigned)(year - era * 400);
  const unsigned adjustedMonth = month > 2 ? month - 3 : month + 9;
  const unsigned dayOfYear = (153 * adjustedMonth + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return (int64_t)era * 146097 + (int64_t)dayOfEra - 719468;
}

static int emailTimeZoneOffsetMinutes(const char * zoneText)
{
  if (zoneText == NULL || zoneText[0] == '\0') return 0;

  String zone(zoneText);
  zone.trim();
  zone.toUpperCase();
  if (zone == "UT" || zone == "UTC" || zone == "GMT") return 0;
  if (zone == "EST") return -5 * 60;
  if (zone == "EDT") return -4 * 60;
  if (zone == "CST") return -6 * 60;
  if (zone == "CDT") return -5 * 60;
  if (zone == "MST") return -7 * 60;
  if (zone == "MDT") return -6 * 60;
  if (zone == "PST") return -8 * 60;
  if (zone == "PDT") return -7 * 60;

  if (zone[0] != '+' && zone[0] != '-') return 0;
  String digits = zone.substring(1);
  digits.replace(":", "");
  if (digits.length() < 4) return 0;
  for (int i = 0; i < 4; i++) {
    if (!isdigit((unsigned char)digits[i])) return 0;
  }
  int hours = digits.substring(0, 2).toInt();
  int minutes = digits.substring(2, 4).toInt();
  if (hours > 23 || minutes > 59) return 0;
  int offset = hours * 60 + minutes;
  return zone[0] == '-' ? -offset : offset;
}

static int64_t emailDateSortKey(String date)
{
  date.trim();
  int comma = date.indexOf(',');
  if (comma >= 0) {
    date = date.substring(comma + 1);
    date.trim();
  }

  int day = 0;
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  char monthText[4] = { 0 };
  char zoneText[8] = { 0 };

  int fields = sscanf(
    date.c_str(),
    "%d %3s %d %d:%d:%d %7s",
    &day, monthText, &year, &hour, &minute, &second, zoneText
  );
  if (fields < 6) {
    second = 0;
    zoneText[0] = '\0';
    fields = sscanf(
      date.c_str(),
      "%d %3s %d %d:%d %7s",
      &day, monthText, &year, &hour, &minute, zoneText
    );
    if (fields < 5) return 0;
  }

  if (year < 100) year += year >= 70 ? 1900 : 2000;
  int month = emailMonthNumber(monthText);
  if (year < 1970 || month < 1 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
      second < 0 || second > 60) {
    return 0;
  }

  int64_t timestamp = emailDaysFromCivil(year, (unsigned)month, (unsigned)day) * 86400LL;
  timestamp += (int64_t)hour * 3600LL + (int64_t)minute * 60LL + second;
  timestamp -= (int64_t)emailTimeZoneOffsetMinutes(zoneText) * 60LL;
  return timestamp;
}

static uint64_t emailNumericFallbackKey(const EmailMessageEntry& msg)
{
  String value = msg.uid.length() > 0 ? msg.uid : msg.filePath;
  int end = (int)value.length() - 1;
  while (end >= 0 && !isdigit((unsigned char)value[end])) end--;
  if (end < 0) return 0;
  int start = end;
  while (start > 0 && isdigit((unsigned char)value[start - 1])) start--;
  return strtoull(value.substring(start, end + 1).c_str(), NULL, 10);
}

static bool emailMessageIsNewer(const EmailMessageEntry& left, const EmailMessageEntry& right)
{
  int64_t leftDate = emailDateSortKey(left.date);
  int64_t rightDate = emailDateSortKey(right.date);
  if (leftDate != rightDate) return leftDate > rightDate;

  uint64_t leftFallback = emailNumericFallbackKey(left);
  uint64_t rightFallback = emailNumericFallbackKey(right);
  if (leftFallback != rightFallback) return leftFallback > rightFallback;
  return left.filePath.compareTo(right.filePath) > 0;
}

static void sortEmailMessagesNewestFirst(EmailMessageEntry * messages, int count)
{
  if (messages == NULL || count < 2) return;
  for (int i = 1; i < count; i++) {
    EmailMessageEntry current = messages[i];
    int insertAt = i;
    while (insertAt > 0 && emailMessageIsNewer(current, messages[insertAt - 1])) {
      messages[insertAt] = messages[insertAt - 1];
      insertAt--;
    }
    messages[insertAt] = current;
  }
}

static void loadEmailMessagesFromDir(const char * directoryPath, EmailMessageEntry * messages, int * count)
{
  if (messages == NULL || count == NULL) return;
  *count = 0;
  if (!ensureEmailDirectories()) return;

  File dir = SD.open(directoryPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String path = String(directoryPath) + "/" + pathBasename(String(entry.name()));
      EmailMessageEntry msg;
      if (parseCachedEmailFile(path, &msg)) {
        if (*count < MAX_EMAIL_MESSAGES) {
          messages[(*count)++] = msg;
        } else {
          int oldestIndex = 0;
          for (int i = 1; i < *count; i++) {
            if (emailMessageIsNewer(messages[oldestIndex], messages[i])) {
              oldestIndex = i;
            }
          }
          if (emailMessageIsNewer(msg, messages[oldestIndex])) {
            messages[oldestIndex] = msg;
          }
        }
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  sortEmailMessagesNewestFirst(messages, *count);
}

static void loadCachedEmails()
{
  loadEmailMessagesFromDir(EMAIL_INBOX_DIR, gEmailMessages, &gEmailMessageCount);
}

static void loadCachedOutgoingEmails()
{
  loadEmailMessagesFromDir(EMAIL_OUTGOING_DIR, gOutgoingEmailMessages, &gOutgoingEmailMessageCount);
}

static void fillEmailRoller(lv_obj_t * roller, EmailMessageEntry * messages, int messageCount, const char * emptyText, bool outgoing)
{
  if (roller == NULL) return;

  String options;
  if (!sd_ok) {
    options = "SD card missing";
  } else if (messageCount == 0) {
    options = emptyText;
  } else {
    for (int i = 0; i < messageCount; i++) {
      String date = emailTrimForRoller(messages[i].date, 16);
      String address = outgoing && messages[i].to.length() > 0 ? messages[i].to : messages[i].from;
      String from = emailTrimForRoller(address, 30);
      String subject = emailTrimForRoller(messages[i].subject, 46);
      if (date.length() == 0) date = "--";
      if (outgoing && messages[i].status.length() > 0) {
        options += date + " | " + messages[i].status + " | " + from + " | " + subject;
      } else {
        options += date + " | " + from + " | " + subject;
      }
      if (i + 1 < messageCount) options += "\n";
    }
  }

  lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(roller, 0, LV_ANIM_OFF);
}

static void fillEmailRollers()
{
  fillEmailRoller(uic_EmailRollerInbox, gEmailMessages, gEmailMessageCount, "No cached emails", false);
  fillEmailRoller(uic_EmailRollerOutgoing, gOutgoingEmailMessages, gOutgoingEmailMessageCount, "No outgoing emails", true);
}

static void emailApplyHeaderLine(EmailMessageEntry * msg, const String& headerLine)
{
  if (msg == NULL) return;
  int sep = headerLine.indexOf(':');
  if (sep <= 0) return;

  String key = headerLine.substring(0, sep);
  String value = headerLine.substring(sep + 1);
  key.trim();
  key.toLowerCase();
  value.trim();
  value = emailDecodeMimeHeader(value);

  if (key == "from") msg->from = value;
  else if (key == "subject") msg->subject = value;
  else if (key == "date") msg->date = value;
}

static int emailFindHeaderBodySplit(const String& raw)
{
  int split = raw.indexOf("\n\n");
  return split < 0 ? -1 : split;
}

static String emailHeaderValue(const String& headers, const String& wantedKey)
{
  String folded;
  String current;
  int start = 0;
  while (start <= (int)headers.length()) {
    int end = headers.indexOf('\n', start);
    if (end < 0) end = headers.length();
    String line = headers.substring(start, end);
    line.replace("\r", "");

    if (line.length() > 0 && (line[0] == ' ' || line[0] == '\t')) {
      current += " ";
      current += line;
    } else {
      if (current.length() > 0) {
        if (folded.length() > 0) folded += "\n";
        folded += current;
      }
      current = line;
    }

    if (end >= (int)headers.length()) break;
    start = end + 1;
  }
  if (current.length() > 0) {
    if (folded.length() > 0) folded += "\n";
    folded += current;
  }

  String wanted = wantedKey;
  wanted.toLowerCase();
  start = 0;
  while (start <= (int)folded.length()) {
    int end = folded.indexOf('\n', start);
    if (end < 0) end = folded.length();
    String line = folded.substring(start, end);
    int sep = line.indexOf(':');
    if (sep > 0) {
      String key = line.substring(0, sep);
      key.trim();
      key.toLowerCase();
      if (key == wanted) {
        String value = line.substring(sep + 1);
        value.trim();
        return value;
      }
    }
    if (end >= (int)folded.length()) break;
    start = end + 1;
  }
  return "";
}

static String emailDecodeQuotedPrintableBody(const String& input)
{
  String out;
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '=' && i + 1 < input.length()) {
      char n1 = input[i + 1];
      if (n1 == '\n') {
        i += 1;
        continue;
      }
      if (n1 == '\r' && i + 2 < input.length() && input[i + 2] == '\n') {
        i += 2;
        continue;
      }
      if (i + 2 < input.length() && isxdigit((unsigned char)n1) && isxdigit((unsigned char)input[i + 2])) {
        char hex[3] = { n1, input[i + 2], 0 };
        out += (char)strtol(hex, NULL, 16);
        i += 2;
        continue;
      }
    }
    if (c != '\r') out += c;
  }
  return out;
}

static String emailDecodeTransferBody(String body, String encoding)
{
  encoding.trim();
  encoding.toLowerCase();
  if (encoding == "base64") {
    return emailBase64Decode(body);
  }
  if (encoding == "quoted-printable") {
    return emailDecodeQuotedPrintableBody(body);
  }
  body.replace("\r", "");
  return body;
}

static String emailDecodeHtmlEntity(const String& entity)
{
  if (entity == "amp") return "&";
  if (entity == "lt") return "<";
  if (entity == "gt") return ">";
  if (entity == "quot") return "\"";
  if (entity == "apos") return "'";
  if (entity == "nbsp") return " ";
  if (entity.startsWith("#x") || entity.startsWith("#X")) {
    long value = strtol(entity.substring(2).c_str(), NULL, 16);
    if (value > 0 && value < 128) {
      String out;
      out += (char)value;
      return out;
    }
  } else if (entity.startsWith("#")) {
    long value = strtol(entity.substring(1).c_str(), NULL, 10);
    if (value > 0 && value < 128) {
      String out;
      out += (char)value;
      return out;
    }
  }
  return "&" + entity + ";";
}

static String emailHtmlToText(const String& html)
{
  String out;
  bool inTag = false;
  bool lastWasSpace = false;
  for (size_t i = 0; i < html.length(); i++) {
    char c = html[i];
    if (c == '<') {
      String tag;
      size_t j = i + 1;
      while (j < html.length() && j < i + 14 && html[j] != '>' && !isspace((unsigned char)html[j])) {
        tag += html[j++];
      }
      tag.toLowerCase();
      if (tag == "br" || tag == "br/" || tag == "p" || tag == "/p" || tag == "div" || tag == "/div" || tag == "li" || tag == "/li") {
        if (out.length() > 0 && out[out.length() - 1] != '\n') out += "\n";
        lastWasSpace = false;
      }
      inTag = true;
      continue;
    }
    if (inTag) {
      if (c == '>') inTag = false;
      continue;
    }
    if (c == '&') {
      int semi = html.indexOf(';', i + 1);
      if (semi > (int)i && semi - (int)i <= 10) {
        out += emailDecodeHtmlEntity(html.substring(i + 1, semi));
        i = semi;
        lastWasSpace = false;
        continue;
      }
    }
    if (c == '\r') continue;
    if (c == '\n' || c == '\t' || c == ' ') {
      if (!lastWasSpace) {
        out += (c == '\n') ? '\n' : ' ';
        lastWasSpace = true;
      }
    } else {
      out += c;
      lastWasSpace = false;
    }
  }
  out.trim();
  return out;
}

static String emailBoundaryFromContentType(String contentType)
{
  String lower = contentType;
  lower.toLowerCase();
  int pos = lower.indexOf("boundary=");
  if (pos < 0) return "";
  pos += 9;
  while (pos < (int)contentType.length() && contentType[pos] == ' ') pos++;

  String boundary;
  if (pos < (int)contentType.length() && contentType[pos] == '"') {
    pos++;
    int end = contentType.indexOf('"', pos);
    if (end < 0) end = contentType.length();
    boundary = contentType.substring(pos, end);
  } else {
    int end = pos;
    while (end < (int)contentType.length() && contentType[end] != ';' && contentType[end] != '\r' && contentType[end] != '\n') end++;
    boundary = contentType.substring(pos, end);
  }
  boundary.trim();
  return boundary;
}

static String emailNormalizeDisplayBody(String body)
{
  body.replace("\r", "");
  body.replace("\t", " ");
  while (body.indexOf("\n\n\n") >= 0) body.replace("\n\n\n", "\n\n");
  body.trim();
  if (body.length() > EMAIL_MAX_BODY_CHARS) {
    body = body.substring(0, EMAIL_MAX_BODY_CHARS - 16);
    body += "\n...(truncated)";
  }
  return body;
}

static void emailExtractMimeText(const String& headers, const String& content, String * plain, String * html)
{
  String contentType = emailHeaderValue(headers, "Content-Type");
  String transferEncoding = emailHeaderValue(headers, "Content-Transfer-Encoding");
  String disposition = emailHeaderValue(headers, "Content-Disposition");
  String lowerType = contentType;
  String lowerDisposition = disposition;
  lowerType.toLowerCase();
  lowerDisposition.toLowerCase();

  if (lowerDisposition.startsWith("attachment")) return;

  if (lowerType.indexOf("multipart/") >= 0) {
    String boundary = emailBoundaryFromContentType(contentType);
    if (boundary.length() == 0) return;
    String marker = "--" + boundary;
    int markerPos = content.indexOf(marker);
    while (markerPos >= 0) {
      int lineEnd = content.indexOf('\n', markerPos);
      if (lineEnd < 0) break;
      String markerLine = content.substring(markerPos, lineEnd);
      markerLine.trim();
      if (markerLine.endsWith("--")) break;

      int partStart = lineEnd + 1;
      int nextMarker = content.indexOf("\n" + marker, partStart);
      int partEnd = nextMarker >= 0 ? nextMarker : content.length();
      String part = content.substring(partStart, partEnd);
      int split = emailFindHeaderBodySplit(part);
      if (split >= 0) {
        String partHeaders = part.substring(0, split);
        String partBody = part.substring(split + 2);
        emailExtractMimeText(partHeaders, partBody, plain, html);
      }
      if (plain != NULL && plain->length() > 0) return;
      if (nextMarker < 0) break;
      markerPos = nextMarker + 1;
    }
    return;
  }

  String decoded = emailDecodeTransferBody(content, transferEncoding);
  if (lowerType.indexOf("text/html") >= 0) {
    if (html != NULL && html->length() == 0) *html = emailHtmlToText(decoded);
  } else if (lowerType.indexOf("text/plain") >= 0 || lowerType.length() == 0) {
    if (plain != NULL && plain->length() == 0) *plain = decoded;
  }
}

static String emailExtractReadableBody(const String& headers, const String& rawBody)
{
  String plain;
  String html;
  emailExtractMimeText(headers, rawBody, &plain, &html);
  if (plain.length() > 0) return emailNormalizeDisplayBody(plain);
  if (html.length() > 0) return emailNormalizeDisplayBody(html);

  String fallback = emailDecodeTransferBody(rawBody, emailHeaderValue(headers, "Content-Transfer-Encoding"));
  return emailNormalizeDisplayBody(fallback);
}

static EmailMessageEntry emailParseRawMessage(const String& uid, const String& raw)
{
  EmailMessageEntry msg;
  msg.uid = uid;
  msg.filePath = "";
  msg.from = "(unknown)";
  msg.to = "";
  msg.subject = "(no subject)";
  msg.date = "";
  msg.status = "";
  msg.body = "";

  bool inHeaders = true;
  String headerLine = "";
  String headerBlock = "";
  String rawBody = "";
  int start = 0;
  while (start <= (int)raw.length()) {
    int end = raw.indexOf('\n', start);
    if (end < 0) end = raw.length();
    String line = raw.substring(start, end);
    line.replace("\r", "");

    if (inHeaders) {
      if (line.length() == 0) {
        if (headerLine.length() > 0) emailApplyHeaderLine(&msg, headerLine);
        headerLine = "";
        inHeaders = false;
      } else if (line[0] == ' ' || line[0] == '\t') {
        headerLine += " ";
        headerLine += line;
      } else {
        if (headerLine.length() > 0) emailApplyHeaderLine(&msg, headerLine);
        headerLine = line;
      }
      if (inHeaders && line.length() > 0) {
        headerBlock += line;
        headerBlock += "\n";
      }
    } else if (rawBody.length() < EMAIL_MAX_RAW_CHARS) {
      rawBody += line;
      rawBody += "\n";
    }

    if (end >= (int)raw.length()) break;
    start = end + 1;
  }

  msg.body = emailExtractReadableBody(headerBlock, rawBody);
  if (msg.body.length() == 0) msg.body = "(empty message)";
  return msg;
}

static bool emailCachedBodyLooksUseful(const String& path)
{
  EmailMessageEntry cached;
  if (!parseCachedEmailFile(path, &cached)) return false;
  cached.body.trim();
  return cached.body.length() > 0 && cached.body != "(empty message)";
}

static bool saveEmailToInbox(const EmailMessageEntry& msg)
{
  if (!ensureEmailDirectories()) return false;
  String fileName = emailSanitizeFileName(msg.uid) + ".txt";
  String path = String(EMAIL_INBOX_DIR) + "/" + fileName;
  if (SD.exists(path.c_str())) return true;

  File f = SD.open(path.c_str(), FILE_WRITE);
  if (!f) return false;
  f.println("uid:" + msg.uid);
  f.println("from:" + msg.from);
  f.println("subject:" + msg.subject);
  f.println("date:" + msg.date);
  f.println("body:");
  f.println(msg.body);
  f.close();
  return true;
}

static bool fetchNewPop3Emails()
{
  if (gEmailFetchRunning) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  EmailLoginConfig cfg;
  if (!loadEmailLoginConfig(&cfg)) return false;
  if (!cfg.pop3Ssl) {
    Serial.println("[EMAIL] Only POP3 over SSL is supported.");
    return false;
  }

  gEmailFetchRunning = true;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  bool ok = false;
  if (!client.connect(cfg.pop3Server.c_str(), cfg.pop3Port)) {
    Serial.println("[EMAIL] POP3 connect failed");
    gEmailFetchRunning = false;
    return false;
  }

  String answer;
  do {
    if (!emailReadExpectedPrefix(client, "+OK", &answer)) break;
    client.print("USER "); client.print(cfg.user); client.print("\r\n");
    if (!emailReadExpectedPrefix(client, "+OK", &answer)) break;
    client.print("PASS "); client.print(cfg.password); client.print("\r\n");
    if (!emailReadExpectedPrefix(client, "+OK", &answer)) break;
    client.print("UIDL\r\n");
    if (!emailReadExpectedPrefix(client, "+OK", &answer)) break;

    struct UidlEntry {
      int number;
      String uid;
    };
    UidlEntry uidls[MAX_EMAIL_MESSAGES];
    int uidlCount = 0;
    while (uidlCount < MAX_EMAIL_MESSAGES) {
      String line = emailReadLine(client);
      if (line == ".") break;
      int sep = line.indexOf(' ');
      if (sep <= 0) continue;
      uidls[uidlCount].number = line.substring(0, sep).toInt();
      uidls[uidlCount].uid = line.substring(sep + 1);
      uidls[uidlCount].uid.trim();
      uidlCount++;
    }

    int fetched = 0;
    for (int i = uidlCount - 1; i >= 0 && fetched < EMAIL_MAX_FETCH_PER_RUN; i--) {
      String cachePath = String(EMAIL_INBOX_DIR) + "/" + emailSanitizeFileName(uidls[i].uid) + ".txt";
      if (SD.exists(cachePath.c_str())) {
        if (emailCachedBodyLooksUseful(cachePath)) continue;
        SD.remove(cachePath.c_str());
      }

      client.print("RETR "); client.print(uidls[i].number); client.print("\r\n");
      if (!emailReadExpectedPrefix(client, "+OK", &answer)) continue;

      String raw;
      while (true) {
        String line = emailReadLine(client, 15000);
        if (line == ".") break;
        if (line.length() == 0 && !client.connected() && !client.available()) break;
        if (line.startsWith("..")) line.remove(0, 1);
        if (raw.length() < EMAIL_MAX_RAW_CHARS) {
          raw += line;
          raw += "\n";
        }
      }

      EmailMessageEntry msg = emailParseRawMessage(uidls[i].uid, raw);
      if (saveEmailToInbox(msg)) fetched++;
    }
    ok = true;
  } while (false);

  client.print("QUIT\r\n");
  client.stop();
  gEmailFetchRunning = false;
  return ok;
}

static String imapQuotedString(const String& value)
{
  String out = "\"";
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '\\' || c == '"') out += '\\';
    out += c;
  }
  out += "\"";
  return out;
}

static String nextImapTag(int * counter)
{
  if (counter == NULL) return "A000";
  (*counter)++;
  char tag[8];
  snprintf(tag, sizeof(tag), "A%03d", *counter);
  return String(tag);
}

static bool imapTaggedLineOk(const String& tag, const String& line)
{
  if (!line.startsWith(tag)) return false;
  int pos = tag.length();
  while (pos < (int)line.length() && line[pos] == ' ') pos++;
  return line.substring(pos).startsWith("OK");
}

static bool imapReadGreeting(WiFiClientSecure& client)
{
  unsigned long start = millis();
  while (millis() - start < 15000) {
    String line = emailReadLine(client, 15000);
    if (line.startsWith("* OK")) return true;
    if (line.startsWith("* BYE") || line.startsWith("* NO") || line.startsWith("* BAD")) return false;
    if (line.length() == 0 && !client.connected() && !client.available()) break;
  }
  return false;
}

static bool imapReadTaggedResponse(WiFiClientSecure& client, const String& tag, String * response, unsigned long timeoutMs = 20000)
{
  if (response != NULL) *response = "";
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    String line = emailReadLine(client, timeoutMs);
    if (response != NULL && response->length() < 3500) {
      *response += line;
      *response += "\n";
    }
    if (line.startsWith(tag)) {
      return imapTaggedLineOk(tag, line);
    }
    if (line.length() == 0 && !client.connected() && !client.available()) break;
  }
  return false;
}

static bool imapSendCommand(WiFiClientSecure& client, int * tagCounter, const String& command, String * response = NULL)
{
  String tag = nextImapTag(tagCounter);
  client.print(tag);
  client.print(" ");
  client.print(command);
  client.print("\r\n");
  return imapReadTaggedResponse(client, tag, response);
}

static int imapParseExistsCount(const String& response)
{
  int start = 0;
  while (start < (int)response.length()) {
    int end = response.indexOf('\n', start);
    if (end < 0) end = response.length();
    String line = response.substring(start, end);
    line.trim();
    if (line.startsWith("* ") && line.endsWith(" EXISTS")) {
      int firstSpace = line.indexOf(' ');
      int secondSpace = line.indexOf(' ', firstSpace + 1);
      if (firstSpace >= 0 && secondSpace > firstSpace) {
        return line.substring(firstSpace + 1, secondSpace).toInt();
      }
    }
    start = end + 1;
  }
  return 0;
}

static String imapParseUidFromLine(const String& line)
{
  int uidPos = line.indexOf("UID ");
  if (uidPos < 0) return "";
  int start = uidPos + 4;
  int end = start;
  while (end < (int)line.length() && isdigit(line[end])) end++;
  return line.substring(start, end);
}

static int imapFetchRecentUidList(WiFiClientSecure& client, int * tagCounter, int existsCount, String * uids, int maxUids)
{
  if (existsCount <= 0 || uids == NULL || maxUids <= 0) return 0;

  int firstSeq = existsCount - maxUids + 1;
  if (firstSeq < 1) firstSeq = 1;

  String tag = nextImapTag(tagCounter);
  client.print(tag);
  client.print(" FETCH ");
  client.print(firstSeq);
  client.print(":* (UID)\r\n");

  int uidCount = 0;
  unsigned long start = millis();
  while (millis() - start < 20000) {
    String line = emailReadLine(client, 20000);
    if (line.startsWith(tag)) {
      if (!imapTaggedLineOk(tag, line)) return 0;
      return uidCount;
    }
    String uid = imapParseUidFromLine(line);
    if (uid.length() > 0 && uidCount < maxUids) {
      uids[uidCount++] = uid;
    }
    if (line.length() == 0 && !client.connected() && !client.available()) break;
  }
  return uidCount;
}

static bool emailReadLiteralBytes(WiFiClientSecure& client, size_t byteCount, String * out, size_t maxStore, unsigned long timeoutMs = 30000)
{
  size_t readCount = 0;
  unsigned long lastDataMs = millis();
  while (readCount < byteCount && millis() - lastDataMs < timeoutMs) {
    while (client.available() && readCount < byteCount) {
      char c = (char)client.read();
      readCount++;
      lastDataMs = millis();
      if (out != NULL && out->length() < maxStore) {
        *out += c;
      }
    }
    if (readCount >= byteCount) break;
    if (!client.connected() && !client.available()) break;
    delay(1);
  }
  return readCount == byteCount;
}

static int imapLiteralSizeFromLine(const String& line)
{
  int open = line.lastIndexOf('{');
  int close = line.lastIndexOf('}');
  if (open < 0 || close <= open) return -1;
  return line.substring(open + 1, close).toInt();
}

static bool imapFetchRawMessage(WiFiClientSecure& client, int * tagCounter, const String& uid, String * raw)
{
  if (raw == NULL) return false;
  *raw = "";

  String tag = nextImapTag(tagCounter);
  client.print(tag);
  client.print(" UID FETCH ");
  client.print(uid);
  client.print(" (BODY.PEEK[])\r\n");

  bool gotLiteral = false;
  bool ok = false;
  unsigned long start = millis();
  while (millis() - start < 45000) {
    String line = emailReadLine(client, 30000);
    int literalSize = imapLiteralSizeFromLine(line);
    if (literalSize >= 0) {
      gotLiteral = emailReadLiteralBytes(client, (size_t)literalSize, raw, EMAIL_MAX_RAW_CHARS);
      continue;
    }
    if (line.startsWith(tag)) {
      ok = imapTaggedLineOk(tag, line);
      break;
    }
    if (line.length() == 0 && !client.connected() && !client.available()) break;
  }

  raw->replace("\r", "");
  return ok && gotLiteral && raw->length() > 0;
}

static bool fetchNewImapEmails()
{
  if (gEmailFetchRunning) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  EmailLoginConfig cfg;
  if (!loadEmailLoginConfig(&cfg)) return false;
  if (!cfg.imapSsl) {
    Serial.println("[EMAIL] Only IMAP over SSL is supported.");
    return false;
  }

  gEmailFetchRunning = true;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  bool ok = false;
  if (!client.connect(cfg.imapServer.c_str(), cfg.imapPort)) {
    Serial.println("[EMAIL] IMAP connect failed");
    gEmailFetchRunning = false;
    return false;
  }

  int tagCounter = 0;
  do {
    if (!imapReadGreeting(client)) break;
    if (!imapSendCommand(client, &tagCounter, "LOGIN " + imapQuotedString(cfg.user) + " " + imapQuotedString(cfg.password))) break;

    String selectResponse;
    if (!imapSendCommand(client, &tagCounter, "SELECT " + imapQuotedString(cfg.imapFolder), &selectResponse)) break;
    int existsCount = imapParseExistsCount(selectResponse);
    if (existsCount <= 0) {
      ok = true;
      break;
    }

    String uids[MAX_EMAIL_MESSAGES];
    int uidCount = imapFetchRecentUidList(client, &tagCounter, existsCount, uids, MAX_EMAIL_MESSAGES);
    int fetched = 0;
    for (int i = uidCount - 1; i >= 0 && fetched < EMAIL_MAX_FETCH_PER_RUN; i--) {
      String cacheUid = "imap_" + uids[i];
      String cachePath = String(EMAIL_INBOX_DIR) + "/" + emailSanitizeFileName(cacheUid) + ".txt";
      if (SD.exists(cachePath.c_str())) {
        if (emailCachedBodyLooksUseful(cachePath)) continue;
        SD.remove(cachePath.c_str());
      }

      String raw;
      if (!imapFetchRawMessage(client, &tagCounter, uids[i], &raw)) continue;
      EmailMessageEntry msg = emailParseRawMessage(cacheUid, raw);
      if (saveEmailToInbox(msg)) fetched++;
    }
    ok = true;
  } while (false);

  imapSendCommand(client, &tagCounter, "LOGOUT");
  client.stop();
  gEmailFetchRunning = false;
  return ok;
}

static bool fetchNewEmails()
{
  EmailLoginConfig cfg;
  if (!loadEmailLoginConfig(&cfg)) return false;
  if (cfg.protocol == "imap") return fetchNewImapEmails();
  return fetchNewPop3Emails();
}

static bool saveOutgoingEmail(const String& recipient, const String& subject, const String& body, const String& status)
{
  if (!ensureEmailDirectories()) return false;
  String fileName = String("out_") + String((uint32_t)time(NULL)) + "_" + String(millis()) + ".txt";
  String path = String(EMAIL_OUTGOING_DIR) + "/" + fileName;
  File f = SD.open(path.c_str(), FILE_WRITE);
  if (!f) return false;
  f.println("uid:" + fileName);
  f.println("to:" + recipient);
  f.println("subject:" + subject);
  f.println("date:" + emailFormatRfc2822Date());
  f.println("status:" + status);
  f.println("body:");
  f.println(body);
  f.close();
  return true;
}

static String emailExtractAddress(String value)
{
  value.trim();
  value.replace("\r", "");
  value.replace("\n", "");

  int lt = value.indexOf('<');
  int gt = value.indexOf('>', lt + 1);
  if (lt >= 0 && gt > lt) {
    value = value.substring(lt + 1, gt);
    value.trim();
  }

  if ((value.startsWith("\"") && value.endsWith("\"")) ||
      (value.startsWith("'") && value.endsWith("'"))) {
    value = value.substring(1, value.length() - 1);
    value.trim();
  }

  return value;
}

static bool emailLooksLikeAddress(const String& value)
{
  int at = value.indexOf('@');
  int dot = value.lastIndexOf('.');
  return value.length() >= 5 &&
         at > 0 &&
         dot > at + 1 &&
         dot < (int)value.length() - 1 &&
         value.indexOf(' ') < 0 &&
         value.indexOf('<') < 0 &&
         value.indexOf('>') < 0 &&
         value.indexOf(',') < 0;
}

static String emailHeaderSafe(String value)
{
  value.replace("\r", " ");
  value.replace("\n", " ");
  value.trim();
  return value;
}

static String emailDomainFromAddress(const String& address)
{
  int at = address.indexOf('@');
  if (at >= 0 && at + 1 < (int)address.length()) {
    String domain = address.substring(at + 1);
    domain.trim();
    if (domain.length() > 0) return domain;
  }
  return "fos.local";
}

static bool emailLooksLikeNetcupSystemSender(const String& address)
{
  String domain = emailDomainFromAddress(address);
  domain.toLowerCase();
  return domain.endsWith(".netcup.net") &&
         (domain.indexOf(".mxe") >= 0 || domain.startsWith("mail"));
}

static String emailFormatMailbox(const String& address, String displayName)
{
  displayName = emailHeaderSafe(displayName);
  if (displayName.length() == 0) {
    return "<" + address + ">";
  }
  return "=?UTF-8?B?" + emailBase64Encode(displayName) + "?= <" + address + ">";
}

static String emailFormatRfc2822Date()
{
  if (!isSystemTimeValid()) {
    requestNtpSync(true);
    unsigned long start = millis();
    while (!isSystemTimeValid() && millis() - start < 3000) {
      delay(50);
    }
  }

  time_t nowTs = time(nullptr);
  if (nowTs < 1609459200) nowTs = 1609459200;

  struct tm utcTm;
  gmtime_r(&nowTs, &utcTm);

  static const char * days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  static const char * months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  char buf[48];
  snprintf(
    buf,
    sizeof(buf),
    "%s, %02d %s %04d %02d:%02d:%02d +0000",
    days[utcTm.tm_wday],
    utcTm.tm_mday,
    months[utcTm.tm_mon],
    utcTm.tm_year + 1900,
    utcTm.tm_hour,
    utcTm.tm_min,
    utcTm.tm_sec
  );
  return String(buf);
}

static char emailHexDigit(uint8_t value)
{
  value &= 0x0F;
  return value < 10 ? ('0' + value) : ('A' + value - 10);
}

static void emailAppendQuotedPrintableByte(String * out, uint8_t value)
{
  if (out == NULL) return;
  *out += '=';
  *out += emailHexDigit(value >> 4);
  *out += emailHexDigit(value);
}

static String emailQuotedPrintableEncode(const String& input)
{
  String out;
  int lineLen = 0;

  for (size_t i = 0; i < input.length(); i++) {
    uint8_t c = (uint8_t)input[i];
    if (c == '\r') continue;
    if (c == '\n') {
      out += "\r\n";
      lineLen = 0;
      continue;
    }

    bool atLineEnd = (i + 1 >= input.length() || input[i + 1] == '\n' || input[i + 1] == '\r');
    bool mustEncode = (c == '=') ||
                      (c < 32 && c != '\t') ||
                      (c >= 127) ||
                      ((c == ' ' || c == '\t') && atLineEnd);
    int addLen = mustEncode ? 3 : 1;
    if (lineLen + addLen > 73) {
      out += "=\r\n";
      lineLen = 0;
    }

    if (mustEncode) {
      emailAppendQuotedPrintableByte(&out, c);
    } else {
      out += (char)c;
    }
    lineLen += addLen;
  }

  return out;
}

static void smtpPrintDotStuffedText(WiFiClientSecure& client, const String& content)
{
  int start = 0;
  while (start <= (int)content.length()) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = content.length();
    String line = content.substring(start, end);
    line.replace("\r", "");
    if (line.startsWith(".")) client.print(".");
    client.print(line);
    client.print("\r\n");
    if (end >= (int)content.length()) break;
    start = end + 1;
  }
}

static bool sendSmtpEmail(const String& recipient, const String& subject, const String& body, String * status)
{
  if (status != NULL) *status = "";
  if (WiFi.status() != WL_CONNECTED) {
    if (status != NULL) *status = "No WiFi connection.";
    return false;
  }

  EmailLoginConfig cfg;
  if (!loadEmailLoginConfig(&cfg)) {
    if (status != NULL) *status = "Missing /system/email/login.txt data.";
    return false;
  }
  if (cfg.smtpStartTls || cfg.smtpPort == 587) {
    if (status != NULL) *status = "SMTP 587 uses STARTTLS. Use SSL port 465.";
    Serial.println("[EMAIL] SMTP STARTTLS requested. Use direct SSL/TLS on port 465 for this build.");
    return false;
  }
  if (!cfg.smtpSsl) {
    if (status != NULL) *status = "SMTP needs SSL. Use smtp_ssl=true and port 465.";
    return false;
  }

  String fromSource = cfg.fromEmail.length() > 0 ? cfg.fromEmail : cfg.email;
  String fromAddress = emailExtractAddress(fromSource);
  String replyToAddress = emailExtractAddress(cfg.replyTo.length() > 0 ? cfg.replyTo : fromAddress);
  String toAddress = emailExtractAddress(recipient);
  if (!emailLooksLikeAddress(fromAddress)) {
    if (status != NULL) *status = "Invalid from_email/email address in login.txt.";
    return false;
  }
  if (!emailLooksLikeAddress(replyToAddress)) {
    if (status != NULL) *status = "Invalid reply_to address in login.txt.";
    return false;
  }
  if (!cfg.allowTechnicalSender && emailLooksLikeNetcupSystemSender(fromAddress)) {
    if (status != NULL) *status = "DMARC risk: set from_email to your real mailbox/domain.";
    Serial.println("[EMAIL] Refusing likely Netcup system sender. Set from_email to a real mailbox/domain authorized on this SMTP account.");
    return false;
  }
  if (!emailLooksLikeAddress(toAddress)) {
    if (status != NULL) *status = "Invalid recipient address.";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);
  Serial.print("[EMAIL] SMTP connect ");
  Serial.print(cfg.smtpServer);
  Serial.print(":");
  Serial.println(cfg.smtpPort);
  if (!client.connect(cfg.smtpServer.c_str(), cfg.smtpPort)) {
    if (status != NULL) {
      *status = "SMTP connect failed: ";
      *status += cfg.smtpServer;
      *status += ":";
      *status += String(cfg.smtpPort);
    }
    Serial.println("[EMAIL] SMTP connect failed");
    return false;
  }

  String answer;
  bool ok = false;
  do {
    if (!smtpExpectCode(client, 220, &answer)) break;
    client.print("EHLO ");
    client.print(emailDomainFromAddress(fromAddress));
    client.print("\r\n");
    if (!smtpExpectCode(client, 250, &answer)) break;
    client.print("AUTH LOGIN\r\n");
    if (!smtpExpectCode(client, 334, &answer)) break;
    client.print(emailBase64Encode(cfg.user)); client.print("\r\n");
    if (!smtpExpectCode(client, 334, &answer)) break;
    client.print(emailBase64Encode(cfg.password)); client.print("\r\n");
    if (!smtpExpectCode(client, 235, &answer)) break;
    client.print("MAIL FROM:<"); client.print(fromAddress); client.print(">\r\n");
    if (!smtpExpectCode(client, 250, &answer)) break;
    client.print("RCPT TO:<"); client.print(toAddress); client.print(">\r\n");
    if (!smtpExpectCode(client, 250, &answer)) break;
    client.print("DATA\r\n");
    if (!smtpExpectCode(client, 354, &answer)) break;

    client.print("From: ");
    client.print(emailFormatMailbox(fromAddress, cfg.senderName));
    client.print("\r\n");
    client.print("To: <");
    client.print(toAddress);
    client.print(">\r\n");
    client.print("Date: ");
    client.print(emailFormatRfc2822Date());
    client.print("\r\n");
    client.print("Message-ID: <");
    client.print(String((uint32_t)time(NULL)));
    client.print(".");
    client.print(String((uint32_t)millis()));
    client.print(".");
    client.print(String((uint32_t)esp_random(), HEX));
    client.print("@");
    client.print(emailDomainFromAddress(fromAddress));
    client.print(">\r\n");
    client.print("Reply-To: <");
    client.print(replyToAddress);
    client.print(">\r\n");
    client.print("Subject: =?UTF-8?B?");
    client.print(emailBase64Encode(emailHeaderSafe(subject)));
    client.print("?=\r\n");
    client.print("MIME-Version: 1.0\r\n");
    client.print("Content-Type: text/plain; charset=UTF-8\r\n");
    client.print("Content-Transfer-Encoding: quoted-printable\r\n\r\n");

    String qpBody = emailQuotedPrintableEncode(body);
    smtpPrintDotStuffedText(client, qpBody);
    client.print(".\r\n");
    if (!smtpExpectCode(client, 250, &answer)) break;
    ok = true;
  } while (false);

  if (!ok && status != NULL) {
    if (answer.length() > 0) *status = "SMTP error: " + answer;
    else *status = "SMTP error.";
  }
  client.print("QUIT\r\n");
  client.stop();
  if (ok && status != NULL) *status = "Email sent.";
  return ok;
}

static bool emailOutgoingTabActive()
{
  return ui_TabView1 != NULL && lv_tabview_get_tab_act(ui_TabView1) == 1;
}

static void resetEmailMessageScroll()
{
  if (uic_EmailMessageLabel == NULL) return;
  lv_obj_t * scrollArea = lv_obj_get_parent(uic_EmailMessageLabel);
  if (scrollArea != NULL) {
    lv_obj_scroll_to_y(scrollArea, 0, LV_ANIM_OFF);
  }
}

static void showSelectedEmail()
{
  if (uic_EmailMessageContainer == NULL) return;

  bool outgoing = emailOutgoingTabActive();
  lv_obj_t * roller = outgoing ? uic_EmailRollerOutgoing : uic_EmailRollerInbox;
  EmailMessageEntry * messages = outgoing ? gOutgoingEmailMessages : gEmailMessages;
  int messageCount = outgoing ? gOutgoingEmailMessageCount : gEmailMessageCount;
  if (roller == NULL) return;

  if (messageCount <= 0) {
    lv_label_set_text(uic_EmailNameLabel, outgoing ? "Outgoing" : "Inbox");
    lv_label_set_text(uic_EmailSubjectLabel, outgoing ? "No outgoing emails" : "No cached emails");
    lv_label_set_text(uic_EmailMessageLabel, outgoing
      ? "No sent emails have been saved yet."
      : "No email has been downloaded yet. Check WiFi and /system/email/login.txt.");
    resetEmailMessageScroll();
    lv_obj_clear_flag(uic_EmailMessageContainer, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  int selected = lv_roller_get_selected(roller);
  if (selected < 0 || selected >= messageCount) selected = 0;
  EmailMessageEntry& msg = messages[selected];
  String headerName = outgoing ? (String("To: ") + (msg.to.length() ? msg.to : msg.from)) : msg.from;
  String headerSubject = msg.subject;
  if (outgoing && msg.status.length() > 0) {
    headerSubject += " (" + msg.status + ")";
  }
  lv_label_set_text(uic_EmailNameLabel, headerName.c_str());
  lv_label_set_text(uic_EmailSubjectLabel, headerSubject.c_str());
  lv_label_set_long_mode(uic_EmailMessageLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(uic_EmailMessageLabel, msg.body.c_str());
  resetEmailMessageScroll();
  lv_obj_clear_flag(uic_EmailMessageContainer, LV_OBJ_FLAG_HIDDEN);
}

static void emailSelectEvent(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  showSelectedEmail();
}

static void emailTextAreaEvent(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) return;
  lv_obj_t * ta = lv_event_get_target(e);
  if (uic_NewEmailKeyboard != NULL && ta != NULL) {
    lv_keyboard_set_textarea(uic_NewEmailKeyboard, ta);
  }
}

static void emailSendButtonEvent(lv_event_t * e)
{
  SendEmail(e);
}

extern "C" void SendEmail(lv_event_t * e)
{
  if (e != NULL && lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  unsigned long nowMs = millis();
  if (nowMs - gEmailLastSendEventMs < 800) return;
  gEmailLastSendEventMs = nowMs;

  if (uic_EmailAddressTextArea == NULL || uic_SubjectTextArea == NULL || ui_MessageTextArea == NULL) return;

  String recipient = lv_textarea_get_text(uic_EmailAddressTextArea);
  String subject = lv_textarea_get_text(uic_SubjectTextArea);
  String body = lv_textarea_get_text(ui_MessageTextArea);
  recipient.trim();
  subject.trim();

  String status;
  bool ok = false;
  if (recipient.length() == 0) {
    status = "Please enter a recipient.";
  } else {
    ok = sendSmtpEmail(recipient, subject, body, &status);
    saveOutgoingEmail(recipient, subject, body, ok ? "sent" : "error");
    loadCachedOutgoingEmails();
    fillEmailRollers();
  }

  if (uic_NewEmailContainer != NULL) {
    lv_obj_add_flag(uic_NewEmailContainer, LV_OBJ_FLAG_HIDDEN);
  }
  if (uic_EmailMessageContainer != NULL) {
    lv_label_set_text(uic_EmailNameLabel, (String("To: ") + recipient).c_str());
    lv_label_set_text(uic_EmailSubjectLabel, subject.length() ? subject.c_str() : "(no subject)");
    lv_label_set_long_mode(uic_EmailMessageLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(uic_EmailMessageLabel, status.c_str());
    resetEmailMessageScroll();
    lv_obj_clear_flag(uic_EmailMessageContainer, LV_OBJ_FLAG_HIDDEN);
  }

  if (ok) {
    lv_textarea_set_text(uic_EmailAddressTextArea, "");
    lv_textarea_set_text(uic_SubjectTextArea, "");
    lv_textarea_set_text(ui_MessageTextArea, "");
  }
}

static void renderEmailApp()
{
  resetEmailAppState();
  gEmailUiVisible = true;

  lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_AppContentArea, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(uic_AppContentArea, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(uic_AppContentArea, LV_SCROLLBAR_MODE_OFF);

  ui_Email_screen_init();
  if (ui_Email == NULL) return;

  while (lv_obj_get_child(ui_Email, 0) != NULL) {
    lv_obj_t * child = lv_obj_get_child(ui_Email, 0);
    lv_obj_set_parent(child, uic_AppContentArea);
  }

  if (uic_EmailMessageContainer != NULL) {
    lv_obj_add_flag(uic_EmailMessageContainer, LV_OBJ_FLAG_HIDDEN);
  }
  if (uic_NewEmailContainer != NULL) {
    lv_obj_add_flag(uic_NewEmailContainer, LV_OBJ_FLAG_HIDDEN);
  }
  if (uic_EmailMessageLabel != NULL) {
    lv_label_set_long_mode(uic_EmailMessageLabel, LV_LABEL_LONG_WRAP);
  }

  if (ui_EmailSelect != NULL) {
    lv_obj_add_event_cb(ui_EmailSelect, emailSelectEvent, LV_EVENT_CLICKED, NULL);
  }
  if (ui_EmailSend != NULL) {
    lv_obj_add_event_cb(ui_EmailSend, emailSendButtonEvent, LV_EVENT_CLICKED, NULL);
  }
  if (uic_EmailAddressTextArea != NULL) {
    lv_obj_add_event_cb(uic_EmailAddressTextArea, emailTextAreaEvent, LV_EVENT_ALL, NULL);
  }
  if (uic_SubjectTextArea != NULL) {
    lv_obj_add_event_cb(uic_SubjectTextArea, emailTextAreaEvent, LV_EVENT_ALL, NULL);
  }
  if (ui_MessageTextArea != NULL) {
    lv_obj_add_event_cb(ui_MessageTextArea, emailTextAreaEvent, LV_EVENT_ALL, NULL);
  }

  ensureEmailDirectories();
  loadCachedEmails();
  loadCachedOutgoingEmails();
  fillEmailRollers();
  if (WiFi.status() == WL_CONNECTED) {
    fetchNewEmails();
    loadCachedEmails();
    loadCachedOutgoingEmails();
    fillEmailRollers();
  }

  if (ui_HomeButton9 != NULL) {
    lv_obj_move_foreground(ui_HomeButton9);
  }
}

static void showAppContentForIndex(int appIndex)
{
  if (appIndex < 0 || appIndex >= gLauncherAppCount) return;
  gCurrentAppIndex = appIndex;
  LauncherAppEntry &app = gLauncherApps[appIndex];

  _ui_screen_change(&ui_AppContent, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_AppContent_screen_init);

  if (uic_AppContentArea == NULL) return;

  clearAppContentArea();

  lv_obj_set_style_pad_all(uic_AppContentArea, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(uic_AppContentArea, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(uic_AppContentArea, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

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
    lv_obj_set_style_text_color(infoLabel, lv_color_hex(getThemeContrastColor(0x000000)), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * demoButton = lv_btn_create(uic_AppContentArea);
    lv_obj_set_size(demoButton, 260, 90);
    lv_obj_align(demoButton, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_radius(demoButton, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(demoButton, lv_color_hex(getThemeAccentColor()), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(demoButton, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * buttonLabel = lv_label_create(demoButton);
    lv_label_set_text(buttonLabel, app.buttonText.c_str());
    lv_obj_center(buttonLabel);
    lv_obj_set_style_text_font(buttonLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(buttonLabel, lv_color_hex(getThemeContrastColor(getThemeAccentColor())), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * statusLabel = lv_label_create(uic_AppContentArea);
    lv_obj_set_width(statusLabel, lv_pct(100));
    lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_WRAP);
    lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 80);
    lv_label_set_text(statusLabel, "Noch nicht gedrueckt.");
    lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(getThemeContrastColor(0x000000)), LV_PART_MAIN | LV_STATE_DEFAULT);

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

  if (app.appType == "clock") {
    renderClockDashboardApp();
    return;
  }

  if (app.appType == "weather") {
    renderWeatherApp();
    return;
  }

  if (app.appType == "email") {
    renderEmailApp();
    return;
  }

  bool isSdTextEditorApp =
    (app.appType == "sd") ||
    (app.appType == "text" && app.folderName == "text");

  if (isSdTextEditorApp) {
    renderSdTextApp();
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
      ui_AppL4 == NULL || ui_AppL5 == NULL || ui_AppL6 == NULL || ui_AppL7 == NULL) return;

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
    ui_object_set_themeable_style_property(appBtn, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_COLOR, _ui_theme_color_MainTheme);
    ui_object_set_themeable_style_property(appBtn, LV_PART_MAIN| LV_STATE_DEFAULT, LV_STYLE_BG_OPA, _ui_theme_alpha_MainTheme);

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

static void refreshVisibleSdAppForThemeChange()
{
  if (gCurrentAppIndex < 0) return;
  if (ui_AppContent == NULL) return;
  if (lv_scr_act() != ui_AppContent) return;

  showAppContentForIndex(gCurrentAppIndex);
}

extern "C" void UnloadApp_Data(lv_event_t * e)
{
  (void)e;
  stopRadioPlayback();
  gCurrentAppIndex = -1;
  clearAppContentArea();
  resetCalculatorState();
  resetRadioState();
  resetSdAppState();
  resetClockDashboardState();
  resetWeatherAppState();
  resetEmailAppState();
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

    if (!SD.exists(DISPLAY_DIR)) {
      SD.mkdir(DISPLAY_DIR);
      Serial.println("Ordner /system/display erstellt");
    }

    if (!SD.exists(UPDATE_DIR)) {
      SD.mkdir(UPDATE_DIR);
      Serial.println("Ordner /system/update erstellt");
    }

    ensureEmailDirectories();
    ensureEmailSdApp();

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
  (void)e;
  if (!sd_ok) return;
  if (!uic_WifiSelectorDropdown || !uic_TextAreaWifiPassword) return;
  if (gScannedNetworkCount == 0) return;

  uint16_t selected = lv_dropdown_get_selected(uic_WifiSelectorDropdown);
  if (selected >= (uint16_t)gScannedNetworkCount) return;

  const char* ssid = gScannedNetworks[selected].ssid.c_str();
  const char* pass = lv_textarea_get_text(uic_TextAreaWifiPassword);

  if (strlen(ssid) == 0) return;

  String encryptedPassword;
  if (!SecureCredentials::encrypt(String(pass), &encryptedPassword)) {
    Serial.println("[SECURITY] WLAN-Passwort konnte nicht verschlüsselt werden; Profil wurde nicht gespeichert.");
    return;
  }

  File f = SD.open(WIFI_FILE, FILE_APPEND);
  if (!f) {
    SecureCredentials::clear(&encryptedPassword);
    return;
  }

  f.print(ssid);
  f.print("|");
  f.println(encryptedPassword);
  f.close();
  SecureCredentials::clear(&encryptedPassword);

  Serial.println("WLAN Profil verschlüsselt hinzugefügt");
}

int loadWifiProfiles(WifiProfile profiles[])
{
  if (!sd_ok) return 0;
  recoverCredentialFileIfNeeded(WIFI_FILE, WIFI_FILE_BACKUP);
  if (!SD.exists(WIFI_FILE)) return 0;

  // Only rewrite the file when a legacy clear-text entry is actually present.
  // This avoids an unnecessary complete SD-card write on every Wi-Fi reconnect.
  bool needsMigration = false;
  File inspection = SD.open(WIFI_FILE, FILE_READ);
  if (!inspection) return 0;
  while (inspection.available() && !needsMigration) {
    String line = inspection.readStringUntil('\n');
    line.replace("\r", "");
    line.trim();
    if (line.length() == 0) continue;

    const int separator = line.indexOf('|');
    if (separator >= 0) {
      String storedPassword = line.substring(separator + 1);
      storedPassword.trim();
      needsMigration = !SecureCredentials::isEncrypted(storedPassword);
      SecureCredentials::clear(&storedPassword);
    }
  }
  inspection.close();

  File f = SD.open(WIFI_FILE, FILE_READ);
  if (!f) return 0;

  File migratedFile;
  bool canMigrate = false;
  if (needsMigration) {
    if (SD.exists(WIFI_FILE_TEMP)) SD.remove(WIFI_FILE_TEMP);
    migratedFile = SD.open(WIFI_FILE_TEMP, FILE_WRITE);
    canMigrate = static_cast<bool>(migratedFile);
  }
  bool migrationNeeded = false;
  bool migrationFailed = false;

  int count = 0;

  while (f.available()) {
    String originalLine = f.readStringUntil('\n');
    originalLine.replace("\r", "");
    String line = originalLine;
    line.trim();
    if (line.length() == 0) {
      if (canMigrate) migratedFile.println();
      continue;
    }

    int sep = line.indexOf('|');
    if (sep < 0) {
      if (canMigrate) migratedFile.println(originalLine);
      continue;
    }

    String ssid = line.substring(0, sep);
    String storedPassword = line.substring(sep + 1);
    ssid.trim();
    storedPassword.trim();

    String plainPassword;
    const bool passwordOk = decodeStoredCredential(storedPassword, &plainPassword);
    String valueForSd = storedPassword;

    if (!SecureCredentials::isEncrypted(storedPassword)) {
      if (canMigrate && SecureCredentials::encrypt(storedPassword, &valueForSd)) {
        migrationNeeded = true;
      } else {
        migrationFailed = true;
      }
    }

    if (canMigrate) {
      migratedFile.print(ssid);
      migratedFile.print("|");
      migratedFile.println(valueForSd);
    }

    if (passwordOk && count < MAX_WIFI_PROFILES) {
      profiles[count].ssid = ssid;
      profiles[count].pass = plainPassword;
      count++;
    } else if (!passwordOk) {
      Serial.println("[SECURITY] Ein WLAN-Passwort kann auf diesem Gerät nicht entschlüsselt werden.");
    }

    SecureCredentials::clear(&plainPassword);
    SecureCredentials::clear(&valueForSd);
    SecureCredentials::clear(&storedPassword);
  }

  f.close();

  if (canMigrate) {
    migratedFile.flush();
    migratedFile.close();
  }

  bool migrationSucceeded = !needsMigration;
  if (needsMigration && migrationNeeded && !migrationFailed && canMigrate) {
    migrationSucceeded = installMigratedCredentialFile(WIFI_FILE, WIFI_FILE_TEMP, WIFI_FILE_BACKUP);
    Serial.println(migrationSucceeded
      ? "[SECURITY] WLAN-Passwörter auf der SD-Karte verschlüsselt."
      : "[SECURITY] Migration der WLAN-Passwörter konnte nicht abgeschlossen werden.");
  } else if (needsMigration) {
    if (canMigrate) SD.remove(WIFI_FILE_TEMP);
    Serial.println("[SECURITY] Mindestens ein Klartext-WLAN-Passwort konnte nicht migriert werden.");
  }

  // Fail closed: do not use clear-text credentials when their secure rewrite
  // could not be completed. The original file remains available for a retry.
  if (!migrationSucceeded) {
    for (int i = 0; i < count; ++i) {
      SecureCredentials::clear(&profiles[i].pass);
    }
    count = 0;
  }

  return count;
}

bool connectKnownWifi(bool updateUiState = true)
{
  if (!gWifiEnabled) return false;

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
  if (!gWifiEnabled) return;
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

extern "C" void runWifiConnection_Data(lv_event_t * e)
{
  (void)e;

  gWifiEnabled = true;
  saveWifiEnabledState(true);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  updateWifiSelectorDropdown();
  reconnectWifi();
}

extern "C" void StopWifiConnection_Data(lv_event_t * e)
{
  (void)e;

  gWifiEnabled = false;
  saveWifiEnabledState(false);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  gScannedNetworkCount = 0;
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
void updateSDUIData(void)
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

static bool isProtectedStoragePath(const String& path)
{
  String clean = path;
  clean.trim();

  while (clean.length() > 1 && clean.endsWith("/")) {
    clean.remove(clean.length() - 1);
  }

  if (clean == "/system" || clean.startsWith("/system/")) {
    return true;
  }

  if (clean == "/apps") {
    return true;
  }

  return false;
}

static bool removeStoragePathRecursive(const String& path)
{
  String clean = path;
  clean.trim();

  while (clean.length() > 1 && clean.endsWith("/")) {
    clean.remove(clean.length() - 1);
  }

  if (clean.length() == 0 || clean == "/") {
    return false;
  }

  if (!SD.exists(clean.c_str())) {
    return false;
  }

  File target = SD.open(clean.c_str(), FILE_READ);
  if (!target) {
    return false;
  }

  bool isDirectory = target.isDirectory();
  target.close();

  if (!isDirectory) {
    return SD.remove(clean.c_str());
  }

  File dir = SD.open(clean.c_str(), FILE_READ);
  if (!dir) {
    return false;
  }

  File entry = dir.openNextFile();
  while (entry) {
    String childPath = joinStorageManagerPath(clean, pathBasename(String(entry.name())));
    bool childIsDirectory = entry.isDirectory();
    entry.close();

    bool childRemoved = false;
    if (childIsDirectory) {
      childRemoved = removeStoragePathRecursive(childPath);
    } else {
      childRemoved = SD.remove(childPath.c_str());
    }

    if (!childRemoved) {
      dir.close();
      return false;
    }

    entry = dir.openNextFile();
  }

  dir.close();
  return SD.rmdir(clean.c_str());
}

void fillFileRoller(void);

static lv_obj_t * gStorageManagerNewFolderTextArea = NULL;
static lv_obj_t * gStorageManagerNewFolderHookedKeyboard = NULL;

extern "C" void storageManagerNewFolderData(lv_event_t * e)
{
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  if (uic_KeyboardFolder == NULL) {
    return;
  }

  if (gStorageManagerNewFolderTextArea != NULL && !lv_obj_is_valid(gStorageManagerNewFolderTextArea)) {
    gStorageManagerNewFolderTextArea = NULL;
  }

  if (gStorageManagerNewFolderTextArea == NULL) {
    lv_obj_t * parent = NULL;

    if (uic_FileRollerFileManager != NULL) {
      parent = lv_obj_get_parent(uic_FileRollerFileManager);
    } else if (uic_KeyboardFolder != NULL) {
      parent = lv_obj_get_parent(uic_KeyboardFolder);
    }

    if (parent == NULL) {
      return;
    }

    gStorageManagerNewFolderTextArea = lv_textarea_create(parent);
    lv_obj_set_width(gStorageManagerNewFolderTextArea, 1);
    lv_obj_set_height(gStorageManagerNewFolderTextArea, 1);
    lv_obj_add_flag(gStorageManagerNewFolderTextArea, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(gStorageManagerNewFolderTextArea, LV_OBJ_FLAG_SCROLLABLE);
    lv_textarea_set_one_line(gStorageManagerNewFolderTextArea, true);
    lv_textarea_set_text(gStorageManagerNewFolderTextArea, "");

    lv_obj_add_event_cb(
      gStorageManagerNewFolderTextArea,
      +[](lv_event_t * taEvent) {
        if (lv_event_get_code(taEvent) != LV_EVENT_VALUE_CHANGED) {
          return;
        }

        if (gStorageManagerNewFolderTextArea == NULL) {
          return;
        }

        String draft = lv_textarea_get_text(gStorageManagerNewFolderTextArea);
        draft.trim();
        draft.replace("\r", "");
        draft.replace("\n", "");
        draft.replace("/", "_");
        while (draft.endsWith("/")) {
          draft.remove(draft.length() - 1);
        }

        gStorageManagerNewFolderDraft = draft;
        fillFileRoller();
      },
      LV_EVENT_VALUE_CHANGED,
      NULL
    );
  }

  if (uic_KeyboardFolder != NULL && uic_KeyboardFolder != gStorageManagerNewFolderHookedKeyboard) {
    lv_obj_add_event_cb(
      uic_KeyboardFolder,
      +[](lv_event_t * keyboardEvent) {
        lv_event_code_t code = lv_event_get_code(keyboardEvent);

        if (code == LV_EVENT_READY) {
          if (gStorageManagerNewFolderTextArea != NULL) {
            String folderName = lv_textarea_get_text(gStorageManagerNewFolderTextArea);
            folderName.trim();
            folderName.replace("\r", "");
            folderName.replace("\n", "");
            folderName.replace("/", "_");
            while (folderName.endsWith("/")) {
              folderName.remove(folderName.length() - 1);
            }

            if (sd_ok && folderName.length() > 0) {
              String path = joinStorageManagerPath(gStorageManagerCurrentDir, folderName);
              if (!SD.exists(path.c_str())) {
                if (SD.mkdir(path.c_str())) {
                  Serial.println("Ordner erstellt: " + path);
                } else {
                  Serial.println("Ordner konnte nicht erstellt werden: " + path);
                }
              } else {
                Serial.println("Ordner existiert bereits: " + path);
              }
            }

            lv_textarea_set_text(gStorageManagerNewFolderTextArea, "");
          }

          gStorageManagerNewFolderDraft = "";
          fillFileRoller();
          updateSDUIData();

          if (uic_KeyboardFolder != NULL) {
            lv_keyboard_set_textarea(uic_KeyboardFolder, NULL);
            lv_obj_add_flag(uic_KeyboardFolder, LV_OBJ_FLAG_HIDDEN);
          }
        }

        if (code == LV_EVENT_CANCEL) {
          gStorageManagerNewFolderDraft = "";

          if (gStorageManagerNewFolderTextArea != NULL) {
            lv_textarea_set_text(gStorageManagerNewFolderTextArea, "");
          }

          fillFileRoller();

          if (uic_KeyboardFolder != NULL) {
            lv_keyboard_set_textarea(uic_KeyboardFolder, NULL);
            lv_obj_add_flag(uic_KeyboardFolder, LV_OBJ_FLAG_HIDDEN);
          }
        }
      },
      LV_EVENT_ALL,
      NULL
    );
    gStorageManagerNewFolderHookedKeyboard = uic_KeyboardFolder;
  }

  if (gStorageManagerNewFolderTextArea == NULL) {
    return;
  }

  lv_textarea_set_text(gStorageManagerNewFolderTextArea, "");
  gStorageManagerNewFolderDraft = "";
  lv_keyboard_set_textarea(uic_KeyboardFolder, gStorageManagerNewFolderTextArea);
  lv_obj_clear_flag(uic_KeyboardFolder, LV_OBJ_FLAG_HIDDEN);
}

/* =========================================================
   FILE ROLLER FUNKTIONEN
   ========================================================= */
extern "C" void StartStorageManager_Data(lv_event_t * e)
{
  fillFileRoller();
  updateSDUIData();
}

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

  if (gStorageManagerNewFolderDraft.length() > 0) {
    String draftLine = gStorageManagerNewFolderDraft;
    if (!draftLine.endsWith("/")) {
      draftLine += "/";
    }

    if (rollerText == "(Leer)") {
      rollerText = draftLine;
    } else {
      rollerText += "\n";
      rollerText += draftLine;
    }
  }

  lv_roller_set_options(uic_FileRollerFileManager, rollerText.c_str(), LV_ROLLER_MODE_NORMAL);

  uint16_t selectedIndex = 0;
  if (gStorageManagerNewFolderDraft.length() > 0) {
    uint16_t lineCount = 1;
    for (uint16_t i = 0; i < rollerText.length(); i++) {
      if (rollerText.charAt(i) == '\n') {
        lineCount++;
      }
    }
    selectedIndex = lineCount > 0 ? (uint16_t)(lineCount - 1) : 0;
  }

  lv_roller_set_selected(uic_FileRollerFileManager, selectedIndex, LV_ANIM_OFF);
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

  bool selectedIsDirectory = selected.endsWith("/");
  if (selectedIsDirectory) {
    selected.remove(selected.length() - 1);
  }

  String path = joinStorageManagerPath(gStorageManagerCurrentDir, selected);

  if (!SD.exists(path)) {
    Serial.println("Datei nicht gefunden: " + path);
    return;
  }

  if (isProtectedStoragePath(path)) {
    Serial.println("Geschuetzter Pfad, nicht geloescht: " + path);
    return;
  }

  if (removeStoragePathRecursive(path)) {
    if (selectedIsDirectory) {
      Serial.println("Ordner geloescht: " + path);
    } else {
      Serial.println("Datei geloescht: " + path);
    }
  } else {
    Serial.println("Loeschen fehlgeschlagen: " + path);
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
  lv_roller_set_options(uic_FileRollerFileManager, "", LV_ROLLER_MODE_NORMAL);
}


/* =========================================================
   FILE ROLLER – TEXT VIEWER SCREEN
   wird beim Laden des Text-Screens aufgerufen
   ========================================================= */
extern "C" void fillFileRoller_TextViewer_Data(void)
{
  lv_obj_t * roller = gSdFileRoller;
#if defined(UI_SCREENTEXT_H)
  if (uic_FileRollerText != NULL) {
    roller = uic_FileRollerText;
  }
#endif
  fillTextRollerFor(roller);
}


extern "C" void load_selected_file_Data(void)
{
  lv_obj_t * roller = gSdFileRoller;
  lv_obj_t * textArea = gSdTextArea;
#if defined(UI_SCREENTEXT_H)
  if (uic_FileRollerText != NULL) {
    roller = uic_FileRollerText;
  }
  if (uic_TextArea != NULL) {
    textArea = uic_TextArea;
  }
#endif
  (void)loadSelectedTextFileFor(roller, textArea);
}


/* =========================================================
   TEXTDATEI SPEICHERN (IMMER ÜBERSCHREIBEN)
   wird von SquareLine Button aufgerufen
   ========================================================= */
extern "C" void save_text_file_data(lv_event_t * e)
{
  (void)e;
  lv_obj_t * textArea = gSdTextArea;
  lv_obj_t * fileNameInput = gSdFileNameInput;
  lv_obj_t * roller = gSdFileRoller;
#if defined(UI_SCREENTEXT_H)
  if (uic_TextArea != NULL) {
    textArea = uic_TextArea;
  }
  if (uic_FileNameInput != NULL) {
    fileNameInput = uic_FileNameInput;
  }
  if (uic_FileRollerText != NULL) {
    roller = uic_FileRollerText;
  }
#endif
  saveTextFileFor(textArea, fileNameInput, roller);
}

extern "C" void OpenNewFile_Data(lv_event_t * e)
{
  (void)e;
  lv_obj_t * textArea = gSdTextArea;
#if defined(UI_SCREENTEXT_H)
  if (uic_TextArea != NULL) {
    textArea = uic_TextArea;
  }
#endif
  openNewTextFileFor(textArea);
}


/* ================= SETUP ================= */
void setup()
{
  Serial.begin(115200);
  Serial.printf(
    "Boot diagnostics: reset=%d wake=%d\n",
    static_cast<int>(esp_reset_reason()),
    static_cast<int>(esp_sleep_get_wakeup_cause())
  );
  setupSleepButton();

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
  OTARecovery_Init();
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
loadSavedBrightness();
loadAndApplyDisplayTheme(); 
updateClockUI();
StartAppLauncher_Data(NULL);


bootProgress(60, "Initialize WiFi");
loadWifiEnabledState();
applyWifiBootUiState();
if (gWifiEnabled) {
  WiFi.mode(WIFI_STA);
  reconnectWifi();
} else {
  WiFi.mode(WIFI_OFF);
}

/* ================= AUDIO INIT ================= */
bootProgress(70, "Initialize Audio");
audio.setPinout(
  42,   // I2S_BCLK
  18,   // I2S_LRC
  17    // I2S_DOUT
);
audio.setVolume(21); // 0..21 (CrowPanel Lautsprecher brauchen meist 10–14)

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
    if (!gDisplaySuspended) {
      lv_timer_handler();
    }
  }

  handleSleepButton();

  OTARecovery_Tick();
  const bool otaBusy = OTARecovery_IsBusy();

  if (otaBusy) {
    // While OTA/Recovery preparation is running, avoid additional UI redraw
    // and network/audio work to keep the RGB panel output stable.
    delay(5);
    return;
  }

  if (gDisplaySuspended && gSleepAfterBusy && !hasUninterruptibleProcess()) {
    gSleepAfterBusy = false;
    enterButtonSleep(false);
    return;
  }

  if (gDisplaySuspended && !hasUninterruptibleProcess()) {
    delay(50);
    return;
  }

  // alle 30 Sekunden Wi-Fi prüfen (non-blocking Reconnect)
  if (gWifiEnabled && millis() - lastReconnectAttempt > kWifiReconnectIntervalMs) {

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

  updateClockDashboardTick();
  refreshWeatherDataIfNeeded(false);

  // Audio
  audio.loop();
  updateRadioProgressSlider();

  delay(5);
}
