#pragma once

#include "FAppRuntime.h"
#include "FAppUiRegistry.h"

class FAppEventBridge {
 public:
  FAppEventBridge() : runtime_(nullptr), registry_(nullptr) {}
  void attach(FAppRuntime& runtime, FAppUiRegistry& registry);
  void clear();

 private:
  static void eventCallback(lv_event_t * event);
  FAppRuntime * runtime_;
  FAppUiRegistry * registry_;
};
