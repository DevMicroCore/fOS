#include "FAppHttpApi.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/FOSVersion.h"

extern "C" bool OTARecovery_IsNetworkBusy(void) __attribute__((weak));

namespace {
constexpr size_t kJsonNotFound = static_cast<size_t>(-1);
}

FAppHttpApi::FAppHttpApi()
  : lastStatus_(0), filesystem_(nullptr), audioPlayCallback_(nullptr),
    restartCallback_(nullptr), timerStartCallback_(nullptr)
{
  appDirectory_[0] = '\0';
  error_[0] = '\0';
}

void FAppHttpApi::configure(
  fs::FS& filesystem,
  const char * appDirectory,
  AudioPlayCallback audioPlay,
  RestartCallback restart,
  TimerStartCallback timerStart)
{
  filesystem_ = &filesystem;
  audioPlayCallback_ = audioPlay;
  restartCallback_ = restart;
  timerStartCallback_ = timerStart;
  snprintf(appDirectory_, sizeof(appDirectory_), "%s", appDirectory == nullptr ? "" : appDirectory);
}

void FAppHttpApi::reset()
{
  response_.~String();
  new (&response_) String();
  lastStatus_ = 0;
  filesystem_ = nullptr;
  audioPlayCallback_ = nullptr;
  restartCallback_ = nullptr;
  timerStartCallback_ = nullptr;
  appDirectory_[0] = '\0';
  error_[0] = '\0';
  eventContext_ = FAppEventContext{};
}

void FAppHttpApi::setError(const char * message) const
{
  snprintf(error_, sizeof(error_), "%s", message == nullptr ? "HTTP API error" : message);
}

