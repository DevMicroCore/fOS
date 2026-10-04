#include "LvglFAppUiApi.h"

#include <new>

namespace {
enum UiProperty : uint8_t { Text = 1, Value = 2, Checked = 3, Hidden = 4, Enabled = 5 };
enum UiMethod : uint8_t {
  Clear = 1, Focus = 2, Blur = 3, ScrollToTop = 4, ScrollToBottom = 5,
  LoadFile = 6, SaveFile = 7, Pixel = 8, Line = 9, Rect = 10, Circle = 11,
  DrawText = 12
};

bool integerArgument(const FAppValue * arguments, uint8_t index, int32_t * value)
{
  if (arguments == nullptr || value == nullptr || !arguments[index].isNumber()) return false;
  *value = arguments[index].asInteger();
  return true;
}

bool canvasColor(const FAppValue * arguments, uint8_t index, lv_color_t * color)
{
  int32_t value = 0;
  if (!integerArgument(arguments, index, &value) || value < 0 || value > 0xFFFFFF) return false;
  *color = lv_color_hex(static_cast<uint32_t>(value));
  return true;
}

const lv_font_t * canvasFont(int32_t size)
{
  switch (size) {
    case 20: return &lv_font_montserrat_20;
    case 24: return &lv_font_montserrat_24;
    case 40: return &lv_font_montserrat_40;
    default: return LV_FONT_DEFAULT;
  }
}
}

void LvglFAppUiApi::configureFileContext(fs::FS& filesystem, const char * appDirectory)
{
  filesystem_ = &filesystem;
  snprintf(appDirectory_, sizeof(appDirectory_), "%s", appDirectory == nullptr ? "" : appDirectory);
}

void LvglFAppUiApi::clearFileContext()
{
  filesystem_ = nullptr;
  appDirectory_[0] = '\0';
}

bool LvglFAppUiApi::resolveAppPath(
  const char * relativePath, char * output, size_t capacity) const
{
  if (filesystem_ == nullptr || appDirectory_[0] == '\0' || relativePath == nullptr ||
      relativePath[0] == '\0' || output == nullptr || capacity == 0) return false;
  if (relativePath[0] == '/' || relativePath[0] == '\\') return false;
  const char * segment = relativePath;
  for (const char * cursor = relativePath; ; ++cursor) {
    const unsigned char character = static_cast<unsigned char>(*cursor);
    if ((character != 0 && character < 0x20) || *cursor == '\\') return false;
    if (*cursor == '/' || *cursor == '\0') {
      const size_t length = static_cast<size_t>(cursor - segment);
      if (length == 0 || (length == 1 && segment[0] == '.') ||
          (length == 2 && segment[0] == '.' && segment[1] == '.')) return false;
      if (*cursor == '\0') break;
      segment = cursor + 1;
    }
  }
  const int written = snprintf(output, capacity, "%s/%s", appDirectory_, relativePath);
  return written >= 0 && static_cast<size_t>(written) < capacity;
}

bool LvglFAppUiApi::ensureParentDirectories(const char * fullPath)
{
  if (filesystem_ == nullptr || fullPath == nullptr) return false;
  char path[256];
  const int copied = snprintf(path, sizeof(path), "%s", fullPath);
  if (copied < 0 || static_cast<size_t>(copied) >= sizeof(path)) return false;
  const size_t rootLength = strlen(appDirectory_);
  for (size_t index = rootLength + 1; path[index] != '\0'; ++index) {
    if (path[index] != '/') continue;
    path[index] = '\0';
    if (!filesystem_->exists(path) && !filesystem_->mkdir(path)) return false;
    fs::File check = filesystem_->open(path, FILE_READ);
    const bool valid = check && check.isDirectory();
    if (check) check.close();
    if (!valid) return false;
    path[index] = '/';
  }
  return true;
}

lv_obj_t * LvglFAppUiApi::textObject(lv_obj_t * object, FAppUiObjectType type)
{
  if (object == nullptr) return nullptr;
  if (type == FAppUiObjectType::Button) return lv_obj_get_child(object, 0);
  return object;
}

