// GT911 polling input. Uses only the I2C driver bundled with Arduino-ESP32.
// SDA/SCL, address probing and orientation follow Waveshare's LCD-7B profile.
#include <Arduino.h>
#include "driver/i2c_master.h"
#include "board_touch.h"

static constexpr uint16_t statusRegister = 0x814E;
static constexpr uint16_t pointsRegister = 0x814F;
static i2c_master_bus_handle_t touchBus;
static i2c_master_dev_handle_t touchDevice;
static lv_point_t lastPoint = {0, 0};
static bool pressed;
static bool touchEnabled = true;

static bool readRegister(uint16_t reg, uint8_t *data, size_t size) {
  const uint8_t address[] = {uint8_t(reg >> 8), uint8_t(reg)};
  return i2c_master_transmit_receive(touchDevice, address, sizeof(address),
                                     data, size, 30) == ESP_OK;
}

static bool acknowledgeReport() {
  const uint8_t packet[] = {0x81, 0x4E, 0};
  return i2c_master_transmit(touchDevice, packet, sizeof(packet), 30) == ESP_OK;
}

static void readTouch(lv_indev_t *, lv_indev_data_t *data) {
  if (!touchEnabled) { pressed = false; data->point = lastPoint; data->state = LV_INDEV_STATE_RELEASED; return; }
  uint8_t status = 0;
  if (!readRegister(statusRegister, &status, 1)) {
    pressed = false;  // A bus failure must not leave a button held down.
  } else if (status & 0x80) {
    const uint8_t count = status & 0x0F;
    uint8_t points[5 * 8] = {};
    const bool valid = count > 0 && count <= 5 &&
                       readRegister(pointsRegister, points, count * 8);
    // Acknowledge only a ready report, after reading its coordinates.
    const bool acknowledged = acknowledgeReport();
    pressed = valid && acknowledged;
    if (pressed) {
      uint16_t x = uint16_t(points[1]) | (uint16_t(points[2]) << 8);
      uint16_t y = uint16_t(points[3]) | (uint16_t(points[4]) << 8);
#if TOUCH_SWAP_XY
      const uint16_t temp = x;
      x = y;
      y = temp;
#endif
      if (x >= 1024 || y >= 600) {
        pressed = false;
      } else {
#if TOUCH_MIRROR_X
        x = 1023 - x;
#endif
#if TOUCH_MIRROR_Y
        y = 599 - y;
#endif
        lastPoint.x = x;
        lastPoint.y = y;
      }
    }
  }
  // No new report means keep the last state. In particular, do not generate
  // spurious release/click events when LVGL polls faster than the controller.
  // A ready report with zero points is the real finger-release event.
  data->point = lastPoint;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool board_touch_begin(lv_display_t *display) {
  if (!board_i2c_begin()) return false;

  // Do not drive unassigned touch INT/RST pins or the LCD's GPIO33 reset.
  // Probe both GT911 addresses after its power-up delay instead.
  delay(200);
  const uint8_t addresses[] = {0x5D, 0x14};
  for (const uint8_t address : addresses) {
    if (i2c_master_probe(touchBus, address, 50) != ESP_OK) continue;
    i2c_device_config_t device = {};
    device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device.device_address = address;
    device.scl_speed_hz = 100000;
    if (i2c_master_bus_add_device(touchBus, &device, &touchDevice) != ESP_OK) continue;
    uint8_t product[4] = {};
    if (readRegister(0x8140, product, sizeof(product)) &&
        product[0] == '9' && product[1] == '1' && product[2] == '1') {
      Serial.printf("Touch: GT911 found at 0x%02X (SDA=7, SCL=8).\n", address);
      break;
    }
    i2c_master_bus_rm_device(touchDevice);
    touchDevice = nullptr;
  }

  if (!touchDevice) {
    Serial.println("Touch: GT911 not found at 0x5D or 0x14. Display stays active.");
    // Keep the shared I2C bus for the audio codec even without touch.
    return false;
  }

  pressed = false;
  lv_indev_t *input = lv_indev_create();
  if (!input) {
    Serial.println("Touch: LVGL input allocation failed.");
    i2c_master_bus_rm_device(touchDevice);
    i2c_del_master_bus(touchBus);
    touchDevice = nullptr;
    touchBus = nullptr;
    return false;
  }
  lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(input, display);
  lv_indev_set_read_cb(input, readTouch);
  Serial.printf("Touch ready: swap_xy=%d mirror_x=%d mirror_y=%d.\n",
                TOUCH_SWAP_XY, TOUCH_MIRROR_X, TOUCH_MIRROR_Y);
  return true;
}

bool board_i2c_begin() {
  if (touchBus) return true;
  i2c_master_bus_config_t bus = {};
  bus.i2c_port = I2C_NUM_0;
  bus.sda_io_num = GPIO_NUM_7;
  bus.scl_io_num = GPIO_NUM_8;
  bus.clk_source = I2C_CLK_SRC_DEFAULT;
  bus.glitch_ignore_cnt = 7;
  bus.flags.enable_internal_pullup = true;
  esp_err_t result = i2c_new_master_bus(&bus, &touchBus);
  if (result != ESP_OK) {
    Serial.printf("Touch: I2C initialization failed: %s\n", esp_err_to_name(result));
    return false;
  }

  return true;
}
i2c_master_bus_handle_t board_i2c_bus() { return board_i2c_begin() ? touchBus : nullptr; }
void board_touch_enable(bool enabled) { touchEnabled = enabled; pressed = false; }