bool FAppHttpApi::call(
  FAppNativeFunction function,
  const FAppValue * arguments,
  uint8_t argumentCount,
  FAppValue * result)
{
  if (result == nullptr) { setError("HTTP result pointer is null"); return false; }
  error_[0] = '\0';
  switch (function) {
    case FAppNativeFunction::HttpGet:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("http_get expects one string"); return false;
      }
      return get(arguments[0].asString(), result);
    case FAppNativeFunction::HttpStatus:
      if (argumentCount != 0) { setError("http_status expects no arguments"); return false; }
      *result = FAppValue::integer(lastStatus_);
      return true;
    case FAppNativeFunction::HttpJson:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("http_json expects one path string"); return false;
      }
      return json(arguments[0].asString(), result);
    case FAppNativeFunction::HttpText:
      if (argumentCount != 1 || !arguments[0].isNumber()) {
        setError("http_text expects one integer offset"); return false;
      }
      return text(arguments[0].asInteger(), result);
    case FAppNativeFunction::UrlEncode:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("url_encode expects one string"); return false;
      }
      return encode(arguments[0].asString(), result);
    case FAppNativeFunction::DateWeekday:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("date_weekday expects one ISO date string"); return false;
      }
      return weekday(arguments[0].asString(), result);
    case FAppNativeFunction::FileRead:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("file_read expects one path string"); return false;
      }
      return fileRead(arguments[0].asString(), result);
    case FAppNativeFunction::FileWrite:
      if (argumentCount != 2 || arguments[0].type() != FAppValueType::String ||
          arguments[1].type() != FAppValueType::String) {
        setError("file_write expects path and data strings"); return false;
      }
      return fileWrite(arguments[0].asString(), arguments[1].asString(), result);
    case FAppNativeFunction::FileList:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("file_list expects one path string"); return false;
      }
      return fileList(arguments[0].asString(), result);
    case FAppNativeFunction::AudioPlay:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("audio_play expects one path string"); return false;
      }
      return audioPlay(arguments[0].asString(), result);
    case FAppNativeFunction::WifiStatus:
      if (argumentCount != 0) { setError("wifi_status expects no arguments"); return false; }
      *result = FAppValue::boolean(WiFi.status() == WL_CONNECTED);
      return true;
    case FAppNativeFunction::SystemRestart:
      if (argumentCount != 0) { setError("system.restart expects no arguments"); return false; }
      if (restartCallback_ == nullptr) { setError("system restart is unavailable"); return false; }
      restartCallback_();
      *result = FAppValue::nil();
      return true;
    case FAppNativeFunction::TimerStart:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::Integer) {
        setError("timer.start expects one integer interval"); return false;
      }
      if (arguments[0].asInteger() < static_cast<int32_t>(kMinimumTimerIntervalMs) ||
          arguments[0].asInteger() > static_cast<int32_t>(kMaximumTimerIntervalMs)) {
        setError("timer interval must be 100..86400000 ms"); return false;
      }
      if (timerStartCallback_ == nullptr ||
          !timerStartCallback_(static_cast<uint32_t>(arguments[0].asInteger()))) {
        setError("app timer could not be started"); return false;
      }
      *result = FAppValue::boolean(true);
      return true;
    case FAppNativeFunction::SerialPrintf:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("Serial.printf expects one string"); return false;
      }
      Serial.printf("%s", arguments[0].asString());
      *result = FAppValue::nil();
      return true;
    case FAppNativeFunction::Text:
      if (argumentCount != 1) { setError("text expects one value"); return false; }
      return toTextValue(arguments[0], result);
    case FAppNativeFunction::NumberParse:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("number_parse expects one string"); return false;
      }
      return numberParse(arguments[0].asString(), result);
    case FAppNativeFunction::Round:
      if (argumentCount != 1 || !arguments[0].isNumber()) {
        setError("round expects one number"); return false;
      }
      return roundNumber(arguments[0], result);
    case FAppNativeFunction::StringLength:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("string_length expects one string"); return false;
      }
      return stringLength(arguments[0].asString(), result);
    case FAppNativeFunction::StringSlice:
      if (argumentCount != 3 || arguments[0].type() != FAppValueType::String ||
          arguments[1].type() != FAppValueType::Integer ||
          arguments[2].type() != FAppValueType::Integer) {
        setError("string_slice expects string, start and length"); return false;
      }
      return stringSlice(arguments[0].asString(), arguments[1].asInteger(), arguments[2].asInteger(), result);
    case FAppNativeFunction::MathEval:
      if (argumentCount != 1 || arguments[0].type() != FAppValueType::String) {
        setError("math_eval expects one expression string"); return false;
      }
      return mathEval(arguments[0].asString(), result);
    case FAppNativeFunction::StringLastIndex:
      if (argumentCount != 2 || arguments[0].type() != FAppValueType::String ||
          arguments[1].type() != FAppValueType::String) {
        setError("string_last_index expects text and search strings"); return false;
      }
      return stringLastIndex(arguments[0].asString(), arguments[1].asString(), result);
    case FAppNativeFunction::EventX:
    case FAppNativeFunction::EventY:
    case FAppNativeFunction::EventScreenX:
    case FAppNativeFunction::EventScreenY:
    case FAppNativeFunction::EventPressed:
      if (argumentCount != 0) { setError("event access expects no arguments"); return false; }
      if (function == FAppNativeFunction::EventPressed) {
        *result = FAppValue::boolean(eventContext_.pointer && eventContext_.pressed);
      } else {
        int32_t coordinate = 0;
        if (eventContext_.pointer) {
          if (function == FAppNativeFunction::EventX) coordinate = eventContext_.x;
          else if (function == FAppNativeFunction::EventY) coordinate = eventContext_.y;
          else if (function == FAppNativeFunction::EventScreenX) coordinate = eventContext_.screenX;
          else coordinate = eventContext_.screenY;
        }
        *result = FAppValue::integer(coordinate);
      }
      return true;
  }
  setError("unknown native fOS function");
  return false;
}

bool FAppHttpApi::resolveAppPath(
  const char * relativePath, char * output, size_t capacity) const
{
  if (filesystem_ == nullptr || appDirectory_[0] == '\0' || relativePath == nullptr ||
      relativePath[0] == '\0' || output == nullptr || capacity == 0) {
    setError("app file context is unavailable");
    return false;
  }
  if (relativePath[0] == '/' || relativePath[0] == '\\') {
    setError("app paths must be relative");
    return false;
  }
  const char * segment = relativePath;
  for (const char * cursor = relativePath; ; ++cursor) {
    const unsigned char character = static_cast<unsigned char>(*cursor);
    if (character != 0 && character < 0x20) {
      setError("app path contains a control character");
      return false;
    }
    if (*cursor == '\\') {
      setError("app paths must use forward slashes");
      return false;
    }
    if (*cursor == '/' || *cursor == '\0') {
      const size_t length = static_cast<size_t>(cursor - segment);
      if (length == 0 || (length == 1 && segment[0] == '.') ||
          (length == 2 && segment[0] == '.' && segment[1] == '.')) {
        setError("app path contains an unsafe segment");
        return false;
      }
      if (*cursor == '\0') break;
      segment = cursor + 1;
    }
  }
  const int written = snprintf(output, capacity, "%s/%s", appDirectory_, relativePath);
  if (written < 0 || static_cast<size_t>(written) >= capacity) {
    setError("app path is too long");
    return false;
  }
  return true;
}

