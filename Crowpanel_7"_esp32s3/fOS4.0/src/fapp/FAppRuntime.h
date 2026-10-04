#pragma once

#include <FS.h>
#include <stddef.h>
#include <stdint.h>

#include "FAppLoader.h"
#include "FAppUiApi.h"
#include "FAppNativeApi.h"

enum class FAppRuntimeState : uint8_t {
  Stopped = 0,
  Ready,
  Running,
  Faulted
};

enum class FAppRuntimeError : uint8_t {
  None = 0,
  NotPrepared,
  ResourceLimit,
  Io,
  InvalidMetadata,
  InvalidInstruction,
  StackUnderflow,
  StackOverflow,
  TypeError,
  DivisionByZero,
  InvalidIndex,
  PermissionDenied,
  UiFailure,
  NativeFailure
};

class FAppRuntime {
 public:
  static constexpr uint16_t kMaxConstants = 128;
  static constexpr uint16_t kMaxFunctions = 48;
  static constexpr uint16_t kMaxEvents = 64;
  static constexpr uint16_t kMaxGlobals = 32;
  static constexpr uint16_t kMaxStack = 64;
  static constexpr uint16_t kMaxLocals = 96;
  static constexpr uint8_t kMaxCallDepth = 8;
  static constexpr uint8_t kEventQueueCapacity = 8;
  static constexpr size_t kMaxCachedExecutableBytes = 65536;

  FAppRuntime();
  ~FAppRuntime();
  FAppRuntime(const FAppRuntime&) = delete;
  FAppRuntime& operator=(const FAppRuntime&) = delete;
  bool begin(
    fs::FS& filesystem,
    const FAppLoader& loader,
    FAppUiApi& uiApi,
    FAppPermissions permissions,
    FAppNativeApi * nativeApi = nullptr);
  bool start();
  void update(uint16_t instructionBudget = 256, uint32_t microsecondBudget = 4000);
  bool queueEvent(uint16_t objectId, uint8_t eventType);
  bool queuePointerEvent(
    uint16_t objectId, uint8_t eventType, int16_t x, int16_t y,
    int16_t screenX, int16_t screenY, bool pressed);
  bool hasEventHandler(uint16_t objectId, uint8_t eventType) const;
  void notifyThemeChanged();
  void notifyTimer();
  void shutdown(uint16_t instructionBudget = 512);
  void stop();

  FAppRuntimeState state() const { return state_; }
  FAppRuntimeError error() const { return error_; }
  bool isActive() const { return state_ == FAppRuntimeState::Ready || state_ == FAppRuntimeState::Running; }
  uint32_t executedInstructions() const { return executedInstructions_; }
  const char * errorMessage() const;

 private:
  struct FunctionRecord {
    uint32_t codeOffset;
    uint32_t codeSize;
    uint8_t arity;
    uint8_t localCount;
    uint8_t maxStack;
  };
  struct EventRecord { uint8_t objectId; uint8_t eventType; uint8_t functionIndex; };
  struct PendingEvent {
    int16_t x;
    int16_t y;
    int16_t screenX;
    int16_t screenY;
    uint8_t objectId;
    uint8_t eventType;
    uint8_t flags;
  };
  struct Frame {
    uint32_t pc;
    uint8_t functionIndex;
    uint8_t localBase;
    uint8_t stackBase;
  };
  static_assert(sizeof(FunctionRecord) <= 12, "FunctionRecord must remain compact");
  static_assert(sizeof(EventRecord) == 3, "EventRecord must remain compact");
  static_assert(sizeof(PendingEvent) <= 12, "PendingEvent must remain compact");
  static_assert(sizeof(Frame) <= 8, "Frame must remain compact");

  bool loadMetadata();
  bool invokeFunction(uint16_t functionIndex, uint8_t argumentCount);
  bool invokeEvent(uint16_t objectId, uint8_t eventType);
  bool enqueueEvent(const PendingEvent& event);
  bool executeInstruction();
  bool allocateValueStorage();
  void releaseValueStorage();
  bool loadExecutableCache();
  void releaseExecutableCache();
  bool readAt(uint32_t offset, uint8_t * destination, size_t size);
  bool readCode(uint8_t * destination, size_t size);
  bool readStringConstant(uint16_t index, FAppValue * result);
  bool push(const FAppValue& value);
  bool pop(FAppValue * value);
  void fault(FAppRuntimeError error);
  bool binary(uint8_t opcode);
  static uint32_t clockMicros();

  fs::FS * filesystem_;
  const FAppLoader * loader_;
  FAppUiApi * uiApi_;
  FAppNativeApi * nativeApi_;
  FAppPermissions permissions_;
  FAppRuntimeState state_;
  FAppRuntimeError error_;
  uint32_t constantsOffset_;
  uint32_t functionsOffset_;
  uint32_t eventsOffset_;
  uint32_t codeOffset_;
  uint16_t constantCount_;
  uint8_t functionCount_;
  uint8_t eventCount_;
  uint8_t globalCount_;
  uint8_t initFunction_;
  uint8_t * executableCache_;
  size_t executableCacheSize_;
  bool executableCacheExternal_;
  // Separate arrays avoid the alignment padding of 128 ConstantRecord
  // structures (8 bytes each for only 5 bytes of actual data).
  uint32_t constantOffsets_[kMaxConstants];
  uint8_t constantSizes_[kMaxConstants];
  FunctionRecord functions_[kMaxFunctions];
  EventRecord events_[kMaxEvents];
  FAppValue * valueStorage_;
  FAppValue * globals_;
  FAppValue * stack_;
  FAppValue * locals_;
  Frame frames_[kMaxCallDepth];
  PendingEvent eventQueue_[kEventQueueCapacity];
  uint8_t stackSize_;
  uint8_t localTop_;
  uint8_t frameCount_;
  uint8_t eventHead_;
  uint8_t eventCountQueued_;
  bool startEventPending_;
  uint32_t executedInstructions_;
};
