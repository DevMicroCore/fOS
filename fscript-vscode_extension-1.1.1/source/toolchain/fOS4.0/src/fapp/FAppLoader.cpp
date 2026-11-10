#include "FAppLoader.h"

#include "FAppFormat.h"
#include "../core/FOSVersion.h"

#include <stdio.h>
#include <string.h>

FAppLoader::FAppLoader()
  : filesystem_(nullptr),
    status_(FAppLoaderStatus::NotInitialized),
    executableSize_(0),
    formatMajor_(0),
    formatMinor_(0),
    bytecodeMajor_(0),
    bytecodeMinor_(0),
    functionCount_(0),
    eventCount_(0),
    globalCount_(0),
    initFunction_(0),
    codeOffset_(0),
    codeSize_(0),
    sourceHash_(0)
{
  appDirectory_[0] = '\0';
  uiPath_[0] = '\0';
  executablePath_[0] = '\0';
}

void FAppLoader::begin(fs::FS& filesystem)
{
  filesystem_ = &filesystem;
  reset();
}

void FAppLoader::reset()
{
  status_ = filesystem_ == nullptr
    ? FAppLoaderStatus::NotInitialized
    : FAppLoaderStatus::Idle;
  executableSize_ = 0;
  formatMajor_ = 0;
  formatMinor_ = 0;
  bytecodeMajor_ = 0;
  bytecodeMinor_ = 0;
  functionCount_ = 0;
  eventCount_ = 0;
  globalCount_ = 0;
  initFunction_ = 0;
  codeOffset_ = 0;
  codeSize_ = 0;
  sourceHash_ = 0;
  appDirectory_[0] = '\0';
  uiPath_[0] = '\0';
  executablePath_[0] = '\0';
}

FAppLoaderStatus FAppLoader::prepare(
  const char * appDirectory,
  const char * uiFilename,
  const char * executableFilename)
{
  reset();

  if (filesystem_ == nullptr) {
    return status_;
  }

  if (appDirectory == nullptr || appDirectory[0] != '/' ||
      strstr(appDirectory, "..") != nullptr || strchr(appDirectory, '\\') != nullptr ||
      !isSafeFilename(uiFilename) || !isSafeFilename(executableFilename)) {
    status_ = FAppLoaderStatus::InvalidPath;
    return status_;
  }

  const int directoryLength = snprintf(
    appDirectory_,
    sizeof(appDirectory_),
    "%s",
    appDirectory);

  if (directoryLength < 1 ||
      static_cast<size_t>(directoryLength) >= sizeof(appDirectory_) ||
      !composePath(uiPath_, sizeof(uiPath_), appDirectory_, uiFilename) ||
      !composePath(executablePath_, sizeof(executablePath_), appDirectory_, executableFilename)) {
    reset();
    status_ = FAppLoaderStatus::InvalidPath;
    return status_;
  }

  if (!filesystem_->exists(uiPath_)) {
    status_ = FAppLoaderStatus::UiMissing;
    return status_;
  }

  if (!filesystem_->exists(executablePath_)) {
    status_ = FAppLoaderStatus::LegacyUiOnly;
    return status_;
  }

  fs::File executable = filesystem_->open(executablePath_, FILE_READ);
  if (!executable) {
    status_ = FAppLoaderStatus::ExecutableOpenFailed;
    return status_;
  }

  executableSize_ = executable.size();
  status_ = validateExecutable(executable);
  executable.close();
  return status_;
}

FAppLoaderStatus FAppLoader::status() const
{
  return status_;
}

bool FAppLoader::hasExecutable() const
{
  return status_ == FAppLoaderStatus::ExecutableReady;
}

size_t FAppLoader::executableSize() const
{
  return executableSize_;
}

uint8_t FAppLoader::formatMajor() const { return formatMajor_; }
uint8_t FAppLoader::formatMinor() const { return formatMinor_; }
uint8_t FAppLoader::bytecodeMajor() const { return bytecodeMajor_; }
uint8_t FAppLoader::bytecodeMinor() const { return bytecodeMinor_; }
uint16_t FAppLoader::functionCount() const { return functionCount_; }
uint16_t FAppLoader::eventCount() const { return eventCount_; }
uint16_t FAppLoader::globalCount() const { return globalCount_; }
uint16_t FAppLoader::initFunction() const { return initFunction_; }
uint32_t FAppLoader::codeOffset() const { return codeOffset_; }
uint32_t FAppLoader::codeSize() const { return codeSize_; }
uint32_t FAppLoader::sourceHash() const { return sourceHash_; }

const char * FAppLoader::appDirectory() const
{
  return appDirectory_;
}

const char * FAppLoader::uiPath() const
{
  return uiPath_;
}

const char * FAppLoader::executablePath() const
{
  return executablePath_;
}

bool FAppLoader::isSafeFilename(const char * filename)
{
  if (filename == nullptr || filename[0] == '\0') {
    return false;
  }

  return strchr(filename, '/') == nullptr &&
         strchr(filename, '\\') == nullptr &&
         strstr(filename, "..") == nullptr;
}

bool FAppLoader::composePath(
  char * destination,
  size_t destinationSize,
  const char * directory,
  const char * filename)
{
  if (destination == nullptr || destinationSize == 0 ||
      directory == nullptr || filename == nullptr) {
    return false;
  }

  const size_t directoryLength = strlen(directory);
  const bool hasTrailingSlash = directoryLength > 0 && directory[directoryLength - 1] == '/';
  const int written = snprintf(
    destination,
    destinationSize,
    hasTrailingSlash ? "%s%s" : "%s/%s",
    directory,
    filename);

  return written > 0 && static_cast<size_t>(written) < destinationSize;
}

