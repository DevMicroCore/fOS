#pragma once

#include "Arduino.h"

#define WL_CONNECTED 3

class WiFiClient {};
class WiFiClass {
 public:
  int status() const { return testStatus; }
  int testStatus = WL_CONNECTED;
};
inline WiFiClass WiFi;
