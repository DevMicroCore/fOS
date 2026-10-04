#pragma once

#include <stddef.h>
#include <stdint.h>
#include <lvgl.h>

enum class FAppUiObjectType : uint8_t {
  Unknown = 0,
  Label,
  Button,
  TextArea,
  Switch,
  CheckBox,
  Panel,
  Roller,
  DropDown,
  Keyboard,
  Canvas
};

class FAppUiRegistry {
 public:
  static constexpr size_t kCapacity = 64;

  FAppUiRegistry();

  void clear();
  bool registerObject(uint16_t objectId, lv_obj_t * object, FAppUiObjectType type);
  lv_obj_t * resolve(uint16_t objectId) const;
  uint16_t objectIdFor(lv_obj_t * object) const;
  FAppUiObjectType objectType(uint16_t objectId) const;
  size_t count() const;
  constexpr size_t capacity() const { return kCapacity; }

 private:
  lv_obj_t * objects_[kCapacity];
  FAppUiObjectType types_[kCapacity];
  uint8_t count_;
};
