#include "FAppKeyboardController.h"

namespace {
int clampCoordinate(int value, int maximum)
{
  if (value < 0) return 0;
  return value > maximum ? maximum : value;
}
}

FAppKeyboardController::FAppKeyboardController()
{
  for (Binding& binding : bindings_) binding.owner = this;
}

void FAppKeyboardController::detach(Binding& binding)
{
  if (binding.textarea != nullptr)
    lv_obj_remove_event_cb_with_user_data(binding.textarea, targetEvent, &binding);
  if (binding.logicalParent != nullptr && binding.logicalParent != root_)
    lv_obj_remove_event_cb_with_user_data(binding.logicalParent, parentEvent, &binding);
  if (binding.keyboard != nullptr)
    lv_obj_remove_event_cb_with_user_data(binding.keyboard, keyboardEvent, &binding);
  binding = Binding{};
  binding.owner = this;
}

void FAppKeyboardController::reset()
{
  for (Binding& binding : bindings_) detach(binding);
  root_ = nullptr;
}

void FAppKeyboardController::configure(lv_obj_t * root)
{
  reset();
  root_ = root;
}

FAppKeyboardController::Binding * FAppKeyboardController::find(lv_obj_t * keyboard)
{
  if (keyboard == nullptr) return nullptr;
  for (Binding& binding : bindings_) if (binding.keyboard == keyboard) return &binding;
  return nullptr;
}

const FAppKeyboardController::Binding * FAppKeyboardController::find(lv_obj_t * keyboard) const
{
  if (keyboard == nullptr) return nullptr;
  for (const Binding& binding : bindings_) if (binding.keyboard == keyboard) return &binding;
  return nullptr;
}

bool FAppKeyboardController::owns(lv_obj_t * keyboard) const { return find(keyboard) != nullptr; }

bool FAppKeyboardController::requestedHidden(lv_obj_t * keyboard) const
{
  const Binding * binding = find(keyboard);
  return binding == nullptr || binding->hidden;
}

lv_obj_t * FAppKeyboardController::create(
  lv_obj_t * logicalParent, int x, int y, int width, int height, bool hidden)
{
  if (root_ == nullptr || logicalParent == nullptr || width <= 0 || height <= 0 ||
      width > 32767 || height > 32767 || x < -32768 || x > 32767 || y < -32768 || y > 32767) return nullptr;
  Binding * slot = nullptr;
  for (Binding& binding : bindings_) if (binding.keyboard == nullptr) { slot = &binding; break; }
  if (slot == nullptr) return nullptr;
  // Keep keyboards in AppContentArea, outside small/scrolling dialog panels.
  // Coordinates still use the logical parent's content area (layout.ui).
  slot->keyboard = lv_keyboard_create(root_);
  if (slot->keyboard == nullptr) return nullptr;
  slot->logicalParent = logicalParent;
  slot->x = static_cast<int16_t>(x);
  slot->y = static_cast<int16_t>(y);
  slot->width = static_cast<int16_t>(width);
  slot->height = static_cast<int16_t>(height);
  slot->hidden = hidden;
  lv_obj_set_align(slot->keyboard, LV_ALIGN_TOP_LEFT);
  lv_obj_add_flag(slot->keyboard, LV_OBJ_FLAG_FLOATING);
  lv_obj_clear_flag(slot->keyboard, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE));
  lv_obj_set_size(slot->keyboard, width, height);
  lv_obj_set_pos(slot->keyboard, x, y);
  lv_obj_add_flag(slot->keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(slot->keyboard, keyboardEvent, LV_EVENT_DELETE, slot);
  if (logicalParent != root_)
    lv_obj_add_event_cb(logicalParent, parentEvent, LV_EVENT_DELETE, slot);
  return slot->keyboard;
}

bool FAppKeyboardController::bind(lv_obj_t * keyboard, lv_obj_t * textarea)
{
  Binding * binding = find(keyboard);
  if (binding == nullptr || textarea == nullptr || !lv_obj_check_type(textarea, &lv_textarea_class)) return false;
  if (binding->textarea == textarea) return true;
  if (binding->textarea != nullptr)
    lv_obj_remove_event_cb_with_user_data(binding->textarea, targetEvent, binding);
  binding->textarea = textarea;
  lv_keyboard_set_textarea(keyboard, textarea);
  lv_obj_add_event_cb(textarea, targetEvent, LV_EVENT_ALL, binding);
  return true;
}

bool FAppKeyboardController::treeVisible(lv_obj_t * object)
{
  if (object == nullptr) return false;
  for (lv_obj_t * current = object; current != nullptr; current = lv_obj_get_parent(current)) {
    if (lv_obj_has_flag(current, LV_OBJ_FLAG_HIDDEN)) return false;
  }
  return true;
}

