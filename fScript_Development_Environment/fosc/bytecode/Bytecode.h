#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fosc {

namespace fapp {

constexpr std::uint8_t kFormatMajor = 1;
constexpr std::uint8_t kFormatMinor = 0;
constexpr std::uint8_t kBytecodeMajor = 1;
constexpr std::uint8_t kBytecodeMinor = 6;
constexpr std::uint16_t kHeaderSize = 80;
constexpr std::size_t kMaximumUiObjects = 64;
constexpr std::size_t kMaximumRuntimeConstants = 128;
constexpr std::size_t kMaximumRuntimeStringBytes = 255;

enum class Opcode : std::uint8_t {
  Nop = 0x00,
  PushNil = 0x01,
  PushFalse = 0x02,
  PushTrue = 0x03,
  PushInt = 0x04,
  PushFloat = 0x05,
  PushString = 0x06,
  Pop = 0x07,
  Duplicate = 0x08,
  LoadGlobal = 0x09,
  StoreGlobal = 0x0A,
  LoadLocal = 0x0B,
  StoreLocal = 0x0C,
  GetUiProperty = 0x0D,
  SetUiProperty = 0x0E,
  CallFunction = 0x0F,
  CallUiMethod = 0x10,
  Negate = 0x11,
  LogicalNot = 0x12,
  Add = 0x13,
  Subtract = 0x14,
  Multiply = 0x15,
  Divide = 0x16,
  Modulo = 0x17,
  Equal = 0x18,
  NotEqual = 0x19,
  Less = 0x1A,
  LessEqual = 0x1B,
  Greater = 0x1C,
  GreaterEqual = 0x1D,
  LogicalAnd = 0x1E,
  LogicalOr = 0x1F,
  Jump = 0x20,
  JumpIfFalse = 0x21,
  Return = 0x22,
  CallNative = 0x23
};

enum class NativeFunction : std::uint8_t {
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

enum class ConstantKind : std::uint8_t {
  String = 1
};

enum class UiProperty : std::uint8_t {
  Text = 1,
  Value = 2,
  Checked = 3,
  Hidden = 4,
  Enabled = 5
};

enum class UiMethod : std::uint8_t {
  Clear = 1,
  Focus = 2,
  Blur = 3,
  ScrollToTop = 4,
  ScrollToBottom = 5,
  LoadFile = 6,
  SaveFile = 7,
  Pixel = 8,
  Line = 9,
  Rect = 10,
  Circle = 11,
  DrawText = 12
};

enum class UiEvent : std::uint8_t {
  Click = 1,
  Changed = 2,
  ValueChanged = 3,
  Pressed = 4,
  Released = 5,
  Ready = 6,
  Cancel = 7,
  PointerDown = 8,
  PointerMove = 9,
  PointerUp = 10,
  AppStart = 16,
  AppClose = 17,
  ThemeChanged = 18,
  AppTimer = 19
};

struct BytecodeFunction {
  std::vector<std::uint8_t> code;
  std::uint16_t arity = 0;
  std::uint16_t localCount = 0;
  std::uint16_t maxStack = 0;
  std::uint16_t flags = 0;
};

struct EventBinding {
  std::uint16_t objectId = 0;
  UiEvent event = UiEvent::Click;
  std::uint16_t functionIndex = 0;
};

struct BytecodeModule {
  std::vector<std::string> stringConstants;
  std::vector<BytecodeFunction> functions;
  std::vector<EventBinding> events;
  std::uint16_t globalCount = 0;
  std::uint16_t initFunction = 0;
  std::uint32_t sourceHash = 0;
};

struct FAppHeader {
  std::uint8_t formatMajor = 0;
  std::uint8_t formatMinor = 0;
  std::uint8_t bytecodeMajor = 0;
  std::uint8_t bytecodeMinor = 0;
  std::uint16_t headerSize = 0;
  std::uint16_t flags = 0;
  std::uint32_t fileSize = 0;
  std::uint32_t crc32 = 0;
  std::uint32_t constantsOffset = 0;
  std::uint32_t constantsSize = 0;
  std::uint16_t constantCount = 0;
  std::uint16_t globalCount = 0;
  std::uint32_t functionsOffset = 0;
  std::uint32_t functionsSize = 0;
  std::uint16_t functionCount = 0;
  std::uint16_t initFunction = 0;
  std::uint32_t eventsOffset = 0;
  std::uint32_t eventsSize = 0;
  std::uint16_t eventCount = 0;
  std::uint32_t codeOffset = 0;
  std::uint32_t codeSize = 0;
  std::uint32_t sourceHash = 0;
  std::uint16_t minimumFosMajor = 4;
  std::uint16_t minimumFosMinor = 0;
  std::uint16_t minimumFosPatch = 0;
};

const char * opcodeName(Opcode opcode);
const char * uiPropertyName(UiProperty property);
const char * uiMethodName(UiMethod method);
const char * uiEventName(UiEvent event);
bool parseUiProperty(const std::string& name, UiProperty * property);
bool parseUiMethod(const std::string& name, UiMethod * method);
bool parseUiEvent(const std::string& name, UiEvent * event);
bool parseSystemEvent(const std::string& objectName, const std::string& eventName, UiEvent * event);
bool isSystemEvent(UiEvent event);
const char * nativeFunctionName(NativeFunction function);
bool parseNativeFunction(const std::string& name, NativeFunction * function, std::uint8_t * arity);

std::uint32_t fnv1a32(const std::string& value);
std::uint32_t crc32(const std::vector<std::uint8_t>& bytes, std::size_t zeroOffset, std::size_t zeroSize);

void appendU16(std::vector<std::uint8_t> * output, std::uint16_t value);
void appendU32(std::vector<std::uint8_t> * output, std::uint32_t value);
void appendI32(std::vector<std::uint8_t> * output, std::int32_t value);
void appendF64(std::vector<std::uint8_t> * output, double value);
void patchU32(std::vector<std::uint8_t> * output, std::size_t offset, std::uint32_t value);
bool readU16(const std::vector<std::uint8_t>& input, std::size_t offset, std::uint16_t * value);
bool readU32(const std::vector<std::uint8_t>& input, std::size_t offset, std::uint32_t * value);
bool readI32(const std::vector<std::uint8_t>& input, std::size_t offset, std::int32_t * value);
bool readF64(const std::vector<std::uint8_t>& input, std::size_t offset, double * value);

}  // namespace fapp
}  // namespace fosc
