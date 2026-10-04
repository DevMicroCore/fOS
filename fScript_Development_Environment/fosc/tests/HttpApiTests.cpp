#include <cstdlib>
#include <iostream>
#include <string>

#include <HTTPClient.h>
#include "src/fapp/FAppHttpApi.h"

bool gOtaNetworkBusy = false;
extern "C" bool OTARecovery_IsNetworkBusy(void) { return gOtaNetworkBusy; }

bool gRestartRequested = false;
std::uint32_t gTimerInterval = 0;
std::string gAudioPath;
bool playAudio(const char * path) { gAudioPath = path == nullptr ? "" : path; return true; }
void requestRestart() { gRestartRequested = true; }
bool startTimer(std::uint32_t interval) { gTimerInterval = interval; return true; }

namespace {
int failures = 0;
void require(bool condition, const char * message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

FAppValue call(FAppHttpApi * api, FAppNativeFunction function, const FAppValue * arguments, uint8_t count)
{
  FAppValue result;
  require(api->call(function, arguments, count, &result), api->errorMessage());
  return result;
}
}

int main()
{
  HTTPClient::testBody =
    "{\"current\":{\"temperature_2m\":18.4},\"results\":[{\"name\":\"Chemnitz\","
    "\"latitude\":50.83}],\"daily\":{\"time\":[\"2026-08-28\",\"2026-08-29\"]}}";
  FAppHttpApi api;
  fs::FS filesystem;
  filesystem.addFile("/apps/test/sound.mp3", {'m', 'p', '3'});
  api.configure(filesystem, "/apps/test", playAudio, requestRestart, startTimer);
  FAppValue url = FAppValue::string("https://example.com/weather.json");
  gOtaNetworkBusy = true;
  FAppValue blockedResult;
  require(api.call(FAppNativeFunction::HttpGet, &url, 1, &blockedResult),
          "OTA network lock should be a contained HTTP result");
  require(!blockedResult.truthy(), "HTTP GET must wait while OTA owns the network");
  require(std::string(api.errorMessage()) == "OTA network operation is active",
          "OTA network lock error differs");
  gOtaNetworkBusy = false;
  require(call(&api, FAppNativeFunction::HttpGet, &url, 1).truthy(), "HTTP GET should succeed");
  FAppValue path = FAppValue::string("current.temperature_2m");
  require(std::string(call(&api, FAppNativeFunction::HttpJson, &path, 1).asString()) == "18.4",
          "nested numeric JSON path differs");
  path = FAppValue::string("results[0].name");
  require(std::string(call(&api, FAppNativeFunction::HttpJson, &path, 1).asString()) == "Chemnitz",
          "object-in-array JSON path differs");
  path = FAppValue::string("daily.time[1]");
  require(std::string(call(&api, FAppNativeFunction::HttpJson, &path, 1).asString()) == "2026-08-29",
          "array JSON path differs");
  path = FAppValue::string("results[9].missing");
  require(std::string(call(&api, FAppNativeFunction::HttpJson, &path, 1).asString()).empty(),
          "missing JSON path should return an empty string without faulting the app");
  FAppValue input = FAppValue::string("Nova Gorica");
  require(std::string(call(&api, FAppNativeFunction::UrlEncode, &input, 1).asString()) == "Nova%20Gorica",
          "URL encoding differs");
  FAppValue date = FAppValue::string("2026-08-04");
  require(std::string(call(&api, FAppNativeFunction::DateWeekday, &date, 1).asString()) == "Tue",
          "ISO weekday conversion differs");
  FAppValue offset = FAppValue::integer(0);
  require(std::string(call(&api, FAppNativeFunction::HttpText, &offset, 1).asString()).find("current") != std::string::npos,
          "raw response chunk missing");
  FAppValue writeArguments[] = {FAppValue::string("state.txt"), FAppValue::string("ready")};
  require(call(&api, FAppNativeFunction::FileWrite, writeArguments, 2).truthy(),
          "app-relative file_write should succeed");
  FAppValue filePath = FAppValue::string("state.txt");
  require(std::string(call(&api, FAppNativeFunction::FileRead, &filePath, 1).asString()) == "ready",
          "file_read should return data written in the app directory");
  FAppValue unsafePath = FAppValue::string("../system/secret.txt");
  FAppValue unsafeResult;
  require(!api.call(FAppNativeFunction::FileRead, &unsafePath, 1, &unsafeResult),
          "file_read must reject parent traversal");
  FAppValue audioPath = FAppValue::string("sound.mp3");
  require(call(&api, FAppNativeFunction::AudioPlay, &audioPath, 1).truthy() &&
          gAudioPath == "/apps/test/sound.mp3", "audio_play should resolve an app-local file");
  WiFi.testStatus = 0;
  require(!call(&api, FAppNativeFunction::WifiStatus, nullptr, 0).truthy(),
          "wifi_status should report a disconnected interface");
  WiFi.testStatus = WL_CONNECTED;
  require(call(&api, FAppNativeFunction::WifiStatus, nullptr, 0).truthy(),
          "wifi_status should report a connected interface");
  FAppValue interval = FAppValue::integer(750);
  require(call(&api, FAppNativeFunction::TimerStart, &interval, 1).truthy() && gTimerInterval == 750,
          "timer.start should pass the validated interval to fOS");
  FAppValue shortInterval = FAppValue::integer(99);
  FAppValue invalidTimerResult;
  require(!api.call(FAppNativeFunction::TimerStart, &shortInterval, 1, &invalidTimerResult),
          "timer.start should reject intervals below the resource limit");
  FAppValue serialText = FAppValue::string("progress 100%\n");
  call(&api, FAppNativeFunction::SerialPrintf, &serialText, 1);
  require(Serial.last == "progress 100%\n", "Serial.printf should treat percent signs as data");
  call(&api, FAppNativeFunction::SystemRestart, nullptr, 0);
  require(gRestartRequested, "system.restart should request a controlled restart");
  FAppValue textValue = FAppValue::floating(12.5);
  require(std::string(call(&api, FAppNativeFunction::Text, &textValue, 1).asString()) == "12.5",
          "text should convert numbers deterministically");
  FAppValue parsedText = FAppValue::string("12,5");
  require(call(&api, FAppNativeFunction::NumberParse, &parsedText, 1).asFloat() == 12.5,
          "number_parse should accept decimal commas");
  FAppValue roundedValue = FAppValue::floating(12.6);
  require(call(&api, FAppNativeFunction::Round, &roundedValue, 1).asInteger() == 13,
          "round should return the nearest integer");
  FAppValue sliceText = FAppValue::string("Calculator");
  require(call(&api, FAppNativeFunction::StringLength, &sliceText, 1).asInteger() == 10,
          "string_length should count bytes");
  FAppValue sliceArgs[] = {sliceText, FAppValue::integer(0), FAppValue::integer(4)};
  require(std::string(call(&api, FAppNativeFunction::StringSlice, sliceArgs, 3).asString()) == "Calc",
          "string_slice should return the requested range");
  FAppValue lastIndexArgs[] = {FAppValue::string("1,2+3,4"), FAppValue::string(",")};
  require(call(&api, FAppNativeFunction::StringLastIndex, lastIndexArgs, 2).asInteger() == 5,
          "string_last_index should find the final match");
  FAppValue expression = FAppValue::string("2+3*4");
  require(std::string(call(&api, FAppNativeFunction::MathEval, &expression, 1).asString()) == "14",
          "math_eval should preserve operator precedence");
  expression = FAppValue::string("1/0");
  require(std::string(call(&api, FAppNativeFunction::MathEval, &expression, 1).asString()) == "Math Error",
          "math_eval should return Math Error for division by zero");
  FAppEventContext pointer;
  pointer.x = 12;
  pointer.y = 23;
  pointer.screenX = 30;
  pointer.screenY = 41;
  pointer.pressed = true;
  pointer.pointer = true;
  api.setEventContext(pointer);
  require(call(&api, FAppNativeFunction::EventX, nullptr, 0).asInteger() == 12 &&
          call(&api, FAppNativeFunction::EventY, nullptr, 0).asInteger() == 23,
          "event coordinates should expose the current pointer queue entry");
  require(call(&api, FAppNativeFunction::EventScreenX, nullptr, 0).asInteger() == 30 &&
          call(&api, FAppNativeFunction::EventScreenY, nullptr, 0).asInteger() == 41,
          "screen coordinates should expose the current pointer queue entry");
  require(call(&api, FAppNativeFunction::EventPressed, nullptr, 0).truthy(),
          "event_pressed should expose the pointer state");
  api.clearEventContext();
  require(call(&api, FAppNativeFunction::EventX, nullptr, 0).asInteger() == 0 &&
          !call(&api, FAppNativeFunction::EventPressed, nullptr, 0).truthy(),
          "clearing an event should reset its pointer data");
  if (failures != 0) return EXIT_FAILURE;
  std::cout << "All native fOS API tests passed.\n";
  return EXIT_SUCCESS;
}
