#pragma once

#include <Arduino.h>
#include <FS.h>

#include "FAppNativeApi.h"

class FAppHttpApi : public FAppNativeApi {
 public:
  static constexpr size_t kMaximumResponseBytes = 32768;
  static constexpr uint32_t kMinimumTimerIntervalMs = 100;
  static constexpr uint32_t kMaximumTimerIntervalMs = 86400000;

  using AudioPlayCallback = bool (*)(const char * path);
  using RestartCallback = void (*)();
  using TimerStartCallback = bool (*)(uint32_t intervalMs);

  FAppHttpApi();
  void configure(
    fs::FS& filesystem,
    const char * appDirectory,
    AudioPlayCallback audioPlay,
    RestartCallback restart,
    TimerStartCallback timerStart);
  bool call(
    FAppNativeFunction function,
    const FAppValue * arguments,
    uint8_t argumentCount,
    FAppValue * result) override;
  const char * errorMessage() const override { return error_; }
  void reset() override;
  void setEventContext(const FAppEventContext& context) override { eventContext_ = context; }
  void clearEventContext() override { eventContext_ = FAppEventContext{}; }

 private:
  bool get(const char * url, FAppValue * result);
  bool json(const char * path, FAppValue * result);
  bool text(int32_t offset, FAppValue * result);
  bool toTextValue(const FAppValue& value, FAppValue * result);
  bool numberParse(const char * input, FAppValue * result);
  bool roundNumber(const FAppValue& value, FAppValue * result);
  bool stringLength(const char * input, FAppValue * result);
  bool stringSlice(const char * input, int32_t start, int32_t length, FAppValue * result);
  bool stringLastIndex(const char * input, const char * needle, FAppValue * result);
  bool mathEval(const char * expression, FAppValue * result);
  bool encode(const char * input, FAppValue * result);
  bool weekday(const char * isoDate, FAppValue * result);
  bool fileRead(const char * relativePath, FAppValue * result);
  bool fileWrite(const char * relativePath, const char * data, FAppValue * result);
  bool fileList(const char * relativePath, FAppValue * result);
  bool audioPlay(const char * relativePath, FAppValue * result);
  bool resolveAppPath(const char * relativePath, char * output, size_t capacity) const;
  bool navigateJson(const char * path, size_t * valueStart) const;
  bool findObjectKey(size_t objectStart, const char * key, size_t * valueStart) const;
  bool findArrayIndex(size_t arrayStart, int index, size_t * valueStart) const;
  bool extractJsonValue(size_t valueStart, char * output, size_t capacity) const;
  size_t skipWhitespace(size_t offset) const;
  size_t skipJsonString(size_t offset) const;
  size_t skipJsonValue(size_t offset) const;
  void setError(const char * message) const;

  String response_;
  int lastStatus_;
  fs::FS * filesystem_;
  AudioPlayCallback audioPlayCallback_;
  RestartCallback restartCallback_;
  TimerStartCallback timerStartCallback_;
  char appDirectory_[160];
  mutable char error_[96];
  FAppEventContext eventContext_;
};
