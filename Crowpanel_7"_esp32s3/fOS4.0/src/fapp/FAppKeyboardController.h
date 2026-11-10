#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

// Only layout.ui keyboards use this controller. Native fOS keyboards keep
// their existing parents, callbacks and lifetime management.
class FAppKeyboardController {
 public:
  static constexpr uint8_t kCapacity = 8;
  FAppKeyboardController();
  void reset();
  void configure(lv_obj_t * root);
  lv_obj_t * create(lv_obj_t * logicalParent, int x, int y, int width, int height, bool hidden);
  bool bind(lv_obj_t * keyboard, lv_obj_t * textarea);
  bool owns(lv_obj_t * keyboard) const;
  bool requestedHidden(lv_obj_t * keyboard) const;
  bool setHidden(lv_obj_t * keyboard, bool hidden);
  void update();

 private:
  struct Binding {
    FAppKeyboardController * owner = nullptr;
    lv_obj_t * keyboard = nullptr;
    lv_obj_t * logicalParent = nullptr;
    lv_obj_t * textarea = nullptr;
    int16_t x = 0, y = 0, width = 0, height = 0;
    bool hidden = false;
    bool visible = false;
  };
  static bool treeVisible(lv_obj_t * object);
  static void targetEvent(lv_event_t * event);
  static void parentEvent(lv_event_t * event);
  static void keyboardEvent(lv_event_t * event);
  Binding * find(lv_obj_t * keyboard);
  const Binding * find(lv_obj_t * keyboard) const;
  void detach(Binding& binding);
  void show(Binding& binding);
  void synchronize(Binding& binding);
  lv_obj_t * root_ = nullptr;
  Binding bindings_[kCapacity];
};
