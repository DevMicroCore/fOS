#include <lvgl.h>
#include "LGFX_CrowPanel.h"
#include "ui.h"
#include "esp_system.h"
#include "Arduino.h"

LGFX gfx;
extern "C" void updateSystemInfoData(void);


static uint32_t last_tick = 0;

/* ---------------- LVGL Display Buffer ---------------- */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[800 * 40];

/* ---------------- Display Flush ---------------- */
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

/* ---------------- Touch Callback ---------------- */
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

void setup()
{
  Serial.begin(115200);

  /* ---------- Init Display ---------- */
  gfx.init();
  gfx.setRotation(0);        // ggf. 0–3 testen
  gfx.setBrightness(255);

  /* ---------- Init LVGL ---------- */
  lv_init();

  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 800 * 40);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 800;
  disp_drv.ver_res = 480;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /* ---------- Init Touch for LVGL ---------- */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  /* ---------- Start SquareLine UI ---------- */
  ui_init();
}

void loop()
{
  uint32_t now = millis();
  lv_tick_inc(now - last_tick);
  last_tick = now;

  lv_timer_handler();
  delay(5);
}

extern "C" void updateSystemInfoData(void)
{
    char buf[256];

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    /* Modell als Text */
    const char * model = "ESP32";
    switch (chip_info.model) {
        case CHIP_ESP32:   model = "ESP32"; break;
        case CHIP_ESP32S2: model = "ESP32-S2"; break;
        case CHIP_ESP32S3: model = "ESP32-S3"; break;
        case CHIP_ESP32C3: model = "ESP32-C3"; break;
        default:           model = "Unbekannt"; break;
    }

    /* Chip ID */
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
