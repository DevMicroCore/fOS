#include "Bytecode.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace fosc {
namespace fapp {
namespace {

std::string lower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

}  // namespace

const char * opcodeName(Opcode opcode)
{
  switch (opcode) {
    case Opcode::Nop: return "NOP";
    case Opcode::PushNil: return "PUSH_NIL";
    case Opcode::PushFalse: return "PUSH_FALSE";
    case Opcode::PushTrue: return "PUSH_TRUE";
    case Opcode::PushInt: return "PUSH_INT";
    case Opcode::PushFloat: return "PUSH_FLOAT";
    case Opcode::PushString: return "PUSH_STRING";
    case Opcode::Pop: return "POP";
    case Opcode::Duplicate: return "DUP";
    case Opcode::LoadGlobal: return "LOAD_GLOBAL";
    case Opcode::StoreGlobal: return "STORE_GLOBAL";
    case Opcode::LoadLocal: return "LOAD_LOCAL";
    case Opcode::StoreLocal: return "STORE_LOCAL";
    case Opcode::GetUiProperty: return "GET_UI_PROPERTY";
    case Opcode::SetUiProperty: return "SET_UI_PROPERTY";
    case Opcode::CallFunction: return "CALL_FUNCTION";
    case Opcode::CallUiMethod: return "CALL_UI_METHOD";
    case Opcode::Negate: return "NEGATE";
    case Opcode::LogicalNot: return "NOT";
    case Opcode::Add: return "ADD";
    case Opcode::Subtract: return "SUBTRACT";
    case Opcode::Multiply: return "MULTIPLY";
    case Opcode::Divide: return "DIVIDE";
    case Opcode::Modulo: return "MODULO";
    case Opcode::Equal: return "EQUAL";
    case Opcode::NotEqual: return "NOT_EQUAL";
    case Opcode::Less: return "LESS";
    case Opcode::LessEqual: return "LESS_EQUAL";
    case Opcode::Greater: return "GREATER";
    case Opcode::GreaterEqual: return "GREATER_EQUAL";
    case Opcode::LogicalAnd: return "AND";
    case Opcode::LogicalOr: return "OR";
    case Opcode::Jump: return "JUMP";
    case Opcode::JumpIfFalse: return "JUMP_IF_FALSE";
    case Opcode::Return: return "RETURN";
    case Opcode::CallNative: return "CALL_NATIVE";
  }
  return "UNKNOWN";
}

const char * uiPropertyName(UiProperty property)
{
  switch (property) {
    case UiProperty::Text: return "text";
    case UiProperty::Value: return "value";
    case UiProperty::Checked: return "checked";
    case UiProperty::Hidden: return "hidden";
    case UiProperty::Enabled: return "enabled";
  }
  return "unknown";
}

const char * uiMethodName(UiMethod method)
{
  switch (method) {
    case UiMethod::Clear: return "clear";
    case UiMethod::Focus: return "focus";
    case UiMethod::Blur: return "blur";
    case UiMethod::ScrollToTop: return "scroll_to_top";
    case UiMethod::ScrollToBottom: return "scroll_to_bottom";
    case UiMethod::LoadFile: return "load_file";
    case UiMethod::SaveFile: return "save_file";
    case UiMethod::Pixel: return "pixel";
    case UiMethod::Line: return "line";
    case UiMethod::Rect: return "rect";
    case UiMethod::Circle: return "circle";
    case UiMethod::DrawText: return "draw_text";
  }
  return "unknown";
}

const char * uiEventName(UiEvent event)
{
  switch (event) {
    case UiEvent::Click: return "click";
    case UiEvent::Changed: return "changed";
    case UiEvent::ValueChanged: return "value_changed";
    case UiEvent::Pressed: return "pressed";
    case UiEvent::Released: return "released";
    case UiEvent::Ready: return "ready";
    case UiEvent::Cancel: return "cancel";
    case UiEvent::PointerDown: return "pointer_down";
    case UiEvent::PointerMove: return "pointer_move";
    case UiEvent::PointerUp: return "pointer_up";
    case UiEvent::AppStart: return "app.start";
    case UiEvent::AppClose: return "app.close";
    case UiEvent::ThemeChanged: return "app.theme_changed";
    case UiEvent::AppTimer: return "app.timer";
  }
  return "unknown";
}

bool parseUiProperty(const std::string& name, UiProperty * property)
{
  const std::string value = lower(name);
  if (value == "text") *property = UiProperty::Text;
  else if (value == "value") *property = UiProperty::Value;
  else if (value == "checked") *property = UiProperty::Checked;
  else if (value == "hidden") *property = UiProperty::Hidden;
  else if (value == "enabled") *property = UiProperty::Enabled;
  else return false;
  return true;
}

bool parseUiMethod(const std::string& name, UiMethod * method)
{
  const std::string value = lower(name);
  if (value == "clear") *method = UiMethod::Clear;
  else if (value == "focus") *method = UiMethod::Focus;
  else if (value == "blur") *method = UiMethod::Blur;
  else if (value == "scroll_to_top") *method = UiMethod::ScrollToTop;
  else if (value == "scroll_to_bottom") *method = UiMethod::ScrollToBottom;
  else if (value == "load_file") *method = UiMethod::LoadFile;
  else if (value == "save_file") *method = UiMethod::SaveFile;
  else if (value == "pixel") *method = UiMethod::Pixel;
  else if (value == "line") *method = UiMethod::Line;
  else if (value == "rect") *method = UiMethod::Rect;
  else if (value == "circle") *method = UiMethod::Circle;
  else if (value == "draw_text") *method = UiMethod::DrawText;
  else return false;
  return true;
}

bool parseUiEvent(const std::string& name, UiEvent * event)
{
  const std::string value = lower(name);
  if (value == "click" || value == "clicked") *event = UiEvent::Click;
  else if (value == "change" || value == "changed") *event = UiEvent::Changed;
  else if (value == "value_changed") *event = UiEvent::ValueChanged;
  else if (value == "pressed") *event = UiEvent::Pressed;
  else if (value == "released") *event = UiEvent::Released;
  else if (value == "ready") *event = UiEvent::Ready;
  else if (value == "cancel" || value == "cancelled") *event = UiEvent::Cancel;
  else if (value == "pointer_down") *event = UiEvent::PointerDown;
  else if (value == "pointer_move") *event = UiEvent::PointerMove;
  else if (value == "pointer_up") *event = UiEvent::PointerUp;
  else return false;
  return true;
}

bool parseSystemEvent(const std::string& objectName, const std::string& eventName, UiEvent * event)
{
  if (lower(objectName) != "app" || event == nullptr) return false;
  const std::string value = lower(eventName);
  if (value == "start") *event = UiEvent::AppStart;
  else if (value == "close") *event = UiEvent::AppClose;
  else if (value == "theme_changed") *event = UiEvent::ThemeChanged;
  else if (value == "timer") *event = UiEvent::AppTimer;
  else return false;
  return true;
}

bool isSystemEvent(UiEvent event)
{
  return event == UiEvent::AppStart || event == UiEvent::AppClose ||
         event == UiEvent::ThemeChanged || event == UiEvent::AppTimer;
}

const char * nativeFunctionName(NativeFunction function)
{
  switch (function) {
    case NativeFunction::HttpGet: return "http_get";
    case NativeFunction::HttpStatus: return "http_status";
    case NativeFunction::HttpJson: return "http_json";
    case NativeFunction::HttpText: return "http_text";
    case NativeFunction::UrlEncode: return "url_encode";
    case NativeFunction::DateWeekday: return "date_weekday";
    case NativeFunction::FileRead: return "file_read";
    case NativeFunction::FileWrite: return "file_write";
    case NativeFunction::AudioPlay: return "audio_play";
    case NativeFunction::WifiStatus: return "wifi_status";
    case NativeFunction::SystemRestart: return "system.restart";
    case NativeFunction::TimerStart: return "timer.start";
    case NativeFunction::SerialPrintf: return "Serial.printf";
    case NativeFunction::Text: return "text";
    case NativeFunction::NumberParse: return "number_parse";
    case NativeFunction::Round: return "round";
    case NativeFunction::StringLength: return "string_length";
    case NativeFunction::StringSlice: return "string_slice";
    case NativeFunction::MathEval: return "math_eval";
    case NativeFunction::StringLastIndex: return "string_last_index";
    case NativeFunction::FileList: return "file_list";
    case NativeFunction::EventX: return "event_x";
    case NativeFunction::EventY: return "event_y";
    case NativeFunction::EventScreenX: return "event_screen_x";
    case NativeFunction::EventScreenY: return "event_screen_y";
    case NativeFunction::EventPressed: return "event_pressed";
  }
  return "unknown_native";
}

bool parseNativeFunction(
  const std::string& name,
  NativeFunction * function,
  std::uint8_t * arity)
{
  if (function == nullptr || arity == nullptr) return false;
  const std::string value = lower(name);
  if (value == "http_get") { *function = NativeFunction::HttpGet; *arity = 1; }
  else if (value == "http_status") { *function = NativeFunction::HttpStatus; *arity = 0; }
  else if (value == "http_json") { *function = NativeFunction::HttpJson; *arity = 1; }
  else if (value == "http_text") { *function = NativeFunction::HttpText; *arity = 1; }
  else if (value == "url_encode") { *function = NativeFunction::UrlEncode; *arity = 1; }
  else if (value == "date_weekday") { *function = NativeFunction::DateWeekday; *arity = 1; }
  else if (value == "file_read") { *function = NativeFunction::FileRead; *arity = 1; }
  else if (value == "file_write") { *function = NativeFunction::FileWrite; *arity = 2; }
  else if (value == "audio_play") { *function = NativeFunction::AudioPlay; *arity = 1; }
  else if (value == "wifi_status") { *function = NativeFunction::WifiStatus; *arity = 0; }
  else if (value == "system.restart") { *function = NativeFunction::SystemRestart; *arity = 0; }
  else if (value == "timer.start") { *function = NativeFunction::TimerStart; *arity = 1; }
  else if (value == "serial.printf") { *function = NativeFunction::SerialPrintf; *arity = 1; }
  else if (value == "text") { *function = NativeFunction::Text; *arity = 1; }
  else if (value == "number_parse") { *function = NativeFunction::NumberParse; *arity = 1; }
  else if (value == "round") { *function = NativeFunction::Round; *arity = 1; }
  else if (value == "string_length") { *function = NativeFunction::StringLength; *arity = 1; }
  else if (value == "string_slice") { *function = NativeFunction::StringSlice; *arity = 3; }
  else if (value == "math_eval") { *function = NativeFunction::MathEval; *arity = 1; }
  else if (value == "string_last_index") { *function = NativeFunction::StringLastIndex; *arity = 2; }
  else if (value == "file_list") { *function = NativeFunction::FileList; *arity = 1; }
  else if (value == "event_x") { *function = NativeFunction::EventX; *arity = 0; }
  else if (value == "event_y") { *function = NativeFunction::EventY; *arity = 0; }
  else if (value == "event_screen_x") { *function = NativeFunction::EventScreenX; *arity = 0; }
  else if (value == "event_screen_y") { *function = NativeFunction::EventScreenY; *arity = 0; }
  else if (value == "event_pressed") { *function = NativeFunction::EventPressed; *arity = 0; }
  else return false;
  return true;
}

std::uint32_t fnv1a32(const std::string& value)
{
  std::uint32_t hash = 2166136261u;
  for (unsigned char byte : value) {
    hash ^= byte;
    hash *= 16777619u;
  }
  return hash;
}

std::uint32_t crc32(
  const std::vector<std::uint8_t>& bytes,
  std::size_t zeroOffset,
  std::size_t zeroSize)
{
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const std::uint8_t byte = index >= zeroOffset && index < zeroOffset + zeroSize
      ? 0
      : bytes[index];
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1u) ^ (0xEDB88320u & static_cast<std::uint32_t>(-(crc & 1u)));
    }
  }
  return ~crc;
}