void FAppKeyboardController::synchronize(Binding& binding)
{
  if (binding.keyboard == nullptr) return;
  bool visible = !binding.hidden && treeVisible(root_) && treeVisible(binding.logicalParent);
  if (binding.textarea != nullptr) visible = visible && treeVisible(binding.textarea);
  if (!visible) {
    lv_obj_add_flag(binding.keyboard, LV_OBJ_FLAG_HIDDEN);
    binding.visible = false;
    return;
  }
  lv_area_t parentArea, rootArea;
  lv_obj_get_content_coords(binding.logicalParent, &parentArea);
  lv_obj_get_content_coords(root_, &rootArea);
  const int rootWidth = lv_area_get_width(&rootArea);
  const int rootHeight = lv_area_get_height(&rootArea);
  if (rootWidth <= 0 || rootHeight <= 0) return;
  const int width = binding.width < rootWidth ? binding.width : rootWidth;
  const int height = binding.height < rootHeight ? binding.height : rootHeight;
  int x = parentArea.x1 - rootArea.x1 + binding.x;
  int y = parentArea.y1 - rootArea.y1 + binding.y;
  if (binding.logicalParent != root_) {
    x -= lv_obj_get_scroll_x(binding.logicalParent);
    y -= lv_obj_get_scroll_y(binding.logicalParent);
  }
  // An out-of-screen keyboard is unusable: retain its requested size where
  // possible and place it inside the actual app viewport instead of clipping.
  x = clampCoordinate(x, rootWidth - width);
  y = clampCoordinate(y, rootHeight - height);
  lv_obj_set_size(binding.keyboard, width, height);
  lv_obj_set_pos(binding.keyboard, x, y);
  lv_obj_clear_flag(binding.keyboard, LV_OBJ_FLAG_HIDDEN);
  if (!binding.visible) lv_obj_move_foreground(binding.keyboard);
  binding.visible = true;
}

void FAppKeyboardController::update()
{
  if (root_ == nullptr) return;
  bool any = false;
  for (const Binding& binding : bindings_) if (binding.keyboard != nullptr) { any = true; break; }
  if (!any) return;
  lv_obj_update_layout(root_);
  for (Binding& binding : bindings_) synchronize(binding);
  // Position/style setters defer coordinate calculation in LVGL. Commit it
  // before callers inspect bounds or draw the newly shown keyboard.
  lv_obj_update_layout(root_);
}

void FAppKeyboardController::show(Binding& binding)
{
  binding.hidden = false;
  if (binding.textarea != nullptr) lv_keyboard_set_textarea(binding.keyboard, binding.textarea);
  update();
  if (binding.visible) lv_obj_move_foreground(binding.keyboard);
}

bool FAppKeyboardController::setHidden(lv_obj_t * keyboard, bool hidden)
{
  Binding * binding = find(keyboard);
  if (binding == nullptr) return false;
  if (!hidden) show(*binding);
  else { binding->hidden = true; synchronize(*binding); }
  return true;
}

void FAppKeyboardController::targetEvent(lv_event_t * event)
{
  Binding * binding = static_cast<Binding *>(lv_event_get_user_data(event));
  if (binding == nullptr || binding->keyboard == nullptr || lv_event_get_target_obj(event) != binding->textarea) return;
  switch (lv_event_get_code(event)) {
    case LV_EVENT_CLICKED:
    case LV_EVENT_FOCUSED: binding->owner->show(*binding); break;
    case LV_EVENT_DELETE:
      lv_keyboard_set_textarea(binding->keyboard, nullptr);
      binding->textarea = nullptr;
      binding->hidden = true;
      binding->owner->synchronize(*binding);
      break;
    default: break;
  }
}

void FAppKeyboardController::parentEvent(lv_event_t * event)
{
  Binding * binding = static_cast<Binding *>(lv_event_get_user_data(event));
  if (binding == nullptr || lv_event_get_target_obj(event) != binding->logicalParent) return;
  binding->logicalParent = nullptr;
  if (binding->keyboard != nullptr) {
    binding->visible = false;
    lv_obj_add_flag(binding->keyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

void FAppKeyboardController::keyboardEvent(lv_event_t * event)
{
  Binding * binding = static_cast<Binding *>(lv_event_get_user_data(event));
  if (binding == nullptr || lv_event_get_target_obj(event) != binding->keyboard) return;
  // The keyboard itself is already being deleted; detach only its other hooks.
  binding->keyboard = nullptr;
  binding->owner->detach(*binding);
}
