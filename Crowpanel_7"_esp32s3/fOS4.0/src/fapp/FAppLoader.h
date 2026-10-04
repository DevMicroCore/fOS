#pragma once

#include <stddef.h>
#include <stdint.h>
#include <FS.h>

enum class FAppLoaderStatus : uint8_t {
  NotInitialized = 0,
  Idle,
  InvalidPath,
  UiMissing,
  LegacyUiOnly,
  ExecutableReady,
  ExecutableOpenFailed,
  ExecutableTooSmall,
  InvalidExecutable,
  IncompatibleExecutable,
  ExecutableChecksumFailed
};

class FAppLoader {
 public:
  static constexpr size_t kPathCapacity = 192;

  FAppLoader();

  void begin(fs::FS& filesystem);
  void reset();

  FAppLoaderStatus prepare(
    const char * appDirectory,
    const char * uiFilename,
    const char * executableFilename);

  FAppLoaderStatus status() const;
  bool hasExecutable() const;
  size_t executableSize() const;
  uint8_t formatMajor() const;
  uint8_t formatMinor() const;
  uint8_t bytecodeMajor() const;
  uint8_t bytecodeMinor() const;
  uint16_t functionCount() const;
  uint16_t eventCount() const;
  uint16_t globalCount() const;
  uint16_t initFunction() const;
  uint32_t codeOffset() const;
  uint32_t codeSize() const;
  uint32_t sourceHash() const;
  const char * appDirectory() const;
  const char * uiPath() const;
  const char * executablePath() const;

 private:
  static bool isSafeFilename(const char * filename);
  static bool composePath(
    char * destination,
    size_t destinationSize,
    const char * directory,
    const char * filename);
  FAppLoaderStatus validateExecutable(fs::File& executable);

  fs::FS * filesystem_;
  FAppLoaderStatus status_;
  size_t executableSize_;
  uint8_t formatMajor_;
  uint8_t formatMinor_;
  uint8_t bytecodeMajor_;
  uint8_t bytecodeMinor_;
  uint16_t functionCount_;
  uint16_t eventCount_;
  uint16_t globalCount_;
  uint16_t initFunction_;
  uint32_t codeOffset_;
  uint32_t codeSize_;
  uint32_t sourceHash_;
  char appDirectory_[kPathCapacity];
  char uiPath_[kPathCapacity];
  char executablePath_[kPathCapacity];
};