bool LvglFAppUiApi::getProperty(uint16_t objectId, uint8_t property, FAppValue * result)
{
  if (result == nullptr) return false;
  lv_obj_t * object = registry_.resolve(objectId);
  const FAppUiObjectType type = registry_.objectType(objectId);
  if (object == nullptr) return false;
  switch (property) {
    case Text: {
      if (type != FAppUiObjectType::Label && type != FAppUiObjectType::Button &&
          type != FAppUiObjectType::TextArea && type != FAppUiObjectType::CheckBox &&
          type != FAppUiObjectType::Roller && type != FAppUiObjectType::DropDown) return false;
      lv_obj_t * target = textObject(object, type);
      if (target == nullptr) return false;
      if (type == FAppUiObjectType::TextArea) *result = FAppValue::string(lv_textarea_get_text(target));
      else if (type == FAppUiObjectType::CheckBox) *result = FAppValue::string(lv_checkbox_get_text(target));
      else if (type == FAppUiObjectType::Roller) {
        char selected[FAppValue::kStringCapacity];
        lv_roller_get_selected_str(target, selected, sizeof(selected));
        *result = FAppValue::string(selected);
      } else if (type == FAppUiObjectType::DropDown) {
        char selected[FAppValue::kStringCapacity];
        lv_dropdown_get_selected_str(target, selected, sizeof(selected));
        *result = FAppValue::string(selected);
      }
      else *result = FAppValue::string(lv_label_get_text(target));
      return true;
    }
    case Value:
      if (type == FAppUiObjectType::Switch || type == FAppUiObjectType::CheckBox) {
        *result = FAppValue::boolean(lv_obj_has_state(object, LV_STATE_CHECKED));
        return true;
      }
      if (type == FAppUiObjectType::Roller) {
        *result = FAppValue::integer(lv_roller_get_selected(object));
        return true;
      }
      if (type == FAppUiObjectType::DropDown) {
        *result = FAppValue::integer(lv_dropdown_get_selected(object));
        return true;
      }
      return false;
    case Checked:
      if (type != FAppUiObjectType::Switch && type != FAppUiObjectType::CheckBox) return false;
      *result = FAppValue::boolean(lv_obj_has_state(object, LV_STATE_CHECKED));
      return true;
    case Hidden:
      *result = FAppValue::boolean(lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN));
      return true;
    case Enabled:
      *result = FAppValue::boolean(!lv_obj_has_state(object, LV_STATE_DISABLED));
      return true;
    default: return false;
  }
}

bool LvglFAppUiApi::setProperty(uint16_t objectId, uint8_t property, const FAppValue& value)
{
  lv_obj_t * object = registry_.resolve(objectId);
  const FAppUiObjectType type = registry_.objectType(objectId);
  if (object == nullptr) return false;
  switch (property) {
    case Text: {
      if (type != FAppUiObjectType::Label && type != FAppUiObjectType::Button &&
          type != FAppUiObjectType::TextArea && type != FAppUiObjectType::CheckBox &&
          type != FAppUiObjectType::Roller && type != FAppUiObjectType::DropDown) return false;
      char text[FAppValue::kStringCapacity];
      value.toText(text, sizeof(text));
      lv_obj_t * target = textObject(object, type);
      if (target == nullptr) return false;
      if (type == FAppUiObjectType::TextArea) lv_textarea_set_text(target, text);
      else if (type == FAppUiObjectType::CheckBox) lv_checkbox_set_text(target, text);
      else if (type == FAppUiObjectType::Roller) lv_roller_set_options(target, text, LV_ROLLER_MODE_NORMAL);
      else if (type == FAppUiObjectType::DropDown) lv_dropdown_set_options(target, text);
      else lv_label_set_text(target, text);
      return true;
    }
    case Value:
      if (type == FAppUiObjectType::Roller || type == FAppUiObjectType::DropDown) {
        if (!value.isNumber() || value.asInteger() < 0) return false;
        const uint16_t selected = static_cast<uint16_t>(value.asInteger());
        if (type == FAppUiObjectType::Roller) lv_roller_set_selected(object, selected, LV_ANIM_OFF);
        else lv_dropdown_set_selected(object, selected);
        return true;
      }
      if (type != FAppUiObjectType::Switch && type != FAppUiObjectType::CheckBox) return false;
      if (value.truthy()) lv_obj_add_state(object, LV_STATE_CHECKED);
      else lv_obj_clear_state(object, LV_STATE_CHECKED);
      return true;
    case Checked:
      if (type != FAppUiObjectType::Switch && type != FAppUiObjectType::CheckBox) return false;
      if (value.truthy()) lv_obj_add_state(object, LV_STATE_CHECKED);
      else lv_obj_clear_state(object, LV_STATE_CHECKED);
      return true;
    case Hidden:
      if (value.truthy()) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(object);
      }
      return true;
    case Enabled:
      if (value.truthy()) lv_obj_clear_state(object, LV_STATE_DISABLED);
      else lv_obj_add_state(object, LV_STATE_DISABLED);
      return true;
    default: return false;
  }
}