void appendU16(std::vector<std::uint8_t> * output, std::uint16_t value)
{
  output->push_back(static_cast<std::uint8_t>(value));
  output->push_back(static_cast<std::uint8_t>(value >> 8u));
}

void appendU32(std::vector<std::uint8_t> * output, std::uint32_t value)
{
  output->push_back(static_cast<std::uint8_t>(value));
  output->push_back(static_cast<std::uint8_t>(value >> 8u));
  output->push_back(static_cast<std::uint8_t>(value >> 16u));
  output->push_back(static_cast<std::uint8_t>(value >> 24u));
}

void appendI32(std::vector<std::uint8_t> * output, std::int32_t value)
{
  appendU32(output, static_cast<std::uint32_t>(value));
}

void appendF64(std::vector<std::uint8_t> * output, double value)
{
  std::uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "double must be 64-bit");
  std::memcpy(&bits, &value, sizeof(bits));
  for (unsigned int shift = 0; shift < 64; shift += 8) {
    output->push_back(static_cast<std::uint8_t>(bits >> shift));
  }
}

void patchU32(std::vector<std::uint8_t> * output, std::size_t offset, std::uint32_t value)
{
  if (offset + 4 > output->size()) return;
  (*output)[offset] = static_cast<std::uint8_t>(value);
  (*output)[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
  (*output)[offset + 2] = static_cast<std::uint8_t>(value >> 16u);
  (*output)[offset + 3] = static_cast<std::uint8_t>(value >> 24u);
}

bool readU16(const std::vector<std::uint8_t>& input, std::size_t offset, std::uint16_t * value)
{
  if (offset + 2 > input.size() || value == nullptr) return false;
  *value = static_cast<std::uint16_t>(input[offset]) |
           static_cast<std::uint16_t>(input[offset + 1] << 8u);
  return true;
}

bool readU32(const std::vector<std::uint8_t>& input, std::size_t offset, std::uint32_t * value)
{
  if (offset + 4 > input.size() || value == nullptr) return false;
  *value = static_cast<std::uint32_t>(input[offset]) |
           (static_cast<std::uint32_t>(input[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(input[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(input[offset + 3]) << 24u);
  return true;
}

bool readI32(const std::vector<std::uint8_t>& input, std::size_t offset, std::int32_t * value)
{
  std::uint32_t unsignedValue = 0;
  if (!readU32(input, offset, &unsignedValue) || value == nullptr) return false;
  *value = static_cast<std::int32_t>(unsignedValue);
  return true;
}

bool readF64(const std::vector<std::uint8_t>& input, std::size_t offset, double * value)
{
  if (offset + 8 > input.size() || value == nullptr) return false;
  std::uint64_t bits = 0;
  for (unsigned int shift = 0; shift < 64; shift += 8) {
    bits |= static_cast<std::uint64_t>(input[offset + shift / 8]) << shift;
  }
  std::memcpy(value, &bits, sizeof(bits));
  return true;
}

}  // namespace fapp
}  // namespace fosc
