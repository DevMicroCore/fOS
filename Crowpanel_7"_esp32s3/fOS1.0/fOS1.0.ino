#include <lvgl.h>
#include "LGFX_CrowPanel.h"
#include "ui.h"
#include "esp_system.h"
#include "Arduino.h"
#include <SD.h>
#include <SPI.h>

#define TEXT_DIR "/text"


extern "C" void deleteSelectedFile(void);


/* ================= DISPLAY ================= */
LGFX gfx;

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

  /* HomeScreen sicher laden */
  lv_scr_load(uic_ScreenHome);

  /* ================= BOOT OVERLAY EIN ================= */
  lv_obj_move_foreground(uic_BootOverlay);
  lv_obj_clear_flag(uic_BootOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_timer_handler();   // sofort zeichnen
  delay(20);

  /* ================= LANGSAME INIT ================= */
  initSD();
  fillFileRoller();
  updateSDUIData();
  fillFileRoller_TextViewer_Data();

  /* ================= BOOT OVERLAY AUS ================= */
  lv_obj_add_flag(uic_BootOverlay, LV_OBJ_FLAG_HIDDEN);
}



/* ================= LOOP ================= */
void loop()
{
  uint32_t now = millis();
  lv_tick_inc(now - last_tick);
  last_tick = now;

  lv_timer_handler();
  delay(5);
}