bool LvglFAppUiApi::callMethod(
  uint16_t objectId,
  uint8_t method,
  const FAppValue * arguments,
  uint8_t argumentCount,
  FAppValue * result)
{
  if (result == nullptr) return false;
  lv_obj_t * object = registry_.resolve(objectId);
  const FAppUiObjectType type = registry_.objectType(objectId);
  if (object == nullptr) return false;
  if (method == LoadFile || method == SaveFile) {
    if (type != FAppUiObjectType::TextArea || argumentCount != 1 || arguments == nullptr ||
        arguments[0].type() != FAppValueType::String) return false;
    char path[256];
    if (!resolveAppPath(arguments[0].asString(), path, sizeof(path))) {
      *result = FAppValue::boolean(false);
      return true;
    }
    if (method == LoadFile) {
      fs::File file = filesystem_->open(path, FILE_READ);
      if (!file || file.isDirectory()) {
        if (file) file.close();
        *result = FAppValue::boolean(false);
        return true;
      }
      constexpr size_t kMaximumTextBytes = 4000;
      constexpr size_t kBufferBytes = 4032;
      char * content = new (std::nothrow) char[kBufferBytes];
      if (content == nullptr) {
        file.close();
        *result = FAppValue::boolean(false);
        return true;
      }
      const size_t fileSize = file.size();
      const size_t wanted = fileSize < kMaximumTextBytes ? fileSize : kMaximumTextBytes;
      const size_t count = file.read(reinterpret_cast<uint8_t *>(content), wanted);
      file.close();
      if (count != wanted) {
        delete[] content;
        *result = FAppValue::boolean(false);
        return true;
      }
      size_t length = count;
      if (fileSize > count) {
        const char marker[] = "\n\n[Datei gekuerzt]";
        memcpy(content + length, marker, sizeof(marker) - 1);
        length += sizeof(marker) - 1;
      }
      content[length] = '\0';
      lv_textarea_set_text(object, content);
      delete[] content;
      *result = FAppValue::boolean(true);
      return true;
    }
    if (!ensureParentDirectories(path)) {
      *result = FAppValue::boolean(false);
      return true;
    }
    if (filesystem_->exists(path)) filesystem_->remove(path);
    fs::File file = filesystem_->open(path, FILE_WRITE);
    if (!file) {
      *result = FAppValue::boolean(false);
      return true;
    }
    const char * text = lv_textarea_get_text(object);
    const size_t length = strlen(text);
    const size_t written = file.write(reinterpret_cast<const uint8_t *>(text), length);
    file.close();
    *result = FAppValue::boolean(written == length);
    return true;
  }
  if (type == FAppUiObjectType::Canvas) {
    lv_color_t color;
    int32_t values[5] = {0, 0, 0, 0, 0};
    switch (method) {
      case Clear:
        if (argumentCount != 1 || !canvasColor(arguments, 0, &color)) return false;
        lv_canvas_fill_bg(object, color, LV_OPA_COVER);
        break;
      case Pixel:
        if (argumentCount != 3 || !integerArgument(arguments, 0, &values[0]) ||
            !integerArgument(arguments, 1, &values[1]) || !canvasColor(arguments, 2, &color)) return false;
        lv_canvas_set_px_color(object, values[0], values[1], color);
        break;
      case Line: {
        if (argumentCount != 6 || !integerArgument(arguments, 0, &values[0]) ||
            !integerArgument(arguments, 1, &values[1]) || !integerArgument(arguments, 2, &values[2]) ||
            !integerArgument(arguments, 3, &values[3]) || !canvasColor(arguments, 4, &color) ||
            !integerArgument(arguments, 5, &values[4]) || values[4] < 1 || values[4] > 64) return false;
        lv_draw_line_dsc_t descriptor;
        lv_draw_line_dsc_init(&descriptor);
        descriptor.color = color;
        descriptor.width = values[4];
        descriptor.round_start = 1;
        descriptor.round_end = 1;
        const lv_point_t points[2] = {
          {static_cast<lv_coord_t>(values[0]), static_cast<lv_coord_t>(values[1])},
          {static_cast<lv_coord_t>(values[2]), static_cast<lv_coord_t>(values[3])}
        };
        lv_canvas_draw_line(object, points, 2, &descriptor);
        break;
      }
      case Rect: {
        if (argumentCount != 6 || !integerArgument(arguments, 0, &values[0]) ||
            !integerArgument(arguments, 1, &values[1]) || !integerArgument(arguments, 2, &values[2]) ||
            !integerArgument(arguments, 3, &values[3]) || !canvasColor(arguments, 4, &color) ||
            values[2] <= 0 || values[3] <= 0) return false;
        lv_draw_rect_dsc_t descriptor;
        lv_draw_rect_dsc_init(&descriptor);
        if (arguments[5].truthy()) {
          descriptor.bg_color = color;
          descriptor.bg_opa = LV_OPA_COVER;
          descriptor.border_opa = LV_OPA_TRANSP;
        } else {
          descriptor.bg_opa = LV_OPA_TRANSP;
          descriptor.border_color = color;
          descriptor.border_opa = LV_OPA_COVER;
          descriptor.border_width = 1;
        }
        lv_canvas_draw_rect(object, values[0], values[1], values[2], values[3], &descriptor);
        break;
      }
      case Circle: {
        if (argumentCount != 5 || !integerArgument(arguments, 0, &values[0]) ||
            !integerArgument(arguments, 1, &values[1]) || !integerArgument(arguments, 2, &values[2]) ||
            !canvasColor(arguments, 3, &color) || values[2] <= 0) return false;
        if (arguments[4].truthy()) {
          lv_draw_rect_dsc_t descriptor;
          lv_draw_rect_dsc_init(&descriptor);
          descriptor.bg_color = color;
          descriptor.bg_opa = LV_OPA_COVER;
          descriptor.border_opa = LV_OPA_TRANSP;
          descriptor.radius = LV_RADIUS_CIRCLE;
          const int32_t diameter = values[2] * 2 + 1;
          lv_canvas_draw_rect(object, values[0] - values[2], values[1] - values[2], diameter, diameter, &descriptor);
        } else {
          lv_draw_arc_dsc_t descriptor;
          lv_draw_arc_dsc_init(&descriptor);
          descriptor.color = color;
          descriptor.width = 1;
          lv_canvas_draw_arc(object, values[0], values[1], values[2], 0, 360, &descriptor);
        }
        break;
      }
      case DrawText: {
        if (argumentCount != 5 || !integerArgument(arguments, 0, &values[0]) ||
            !integerArgument(arguments, 1, &values[1]) || arguments[2].type() != FAppValueType::String ||
            !canvasColor(arguments, 3, &color) || !integerArgument(arguments, 4, &values[2])) return false;
        lv_draw_label_dsc_t descriptor;
        lv_draw_label_dsc_init(&descriptor);
        descriptor.color = color;
        descriptor.font = canvasFont(values[2]);
        const lv_coord_t width = lv_obj_get_width(object) - values[0];
        lv_canvas_draw_text(object, values[0], values[1], width > 0 ? width : 1, &descriptor, arguments[2].asString());
        break;
      }
      default: return false;
    }
    lv_obj_invalidate(object);
    *result = FAppValue::boolean(true);
    return true;
  }
  if (argumentCount != 0) return false;
  switch (method) {
    case Clear:
      if (type == FAppUiObjectType::TextArea) lv_textarea_set_text(object, "");
      else if (type == FAppUiObjectType::Label) lv_label_set_text(object, "");
      else return false;
      break;
    case Focus:
      lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_state(object, LV_STATE_FOCUSED);
      lv_group_focus_obj(object);
      lv_obj_move_foreground(object);
      break;
    case Blur: lv_obj_clear_state(object, LV_STATE_FOCUSED); break;
    case ScrollToTop: lv_obj_scroll_to_y(object, 0, LV_ANIM_OFF); break;
    case ScrollToBottom: lv_obj_scroll_to_y(object, LV_COORD_MAX, LV_ANIM_OFF); break;
    default: return false;
  }
  *result = FAppValue::nil();
  return true;
}