bool FAppHttpApi::toTextValue(const FAppValue& value, FAppValue * result)
{
  char buffer[FAppValue::kStringCapacity];
  if (!value.toText(buffer, sizeof(buffer))) { setError("text conversion failed"); return false; }
  *result = FAppValue::string(buffer);
  return true;
}

bool FAppHttpApi::numberParse(const char * input, FAppValue * result)
{
  if (input == nullptr) { setError("number_parse input is null"); return false; }
  char normalized[48];
  size_t length = 0;
  while (*input != '\0' && isspace(static_cast<unsigned char>(*input))) ++input;
  for (const char * cursor = input; *cursor != '\0'; ++cursor) {
    if (length + 1 >= sizeof(normalized)) { setError("number_parse input is too long"); return false; }
    normalized[length++] = (*cursor == ',') ? '.' : *cursor;
  }
  while (length > 0 && isspace(static_cast<unsigned char>(normalized[length - 1]))) --length;
  normalized[length] = '\0';
  if (length == 0) { *result = FAppValue::floating(0.0); return true; }
  char * end = nullptr;
  const double value = strtod(normalized, &end);
  if (end == normalized || *end != '\0') {
    *result = FAppValue::floating(0.0);
    return true;
  }
  *result = FAppValue::floating(value);
  return true;
}

bool FAppHttpApi::roundNumber(const FAppValue& value, FAppValue * result)
{
  const double rounded = value.asFloat() >= 0.0 ? floor(value.asFloat() + 0.5) : ceil(value.asFloat() - 0.5);
  if (rounded < static_cast<double>(INT32_MIN) || rounded > static_cast<double>(INT32_MAX)) {
    setError("round result is outside integer range");
    return false;
  }
  *result = FAppValue::integer(static_cast<int32_t>(rounded));
  return true;
}

bool FAppHttpApi::stringLength(const char * input, FAppValue * result)
{
  *result = FAppValue::integer(static_cast<int32_t>(strlen(input == nullptr ? "" : input)));
  return true;
}

bool FAppHttpApi::stringSlice(const char * input, int32_t start, int32_t length, FAppValue * result)
{
  const char * text = input == nullptr ? "" : input;
  const int32_t total = static_cast<int32_t>(strlen(text));
  if (start < 0) start = total + start;
  if (start < 0) start = 0;
  if (start > total) start = total;
  if (length < 0) length = 0;
  if (length > total - start) length = total - start;
  char output[FAppValue::kStringCapacity];
  if (static_cast<size_t>(length) >= sizeof(output)) {
    setError("string_slice result is too long");
    return false;
  }
  memcpy(output, text + start, static_cast<size_t>(length));
  output[length] = '\0';
  *result = FAppValue::string(output);
  return true;
}

bool FAppHttpApi::stringLastIndex(const char * input, const char * needle, FAppValue * result)
{
  const char * text = input == nullptr ? "" : input;
  const char * search = needle == nullptr ? "" : needle;
  if (search[0] == '\0') {
    *result = FAppValue::integer(static_cast<int32_t>(strlen(text)));
    return true;
  }
  int32_t found = -1;
  const size_t searchLength = strlen(search);
  for (const char * cursor = strstr(text, search); cursor != nullptr; cursor = strstr(cursor + 1, search)) {
    found = static_cast<int32_t>(cursor - text);
    if (searchLength == 0) break;
  }
  *result = FAppValue::integer(found);
  return true;
}

