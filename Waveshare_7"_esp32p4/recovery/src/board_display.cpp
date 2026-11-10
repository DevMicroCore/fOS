// Display-only adapter using the ESP-IDF drivers included in Arduino-ESP32.
// Panel timings/register values follow Waveshare's official LCD-7B profile.
#include <Arduino.h>
#include <cstring>
#include "esp_cache.h"
#include "esp_err.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "board_display.h"
#include "board_touch.h"
#include "BoardOrientation.h"

static constexpr int width = 1024;
static constexpr int height = 600;
static constexpr int resetPin = 33;
static constexpr int backlightPin = 32;  // Active LOW on LCD-7B.
static esp_ldo_channel_handle_t phyPower;
static esp_lcd_dsi_bus_handle_t dsiBus;
static esp_lcd_panel_io_handle_t commandIO;
static esp_lcd_panel_handle_t panel;
static uint16_t *framebuffer;

static bool pwmReady;
void board_display_brightness(uint8_t level) {
  if (!pwmReady) {
    pwmReady = ledcAttach(backlightPin, 5000, 10);
    if (pwmReady) ledcOutputInvert(backlightPin, true);
  }
  if (pwmReady) ledcWrite(backlightPin, (uint32_t(level) * 1023U) / 255U);
  else digitalWrite(backlightPin, level ? LOW : HIGH);
}
void board_display_backlight(bool on) { board_display_brightness(on ? 255 : 0); }
void board_display_power(bool enabled) {
  // The DPI handle has no panel-specific sleep callback. Keep scan-out alive;
  // gate the backlight and input. This avoids unsupported raw-panel APIs.
  board_touch_enable(enabled);
  if (!enabled) board_display_brightness(0);
}

void board_display_begin() {
  // Keep the backlight dark until the complete first UI frame is rendered.
  digitalWrite(backlightPin, HIGH);
  pinMode(backlightPin, OUTPUT);
  board_display_backlight(false);
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW);
  delay(10);
  digitalWrite(resetPin, HIGH);
  delay(20);

  esp_ldo_channel_config_t power = {};
  power.chan_id = 3;
  power.voltage_mv = 2500;
  ESP_ERROR_CHECK(esp_ldo_acquire_channel(&power, &phyPower));

  esp_lcd_dsi_bus_config_t bus = {};
  bus.bus_id = 0;
  bus.num_data_lanes = 2;
  // Let IDF select the reference clock for the selected P4 silicon revision.
  bus.phy_clk_src = static_cast<mipi_dsi_phy_pllref_clock_source_t>(0);
  bus.lane_bit_rate_mbps = 1000;
  ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus, &dsiBus));

  esp_lcd_dbi_io_config_t dbi = {};
  dbi.virtual_channel = 0;
  dbi.lcd_cmd_bits = 8;
  dbi.lcd_param_bits = 8;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsiBus, &dbi, &commandIO));

  esp_lcd_dpi_panel_config_t dpi = {};
  dpi.virtual_channel = 0;
  dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
  dpi.dpi_clock_freq_mhz = 52;
  dpi.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
  dpi.num_fbs = 1;
  dpi.video_timing.h_size = width;
  dpi.video_timing.v_size = height;
  dpi.video_timing.hsync_pulse_width = 10;
  dpi.video_timing.hsync_back_porch = 160;
  dpi.video_timing.hsync_front_porch = 160;
  dpi.video_timing.vsync_pulse_width = 1;
  dpi.video_timing.vsync_back_porch = 23;
  dpi.video_timing.vsync_front_porch = 12;
  // CPU copies below are synchronous. No DMA2D/PPA integration is needed.
  dpi.flags.use_dma2d = false;
  ESP_ERROR_CHECK(esp_lcd_new_panel_dpi(dsiBus, &dpi, &panel));

  static const uint8_t commands[][2] = {
    {0xB2, 0x10},  // Two DSI lanes.
    {0x80, 0x8B}, {0x81, 0x78}, {0x82, 0x84}, {0x83, 0x88},
    {0x84, 0xA8}, {0x85, 0xE3}, {0x86, 0x88}
  };
  for (const auto &command : commands) {
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(commandIO, command[0], &command[1], 1));
  }
  ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(commandIO, 0x11, nullptr, 0));
  delay(120);

  // This is the raw DPI panel API; initialization starts continuous scan-out.
  // Do not call optional wrapper-panel APIs such as disp_on_off on this handle.
  ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel, 1,
                                                  reinterpret_cast<void **>(&framebuffer)));
  memset(framebuffer, 0, width * height * sizeof(uint16_t));
  ESP_ERROR_CHECK(esp_cache_msync(framebuffer, width * height * sizeof(uint16_t),
                                 ESP_CACHE_MSYNC_FLAG_DIR_C2M));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  Serial.println("EK79007 display initialized: 1024x600 RGB565, rotated 180 degrees.");
}

void board_display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
  const size_t rowBytes = (area->x2 - area->x1 + 1) * sizeof(uint16_t);
  const size_t rows = area->y2 - area->y1 + 1;
  fos_copy_rgb565_rotated_180(framebuffer, width, height,
      area->x1, area->y1, area->x2, area->y2,
      reinterpret_cast<const uint16_t *>(pixels));
  uint16_t *first = framebuffer + (height - 1 - area->y2) * width +
                    (width - 1 - area->x2);
  // Synchronize PSRAM cache before the LCD DMA reads the modified pixels.
  const size_t changedBytes = (rows - 1) * width * sizeof(uint16_t) + rowBytes;
  ESP_ERROR_CHECK(esp_cache_msync(first, changedBytes,
                                 ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                                 ESP_CACHE_MSYNC_FLAG_UNALIGNED));
  // The CPU copy has completed; LVGL may immediately reuse its draw buffer.
  lv_display_flush_ready(display);
}
