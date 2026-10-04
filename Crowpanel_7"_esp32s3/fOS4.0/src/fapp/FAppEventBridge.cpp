#include "FAppEventBridge.h"

#define FAPP_EVENT_CLICK 1U
#define FAPP_EVENT_CHANGED 2U
#define FAPP_EVENT_VALUE_CHANGED 3U
#define FAPP_EVENT_PRESSED 4U
#define FAPP_EVENT_RELEASED 5U
#define FAPP_EVENT_READY 6U
#define FAPP_EVENT_CANCEL 7U
#define FAPP_EVENT_POINTER_DOWN 8U
#define FAPP_EVENT_POINTER_MOVE 9U
#define FAPP_EVENT_POINTER_UP 10U

namespace {
void queuePointer(FAppRuntime * runtime, uint16_t objectId, uint8_t eventType, lv_obj_t * object, bool pressed)
{
  if (runtime == nullptr || object == nullptr || !runtime->hasEventHandler(objectId, eventType)) return;
  lv_indev_t * input = lv_indev_get_act();
  if (input == nullptr) return;
  lv_point_t point;
  lv_indev_get_point(input, &point);
  lv_area_t bounds;
  lv_obj_get_coords(object, &bounds);
  runtime->queuePointerEvent(
    objectId, eventType,
    static_cast<int16_t>(point.x - bounds.x1), static_cast<int16_t>(point.y - bounds.y1),
    static_cast<int16_t>(point.x), static_cast<int16_t>(point.y), pressed);
}
}

void FAppEventBridge::attach(FAppRuntime& runtime, FAppUiRegistry& registry)
{
  runtime_ = &runtime;
  registry_ = &registry;
  for (uint16_t objectId = 1; objectId <= registry.capacity(); ++objectId) {
    lv_obj_t * object = registry.resolve(objectId);
    if (object == nullptr) continue;
    if (runtime.hasEventHandler(objectId, FAPP_EVENT_CLICK) || runtime.hasEventHandler(objectId, FAPP_EVENT_CHANGED) ||
        runtime.hasEventHandler(objectId, FAPP_EVENT_VALUE_CHANGED) || runtime.hasEventHandler(objectId, FAPP_EVENT_PRESSED) ||
        runtime.hasEventHandler(objectId, FAPP_EVENT_RELEASED) || runtime.hasEventHandler(objectId, FAPP_EVENT_READY) ||
        runtime.hasEventHandler(objectId, FAPP_EVENT_CANCEL) ||
        runtime.hasEventHandler(objectId, FAPP_EVENT_POINTER_DOWN) ||
        runtime.hasEventHandler(objectId, FAPP_EVENT_POINTER_MOVE) ||
        runtime.hasEventHandler(objectId, FAPP_EVENT_POINTER_UP)) {
      lv_obj_add_event_cb(object, eventCallback, LV_EVENT_ALL, this);
    }
  }
}

void FAppEventBridge::clear()
{
  runtime_ = nullptr;
  registry_ = nullptr;
}

void FAppEventBridge::eventCallback(lv_event_t * event)
{
  FAppEventBridge * bridge = static_cast<FAppEventBridge *>(lv_event_get_user_data(event));
  if (bridge == nullptr || bridge->runtime_ == nullptr || bridge->registry_ == nullptr) return;
  const uint16_t objectId = bridge->registry_->objectIdFor(lv_event_get_target(event));
  if (objectId == 0) return;
  switch (lv_event_get_code(event)) {
    case LV_EVENT_CLICKED: bridge->runtime_->queueEvent(objectId, FAPP_EVENT_CLICK); break;
    case LV_EVENT_VALUE_CHANGED:
      bridge->runtime_->queueEvent(objectId, FAPP_EVENT_VALUE_CHANGED);
      bridge->runtime_->queueEvent(objectId, FAPP_EVENT_CHANGED);
      break;
    case LV_EVENT_PRESSED:
      bridge->runtime_->queueEvent(objectId, FAPP_EVENT_PRESSED);
      queuePointer(bridge->runtime_, objectId, FAPP_EVENT_POINTER_DOWN, lv_event_get_target(event), true);
      break;
    case LV_EVENT_PRESSING:
      queuePointer(bridge->runtime_, objectId, FAPP_EVENT_POINTER_MOVE, lv_event_get_target(event), true);
      break;
    case LV_EVENT_RELEASED:
      bridge->runtime_->queueEvent(objectId, FAPP_EVENT_RELEASED);
      queuePointer(bridge->runtime_, objectId, FAPP_EVENT_POINTER_UP, lv_event_get_target(event), false);
      break;
    case LV_EVENT_PRESS_LOST:
      queuePointer(bridge->runtime_, objectId, FAPP_EVENT_POINTER_UP, lv_event_get_target(event), false);
      break;
    case LV_EVENT_READY: bridge->runtime_->queueEvent(objectId, FAPP_EVENT_READY); break;
    case LV_EVENT_CANCEL: bridge->runtime_->queueEvent(objectId, FAPP_EVENT_CANCEL); break;
    default: break;
  }
}