namespace {

bool fappCalcOperator(char character)
{
  return character == '+' || character == '-' || character == '*' || character == '/';
}

int fappCalcPrecedence(char character)
{
  if (character == '*' || character == '/') return 2;
  if (character == '+' || character == '-') return 1;
  return 0;
}

bool fappCalcApply(double * values, int * valueTop, char op, bool * divisionByZero)
{
  if (*valueTop < 2) return false;
  const double right = values[--(*valueTop)];
  const double left = values[--(*valueTop)];
  double value = 0.0;
  switch (op) {
    case '+': value = left + right; break;
    case '-': value = left - right; break;
    case '*': value = left * right; break;
    case '/':
      if (fabs(right) < 1e-12) { *divisionByZero = true; return false; }
      value = left / right;
      break;
    default:
      return false;
  }
  values[(*valueTop)++] = value;
  return true;
}

bool fappCalcEvaluate(const char * expression, double * output, bool * divisionByZero)
{
  if (expression == nullptr || output == nullptr || divisionByZero == nullptr) return false;
  *divisionByZero = false;
  char normalized[FAppValue::kStringCapacity];
  size_t length = 0;
  for (const char * cursor = expression; *cursor != '\0'; ++cursor) {
    if (length + 1 >= sizeof(normalized)) return false;
    normalized[length++] = (*cursor == ',') ? '.' : *cursor;
  }
  normalized[length] = '\0';
  const int kMaxStack = 32;
  double values[kMaxStack];
  char operators[kMaxStack];
  int valueTop = 0;
  int operatorTop = 0;
  const char * cursor = normalized;
  bool expectNumber = true;
  while (*cursor != '\0') {
    if (isspace(static_cast<unsigned char>(*cursor))) { ++cursor; continue; }
    if (expectNumber) {
      char * end = nullptr;
      const double number = strtod(cursor, &end);
      if (end == cursor || valueTop >= kMaxStack) return false;
      values[valueTop++] = number;
      cursor = end;
      expectNumber = false;
      continue;
    }
    const char op = *cursor;
    if (!fappCalcOperator(op)) return false;
    while (operatorTop > 0 && fappCalcPrecedence(operators[operatorTop - 1]) >= fappCalcPrecedence(op)) {
      if (!fappCalcApply(values, &valueTop, operators[--operatorTop], divisionByZero)) return false;
    }
    if (operatorTop >= kMaxStack) return false;
    operators[operatorTop++] = op;
    ++cursor;
    expectNumber = true;
  }
  if (expectNumber) return false;
  while (operatorTop > 0) {
    if (!fappCalcApply(values, &valueTop, operators[--operatorTop], divisionByZero)) return false;
  }
  if (valueTop != 1) return false;
  *output = values[0];
  return true;
}

}  // namespace

bool FAppHttpApi::mathEval(const char * expression, FAppValue * result)
{
  double value = 0.0;
  bool divisionByZero = false;
  if (!fappCalcEvaluate(expression, &value, &divisionByZero)) {
    *result = FAppValue::string("Math Error");
    return true;
  }
  char buffer[48];
  snprintf(buffer, sizeof(buffer), "%.10g", value);
  for (char * cursor = buffer; *cursor != '\0'; ++cursor) {
    if (*cursor == '.') *cursor = ',';
  }
  *result = FAppValue::string(buffer);
  return true;
}

bool FAppHttpApi::fileRead(const char * relativePath, FAppValue * result)
{
  char path[256];
  if (!resolveAppPath(relativePath, path, sizeof(path))) return false;
  fs::File file = filesystem_->open(path, FILE_READ);
  if (!file) {
    *result = FAppValue::string("");
    return true;
  }
  if (file.size() >= FAppValue::kStringCapacity) {
    file.close();
    setError("file_read file exceeds 255 bytes");
    *result = FAppValue::string("");
    return true;
  }
  char data[FAppValue::kStringCapacity];
  const size_t size = file.size();
  const size_t count = file.read(reinterpret_cast<uint8_t *>(data), size);
  file.close();
  if (count != size) { setError("file_read could not read the complete file"); return false; }
  data[count] = '\0';
  *result = FAppValue::string(data);
  return true;
}

