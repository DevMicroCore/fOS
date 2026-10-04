#include "FAppRuntime.h"

#include "FAppFormat.h"

#include <math.h>
#include <limits.h>
#include <new>
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif
#else
#include <chrono>
#endif

namespace {

#define FAPP_EVENT_APP_START 16U
#define FAPP_EVENT_APP_CLOSE 17U
#define FAPP_EVENT_THEME_CHANGED 18U
#define FAPP_EVENT_APP_TIMER 19U
#define FAPP_EVENT_POINTER_MOVE 9U
#define FAPP_EVENT_POINTER_UP 10U

enum Opcode : uint8_t {
  Nop = 0x00, PushNil = 0x01, PushFalse = 0x02, PushTrue = 0x03,
  PushInt = 0x04, PushFloat = 0x05, PushString = 0x06, Pop = 0x07,
  Duplicate = 0x08, LoadGlobal = 0x09, StoreGlobal = 0x0A,
  LoadLocal = 0x0B, StoreLocal = 0x0C, GetUiProperty = 0x0D,
  SetUiProperty = 0x0E, CallFunction = 0x0F, CallUiMethod = 0x10,
  Negate = 0x11, LogicalNot = 0x12, Add = 0x13, Subtract = 0x14,
  Multiply = 0x15, Divide = 0x16, Modulo = 0x17, Equal = 0x18,
  NotEqual = 0x19, Less = 0x1A, LessEqual = 0x1B, Greater = 0x1C,
  GreaterEqual = 0x1D, LogicalAnd = 0x1E, LogicalOr = 0x1F,
  Jump = 0x20, JumpIfFalse = 0x21, Return = 0x22, CallNative = 0x23
};

int32_t readI32(const uint8_t * bytes)
{
  return static_cast<int32_t>(FAppFormat::readU32(bytes));
}

double readF64(const uint8_t * bytes)
{
  uint64_t bits = 0;
  for (uint8_t index = 0; index < 8; ++index) {
    bits |= static_cast<uint64_t>(bytes[index]) << (index * 8u);
  }
  double value = 0.0;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

bool nativePermissionGranted(FAppPermissions permissions, FAppNativeFunction function)
{
  switch (function) {
    case FAppNativeFunction::HttpGet:
    case FAppNativeFunction::HttpStatus:
    case FAppNativeFunction::HttpJson:
    case FAppNativeFunction::HttpText:
    case FAppNativeFunction::UrlEncode:
    case FAppNativeFunction::WifiStatus:
      return fappHasPermission(permissions, FAppPermission::Network);
    case FAppNativeFunction::FileRead:
    case FAppNativeFunction::FileList:
      return fappHasPermission(permissions, FAppPermission::StorageRead);
    case FAppNativeFunction::FileWrite:
      return fappHasPermission(permissions, FAppPermission::StorageWrite);
    case FAppNativeFunction::AudioPlay:
      return fappHasPermission(permissions, FAppPermission::Audio);
    case FAppNativeFunction::SystemRestart:
      return fappHasPermission(permissions, FAppPermission::SystemRestart);
    case FAppNativeFunction::DateWeekday:
    case FAppNativeFunction::TimerStart:
    case FAppNativeFunction::SerialPrintf:
    case FAppNativeFunction::Text:
    case FAppNativeFunction::NumberParse:
    case FAppNativeFunction::Round:
    case FAppNativeFunction::StringLength:
    case FAppNativeFunction::StringSlice:
    case FAppNativeFunction::MathEval:
    case FAppNativeFunction::StringLastIndex:
    case FAppNativeFunction::EventX:
    case FAppNativeFunction::EventY:
    case FAppNativeFunction::EventScreenX:
    case FAppNativeFunction::EventScreenY:
    case FAppNativeFunction::EventPressed:
      return true;
  }
  return false;
}

}  // namespace

FAppRuntime::FAppRuntime()
  : filesystem_(nullptr), loader_(nullptr), uiApi_(nullptr), nativeApi_(nullptr), permissions_(0),
    state_(FAppRuntimeState::Stopped), error_(FAppRuntimeError::None),
    constantsOffset_(0), functionsOffset_(0), eventsOffset_(0), codeOffset_(0),
    constantCount_(0), functionCount_(0), eventCount_(0), globalCount_(0),
    initFunction_(0), executableCache_(nullptr), executableCacheSize_(0), executableCacheExternal_(false),
    valueStorage_(nullptr), globals_(nullptr), stack_(nullptr), locals_(nullptr),
    stackSize_(0), localTop_(0), frameCount_(0), eventHead_(0),
    eventCountQueued_(0), startEventPending_(false), executedInstructions_(0)
{
}

FAppRuntime::~FAppRuntime()
{
  releaseExecutableCache();
  releaseValueStorage();
}

bool FAppRuntime::allocateValueStorage()
{
  releaseValueStorage();
  constexpr size_t valueCount = kMaxGlobals + kMaxStack + kMaxLocals;
#if defined(ARDUINO_ARCH_ESP32)
  constexpr size_t storageBytes = valueCount * sizeof(FAppValue);
  void * memory = heap_caps_malloc(storageBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const bool external = memory != nullptr;
  if (memory == nullptr) memory = heap_caps_malloc(storageBytes, MALLOC_CAP_8BIT);
  if (memory == nullptr) return false;
  valueStorage_ = static_cast<FAppValue *>(memory);
  for (size_t index = 0; index < valueCount; ++index) new (&valueStorage_[index]) FAppValue();
  Serial.printf(
    "[FAPP] Runtime value storage: %u bytes in %s RAM; internal free=%u, largest=%u\n",
    static_cast<unsigned int>(storageBytes), external ? "PS" : "internal",
    static_cast<unsigned int>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
    static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
#else
  valueStorage_ = new (std::nothrow) FAppValue[valueCount];
  if (valueStorage_ == nullptr) return false;
#endif
  globals_ = valueStorage_;
  stack_ = globals_ + kMaxGlobals;
  locals_ = stack_ + kMaxStack;
  return true;
}

void FAppRuntime::releaseValueStorage()
{
  if (valueStorage_ == nullptr) return;
#if defined(ARDUINO_ARCH_ESP32)
  constexpr size_t valueCount = kMaxGlobals + kMaxStack + kMaxLocals;
  for (size_t index = 0; index < valueCount; ++index) valueStorage_[index].~FAppValue();
  heap_caps_free(valueStorage_);
#else
  delete[] valueStorage_;
#endif
  valueStorage_ = nullptr;
  globals_ = nullptr;
  stack_ = nullptr;
  locals_ = nullptr;
}

bool FAppRuntime::begin(
  fs::FS& filesystem,
  const FAppLoader& loader,
  FAppUiApi& uiApi,
  FAppPermissions permissions,
  FAppNativeApi * nativeApi)
{
  stop();
  filesystem_ = &filesystem;
  loader_ = &loader;
  uiApi_ = &uiApi;
  nativeApi_ = nativeApi;
  permissions_ = permissions;
  if (!loader.hasExecutable()) {
    fault(FAppRuntimeError::NotPrepared);
    return false;
  }
  (void)loadExecutableCache();
  if (!loadMetadata()) return false;
  if (!allocateValueStorage()) {
    fault(FAppRuntimeError::ResourceLimit);
    return false;
  }
  for (uint16_t index = 0; index < globalCount_; ++index) globals_[index] = FAppValue::nil();
  state_ = FAppRuntimeState::Ready;
  return true;
}

bool FAppRuntime::loadMetadata()
{
  uint8_t header[FAppFormat::kHeaderSize];
  if (!readAt(0, header, sizeof(header))) {
    fault(FAppRuntimeError::Io);
    return false;
  }
  constantsOffset_ = FAppFormat::readU32(header + 20);
  const uint32_t constantsSize = FAppFormat::readU32(header + 24);
  const uint16_t constantCount = FAppFormat::readU16(header + 28);
  const uint16_t globalCount = FAppFormat::readU16(header + 30);
  functionsOffset_ = FAppFormat::readU32(header + 32);
  const uint16_t functionCount = FAppFormat::readU16(header + 40);
  const uint16_t initFunction = FAppFormat::readU16(header + 42);
  eventsOffset_ = FAppFormat::readU32(header + 44);
  const uint16_t eventCount = FAppFormat::readU16(header + 52);
  codeOffset_ = FAppFormat::readU32(header + 56);
  if (constantCount > kMaxConstants || functionCount > kMaxFunctions ||
      eventCount > kMaxEvents || globalCount > kMaxGlobals ||
      initFunction >= functionCount) {
    fault(FAppRuntimeError::ResourceLimit);
    return false;
  }
  constantCount_ = constantCount;
  functionCount_ = static_cast<uint8_t>(functionCount);
  eventCount_ = static_cast<uint8_t>(eventCount);
  globalCount_ = static_cast<uint8_t>(globalCount);
  initFunction_ = static_cast<uint8_t>(initFunction);

  uint32_t cursor = constantsOffset_;
  const uint32_t constantsEnd = constantsOffset_ + constantsSize;
  for (uint16_t index = 0; index < constantCount_; ++index) {
    uint8_t record[4];
    if (!readAt(cursor, record, sizeof(record)) || record[0] != 1) {
      fault(FAppRuntimeError::InvalidMetadata);
      return false;
    }
    const uint16_t size = FAppFormat::readU16(record + 2);
    cursor += 4;
    if (size >= FAppValue::kStringCapacity || cursor > constantsEnd || size > constantsEnd - cursor) {
      fault(size >= FAppValue::kStringCapacity ? FAppRuntimeError::ResourceLimit : FAppRuntimeError::InvalidMetadata);
      return false;
    }
    constantOffsets_[index] = cursor;
    constantSizes_[index] = static_cast<uint8_t>(size);
    cursor += size;
  }
  if (cursor != constantsEnd) {
    fault(FAppRuntimeError::InvalidMetadata);
    return false;
  }

  for (uint16_t index = 0; index < functionCount_; ++index) {
    uint8_t record[FAppFormat::kFunctionRecordSize];
    if (!readAt(functionsOffset_ + static_cast<uint32_t>(index) * sizeof(record), record, sizeof(record))) {
      fault(FAppRuntimeError::Io);
      return false;
    }
    FunctionRecord& function = functions_[index];
    function.codeOffset = FAppFormat::readU32(record);
    function.codeSize = FAppFormat::readU32(record + 4);
    const uint16_t arity = FAppFormat::readU16(record + 8);
    const uint16_t localCount = FAppFormat::readU16(record + 10);
    const uint16_t maxStack = FAppFormat::readU16(record + 12);
    if (localCount > kMaxLocals || maxStack > kMaxStack ||
        arity > localCount || function.codeOffset > loader_->codeSize() ||
        function.codeSize > loader_->codeSize() - function.codeOffset) {
      fault(FAppRuntimeError::ResourceLimit);
      return false;
    }
    function.arity = static_cast<uint8_t>(arity);
    function.localCount = static_cast<uint8_t>(localCount);
    function.maxStack = static_cast<uint8_t>(maxStack);
  }
  for (uint16_t index = 0; index < eventCount_; ++index) {
    uint8_t record[FAppFormat::kEventRecordSize];
    if (!readAt(eventsOffset_ + static_cast<uint32_t>(index) * sizeof(record), record, sizeof(record))) {
      fault(FAppRuntimeError::Io);
      return false;
    }
    const uint16_t objectId = FAppFormat::readU16(record);
    const uint16_t functionIndex = FAppFormat::readU16(record + 4);
    const bool validEvent = objectId == 0
      ? record[2] >= FAPP_EVENT_APP_START && record[2] <= FAPP_EVENT_APP_TIMER
      : objectId <= 64 && record[2] >= 1 && record[2] <= 10;
    if (!validEvent || functionIndex >= functionCount_) {
      fault(FAppRuntimeError::InvalidMetadata);
      return false;
    }
    events_[index] = {
      static_cast<uint8_t>(objectId),
      record[2],
      static_cast<uint8_t>(functionIndex)
    };
  }
  return true;
}

bool FAppRuntime::start()
{
  if (state_ != FAppRuntimeState::Ready) return false;
  startEventPending_ = true;
  return invokeFunction(initFunction_, 0);
}

void FAppRuntime::update(uint16_t instructionBudget, uint32_t microsecondBudget)
{
  if (!isActive() || instructionBudget == 0) return;
  const uint32_t started = clockMicros();
  uint16_t consumed = 0;
  while (consumed < instructionBudget && isActive()) {
    if (frameCount_ == 0) {
      state_ = FAppRuntimeState::Ready;
      if (startEventPending_) {
        startEventPending_ = false;
        if (nativeApi_ != nullptr) nativeApi_->clearEventContext();
        if (!invokeEvent(0, FAPP_EVENT_APP_START)) continue;
      } else if (eventCountQueued_ > 0) {
        const PendingEvent pending = eventQueue_[eventHead_];
        eventHead_ = static_cast<uint8_t>((eventHead_ + 1) % kEventQueueCapacity);
        --eventCountQueued_;
        if (nativeApi_ != nullptr) {
          if ((pending.flags & 1U) != 0) {
            FAppEventContext context;
            context.x = pending.x;
            context.y = pending.y;
            context.screenX = pending.screenX;
            context.screenY = pending.screenY;
            context.pressed = (pending.flags & 2U) != 0;
            context.pointer = true;
            nativeApi_->setEventContext(context);
          } else {
            nativeApi_->clearEventContext();
          }
        }
        if (!invokeEvent(pending.objectId, pending.eventType)) continue;
      } else {
        break;
      }
    }
    if (!executeInstruction()) break;
    ++consumed;
    ++executedInstructions_;
    if (microsecondBudget > 0 && static_cast<uint32_t>(clockMicros() - started) >= microsecondBudget) break;
  }
}

bool FAppRuntime::queueEvent(uint16_t objectId, uint8_t eventType)
{
  return enqueueEvent({0, 0, 0, 0, static_cast<uint8_t>(objectId), eventType, 0});
}

bool FAppRuntime::queuePointerEvent(
  uint16_t objectId, uint8_t eventType, int16_t x, int16_t y,
  int16_t screenX, int16_t screenY, bool pressed)
{
  if (!isActive() || !hasEventHandler(objectId, eventType)) return false;
  const PendingEvent incoming = {
    x, y, screenX, screenY, static_cast<uint8_t>(objectId), eventType,
    static_cast<uint8_t>(pressed ? 3U : 1U)
  };

  // A touch panel can report movement faster than fScript can execute a
  // handler. Keep only the newest pending position instead of filling the
  // bounded queue with obsolete intermediate points.
  if (eventType == FAPP_EVENT_POINTER_MOVE && eventCountQueued_ > 0) {
    const uint8_t last = static_cast<uint8_t>(
      (eventHead_ + eventCountQueued_ - 1U) % kEventQueueCapacity);
    if (eventQueue_[last].objectId == objectId &&
        eventQueue_[last].eventType == FAPP_EVENT_POINTER_MOVE) {
      eventQueue_[last] = incoming;
      return true;
    }
  }

  // Releasing the finger is state-critical. If movement filled the queue,
  // replace the newest pending move so pointer_up is never lost.
  if (eventType == FAPP_EVENT_POINTER_UP && eventCountQueued_ >= kEventQueueCapacity) {
    for (uint8_t offset = 0; offset < eventCountQueued_; ++offset) {
      const uint8_t index = static_cast<uint8_t>(
        (eventHead_ + eventCountQueued_ - 1U - offset) % kEventQueueCapacity);
      if (eventQueue_[index].objectId == objectId &&
          eventQueue_[index].eventType == FAPP_EVENT_POINTER_MOVE) {
        eventQueue_[index] = incoming;
        return true;
      }
    }
  }
  return enqueueEvent(incoming);
}

bool FAppRuntime::enqueueEvent(const PendingEvent& event)
{
  if (!isActive() || !hasEventHandler(event.objectId, event.eventType)) return false;
  if (eventCountQueued_ >= kEventQueueCapacity) return false;
  const uint8_t tail = static_cast<uint8_t>((eventHead_ + eventCountQueued_) % kEventQueueCapacity);
  eventQueue_[tail] = event;
  ++eventCountQueued_;
  return true;
}

bool FAppRuntime::hasEventHandler(uint16_t objectId, uint8_t eventType) const
{
  for (uint16_t index = 0; index < eventCount_; ++index) {
    if (events_[index].objectId == objectId && events_[index].eventType == eventType) return true;
  }
  return false;
}

void FAppRuntime::notifyThemeChanged()
{
  queueEvent(0, FAPP_EVENT_THEME_CHANGED);
}

void FAppRuntime::notifyTimer()
{
  queueEvent(0, FAPP_EVENT_APP_TIMER);
}

bool FAppRuntime::invokeEvent(uint16_t objectId, uint8_t eventType)
{
  for (uint16_t index = 0; index < eventCount_; ++index) {
    if (events_[index].objectId == objectId && events_[index].eventType == eventType) {
      return invokeFunction(events_[index].functionIndex, 0);
    }
  }
  return false;
}

bool FAppRuntime::invokeFunction(uint16_t functionIndex, uint8_t argumentCount)
{
  if (functionIndex >= functionCount_ || frameCount_ >= kMaxCallDepth || stackSize_ < argumentCount) {
    fault(functionIndex >= functionCount_ ? FAppRuntimeError::InvalidIndex : FAppRuntimeError::ResourceLimit);
    return false;
  }
  const FunctionRecord& function = functions_[functionIndex];
  if (argumentCount != function.arity || localTop_ + function.localCount > kMaxLocals) {
    fault(argumentCount != function.arity ? FAppRuntimeError::InvalidMetadata : FAppRuntimeError::ResourceLimit);
    return false;
  }
  const uint8_t argumentBase = static_cast<uint8_t>(stackSize_ - argumentCount);
  const uint8_t localBase = localTop_;
  for (uint8_t index = 0; index < function.localCount; ++index) locals_[localBase + index] = FAppValue::nil();
  for (uint8_t index = 0; index < argumentCount; ++index) locals_[localBase + index] = stack_[argumentBase + index];
  stackSize_ = argumentBase;
  frames_[frameCount_++] = {
    0,
    static_cast<uint8_t>(functionIndex),
    localBase,
    stackSize_
  };
  localTop_ += function.localCount;
  state_ = FAppRuntimeState::Running;
  return true;
}

bool FAppRuntime::readAt(uint32_t offset, uint8_t * destination, size_t size)
{
  if (filesystem_ == nullptr || loader_ == nullptr || destination == nullptr) return false;
  if (executableCache_ != nullptr) {
    if (offset > executableCacheSize_ || size > executableCacheSize_ - offset) return false;
    memcpy(destination, executableCache_ + offset, size);
    return true;
  }
  fs::File file = filesystem_->open(loader_->executablePath(), FILE_READ);
  if (!file) return false;
  const bool ok = file.seek(offset) && file.read(destination, size) == size;
  file.close();
  return ok;
}

bool FAppRuntime::readCode(uint8_t * destination, size_t size)
{
  if (frameCount_ == 0) return false;
  Frame& frame = frames_[frameCount_ - 1];
  const FunctionRecord& function = functions_[frame.functionIndex];
  if (frame.pc > function.codeSize || size > function.codeSize - frame.pc ||
      !readAt(codeOffset_ + function.codeOffset + frame.pc, destination, size)) return false;
  frame.pc += static_cast<uint32_t>(size);
  return true;
}

bool FAppRuntime::loadExecutableCache()
{
  releaseExecutableCache();
  if (filesystem_ == nullptr || loader_ == nullptr || !loader_->hasExecutable()) return false;
  const size_t size = loader_->executableSize();
  if (size == 0 || size > kMaxCachedExecutableBytes) return false;
#if defined(ARDUINO_ARCH_ESP32)
  void * memory = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  executableCacheExternal_ = memory != nullptr;
  if (memory == nullptr) memory = heap_caps_malloc(size, MALLOC_CAP_8BIT);
  if (memory == nullptr) return false;
  executableCache_ = static_cast<uint8_t *>(memory);
#else
  executableCache_ = new (std::nothrow) uint8_t[size];
  executableCacheExternal_ = false;
  if (executableCache_ == nullptr) return false;
#endif
  fs::File file = filesystem_->open(loader_->executablePath(), FILE_READ);
  if (!file) {
    releaseExecutableCache();
    return false;
  }
  const bool ok = file.seek(0) && file.read(executableCache_, size) == size;
  file.close();
  if (!ok) {
    releaseExecutableCache();
    return false;
  }
  executableCacheSize_ = size;
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf(
    "[FAPP] Runtime executable cache: %u bytes in %s RAM\n",
    static_cast<unsigned int>(size),
    executableCacheExternal_ ? "PS" : "internal");
#endif
  return true;
}

void FAppRuntime::releaseExecutableCache()
{
  if (executableCache_ == nullptr) return;
#if defined(ARDUINO_ARCH_ESP32)
  heap_caps_free(executableCache_);
#else
  delete[] executableCache_;
#endif
  executableCache_ = nullptr;
  executableCacheSize_ = 0;
  executableCacheExternal_ = false;
}

bool FAppRuntime::readStringConstant(uint16_t index, FAppValue * result)
{
  if (index >= constantCount_ || result == nullptr) return false;
  char text[FAppValue::kStringCapacity];
  const uint8_t size = constantSizes_[index];
  if (!readAt(constantOffsets_[index], reinterpret_cast<uint8_t *>(text), size)) return false;
  text[size] = '\0';
  *result = FAppValue::string(text);
  return true;
}

bool FAppRuntime::push(const FAppValue& value)
{
  if (stackSize_ >= kMaxStack) { fault(FAppRuntimeError::StackOverflow); return false; }
  stack_[stackSize_++] = value;
  return true;
}

bool FAppRuntime::pop(FAppValue * value)
{
  if (stackSize_ == 0 || value == nullptr) { fault(FAppRuntimeError::StackUnderflow); return false; }
  *value = stack_[--stackSize_];
  return true;
}

bool FAppRuntime::executeInstruction()
{
  uint8_t opcode = 0;
  if (!readCode(&opcode, 1)) { fault(FAppRuntimeError::Io); return false; }
  uint8_t bytes[8];
  FAppValue value;
  Frame& frame = frames_[frameCount_ - 1];
  switch (opcode) {
    case Nop: return true;
    case PushNil: return push(FAppValue::nil());
    case PushFalse: return push(FAppValue::boolean(false));
    case PushTrue: return push(FAppValue::boolean(true));
    case PushInt:
      if (!readCode(bytes, 4)) break;
      return push(FAppValue::integer(readI32(bytes)));
    case PushFloat:
      if (!readCode(bytes, 8)) break;
      return push(FAppValue::floating(readF64(bytes)));
    case PushString: {
      if (!readCode(bytes, 2) || !readStringConstant(FAppFormat::readU16(bytes), &value)) break;
      return push(value);
    }
    case Pop: return pop(&value);
    case Duplicate:
      if (stackSize_ == 0) { fault(FAppRuntimeError::StackUnderflow); return false; }
      return push(stack_[stackSize_ - 1]);
    case LoadGlobal: {
      if (!readCode(bytes, 2)) break;
      const uint16_t index = FAppFormat::readU16(bytes);
      if (index >= globalCount_) { fault(FAppRuntimeError::InvalidIndex); return false; }
      return push(globals_[index]);
    }
    case StoreGlobal: {
      if (!readCode(bytes, 2) || !pop(&value)) return false;
      const uint16_t index = FAppFormat::readU16(bytes);
      if (index >= globalCount_) { fault(FAppRuntimeError::InvalidIndex); return false; }
      globals_[index] = value;
      return true;
    }
    case LoadLocal: {
      if (!readCode(bytes, 2)) break;
      const uint16_t index = FAppFormat::readU16(bytes);
      if (index >= functions_[frame.functionIndex].localCount) { fault(FAppRuntimeError::InvalidIndex); return false; }
      return push(locals_[frame.localBase + index]);
    }
    case StoreLocal: {
      if (!readCode(bytes, 2) || !pop(&value)) return false;
      const uint16_t index = FAppFormat::readU16(bytes);
      if (index >= functions_[frame.functionIndex].localCount) { fault(FAppRuntimeError::InvalidIndex); return false; }
      locals_[frame.localBase + index] = value;
      return true;
    }
    case GetUiProperty: {
      if (!readCode(bytes, 3)) break;
      if (!fappHasPermission(permissions_, FAppPermission::Ui)) { fault(FAppRuntimeError::PermissionDenied); return false; }
      if (!uiApi_->getProperty(FAppFormat::readU16(bytes), bytes[2], &value)) { fault(FAppRuntimeError::UiFailure); return false; }
      return push(value);
    }
    case SetUiProperty: {
      if (!readCode(bytes, 3) || !pop(&value)) return false;
      if (!fappHasPermission(permissions_, FAppPermission::Ui)) { fault(FAppRuntimeError::PermissionDenied); return false; }
      if (!uiApi_->setProperty(FAppFormat::readU16(bytes), bytes[2], value)) { fault(FAppRuntimeError::UiFailure); return false; }
      return true;
    }
    case CallFunction: {
      if (!readCode(bytes, 3)) break;
      return invokeFunction(FAppFormat::readU16(bytes), bytes[2]);
    }
    case CallUiMethod: {
      if (!readCode(bytes, 4)) break;
      const uint8_t count = bytes[3];
      if (count > stackSize_) { fault(FAppRuntimeError::StackUnderflow); return false; }
      if (!fappHasPermission(permissions_, FAppPermission::Ui)) { fault(FAppRuntimeError::PermissionDenied); return false; }
      if (bytes[2] == 6 && !fappHasPermission(permissions_, FAppPermission::StorageRead)) {
        fault(FAppRuntimeError::PermissionDenied); return false;
      }
      if (bytes[2] == 7 && !fappHasPermission(permissions_, FAppPermission::StorageWrite)) {
        fault(FAppRuntimeError::PermissionDenied); return false;
      }
      const FAppValue * arguments = count == 0 ? nullptr : &stack_[stackSize_ - count];
      if (!uiApi_->callMethod(FAppFormat::readU16(bytes), bytes[2], arguments, count, &value)) {
        fault(FAppRuntimeError::UiFailure); return false;
      }
      stackSize_ -= count;
      return push(value);
    }
    case CallNative: {
      if (!readCode(bytes, 2)) break;
      const uint8_t count = bytes[1];
      if (count > stackSize_) { fault(FAppRuntimeError::StackUnderflow); return false; }
      if (!nativePermissionGranted(permissions_, static_cast<FAppNativeFunction>(bytes[0]))) {
        fault(FAppRuntimeError::PermissionDenied); return false;
      }
      if (nativeApi_ == nullptr) { fault(FAppRuntimeError::NativeFailure); return false; }
      const FAppValue * arguments = count == 0 ? nullptr : &stack_[stackSize_ - count];
      if (!nativeApi_->call(static_cast<FAppNativeFunction>(bytes[0]), arguments, count, &value)) {
        fault(FAppRuntimeError::NativeFailure); return false;
      }
      stackSize_ -= count;
      return push(value);
    }
    case Negate:
      if (!pop(&value)) return false;
      if (!value.isNumber()) { fault(FAppRuntimeError::TypeError); return false; }
      if (value.type() == FAppValueType::Integer && value.asInteger() == INT32_MIN) {
        fault(FAppRuntimeError::ResourceLimit); return false;
      }
      return value.type() == FAppValueType::Integer
        ? push(FAppValue::integer(-value.asInteger())) : push(FAppValue::floating(-value.asFloat()));
    case LogicalNot:
      if (!pop(&value)) return false;
      return push(FAppValue::boolean(!value.truthy()));
    case Add: case Subtract: case Multiply: case Divide: case Modulo:
    case Equal: case NotEqual: case Less: case LessEqual: case Greater: case GreaterEqual:
    case LogicalAnd: case LogicalOr:
      return binary(opcode);
    case Jump: case JumpIfFalse: {
      if (!readCode(bytes, 4)) break;
      bool take = true;
      if (opcode == JumpIfFalse) { if (!pop(&value)) return false; take = !value.truthy(); }
      if (take) {
        const int64_t target = static_cast<int64_t>(frame.pc) + readI32(bytes);
        if (target < 0 || target >= functions_[frame.functionIndex].codeSize) {
          fault(FAppRuntimeError::InvalidInstruction); return false;
        }
        frame.pc = static_cast<uint32_t>(target);
      }
      return true;
    }
    case Return: {
      if (!pop(&value)) return false;
      const Frame completed = frames_[frameCount_ - 1];
      stackSize_ = completed.stackBase;
      localTop_ = completed.localBase;
      --frameCount_;
      if (frameCount_ == 0) { state_ = FAppRuntimeState::Ready; return true; }
      return push(value);
    }
    default:
      fault(FAppRuntimeError::InvalidInstruction);
      return false;
  }
  fault(FAppRuntimeError::Io);
  return false;
}

bool FAppRuntime::binary(uint8_t opcode)
{
  FAppValue right;
  FAppValue left;
  if (!pop(&right) || !pop(&left)) return false;
  if (opcode == Equal || opcode == NotEqual) {
    const bool equal = FAppValue::equals(left, right);
    return push(FAppValue::boolean(opcode == Equal ? equal : !equal));
  }
  if (opcode == LogicalAnd || opcode == LogicalOr) {
    return push(FAppValue::boolean(opcode == LogicalAnd
      ? left.truthy() && right.truthy() : left.truthy() || right.truthy()));
  }
  if (opcode == Add && (left.type() == FAppValueType::String || right.type() == FAppValueType::String)) {
    char leftText[FAppValue::kStringCapacity];
    char rightText[FAppValue::kStringCapacity];
    char joined[FAppValue::kStringCapacity];
    left.toText(leftText, sizeof(leftText));
    right.toText(rightText, sizeof(rightText));
    const int written = snprintf(joined, sizeof(joined), "%s%s", leftText, rightText);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(joined)) { fault(FAppRuntimeError::ResourceLimit); return false; }
    return push(FAppValue::string(joined));
  }
  if (!left.isNumber() || !right.isNumber()) { fault(FAppRuntimeError::TypeError); return false; }
  const double a = left.asFloat();
  const double b = right.asFloat();
  if ((opcode == Divide || opcode == Modulo) && b == 0.0) { fault(FAppRuntimeError::DivisionByZero); return false; }
  switch (opcode) {
    case Add:
      if (left.type() == FAppValueType::Integer && right.type() == FAppValueType::Integer) {
        const int64_t result = static_cast<int64_t>(left.asInteger()) + right.asInteger();
        if (result < INT32_MIN || result > INT32_MAX) { fault(FAppRuntimeError::ResourceLimit); return false; }
        return push(FAppValue::integer(static_cast<int32_t>(result)));
      }
      return push(FAppValue::floating(a + b));
    case Subtract:
      if (left.type() == FAppValueType::Integer && right.type() == FAppValueType::Integer) {
        const int64_t result = static_cast<int64_t>(left.asInteger()) - right.asInteger();
        if (result < INT32_MIN || result > INT32_MAX) { fault(FAppRuntimeError::ResourceLimit); return false; }
        return push(FAppValue::integer(static_cast<int32_t>(result)));
      }
      return push(FAppValue::floating(a - b));
    case Multiply:
      if (left.type() == FAppValueType::Integer && right.type() == FAppValueType::Integer) {
        const int64_t result = static_cast<int64_t>(left.asInteger()) * right.asInteger();
        if (result < INT32_MIN || result > INT32_MAX) { fault(FAppRuntimeError::ResourceLimit); return false; }
        return push(FAppValue::integer(static_cast<int32_t>(result)));
      }
      return push(FAppValue::floating(a * b));
    case Divide: return push(FAppValue::floating(a / b));
    case Modulo:
      if (left.type() == FAppValueType::Integer && right.type() == FAppValueType::Integer) {
        if (left.asInteger() == INT32_MIN && right.asInteger() == -1) return push(FAppValue::integer(0));
        return push(FAppValue::integer(left.asInteger() % right.asInteger()));
      }
      return push(FAppValue::floating(fmod(a, b)));
    case Less: return push(FAppValue::boolean(a < b));
    case LessEqual: return push(FAppValue::boolean(a <= b));
    case Greater: return push(FAppValue::boolean(a > b));
    case GreaterEqual: return push(FAppValue::boolean(a >= b));
    default: fault(FAppRuntimeError::InvalidInstruction); return false;
  }
}

void FAppRuntime::shutdown(uint16_t instructionBudget)
{
  if (isActive() && hasEventHandler(0, FAPP_EVENT_APP_CLOSE)) {
    frameCount_ = 0; stackSize_ = 0; localTop_ = 0; eventCountQueued_ = 0; startEventPending_ = false;
    if (nativeApi_ != nullptr) nativeApi_->clearEventContext();
    if (invokeEvent(0, FAPP_EVENT_APP_CLOSE)) update(instructionBudget, 5000);
  }
  stop();
}

void FAppRuntime::stop()
{
  state_ = FAppRuntimeState::Stopped;
  error_ = FAppRuntimeError::None;
  stackSize_ = 0;
  localTop_ = 0;
  frameCount_ = 0;
  eventHead_ = 0;
  eventCountQueued_ = 0;
  startEventPending_ = false;
  executedInstructions_ = 0;
  if (nativeApi_ != nullptr) nativeApi_->reset();
  releaseExecutableCache();
  releaseValueStorage();
}

void FAppRuntime::fault(FAppRuntimeError error)
{
  error_ = error;
  state_ = FAppRuntimeState::Faulted;
  frameCount_ = 0;
  eventCountQueued_ = 0;
  releaseValueStorage();
}

const char * FAppRuntime::errorMessage() const
{
  switch (error_) {
    case FAppRuntimeError::None: return "no error";
    case FAppRuntimeError::NotPrepared: return "executable not prepared";
    case FAppRuntimeError::ResourceLimit: return "runtime resource limit exceeded";
    case FAppRuntimeError::Io: return "executable read failed";
    case FAppRuntimeError::InvalidMetadata: return "invalid executable metadata";
    case FAppRuntimeError::InvalidInstruction: return "invalid bytecode instruction";
    case FAppRuntimeError::StackUnderflow: return "operand stack underflow";
    case FAppRuntimeError::StackOverflow: return "operand stack overflow";
    case FAppRuntimeError::TypeError: return "runtime type error";
    case FAppRuntimeError::DivisionByZero: return "division by zero";
    case FAppRuntimeError::InvalidIndex: return "invalid runtime index";
    case FAppRuntimeError::PermissionDenied: return "permission denied";
    case FAppRuntimeError::UiFailure: return "UI operation failed";
    case FAppRuntimeError::NativeFailure:
      return nativeApi_ == nullptr ? "native API unavailable" : nativeApi_->errorMessage();
  }
  return "unknown runtime error";
}

uint32_t FAppRuntime::clockMicros()
{
#if defined(ARDUINO)
  return micros();
#else
  using namespace std::chrono;
  return static_cast<uint32_t>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}