FAppLoaderStatus FAppLoader::validateExecutable(fs::File& executable)
{
  if (executableSize_ < FAppFormat::kHeaderSize) {
    return FAppLoaderStatus::ExecutableTooSmall;
  }

  uint8_t header[FAppFormat::kHeaderSize];
  if (!executable.seek(0) || executable.read(header, sizeof(header)) != sizeof(header)) {
    return FAppLoaderStatus::ExecutableOpenFailed;
  }
  if (header[0] != 'F' || header[1] != 'A' || header[2] != 'P' || header[3] != 'P' ||
      FAppFormat::readU16(header + 8) != FAppFormat::kHeaderSize ||
      FAppFormat::readU32(header + 12) != executableSize_) {
    return FAppLoaderStatus::InvalidExecutable;
  }

  formatMajor_ = header[4];
  formatMinor_ = header[5];
  bytecodeMajor_ = header[6];
  bytecodeMinor_ = header[7];
  if (formatMajor_ != FAppFormat::kFormatMajor ||
      formatMinor_ > FAppFormat::kFormatMinor ||
      bytecodeMajor_ != FAppFormat::kBytecodeMajor ||
      bytecodeMinor_ > FAppFormat::kBytecodeMinor) {
    return FAppLoaderStatus::IncompatibleExecutable;
  }

  const uint16_t minimumMajor = FAppFormat::readU16(header + 68);
  const uint16_t minimumMinor = FAppFormat::readU16(header + 70);
  const uint16_t minimumPatch = FAppFormat::readU16(header + 72);
  const bool requiresNewerFos =
    minimumMajor > FOSVersion::kMajor ||
    (minimumMajor == FOSVersion::kMajor && minimumMinor > FOSVersion::kMinor) ||
    (minimumMajor == FOSVersion::kMajor && minimumMinor == FOSVersion::kMinor &&
     minimumPatch > FOSVersion::kPatch);
  if (requiresNewerFos) {
    return FAppLoaderStatus::IncompatibleExecutable;
  }

  const uint32_t fileSize = static_cast<uint32_t>(executableSize_);
  const uint32_t constantsOffset = FAppFormat::readU32(header + 20);
  const uint32_t constantsSize = FAppFormat::readU32(header + 24);
  const uint32_t functionsOffset = FAppFormat::readU32(header + 32);
  const uint32_t functionsSize = FAppFormat::readU32(header + 36);
  functionCount_ = FAppFormat::readU16(header + 40);
  initFunction_ = FAppFormat::readU16(header + 42);
  const uint32_t eventsOffset = FAppFormat::readU32(header + 44);
  const uint32_t eventsSize = FAppFormat::readU32(header + 48);
  eventCount_ = FAppFormat::readU16(header + 52);
  codeOffset_ = FAppFormat::readU32(header + 56);
  codeSize_ = FAppFormat::readU32(header + 60);
  globalCount_ = FAppFormat::readU16(header + 30);
  sourceHash_ = FAppFormat::readU32(header + 64);

  if (!FAppFormat::rangeIsValid(constantsOffset, constantsSize, fileSize) ||
      !FAppFormat::rangeIsValid(functionsOffset, functionsSize, fileSize) ||
      !FAppFormat::rangeIsValid(eventsOffset, eventsSize, fileSize) ||
      !FAppFormat::rangeIsValid(codeOffset_, codeSize_, fileSize) ||
      constantsOffset < FAppFormat::kHeaderSize ||
      constantsOffset + constantsSize > functionsOffset ||
      functionsOffset + functionsSize > eventsOffset ||
      eventsOffset + eventsSize > codeOffset_ ||
      codeOffset_ + codeSize_ != fileSize ||
      functionsSize != static_cast<uint32_t>(functionCount_) * FAppFormat::kFunctionRecordSize ||
      eventsSize != static_cast<uint32_t>(eventCount_) * FAppFormat::kEventRecordSize ||
      functionCount_ == 0 || initFunction_ >= functionCount_) {
    return FAppLoaderStatus::InvalidExecutable;
  }

  const uint32_t expectedCrc = FAppFormat::readU32(header + FAppFormat::kCrcOffset);
  if (!executable.seek(0)) return FAppLoaderStatus::ExecutableOpenFailed;
  uint8_t buffer[256];
  uint32_t crc = 0xFFFFFFFFu;
  uint32_t absoluteOffset = 0;
  while (absoluteOffset < fileSize) {
    const size_t requested = fileSize - absoluteOffset > sizeof(buffer)
      ? sizeof(buffer)
      : static_cast<size_t>(fileSize - absoluteOffset);
    const size_t readCount = executable.read(buffer, requested);
    if (readCount != requested) return FAppLoaderStatus::ExecutableOpenFailed;
    for (size_t index = 0; index < readCount; ++index) {
      const uint32_t position = absoluteOffset + static_cast<uint32_t>(index);
      const uint8_t byte = position >= FAppFormat::kCrcOffset &&
                           position < FAppFormat::kCrcOffset + FAppFormat::kCrcSize
        ? 0
        : buffer[index];
      crc = FAppFormat::crc32Update(crc, byte);
    }
    absoluteOffset += static_cast<uint32_t>(readCount);
  }
  if (~crc != expectedCrc) return FAppLoaderStatus::ExecutableChecksumFailed;
  return FAppLoaderStatus::ExecutableReady;
}