bool FAppHttpApi::fileWrite(
  const char * relativePath, const char * data, FAppValue * result)
{
  char path[256];
  if (!resolveAppPath(relativePath, path, sizeof(path))) return false;
  fs::File file = filesystem_->open(path, FILE_WRITE);
  if (!file) {
    *result = FAppValue::boolean(false);
    return true;
  }
  const size_t size = strlen(data);
  const size_t count = file.write(reinterpret_cast<const uint8_t *>(data), size);
  file.close();
  *result = FAppValue::boolean(count == size);
  return true;
}

bool FAppHttpApi::fileList(const char * relativePath, FAppValue * result)
{
  char path[256];
  if (!resolveAppPath(relativePath, path, sizeof(path))) return false;
  fs::File directory = filesystem_->open(path, FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    *result = FAppValue::string("");
    return true;
  }

  String folders;
  String files;
  fs::File entry = directory.openNextFile();
  while (entry) {
    String name(entry.name());
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    name.trim();
    if (name.length() > 0 && !name.startsWith(".")) {
      String& target = entry.isDirectory() ? folders : files;
      if (target.length() > 0) target += "\n";
      target += name;
      if (entry.isDirectory()) target += "/";
    }
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();

  String listing = folders;
  if (listing.length() > 0 && files.length() > 0) listing += "\n";
  listing += files;
  if (listing.length() >= FAppValue::kStringCapacity) {
    listing.remove(FAppValue::kStringCapacity - 1);
    const int lastNewline = listing.lastIndexOf('\n');
    if (lastNewline > 0) listing.remove(lastNewline);
  }
  *result = FAppValue::string(listing.c_str());
  return true;
}

bool FAppHttpApi::audioPlay(const char * relativePath, FAppValue * result)
{
  char path[256];
  if (!resolveAppPath(relativePath, path, sizeof(path))) return false;
  if (!filesystem_->exists(path) || audioPlayCallback_ == nullptr) {
    *result = FAppValue::boolean(false);
    return true;
  }
  *result = FAppValue::boolean(audioPlayCallback_(path));
  return true;
}

bool FAppHttpApi::get(const char * url, FAppValue * result)
{
  response_ = "";
  lastStatus_ = 0;
  if (OTARecovery_IsNetworkBusy != nullptr && OTARecovery_IsNetworkBusy()) {
    setError("OTA network operation is active");
    *result = FAppValue::boolean(false);
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    setError("WiFi is not connected");
    *result = FAppValue::boolean(false);
    return true;
  }
  if (url == nullptr || (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0)) {
    setError("only http:// and https:// URLs are allowed");
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(7000);
  http.setUserAgent(String("fOS/") + FOSVersion::kString + " fScript-http/1.0");
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  const bool secure = strncmp(url, "https://", 8) == 0;
  const bool begun = secure ? http.begin(secureClient, url) : http.begin(plainClient, url);
  if (!begun) {
    setError("HTTP connection setup failed");
    *result = FAppValue::boolean(false);
    return true;
  }
  lastStatus_ = http.GET();
  if (lastStatus_ > 0) {
    const int declaredSize = http.getSize();
    if (declaredSize > static_cast<int>(kMaximumResponseBytes)) {
      http.end();
      setError("HTTP response exceeds 32768 bytes");
      *result = FAppValue::boolean(false);
      return true;
    }
    response_ = http.getString();
    if (response_.length() > kMaximumResponseBytes) {
      response_ = "";
      setError("HTTP response exceeds 32768 bytes");
      *result = FAppValue::boolean(false);
      http.end();
      return true;
    }
  } else {
    setError(HTTPClient::errorToString(lastStatus_).c_str());
  }
  http.end();
  *result = FAppValue::boolean(lastStatus_ >= 200 && lastStatus_ < 300);
  return true;
}

bool FAppHttpApi::text(int32_t offset, FAppValue * result)
{
  if (offset < 0 || static_cast<size_t>(offset) > response_.length()) {
    setError("HTTP text offset is outside the response"); return false;
  }
  const size_t available = response_.length() - static_cast<size_t>(offset);
  const size_t count = available < FAppValue::kStringCapacity - 1
    ? available : FAppValue::kStringCapacity - 1;
  char output[FAppValue::kStringCapacity];
  memcpy(output, response_.c_str() + offset, count);
  output[count] = '\0';
  *result = FAppValue::string(output);
  return true;
}

bool FAppHttpApi::encode(const char * input, FAppValue * result)
{
  char output[FAppValue::kStringCapacity];
  size_t length = 0;
  const char hex[] = "0123456789ABCDEF";
  for (const unsigned char * cursor = reinterpret_cast<const unsigned char *>(input); *cursor != 0; ++cursor) {
    const bool safe = isalnum(*cursor) || *cursor == '-' || *cursor == '_' || *cursor == '.' || *cursor == '~';
    const size_t needed = safe ? 1 : 3;
    if (length + needed >= sizeof(output)) { setError("URL-encoded value is too long"); return false; }
    if (safe) output[length++] = static_cast<char>(*cursor);
    else {
      output[length++] = '%';
      output[length++] = hex[*cursor >> 4u];
      output[length++] = hex[*cursor & 0x0Fu];
    }
  }
  output[length] = '\0';
  *result = FAppValue::string(output);
  return true;
}

bool FAppHttpApi::weekday(const char * isoDate, FAppValue * result)
{
  if (isoDate == nullptr || strlen(isoDate) != 10 || isoDate[4] != '-' || isoDate[7] != '-') {
    setError("date_weekday expects YYYY-MM-DD");
    return false;
  }
  const int yearValue = atoi(isoDate);
  const int month = atoi(isoDate + 5);
  const int day = atoi(isoDate + 8);
  if (yearValue < 1970 || month < 1 || month > 12 || day < 1 || day > 31) {
    setError("date_weekday date is outside the supported range");
    return false;
  }
  static const uint8_t monthOffset[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  static const char * const names[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  int year = yearValue;
  if (month < 3) --year;
  const int index = (year + year / 4 - year / 100 + year / 400 + monthOffset[month - 1] + day) % 7;
  *result = FAppValue::string(names[index]);
  return true;
}

size_t FAppHttpApi::skipWhitespace(size_t offset) const
{
  while (offset < response_.length() && isspace(static_cast<unsigned char>(response_[offset]))) ++offset;
  return offset;
}

size_t FAppHttpApi::skipJsonString(size_t offset) const
{
  if (offset >= response_.length() || response_[offset] != '"') return kJsonNotFound;
  ++offset;
  while (offset < response_.length()) {
    if (response_[offset] == '\\') { offset += 2; continue; }
    if (response_[offset++] == '"') return offset;
  }
  return kJsonNotFound;
}

size_t FAppHttpApi::skipJsonValue(size_t offset) const
{
  offset = skipWhitespace(offset);
  if (offset >= response_.length()) return kJsonNotFound;
  const char first = response_[offset];
  if (first == '"') return skipJsonString(offset);
  if (first == '{' || first == '[') {
    const char open = first;
    const char close = first == '{' ? '}' : ']';
    int depth = 0;
    for (size_t cursor = offset; cursor < response_.length(); ++cursor) {
      if (response_[cursor] == '"') {
        cursor = skipJsonString(cursor);
        if (cursor == kJsonNotFound) return kJsonNotFound;
        --cursor;
        continue;
      }
      if (response_[cursor] == open) ++depth;
      else if (response_[cursor] == close && --depth == 0) return cursor + 1;
    }
    return kJsonNotFound;
  }
  while (offset < response_.length() && response_[offset] != ',' &&
         response_[offset] != '}' && response_[offset] != ']') ++offset;
  return offset;
}

bool FAppHttpApi::findObjectKey(size_t objectStart, const char * key, size_t * valueStart) const
{
  size_t cursor = skipWhitespace(objectStart);
  if (cursor >= response_.length() || response_[cursor++] != '{') return false;
  while (cursor < response_.length()) {
    cursor = skipWhitespace(cursor);
    if (cursor < response_.length() && response_[cursor] == '}') return false;
    if (cursor >= response_.length() || response_[cursor] != '"') return false;
    const size_t keyBegin = cursor + 1;
    const size_t keyEndAfter = skipJsonString(cursor);
    if (keyEndAfter == kJsonNotFound) return false;
    const size_t keyEnd = keyEndAfter - 1;
    cursor = skipWhitespace(keyEndAfter);
    if (cursor >= response_.length() || response_[cursor++] != ':') return false;
    cursor = skipWhitespace(cursor);
    const bool matches = strlen(key) == keyEnd - keyBegin &&
      strncmp(response_.c_str() + keyBegin, key, keyEnd - keyBegin) == 0;
    if (matches) { *valueStart = cursor; return true; }
    cursor = skipJsonValue(cursor);
    if (cursor == kJsonNotFound) return false;
    cursor = skipWhitespace(cursor);
    if (cursor < response_.length() && response_[cursor] == ',') ++cursor;
  }
  return false;
}

bool FAppHttpApi::findArrayIndex(size_t arrayStart, int index, size_t * valueStart) const
{
  if (index < 0) return false;
  size_t cursor = skipWhitespace(arrayStart);
  if (cursor >= response_.length() || response_[cursor++] != '[') return false;
  for (int current = 0; current <= index; ++current) {
    cursor = skipWhitespace(cursor);
    if (cursor >= response_.length() || response_[cursor] == ']') return false;
    if (current == index) { *valueStart = cursor; return true; }
    cursor = skipJsonValue(cursor);
    if (cursor == kJsonNotFound) return false;
    cursor = skipWhitespace(cursor);
    if (cursor < response_.length() && response_[cursor] == ',') ++cursor;
  }
  return false;
}

bool FAppHttpApi::navigateJson(const char * path, size_t * valueStart) const
{
  if (path == nullptr || path[0] == '\0' || valueStart == nullptr) return false;
  size_t current = skipWhitespace(0);
  const char * cursor = path;
  while (*cursor != '\0') {
    char key[64];
    size_t length = 0;
    while (*cursor != '\0' && *cursor != '.' && *cursor != '[') {
      if (length + 1 >= sizeof(key)) return false;
      key[length++] = *cursor++;
    }
    key[length] = '\0';
    if (length > 0 && !findObjectKey(current, key, &current)) return false;
    while (*cursor == '[') {
      ++cursor;
      char * end = nullptr;
      const long index = strtol(cursor, &end, 10);
      if (end == cursor || *end != ']' || index < 0 || index > 32767 ||
          !findArrayIndex(current, static_cast<int>(index), &current)) return false;
      cursor = end + 1;
    }
    if (*cursor == '.') ++cursor;
    else if (*cursor != '\0') return false;
  }
  *valueStart = current;
  return true;
}

bool FAppHttpApi::extractJsonValue(size_t valueStart, char * output, size_t capacity) const
{
  if (output == nullptr || capacity == 0) return false;
  size_t cursor = skipWhitespace(valueStart);
  size_t length = 0;
  if (cursor < response_.length() && response_[cursor] == '"') {
    ++cursor;
    while (cursor < response_.length() && response_[cursor] != '"') {
      char value = response_[cursor++];
      if (value == '\\') {
        if (cursor >= response_.length()) return false;
        value = response_[cursor++];
        if (value == 'n') value = '\n';
        else if (value == 'r') value = '\r';
        else if (value == 't') value = '\t';
        else if (value == 'u') {
          if (cursor + 4 > response_.length()) return false;
          cursor += 4;
          value = '?';
        }
      }
      if (length + 1 >= capacity) { setError("JSON value exceeds runtime string limit"); return false; }
      output[length++] = value;
    }
    if (cursor >= response_.length()) return false;
  } else {
    const size_t end = skipJsonValue(cursor);
    if (end == kJsonNotFound) return false;
    size_t trimmedEnd = end;
    while (trimmedEnd > cursor && isspace(static_cast<unsigned char>(response_[trimmedEnd - 1]))) --trimmedEnd;
    if (trimmedEnd - cursor + 1 > capacity || response_[cursor] == '{' || response_[cursor] == '[') return false;
    length = trimmedEnd - cursor;
    memcpy(output, response_.c_str() + cursor, length);
  }
  output[length] = '\0';
  return true;
}

bool FAppHttpApi::json(const char * path, FAppValue * result)
{
  if (response_.length() == 0) { setError("HTTP response is empty"); return false; }
  size_t valueStart = 0;
  char output[FAppValue::kStringCapacity];
  if (!navigateJson(path, &valueStart)) {
    *result = FAppValue::string("");
    return true;
  }
  if (!extractJsonValue(valueStart, output, sizeof(output))) {
    if (error_[0] == '\0') setError("JSON value is not a supported scalar");
    return false;
  }
  *result = FAppValue::string(output);
  return true;
}
