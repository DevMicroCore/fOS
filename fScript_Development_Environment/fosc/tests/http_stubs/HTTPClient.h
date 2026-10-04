#pragma once

#include "Arduino.h"
#include "WiFi.h"

class HTTPClient {
 public:
  inline static String testBody = "{}";
  inline static int testStatus = 200;
  void setConnectTimeout(int) {}
  void setTimeout(int) {}
  void setUserAgent(const String&) {}
  bool begin(WiFiClient&, const char *) { return true; }
  int GET() { return testStatus; }
  int getSize() const { return static_cast<int>(testBody.length()); }
  String getString() const { return testBody; }
  void end() {}
  static String errorToString(int) { return String("HTTP error"); }
};
