#pragma once

#include <stdint.h>

#include "FAppValue.h"

enum class FAppNativeFunction : uint8_t {
  HttpGet = 1,
  HttpStatus = 2,
  HttpJson = 3,
  HttpText = 4,
  UrlEncode = 5,
  DateWeekday = 6,
  FileRead = 7,
  FileWrite = 8,
  AudioPlay = 9,
  WifiStatus = 10,
  SystemRestart = 11,
  TimerStart = 12,
  SerialPrintf = 13,
  Text = 14,
  NumberParse = 15,
  Round = 16,
  StringLength = 17,
  StringSlice = 18,
  MathEval = 19,
  StringLastIndex = 20,
  FileList = 21,
  EventX = 22,
  EventY = 23,
  EventScreenX = 24,
  EventScreenY = 25,
  EventPressed = 26
};

struct FAppEventContext {
  int16_t x = 0;
  int16_t y = 0;
  int16_t screenX = 0;
  int16_t screenY = 0;
  bool pressed = false;
  bool pointer = false;
};

class FAppNativeApi {
 public:
  virtual ~FAppNativeApi() = default;
  virtual bool call(
    FAppNativeFunction function,
    const FAppValue * arguments,
    uint8_t argumentCount,
    FAppValue * result) = 0;
  virtual const char * errorMessage() const = 0;
  virtual void reset() = 0;
  virtual void setEventContext(const FAppEventContext&) {}
  virtual void clearEventContext() {}
};
